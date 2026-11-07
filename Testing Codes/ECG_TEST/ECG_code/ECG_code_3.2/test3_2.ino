// ECG Filter - BioAmp EXG Pill
// https://github.com/upsidedownlabs/BioAmp-EXG-Pill

// Upside Down Labs invests time and resources providing this open source code,
// please support Upside Down Labs and open-source hardware by purchasing
// products from Upside Down Labs!

// Copyright (c) 2021 Upside Down Labs - contact@upsidedownlabs.tech

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// At Upside Down Labs, we create open-source DIY neuroscience hardware and software.
// Our mission is to make neuroscience affordable and accessible for everyone.
// By supporting us with your purchase, you help spread innovation and open science.
// Thank you for being part of this journey with us!




#include <Arduino.h>
#include <math.h>
#include "MitBih100.h"
#include "Osborn.h"
#include "PeakWindow.h"

#if defined(ARDUINO_ARCH_ESP32)   // if the board is using ESP the input terminal and setting are as follows
constexpr uint8_t InputPin = 36;  // VP on the ESP32 DevKitC
constexpr int AdcMax = 4095;      // Max analoge to digital converter range for ESP32
constexpr int ReplayGain = 4;
#else
constexpr uint8_t InputPin = A0;
constexpr int AdcMax = 1023;      // Max analoge to digital converter range for ARDUINO
constexpr int ReplayGain = 1;     
#endif

constexpr uint32_t LiveSampleUs = 4000;    // 250 Hz
constexpr uint32_t DatasetSampleUs = 8000; // 125 Hz, matching the recording
constexpr uint8_t DatasetSampleMs = DatasetSampleUs / 1000;
constexpr uint8_t LiveRateScale = DatasetSampleUs / LiveSampleUs;
constexpr uint16_t DatasetSamples = sizeof(DatasetEcg) / sizeof(DatasetEcg[0]); // determines the number of samples in a dataset/
constexpr uint16_t ReferenceCount = sizeof(DatasetBeatMs) / sizeof(DatasetBeatMs[0]);
constexpr uint8_t RefractoryMs = 240;
constexpr uint8_t CandidateMs = 88;
// constexpr uint8_t RrSize = 3; // One-second reporting.
constexpr uint8_t HrWindowSeconds = 10;
constexpr uint16_t MaxRrMs = 4000;

constexpr float MinimumSlope = 2.0f * ReplayGain;
constexpr float MinimumAmplitude = 5.0f * ReplayGain;

enum class Source : uint8_t { Live, Dataset };
Source source = Source::Live;
uint32_t sampleUs = LiveSampleUs;
uint8_t sampleMs = LiveSampleUs / 1000;
uint16_t samplesPerSecond = 1000000UL / LiveSampleUs;
uint8_t rateScale = LiveRateScale;
// Set true to open Serial Plotter directly on boards that reset on connect.
constexpr bool StartInPlotMode = false;
bool plotMode = StartInPlotMode;
bool streamSamples = false;
bool datasetEnded = false;
// bool endReported = false; // One-second reporting.
uint32_t nextSampleUs;
uint32_t nextReportSample;
uint32_t hrWindowSamples;
uint32_t sampleIndex;
uint32_t skippedSamples;
uint32_t clippedSamples;
uint16_t referenceCursor;

float filterState[4][2];
PeakWindow peakWindow;
float candidateBaseline;
float previous[2 * LiveRateScale];
float envelope;
float noiseLevel;
float typicalSlope;
float peakSlope;
uint32_t recentSpikeIndex;
bool candidate;
uint32_t candidateStart;
uint32_t peakIndex;
float peakAmplitude;

uint32_t lastClipIndex;
bool clippedRecently;
bool haveBeat;
uint32_t lastBeatIndex;
uint32_t beatCount;
/*
uint16_t rrIntervals[RrSize];
uint8_t rrCount;
uint8_t rrNext;
*/

struct RrWindow {
  uint32_t number;
  uint32_t sumMs;
  uint32_t lastBeatMs;
  uint16_t count;
};

// A candidate near a boundary can be confirmed after the next window starts.
RrWindow rrWindows[2];

constexpr uint8_t MorphologyRingSize = 128;
static_assert(MorphologyRingSize >= (Osborn::WindowSize + 1) * LiveRateScale + 1,
              "Morphology buffer too small");
