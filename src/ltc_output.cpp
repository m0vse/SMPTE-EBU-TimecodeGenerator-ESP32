/* Gapless LTC output using one continuous ESP-IDF 5 RMT transaction.
 * Licensed under the Apache License, Version 2.0.
 */

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#ifdef LTC_TIMING_DIAGNOSTIC
#include "driver/mcpwm_cap.h"
#endif
#include "esp_attr.h"
#include "esp_err.h"

#include "firmware_config.h"
#include "ltc_frame_builder.h"
#include "ltc_output.h"
#include "ntp_clock.h"

static constexpr size_t LTC_BITS_PER_FRAME = 80;
static constexpr size_t SYMBOLS_PER_BUFFER =
    LTC_BITS_PER_FRAME * LTC_FRAMES_PER_BUFFER;
static constexpr size_t STREAM_BUFFER_COUNT = 2;
static constexpr uint32_t RMT_RESOLUTION_HZ = 1000000;
static constexpr size_t RMT_MEMORY_SYMBOLS = 64;

static rmt_channel_handle_t txChannel = nullptr;
static rmt_encoder_handle_t streamEncoder = nullptr;
static DRAM_ATTR
    rmt_symbol_word_t symbolBuffers[STREAM_BUFFER_COUNT][SYMBOLS_PER_BUFFER];
static DRAM_ATTR uint8_t streamPayload = 0;
static portMUX_TYPE rmtMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t completedStreamBuffers = 0;
static volatile size_t streamReadBuffer = 0;
static volatile size_t streamReadSymbol = 0;
static size_t nextRefillBuffer = 0;
static uint32_t underruns = 0;
static bool outputRunning = false;
static bool outputRequested = true;
static bool outputInitialized = false;
static uint8_t outputLevel = 0;
static double durationDither = 0.0;
static double frequencyCorrectionPpm = 0.0;
static uint32_t outputStartMicros = 0;

#ifdef LTC_TIMING_DIAGNOSTIC
static mcpwm_cap_timer_handle_t timingCaptureTimer = nullptr;
static mcpwm_cap_channel_handle_t timingCaptureChannel = nullptr;
static uint32_t timingResolutionHz = 80000000;
static volatile uint32_t timingLastCapture = 0;
static volatile uint32_t timingMinIntervalTicks = UINT32_MAX;
static volatile uint32_t timingMaxIntervalTicks = 0;
static volatile uint32_t timingOver510Micros = 0;
static volatile uint32_t timingOver550Micros = 0;
static volatile uint32_t timingEdgeCount = 0;

static bool IRAM_ATTR onTimingCapture(
    mcpwm_cap_channel_handle_t, const mcpwm_capture_event_data_t *event,
    void *) {
  const uint32_t now = event->cap_value;
  const uint32_t previous = timingLastCapture;
  timingLastCapture = now;
  if (previous == 0) {
    return false;
  }
  const uint32_t interval = now - previous;
  if (interval < timingMinIntervalTicks) {
    timingMinIntervalTicks = interval;
  }
  if (interval > timingMaxIntervalTicks) {
    timingMaxIntervalTicks = interval;
  }
  if (static_cast<uint64_t>(interval) * 1000000ULL >
      static_cast<uint64_t>(timingResolutionHz) * 510ULL) {
    timingOver510Micros = timingOver510Micros + 1U;
  }
  if (static_cast<uint64_t>(interval) * 1000000ULL >
      static_cast<uint64_t>(timingResolutionHz) * 550ULL) {
    timingOver550Micros = timingOver550Micros + 1U;
  }
  timingEdgeCount = timingEdgeCount + 1U;
  return false;
}
#endif

static uint16_t nextHalfBitDuration() {
  const double nominalTicks =
      static_cast<double>(RMT_RESOLUTION_HZ) /
      static_cast<double>(LTC_BITS_PER_FRAME * FPS * 2U);
  const double rateMultiplier = 1.0 + frequencyCorrectionPpm / 1000000.0;
  const double exactTicks = nominalTicks / rateMultiplier;

  const double accumulated = exactTicks + durationDither;
  uint16_t duration = static_cast<uint16_t>(floor(accumulated));
  durationDither = accumulated - static_cast<double>(duration);
  return duration == 0 ? 1 : duration;
}

