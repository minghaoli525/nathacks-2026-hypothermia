#pragma once

#include <stdint.h>
#include <math.h>

namespace Osborn {
constexpr uint8_t Before = 15; // 120 ms at 125 Hz.
constexpr uint8_t After = 25;  // 200 ms.
constexpr uint8_t WindowSize = Before + After + 1;

struct Features {
  int8_t anchor = Before;
  int8_t qrsEnd = -1;
  int8_t peak = -1;
  float anchorHeight = 0;
  float peakHeight = 0;
  float prominence = 0;
  float rise = 0; // Magnitude from left shoulder to peak in ADC counts.
};

// Analyze a lightly smoothed beat with the filtered QRS detection at Before.
// This heuristic locates a distinct bump, not a terminal slur or a diagnosis.
inline Features locate(const float *beat, float minimumCounts) {
  Features f;
  float baseline = 0;
  float low = beat[0], high = beat[0];
  for (uint8_t i = 0; i < 5; ++i) {
    baseline += beat[i];
    low = fminf(low, beat[i]);
    high = fmaxf(high, beat[i]);
  }
  baseline /= 5.0f;
  for (uint8_t i = Before - 2; i <= Before + 2; ++i) {
    if (fabsf(beat[i] - baseline) > fabsf(beat[f.anchor] - baseline)) f.anchor = i;
  }
  f.anchorHeight = beat[f.anchor] - baseline;
  const float qrsHeight = fabsf(f.anchorHeight);
  const float noise = high - low;
  if (qrsHeight < 4.0f * minimumCounts || noise > 0.20f * qrsHeight) return f;

  float qrsSlope = 0;
  for (int i = f.anchor - 5; i <= f.anchor + 4; ++i) {
    qrsSlope = fmaxf(qrsSlope, fabsf(beat[i] - beat[i - 1]));
  }
  const float quiet = fmaxf(minimumCounts, fmaxf(1.25f * noise, 0.12f * qrsSlope));
  for (int i = f.anchor + 2; i <= f.anchor + 15; ++i) {
    if (fabsf(beat[i] - baseline) <= 0.45f * qrsHeight &&
        fabsf(beat[i + 1] - beat[i]) <= quiet &&
        fabsf(beat[i + 2] - beat[i + 1]) <= quiet) {
      f.qrsEnd = i;
      break;
    }
  }
  if (f.qrsEnd < 0) return f;

  const float minimumProminence = fmaxf(minimumCounts, 3.0f * noise);
  // Require a local rise and fall, with shoulders after the estimated QRS end.
  for (int i = f.qrsEnd + 2; i <= f.qrsEnd + 6 && i + 3 < WindowSize; ++i) {
    const float height = beat[i] - baseline;
    const float polarity = height < 0 ? -1.0f : 1.0f;
    const float rise = polarity * (beat[i] - beat[i - 2]);
    const float fall = polarity * (beat[i] - beat[i + 3]);
    const float prominence = fminf(rise, fall);
    if (polarity * (beat[i] - beat[i - 1]) < 0 ||
        polarity * (beat[i] - beat[i + 1]) < 0 ||
        fabsf(height) < minimumProminence || prominence < minimumProminence ||
        prominence <= f.prominence) continue;
    f.peak = i;
    f.peakHeight = height;
    f.prominence = prominence;
    f.rise = rise;
  }
  return f;
}
}