int16_t morphology[MorphologyRingSize];
uint8_t morphologySamples;
bool pendingOsborn;
bool haveOsbornReport;
bool osbornUnreadable;
bool repeatedOsborn;
uint32_t osbornAnchor;
uint32_t osbornReportAnchor;
uint32_t previousOsbornAnchor;
int8_t previousOsbornDelay;
float previousOsbornHeight;
Osborn::Features osbornFeatures;

// One queued row, drained without blocking the next ECG sample.
char tx[160];
size_t txSize = 0;
size_t txSent = 0;

float biquad(float input, uint8_t section, float a1, float a2,
             float b0, float b1, float b2) {
  float *z = filterState[section];
  const float x = input - a1 * z[0] - a2 * z[1];
  const float y = b0 * x + b1 * z[0] + b2 * z[1];
  z[1] = z[0];
  z[0] = x;
  return y;
}

float filterEcg(float input) {
  // Use the same fourth-order Butterworth band-pass at each source rate.
  if (source == Source::Live) {
    float y = biquad(input, 0, -0.49508698f, 0.09768299f, 0.03104488f, 0.06208975f, 0.03104488f);
    y = biquad(y, 1, -0.65689312f, 0.49491779f, 1.0f, 2.0f, 1.0f);
    y = biquad(y, 2, -1.97656072f, 0.97672230f, 1.0f, -2.0f, 1.0f);
    return biquad(y, 3, -1.99040690f, 0.99056496f, 1.0f, -2.0f, 1.0f);
  }
  // BioAmp's 0.5-44.5 Hz band-pass coefficients for 125 Hz.
  float y = biquad(input, 0, 0.70682283f, 0.15621030f, 0.28064917f, 0.56129834f, 0.28064917f);
  y = biquad(y, 1, 0.95028224f, 0.54073140f, 1.0f, 2.0f, 1.0f);
  y = biquad(y, 2, -1.95360385f, 0.95423412f, 1.0f, -2.0f, 1.0f);
  return biquad(y, 3, -1.98048558f, 0.98111344f, 1.0f, -2.0f, 1.0f);
}

void resetDetector() {
  memset(filterState, 0, sizeof(filterState));
  peakWindow.reset(rateScale);
  memset(previous, 0, sizeof(previous));
  envelope = 0.0f;
  noiseLevel = MinimumSlope * 0.5f;
  typicalSlope = MinimumSlope * 6.0f;
  recentSpikeIndex = 0;
  candidate = haveBeat = clippedRecently = false;
  // rrCount = rrNext = 0; // One-second reporting.
  morphologySamples = 0;
  pendingOsborn = haveOsbornReport = false;
  previousOsbornHeight = 0;
}

void startSource(Source selected) {
  source = selected;
  sampleUs = source == Source::Live ? LiveSampleUs : DatasetSampleUs;
  sampleMs = sampleUs / 1000;
  samplesPerSecond = 1000000UL / sampleUs;
  rateScale = DatasetSampleUs / sampleUs;
  sampleIndex = skippedSamples = clippedSamples = beatCount = 0;
  referenceCursor = 0;
  // datasetEnded = endReported = false; // One-second reporting.
  datasetEnded = false;
  // nextReportSample = samplesPerSecond; // One-second reporting.
  hrWindowSamples = HrWindowSeconds * (uint32_t)samplesPerSecond;
  nextReportSample = hrWindowSamples;
  memset(rrWindows, 0, sizeof(rrWindows));
  nextSampleUs = micros();
  resetDetector();
}

const char *sourceName() {
  if (source == Source::Dataset) return "mitbih_100_MLII";
  return "live";
}

/*
uint16_t averageRr(const uint16_t *values, uint8_t count) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < count; ++i) sum += values[i];
  return (uint16_t)((sum + count / 2) / count);
}
*/

void acceptBeat(uint32_t index) {
  if (haveBeat) {
    const uint32_t rrMs = (index - lastBeatIndex) * sampleMs;
    /*
    if (rrMs <= MaxRrMs) {
      rrIntervals[rrNext] = (uint16_t)rrMs;
      rrNext = (rrNext + 1) % RrSize;
      if (rrCount < RrSize) ++rrCount;
    } else {
      rrCount = rrNext = 0;
    }
    */
    if (rrMs > 0 && rrMs <= MaxRrMs) {
      // Assign an interval to the window containing its ending beat.
      const uint32_t number = index / hrWindowSamples;
      RrWindow &window = rrWindows[number % 2];
      if (window.number != number) window = RrWindow{number, 0, 0, 0};
      window.sumMs += rrMs;
      window.lastBeatMs = index * sampleMs;
      ++window.count;
    }
  }
  lastBeatIndex = index;
  haveBeat = true;
  ++beatCount;
  osbornAnchor = index;
  pendingOsborn = true;
}