static void fillSymbolBuffer(size_t bufferIndex) {
  uint8_t frame[10]{};
  size_t symbolIndex = 0;

  for (size_t frameIndex = 0; frameIndex < LTC_FRAMES_PER_BUFFER;
       ++frameIndex) {
    fillNextBlock(frame, FPS);
    for (size_t bit = 0; bit < LTC_BITS_PER_FRAME; ++bit) {
      const bool one = ((frame[bit >> 3U] >> (bit & 7U)) & 1U) != 0U;
      rmt_symbol_word_t &symbol = symbolBuffers[bufferIndex][symbolIndex++];
      symbol.duration0 = nextHalfBitDuration();
      symbol.level0 = outputLevel;
      symbol.duration1 = nextHalfBitDuration();
      symbol.level1 = one ? !outputLevel : outputLevel;
      if (!one) {
        outputLevel = !outputLevel;
      }
    }
  }
}

static size_t IRAM_ATTR encodeContinuousLtc(
    const void *, size_t, size_t, size_t symbolsFree,
    rmt_symbol_word_t *symbols, bool *done, void *) {
  *done = false;
  size_t written = 0;

  while (written < symbolsFree) {
    const size_t bufferIndex = streamReadBuffer;
    const size_t sourceIndex = streamReadSymbol;
    const size_t available = SYMBOLS_PER_BUFFER - sourceIndex;
    const size_t count =
        available < (symbolsFree - written) ? available
                                            : (symbolsFree - written);
    memcpy(symbols + written, symbolBuffers[bufferIndex] + sourceIndex,
           count * sizeof(rmt_symbol_word_t));
    written += count;
    streamReadSymbol = sourceIndex + count;

    if (streamReadSymbol == SYMBOLS_PER_BUFFER) {
      streamReadSymbol = 0;
      streamReadBuffer = (bufferIndex + 1U) % STREAM_BUFFER_COUNT;
      portENTER_CRITICAL_ISR(&rmtMux);
      completedStreamBuffers = completedStreamBuffers + 1U;
      portEXIT_CRITICAL_ISR(&rmtMux);
    }
  }
  return written;
}

void rmt_setup(gpio_num_t pin) {
  if (outputInitialized) {
    return;
  }

  rmt_tx_channel_config_t channelConfig{};
  channelConfig.clk_src = RMT_CLK_SRC_DEFAULT;
  channelConfig.gpio_num = pin;
  channelConfig.mem_block_symbols = RMT_MEMORY_SYMBOLS;
  channelConfig.resolution_hz = RMT_RESOLUTION_HZ;
  channelConfig.trans_queue_depth = 1;
  channelConfig.intr_priority = 0;
  ESP_ERROR_CHECK(rmt_new_tx_channel(&channelConfig, &txChannel));

  rmt_simple_encoder_config_t encoderConfig{};
  encoderConfig.callback = encodeContinuousLtc;
  encoderConfig.min_chunk_size = 1;
  ESP_ERROR_CHECK(rmt_new_simple_encoder(&encoderConfig, &streamEncoder));

#ifdef LTC_TIMING_DIAGNOSTIC
  mcpwm_capture_timer_config_t timerConfig{};
  timerConfig.group_id = 0;
  timerConfig.clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT;
  timerConfig.resolution_hz = 1000000;
  ESP_ERROR_CHECK(
      mcpwm_new_capture_timer(&timerConfig, &timingCaptureTimer));
  ESP_ERROR_CHECK(mcpwm_capture_timer_get_resolution(
      timingCaptureTimer, &timingResolutionHz));

  mcpwm_capture_channel_config_t captureConfig{};
  captureConfig.gpio_num = pin;
  captureConfig.prescale = 1;
  captureConfig.flags.pos_edge = 1;
  captureConfig.flags.neg_edge = 1;
  captureConfig.flags.io_loop_back = 1;
  ESP_ERROR_CHECK(mcpwm_new_capture_channel(
      timingCaptureTimer, &captureConfig, &timingCaptureChannel));

  mcpwm_capture_event_callbacks_t captureCallbacks{};
  captureCallbacks.on_cap = onTimingCapture;
  ESP_ERROR_CHECK(mcpwm_capture_channel_register_event_callbacks(
      timingCaptureChannel, &captureCallbacks, nullptr));
  ESP_ERROR_CHECK(mcpwm_capture_timer_enable(timingCaptureTimer));
  ESP_ERROR_CHECK(mcpwm_capture_channel_enable(timingCaptureChannel));
  ESP_ERROR_CHECK(mcpwm_capture_timer_start(timingCaptureTimer));
#endif
  outputInitialized = true;
}

