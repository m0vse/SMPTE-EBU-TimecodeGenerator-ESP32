#include "ClockDiscipline.h"

#include <algorithm>

namespace {
constexpr double kMaximumCorrectionPpm = 500.0;
constexpr double kProportionalGain = 3000.0;
constexpr double kIntegralGain = 0.25;
constexpr double kMaximumIntegralPpm = 150.0;
}  // namespace

double wrapDayPhaseError(double error_seconds) {
  while (error_seconds > 43200.0) {
    error_seconds -= 86400.0;
  }
  while (error_seconds < -43200.0) {
    error_seconds += 86400.0;
  }
  return error_seconds;
}

void resetClockDiscipline(ClockDisciplineState &state) {
  state = {};
}

double updateClockDiscipline(ClockDisciplineState &state,
                             double raw_phase_error_seconds,
                             double elapsed_seconds) {
  state.filtered_phase_seconds =
      0.80 * state.filtered_phase_seconds + 0.20 * raw_phase_error_seconds;
  state.integral_ppm +=
      state.filtered_phase_seconds * elapsed_seconds * kIntegralGain;
  state.integral_ppm =
      std::min(std::max(state.integral_ppm, -kMaximumIntegralPpm),
               kMaximumIntegralPpm);
  const double correction =
      state.filtered_phase_seconds * kProportionalGain + state.integral_ppm;
  return std::min(std::max(correction, -kMaximumCorrectionPpm),
                  kMaximumCorrectionPpm);
}