// Start a candidate only when the vendor-style amplitude spike also has
// a QRS-like slope. A small post-beat deflection can otherwise double-count.
void detectBeat(float signal, uint32_t index) {
  // Compare samples 16 ms apart at either rate.
  const uint8_t lag = 2 * rateScale;
  const float slope = fabsf(signal - previous[(index - lag) % (2 * LiveRateScale)]);
  previous[index % (2 * LiveRateScale)] = signal;
  const bool live = source == Source::Live;
  const float noiseAlpha = live ? 0.00904f : 0.018f;
  envelope += (live ? 0.19377f : 0.35f) * (slope - envelope);
  if (clippedRecently) {
    if (index - lastClipIndex <= samplesPerSecond / 2) return;
    clippedRecently = false;
  }

  float baseline, deviation;
  const bool spike = peakWindow.observe(signal, MinimumAmplitude, baseline, deviation);
  if (spike) recentSpikeIndex = index;
  if (index < samplesPerSecond * 2UL) {
    typicalSlope = fmaxf(typicalSlope, envelope);
    noiseLevel += noiseAlpha * (fminf(envelope, 0.14f * typicalSlope) - noiseLevel);
    return;
  }

  const uint32_t sinceBeat = haveBeat ? index - lastBeatIndex : UINT32_MAX;
  if (candidate) {
    const float amplitude = fabsf(signal - candidateBaseline);
    if (amplitude > peakAmplitude) {
      peakAmplitude = amplitude;
      peakIndex = index;
    }
    peakSlope = fmaxf(peakSlope, envelope);
    if (index - candidateStart < (CandidateMs / DatasetSampleMs) * rateScale) return;
    candidate = false;
    const bool closeToBeat = haveBeat && peakIndex - lastBeatIndex < (400UL / DatasetSampleMs) * rateScale;
    if (peakAmplitude >= MinimumAmplitude &&
        (!closeToBeat || peakSlope >= 0.55f * typicalSlope)) {
      acceptBeat(peakIndex);
      typicalSlope += 0.18f * (peakSlope - typicalSlope);
    }
    return;
  }

  if (sinceBeat < (RefractoryMs / DatasetSampleMs) * rateScale) return;
  if (sinceBeat > 2 * samplesPerSecond) typicalSlope = fmaxf(MinimumSlope * 4, (live ? 0.99850f : 0.997f) * typicalSlope);
  const float threshold = fmaxf(MinimumSlope, fmaxf(3.0f * noiseLevel, 0.30f * typicalSlope));
  if (envelope >= threshold && index - recentSpikeIndex <= 3UL * rateScale) {
    candidate = true;
    candidateStart = peakIndex = index;
    candidateBaseline = baseline;
    peakAmplitude = deviation;
    peakSlope = envelope;
  } else if (envelope < threshold) {
    noiseLevel += noiseAlpha * (envelope - noiseLevel);
  }
}

void drainOutput() {
  const int room = Serial.availableForWrite();
  if (txSent == txSize || room <= 0) return;
  const size_t left = txSize - txSent;
  const size_t bytes = left < (size_t)room ? left : (size_t)room;
  txSent += Serial.write((const uint8_t *)tx + txSent, bytes);
}

bool hrReportDue() {
  return sampleIndex >= nextReportSample &&
         (datasetEnded || !candidate || candidateStart >= nextReportSample);
}

void queueSample(uint32_t index, float filtered, int centered) {
  if ((!plotMode && !streamSamples) || txSent != txSize) return;
  if (!plotMode && hrReportDue()) return;
  const int counts = (int)lroundf(filtered);
  if (plotMode) {
    const char *label = source == Source::Dataset ? "Dataset_ECG" : "ECG";
    txSize = snprintf(tx, sizeof(tx), "%s:%d\n", label, counts);
  } else {
    txSize = snprintf(tx, sizeof(tx), "ECG,%s,%lu,%d,%d\n", sourceName(),
                      (unsigned long)(index * sampleMs), counts, centered);
  }
  txSent = 0;
}