void rmt_start() {
  if (!outputInitialized || outputRunning) {
    return;
  }

  outputLevel = 0;
  durationDither = 0.0;
  nextRefillBuffer = 0;
  streamReadBuffer = 0;
  streamReadSymbol = 0;
  underruns = 0;
  portENTER_CRITICAL(&rmtMux);
  completedStreamBuffers = 0;
  portEXIT_CRITICAL(&rmtMux);

  for (size_t i = 0; i < STREAM_BUFFER_COUNT; ++i) {
    fillSymbolBuffer(i);
  }

  ESP_ERROR_CHECK(rmt_encoder_reset(streamEncoder));
  ESP_ERROR_CHECK(rmt_enable(txChannel));
  rmt_transmit_config_t transmitConfig{};
  transmitConfig.loop_count = 0;
  transmitConfig.flags.eot_level = 0;
  outputStartMicros = micros();
  ESP_ERROR_CHECK(rmt_transmit(txChannel, streamEncoder, &streamPayload,
                               sizeof(streamPayload), &transmitConfig));
  outputRunning = true;
  outputRequested = true;
  Serial.printf(
      "LTC output started: %u fps, %.2f bits/s, %.3f s refill reserve\n",
      FPS, static_cast<double>(LTC_BITS_PER_FRAME * FPS),
      static_cast<double>(FIDDLE_BUFFER_DELAY));
}

void rmt_loop() {
  if (!outputRunning) {
    return;
  }

#ifdef LTC_TIMING_DIAGNOSTIC
  static unsigned long lastTimingReport = 0;
  if (millis() - lastTimingReport >= 5000) {
    lastTimingReport = millis();
    portENTER_CRITICAL(&rmtMux);
    const uint32_t edges = timingEdgeCount;
    const uint32_t minimumTicks = timingMinIntervalTicks;
    const uint32_t maximumTicks = timingMaxIntervalTicks;
    const uint32_t over510 = timingOver510Micros;
    const uint32_t over550 = timingOver550Micros;
    timingEdgeCount = 0;
    timingMinIntervalTicks = UINT32_MAX;
    timingMaxIntervalTicks = 0;
    timingOver510Micros = 0;
    timingOver550Micros = 0;
    portEXIT_CRITICAL(&rmtMux);
    const double minimumMicros =
        static_cast<double>(minimumTicks) * 1000000.0 /
        static_cast<double>(timingResolutionHz);
    const double maximumMicros =
        static_cast<double>(maximumTicks) * 1000000.0 /
        static_cast<double>(timingResolutionHz);
    Serial.printf(
        "RMT edge timing: %lu edges, %.2f..%.2f us, >510: %lu, >550: %lu\n",
        static_cast<unsigned long>(edges), minimumMicros, maximumMicros,
        static_cast<unsigned long>(over510),
        static_cast<unsigned long>(over550));
  }
#endif

  while (true) {
    uint32_t pending = 0;
    portENTER_CRITICAL(&rmtMux);
    pending = completedStreamBuffers;
    if (completedStreamBuffers > 0) {
      completedStreamBuffers = completedStreamBuffers - 1U;
    }
    portEXIT_CRITICAL(&rmtMux);
    if (pending == 0) {
      break;
    }
    if (pending >= STREAM_BUFFER_COUNT) {
      underruns = underruns + 1U;
    }

    fillSymbolBuffer(nextRefillBuffer);
    nextRefillBuffer = (nextRefillBuffer + 1U) % STREAM_BUFFER_COUNT;
  }
}

bool rmtOutputEnabled() {
  return outputRunning;
}

bool rmtOutputRequested() {
  return outputRequested;
}

void rmtSetOutputEnabled(bool enabled) {
  outputRequested = enabled;
  if (enabled == outputRunning) {
    return;
  }
  if (enabled) {
    ntpRestartOutput();
    return;
  }

  outputRunning = false;
  const esp_err_t disableResult = rmt_disable(txChannel);
  if (disableResult != ESP_OK) {
    Serial.printf("RMT disable: %s\n", esp_err_to_name(disableResult));
  }
  digitalWrite(RED_PIN, LOW);
  portENTER_CRITICAL(&rmtMux);
  completedStreamBuffers = 0;
  portEXIT_CRITICAL(&rmtMux);
  ntpOutputStopped();
  Serial.println("LTC output stopped");
}

void rmtSetFrequencyCorrectionPpm(double correctionPpm) {
  frequencyCorrectionPpm = constrain(correctionPpm, -500.0, 500.0);
}

double rmtFrequencyCorrectionPpm() {
  return frequencyCorrectionPpm;
}

uint32_t rmtUnderrunCount() {
  return underruns;
}

uint32_t rmtOutputStartMicros() {
  return outputStartMicros;
}
