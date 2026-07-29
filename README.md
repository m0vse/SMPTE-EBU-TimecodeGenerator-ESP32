# ESP32 SMPTE/EBU timecode generator

An NTP-disciplined, 25 or 30 fps linear timecode (LTC) generator for Leitch
and similar studio/broadcast clocks. An ESP32 produces a continuous
biphase-mark signal using its RMT peripheral and provides a responsive web
interface for status, configuration, provisioning, and output control.

![Analog studio clock](images/analog.png)
![Digital studio clock](images/digital.jpg)

## Current platform

- PlatformIO project
- Conventional `src/*.cpp` firmware layout with explicit module headers in
  `include/`; no Arduino sketch preprocessing
- Arduino-ESP32 3.3.8
- ESP-IDF 5.5.4
- PioArduino Espressif32 platform 55.03.38-1
- No third-party runtime libraries; networking, OTA, Preferences, mDNS, and
  WebServer come from the pinned ESP32 core

The versions are pinned in `platformio.ini`, so local and CI builds use the
same toolchain.

## Build and upload

1. Install [PlatformIO](https://platformio.org/).
2. Copy `include/secrets.example.h` to `include/secrets.h`.
3. Set the Wi-Fi, web, OTA, and provisioning credentials in `secrets.h`.
   This file is ignored by Git.
4. Build and upload:

   ```text
   pio run
   pio run --target upload --upload-port COM7
   pio device monitor --port COM7 --baud 115200
   ```

The station hostname is selected from the sense input:
`smpte-digital-clock` or `smpte-analog-clock`. The UI is available at
`http://<hostname>.local/` or the DHCP address.

If the configured network cannot be reached for 20 seconds, the device starts
a password-protected `smpte-setup-xxxxxx` access point. Connect to it and open
the device address to update Wi-Fi settings. Configuration is stored in NVS.

Web control uses HTTP authentication when a web password is configured. OTA
also requires its configured password. Keep this device on a trusted management
network and do not commit `secrets.h`.

## LTC timing: do not replace with queued transactions

Some broadcast clocks reject even a very short pause or a skipped frame. An
ordinary sequence of IDF 5 `rmt_transmit()` jobs was measured to insert about
50 microseconds at transaction boundaries, so this project deliberately does
not use that approach.

The implementation uses:

- one RMT transaction that never completes during normal operation;
- an IDF 5 simple encoder that continuously feeds RMT memory;
- two application-side symbol buffers, refilled outside interrupt context;
- 1 MHz RMT resolution with fractional-tick dithering;
- complete 80-bit LTC frames carried seamlessly across the 64-symbol hardware
  memory boundary.

The optional `LTC_TIMING_DIAGNOSTIC` build flag routes the output internally to
an MCPWM hardware capture channel. On the target ESP32, sustained capture
measured 249–500 microsecond edge intervals and no intervals over 510
microseconds across refill boundaries. The flag is for validation only and
should not be enabled in normal firmware.

## Clock discipline

ESP-IDF smooth SNTP disciplines the ESP32 system clock rather than stepping it
on every poll. The LTC clock has a second, bounded PI control loop:

- initial time is acquired from the valid system clock;
- output waits for the first real NTP response after every boot, so a stale
  timestamp retained across OTA cannot be emitted;
- the currently emitted LTC phase is modeled independently of the look-ahead
  buffers;
- phase error is low-pass filtered;
- RMT half-bit durations are slewed within ±500 ppm;
- frames are not periodically skipped or repeated to correct small errors;
- a gross clock discontinuity (including a daylight-saving transition) forces
  an explicit LTC reacquisition instead of attempting a multi-day slew;
- the controller handles midnight wrap correctly.

The default NTP poll interval is 15 minutes. The web UI reports phase error,
frequency correction, NTP state, and buffer underruns.

The default POSIX timezone is:

```text
GMT0BST,M3.5.0/1,M10.5.0/2
```

This applies UK daylight-saving rules. The UI accepts another POSIX timezone
and a fractional output offset ("fiddle factor").

## Frame rate

The default is EBU 25 fps. Build non-drop-frame 30 fps with
`pio run -e esp32dev_30fps`.

Only 25 and 30 fps are supported.

## Hardware

The red timecode output is GPIO 13, signal return is GPIO 12, and the
analog/digital model sense input is GPIO 14. The existing transistor interface
can raise the ESP32 signal to the level expected by older clocks. Some
capacitively coupled clock inputs need a 10 µF series capacitor so the LTC
waveform crosses ground.

The auxiliary 5 V rail in some clock revisions is marginal during ESP32 RF
startup. Firmware reduces the CPU clock to 80 MHz, selects maximum modem sleep,
uses 2 dBm transmit power, and masks brownout reset only during the first few
seconds of radio initialization. Brownout protection is then restored. For a
new hardware revision, a better regulator and additional local bulk/decoupling
capacitance are still recommended.

Original interface schematic:
[EasyEDA SMPTE ESP32 LTC NTP](https://easyeda.com/dirkx/smpte-esp32-ltc-ntp)

## Tests

Host tests (Linux/macOS or Windows with GCC available):

```text
pio test -e native
```

Tests can also run on an attached ESP32:

```text
pio test -e esp32dev --upload-port COM7
```

They cover BCD conversion, 25/30 fps rollover, LTC sync/parity, midnight phase
wrap, and discipline direction/limits.

## Historical experimental sketches

`legacy/experimental/` retains the historical experimental sketches and is not
part of the PlatformIO build. Their useful concept—persistent configuration—has been
reimplemented in the main firmware. Its mutex handling, one-shot GPS/NMEA
connection, NTP disabling behavior, open provisioning AP, credentials in URLs,
and non-continuous RMT control are not suitable for production and should not
be copied into the main path.

## License

Apache License 2.0. See [LICENSE](LICENSE).