void analyzeOsborn(uint32_t index) {
  if (!pendingOsborn || index - osbornAnchor < (Osborn::After + 1) * rateScale) return;
  pendingOsborn = false;
  haveOsbornReport = true;
  osbornReportAnchor = osbornAnchor;
  osbornFeatures = Osborn::Features{};
  repeatedOsborn = false;
  const uint8_t requiredSamples = (Osborn::WindowSize + 1) * rateScale + 1;
  osbornUnreadable = morphologySamples < requiredSamples ||
                     (clippedRecently && index - lastClipIndex < requiredSamples);
  if (osbornUnreadable) {
    previousOsbornHeight = 0;
    return;
  }

  // A centered three-tap smoother avoids the causal band-pass delay used by
  // the beat detector. Analyze morphology on an 8 ms grid at either rate.
  float beat[Osborn::WindowSize];
  const uint32_t first = osbornAnchor - Osborn::Before * rateScale;
  for (uint8_t i = 0; i < Osborn::WindowSize; ++i) {
    const uint32_t at = first + i * rateScale;
    beat[i] = (morphology[(at - rateScale) % MorphologyRingSize] +
               2.0f * morphology[at % MorphologyRingSize] +
               morphology[(at + rateScale) % MorphologyRingSize]) * 0.25f;
  }
  osbornFeatures = Osborn::locate(beat, 3.0f * ReplayGain);
  if (osbornFeatures.peak < 0) {
    previousOsbornHeight = 0;
    return;
  }

  const int8_t delay = osbornFeatures.peak - osbornFeatures.anchor;
  repeatedOsborn = previousOsbornHeight * osbornFeatures.peakHeight > 0 &&
                   osbornAnchor - previousOsbornAnchor <= MaxRrMs / sampleMs &&
                   abs(delay - previousOsbornDelay) <= 3; // 24 ms.
  previousOsbornHeight = osbornFeatures.peakHeight;
  previousOsbornAnchor = osbornAnchor;
  previousOsbornDelay = delay;
}

void signedFeature(char *field, size_t size, bool available, float value) {
  if (available) snprintf(field, size, "%ld", lroundf(value));
  else snprintf(field, size, "NA");
}

void queueOsbornReport() {
  haveOsbornReport = false;
  const Osborn::Features &f = osbornFeatures;
  const bool haveBoundary = !osbornUnreadable && f.qrsEnd >= 0;
  const bool havePeak = haveBoundary && f.peak >= 0;
  const uint32_t firstMs = osbornReportAnchor * sampleMs - Osborn::Before * (uint32_t)DatasetSampleMs;
  const char *status = osbornUnreadable ? "unreadable" :
                       !haveBoundary ? "qrs_end_uncertain" :
                       !havePeak ? "no_distinct_bump" :
                       repeatedOsborn ? "repeated_candidate" : "candidate";
  char endMs[12], peakMs[12], anchor[12], height[12], ratio[12], prominence[12], slope[12];
  if (haveBoundary) snprintf(endMs, sizeof(endMs), "%lu", (unsigned long)(firstMs + f.qrsEnd * (uint32_t)DatasetSampleMs));
  else snprintf(endMs, sizeof(endMs), "NA");
  if (havePeak) snprintf(peakMs, sizeof(peakMs), "%lu", (unsigned long)(firstMs + f.peak * (uint32_t)DatasetSampleMs));
  else snprintf(peakMs, sizeof(peakMs), "NA");
  signedFeature(anchor, sizeof(anchor), !osbornUnreadable, f.anchorHeight);
  signedFeature(height, sizeof(height), havePeak, f.peakHeight);
  signedFeature(ratio, sizeof(ratio), havePeak,
                havePeak ? 1000.0f * fabsf(f.peakHeight / f.anchorHeight) : 0);
  signedFeature(prominence, sizeof(prominence), havePeak, f.prominence);
  signedFeature(slope, sizeof(slope), havePeak, f.rise * (1000.0f / (2 * DatasetSampleMs)));
  txSize = snprintf(tx, sizeof(tx), "J,%s,%lu,%s,%s,%s,%s,%s,%s,%s,%s,undetermined\n",
                    sourceName(), (unsigned long)(firstMs + f.anchor * (uint32_t)DatasetSampleMs),
                    endMs, peakMs, anchor, height, ratio, prominence, slope, status);
  txSent = 0;
}

