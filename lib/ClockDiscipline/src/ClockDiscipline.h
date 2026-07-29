#pragma once

struct ClockDisciplineState {
  double filtered_phase_seconds = 0.0;
  double integral_ppm = 0.0;
};

double wrapDayPhaseError(double error_seconds);
void resetClockDiscipline(ClockDisciplineState &state);
double updateClockDiscipline(ClockDisciplineState &state,
                             double raw_phase_error_seconds,
                             double elapsed_seconds);