void acquireSample(uint32_t index) {
  int centered;
  if (source == Source::Dataset) {
    centered = (int8_t)pgm_read_byte(&DatasetEcg[index]);
    centered *= DatasetStep * ReplayGain;
  } else {
    const int raw = analogRead(InputPin);
    centered = raw - (AdcMax + 1) / 2;
    if (raw <= 2 || raw >= AdcMax - 2) {
      ++clippedSamples;
      if (!clippedRecently) {
        peakWindow.reset(rateScale);
        recentSpikeIndex = 0;
      }
      previousOsbornHeight = 0;
      clippedRecently = true;
      lastClipIndex = index;
      candidate = haveBeat = false;
      // rrCount = rrNext = 0; // One-second reporting.
    }
  }
  morphology[index % MorphologyRingSize] = centered;
  if (morphologySamples < MorphologyRingSize) ++morphologySamples;
  const float filtered = filterEcg(centered);
  detectBeat(filtered, index);
  analyzeOsborn(index);
  queueSample(index, filtered, centered);
}

uint32_t referenceTime(uint16_t index) {
  return pgm_read_dword(&DatasetBeatMs[index]);
}

void formatValue(char *out, size_t size, long value) {
  if (value < 0) snprintf(out, size, "NA");
  else snprintf(out, size, "%ld", value);
}

void formatBpm(char *out, size_t size, long bpmTenths) {
  if (bpmTenths < 0) formatValue(out, size, -1);
  else snprintf(out, size, "%ld.%ld", bpmTenths / 10, bpmTenths % 10);
}

/*
void queueReport() {
  const uint32_t elapsedMs = sampleIndex * sampleMs;
  const bool warming = sampleIndex < (15UL * samplesPerSecond);
  const bool valid = !warming && haveBeat && rrCount == RrSize &&
                     (sampleIndex - lastBeatIndex) * sampleMs <= MaxRrMs;
  const char *status = datasetEnded ? "dataset_end" :
                       warming ? "please wait" :
                       valid ? "measuring" : "no reliable beat";
  uint16_t rr = 0;
  if (valid) {
    rr = averageRr(rrIntervals, RrSize);
  }

  long referenceBpm = -1;
  if (source == Source::Dataset) {
    while (referenceCursor < ReferenceCount && referenceTime(referenceCursor) <= elapsedMs) {
      ++referenceCursor;
    }
    if (!warming && referenceCursor > RrSize) {
      const uint16_t last = referenceCursor - 1;
      const uint32_t span = referenceTime(last) - referenceTime(last - RrSize);
      if (span) referenceBpm = 600000L * RrSize / span;
    }
  }

  char recordMs[12], bpm[12], rrMs[12], beatMs[12], count[12], reference[12];
  formatValue(recordMs, sizeof(recordMs), source == Source::Dataset ? (long)elapsedMs : -1L);
  formatBpm(bpm, sizeof(bpm), rr ? 600000L / rr : -1L);
  formatValue(rrMs, sizeof(rrMs), rr ? (long)rr : -1L);
  formatValue(beatMs, sizeof(beatMs), valid ? (long)(lastBeatIndex * sampleMs) : -1L);
  formatValue(count, sizeof(count), warming ? -1L : (long)beatCount);
  formatBpm(reference, sizeof(reference), referenceBpm);
  txSize = snprintf(tx, sizeof(tx), "HR,%s,%lu,%s,%s,%s,%s,%s,%s,%s,%lu,%lu,%u\n",
                    sourceName(), (unsigned long)elapsedMs, recordMs, status, bpm,
                    rrMs, beatMs, count, reference, (unsigned long)skippedSamples,
                    (unsigned long)clippedSamples, (unsigned int)rrCount);
  txSent = 0;
}

*/

void queueWindowReport(uint32_t endSample, bool finalWindow) {
  const uint32_t number = endSample / hrWindowSamples - 1;
  const RrWindow &window = rrWindows[number % 2];
  const uint16_t intervalCount = window.number == number ? window.count : 0;
  const uint32_t elapsedMs = endSample * sampleMs;
  const bool warming = endSample < 15UL * samplesPerSecond;
  const bool valid = !warming && intervalCount > 0;
  const char *status = finalWindow ? "dataset_end" :
                       warming ? "please wait" :
                       valid ? "measuring" : "no reliable beat";

  long referenceBpm = -1;
  if (source == Source::Dataset) {
    uint32_t sumMs = 0;
    uint16_t count = 0;
    while (referenceCursor < ReferenceCount && referenceTime(referenceCursor) < elapsedMs) {
      if (referenceCursor > 0 && referenceTime(referenceCursor) >= elapsedMs - 1000UL * HrWindowSeconds) {
        const uint32_t rr = referenceTime(referenceCursor) - referenceTime(referenceCursor - 1);
        if (rr > 0 && rr <= MaxRrMs) {
          sumMs += rr;
          ++count;
        }
      }
      ++referenceCursor;
    }
    if (!warming && count > 0) referenceBpm = 600000UL * count / sumMs;
  }

  char recordMs[12], bpm[12], rrMs[12], beatMs[12], count[12], reference[12];
  formatValue(recordMs, sizeof(recordMs), source == Source::Dataset ? (long)elapsedMs : -1L);
  formatBpm(bpm, sizeof(bpm), valid ? (long)(600000UL * intervalCount / window.sumMs) : -1L);
  formatValue(rrMs, sizeof(rrMs), valid ? (long)((window.sumMs + intervalCount / 2) / intervalCount) : -1L);
  formatValue(beatMs, sizeof(beatMs), valid ? (long)window.lastBeatMs : -1L);
  formatValue(count, sizeof(count), warming ? -1L : (long)beatCount);
  formatBpm(reference, sizeof(reference), referenceBpm);
  txSize = snprintf(tx, sizeof(tx), "HR,%s,%lu,%s,%s,%s,%s,%s,%s,%s,%lu,%lu,%u\n",
                    sourceName(), (unsigned long)elapsedMs, recordMs, status, bpm,
                    rrMs, beatMs, count, reference, (unsigned long)skippedSamples,
                    (unsigned long)clippedSamples, (unsigned int)intervalCount);
  txSent = 0;
}

void setup() {
  Serial.begin(115200);
#if defined(ARDUINO_ARCH_ESP32)
  analogReadResolution(12);
  analogSetPinAttenuation(InputPin, ADC_11db);
#endif
  startSource(Source::Live);
}

void loop() {
  if (Serial.available()) {
    const int command = Serial.read();
    if (command == 'l' || command == 'L') startSource(Source::Live);
    if (command == 'd' || command == 'D') startSource(Source::Dataset);
    if (command == 'w' || command == 'W') streamSamples = !streamSamples;
    if (command == 'p' || command == 'P') plotMode = true;
    if (command == 'c' || command == 'C') {
      plotMode = false;
      // endReported = false; // One-second reporting.
    }
  }
  const uint32_t now = micros();
  if (!datasetEnded && (int32_t)(now - nextSampleUs) >= 0) {
    const uint32_t missed = (now - nextSampleUs) / sampleUs;
    if (missed) {
      sampleIndex += missed;
      skippedSamples += missed;
      resetDetector();
    }
    nextSampleUs += (missed + 1) * sampleUs;
    if (source == Source::Dataset && sampleIndex >= DatasetSamples) {
      sampleIndex = DatasetSamples;
      datasetEnded = true;
    } else {
      acquireSample(sampleIndex++);
    }
  }

  drainOutput();
  /*
  if (plotMode || txSent != txSize) return;
  if (datasetEnded) {
    if (!endReported) {
      queueReport();
      endReported = true;
    }
  } else if (sampleIndex >= nextReportSample &&
             (source != Source::Dataset || sampleIndex < DatasetSamples)) {
    nextReportSample = (sampleIndex / samplesPerSecond + 1) * samplesPerSecond;
    queueReport();
  } else if (haveOsbornReport && sampleIndex >= (15UL * samplesPerSecond)) {
    queueOsbornReport();
  }
  */

  if (txSent != txSize) return;
  if (hrReportDue()) {
    const bool finalWindow = source == Source::Dataset && nextReportSample == DatasetSamples;
    if (!plotMode) queueWindowReport(nextReportSample, finalWindow);
    nextReportSample += hrWindowSamples;
  } else if (!plotMode && haveOsbornReport && sampleIndex >= 15UL * samplesPerSecond) {
    queueOsbornReport();
  }
}

