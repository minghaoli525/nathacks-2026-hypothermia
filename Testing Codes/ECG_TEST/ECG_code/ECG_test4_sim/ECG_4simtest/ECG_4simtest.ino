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

// The Uno keeps fixed format strings in flash to leave RAM for ECG buffers.
#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#define ECG_PRINT(out, size, format, ...) snprintf_P(out, size, PSTR(format), __VA_ARGS__)
#else
#define ECG_PRINT(out, size, format, ...) snprintf(out, size, format, __VA_ARGS__)
#endif

#if defined(ARDUINO_ARCH_ESP32)
constexpr uint8_t InputPin = 36;  // VP on the ESP32 DevKit.
constexpr int AdcMax = 4095;
constexpr int ReplayGain = 4;
#else
constexpr uint8_t InputPin = A0;
constexpr int AdcMax = 1023;
constexpr int ReplayGain = 1;
#endif

// Sampling rate depends on the source: 250 Hz live and 125 Hz on replay.
constexpr uint32_t LiveSampleUs = 4000;    // 250 Hz.
constexpr uint32_t DatasetSampleUs = 8000; // 125 Hz, matching the recording
constexpr uint8_t DatasetSampleMs = DatasetSampleUs / 1000;
constexpr uint8_t LiveRateScale = DatasetSampleUs / LiveSampleUs;
constexpr uint16_t DatasetSamples = sizeof(DatasetEcg) / sizeof(DatasetEcg[0]);
constexpr uint8_t RefractoryMs = 240;
constexpr uint8_t CandidateMs = 88;

constexpr uint16_t MaxCandidateGapMs = 4000;

constexpr float MinimumSlope = 2.0f * ReplayGain;
constexpr float MinimumAmplitude = 5.0f * ReplayGain;

enum class Source : uint8_t { Live, Dataset };
Source source = Source::Live;

const char *pendingStatus = nullptr;
char commandText[12];
uint8_t commandLength = 0;
bool commandOverflow = false;
uint32_t sampleUs = LiveSampleUs;
uint8_t sampleMs = LiveSampleUs / 1000;
uint16_t samplesPerSecond = 1000000UL / LiveSampleUs;
uint8_t rateScale = LiveRateScale;
// Set true to open Serial Plotter directly on boards that reset on connect.
constexpr bool StartInPlotMode = false;
bool plotMode = StartInPlotMode;
bool streamSamples = false;
bool datasetEnded = false;
uint32_t nextSampleUs;
uint32_t sampleIndex;

// Filter and QRS locator state. These reset on input changes or missing samples.
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
bool haveQrsAnchor;
uint32_t lastQrsIndex;

// Raw ECG ring used to measure the waveform near the end of the QRS.
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
char tx[192];
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
  // The 0.5-44.5 Hz filter has different coefficients at 250 Hz and 125 Hz.
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

void resetQrsLocator() {
  memset(filterState, 0, sizeof(filterState));
  peakWindow.reset(rateScale);
  memset(previous, 0, sizeof(previous));
  envelope = 0.0f;
  noiseLevel = MinimumSlope * 0.5f;
  typicalSlope = MinimumSlope * 6.0f;
  recentSpikeIndex = 0;
  candidate = haveQrsAnchor = clippedRecently = false;
  morphologySamples = 0;
  pendingOsborn = haveOsbornReport = false;
  previousOsbornHeight = 0;
}

void startSource(Source selected) {
  source = selected;
  pendingStatus = nullptr;
  sampleUs = source == Source::Live ? LiveSampleUs : DatasetSampleUs;
  sampleMs = sampleUs / 1000;
  samplesPerSecond = 1000000UL / sampleUs;
  rateScale = DatasetSampleUs / sampleUs;
  sampleIndex = 0;
  datasetEnded = false;
  pendingStatus = "j_wave_only";
  nextSampleUs = micros();
  resetQrsLocator();
}

const char *sourceName() {
  if (source == Source::Dataset) return "mitbih_100_MLII";
  return "live";
}

void anchorOsbornWindow(uint32_t index) {
  // Save the QRS index so the morphology window can be read once it is full.
  lastQrsIndex = index;
  haveQrsAnchor = true;
  osbornAnchor = index;
  pendingOsborn = true;
}

// Start a candidate only when the vendor-style amplitude spike also has
// a QRS-like slope. Reject nearby deflections so they do not open another J window.
void locateQrsAnchor(float signal, uint32_t index) {
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

  const uint32_t sinceQrs = haveQrsAnchor ? index - lastQrsIndex : UINT32_MAX;
  if (candidate) {
    const float amplitude = fabsf(signal - candidateBaseline);
    if (amplitude > peakAmplitude) {
      peakAmplitude = amplitude;
      peakIndex = index;
    }
    peakSlope = fmaxf(peakSlope, envelope);
    if (index - candidateStart < (CandidateMs / DatasetSampleMs) * rateScale) return;
    candidate = false;
    const bool closeToQrs = haveQrsAnchor && peakIndex - lastQrsIndex < (400UL / DatasetSampleMs) * rateScale;
    if (peakAmplitude >= MinimumAmplitude &&
        (!closeToQrs || peakSlope >= 0.55f * typicalSlope)) {
      anchorOsbornWindow(peakIndex);
      typicalSlope += 0.18f * (peakSlope - typicalSlope);
    }
    return;
  }

  if (sinceQrs < (RefractoryMs / DatasetSampleMs) * rateScale) return;
  if (sinceQrs > 2 * samplesPerSecond) typicalSlope = fmaxf(MinimumSlope * 4, (live ? 0.99850f : 0.997f) * typicalSlope);
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

void queueSample(uint32_t index, float filtered, int centered) {
  if ((!plotMode && !streamSamples) || txSent != txSize) return;
  // Keep the J-wave row ahead of optional raw/filtered waveform logging.
  if (!plotMode && (pendingStatus || haveOsbornReport)) return;
  const int counts = (int)lroundf(filtered);
  if (plotMode) {
    const char *label = source == Source::Dataset ? "Dataset_ECG" : "ECG";
    txSize = ECG_PRINT(tx, sizeof(tx), "%s:%d\n", label, counts);
  } else {
    txSize = ECG_PRINT(tx, sizeof(tx), "ECG,%s,%lu,%d,%d\n", sourceName(),
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
                   osbornAnchor - previousOsbornAnchor <= MaxCandidateGapMs / sampleMs &&
                   abs(delay - previousOsbornDelay) <= 3; // 24 ms.
  previousOsbornHeight = osbornFeatures.peakHeight;
  previousOsbornAnchor = osbornAnchor;
  previousOsbornDelay = delay;
}

void writeNa(char *field) {
  field[0] = 'N';
  field[1] = 'A';
  field[2] = '\0';
}

void signedFeature(char *field, size_t size, bool available, float value) {
  if (available) ECG_PRINT(field, size, "%ld", lroundf(value));
  else writeNa(field);
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
                       !repeatedOsborn ? "candidate" :
                       source == Source::Live ? "j_wave_confirmed" : "repeated_candidate";
  char endMs[12], peakMs[12], anchor[12], height[12], ratio[12], prominence[12], slope[12];
  if (haveBoundary) ECG_PRINT(endMs, sizeof(endMs), "%lu", (unsigned long)(firstMs + f.qrsEnd * (uint32_t)DatasetSampleMs));
  else writeNa(endMs);
  if (havePeak) ECG_PRINT(peakMs, sizeof(peakMs), "%lu", (unsigned long)(firstMs + f.peak * (uint32_t)DatasetSampleMs));
  else writeNa(peakMs);
  signedFeature(anchor, sizeof(anchor), !osbornUnreadable, f.anchorHeight);
  signedFeature(height, sizeof(height), havePeak, f.peakHeight);
  signedFeature(ratio, sizeof(ratio), havePeak,
                havePeak ? 1000.0f * fabsf(f.peakHeight / f.anchorHeight) : 0);
  signedFeature(prominence, sizeof(prominence), havePeak, f.prominence);
  signedFeature(slope, sizeof(slope), havePeak, f.rise * (1000.0f / (2 * DatasetSampleMs)));
  txSize = ECG_PRINT(tx, sizeof(tx), "J,%s,%lu,%s,%s,%s,%s,%s,%s,%s,%s,undetermined\n",
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
    // Replay the same synthetic QRS every 800 ms at 250 samples per second.
    const uint16_t phase = index % 200;
    const int16_t qrs[] = {
      -15, -40, -65, 0, 65, 180, 300, 180,
      65, -65, -195, -155, -115, -77, -40, -20
    };
    const uint8_t bump[] = {4, 15, 29, 40, 45, 40, 29, 15, 4};

    int signal = phase < 16 ? qrs[phase] : 0;
    // Add the same post-QRS bump to each beat from 8 seconds onward.
    if (index >= 2000 && phase >= 20 && phase <= 28) {
      signal += bump[phase - 20];
    }
    const int raw = (AdcMax + 1) / 2 + signal * ReplayGain;
    centered = raw - (AdcMax + 1) / 2;
    if (raw <= 2 || raw >= AdcMax - 2) {
      if (!clippedRecently) {
        peakWindow.reset(rateScale);
        recentSpikeIndex = 0;
      }
      previousOsbornHeight = 0;
      clippedRecently = true;
      lastClipIndex = index;
      candidate = haveQrsAnchor = false;
    }
  }
  morphology[index % MorphologyRingSize] = centered;
  if (morphologySamples < MorphologyRingSize) ++morphologySamples;
  const float filtered = filterEcg(centered);
  locateQrsAnchor(filtered, index);
  analyzeOsborn(index);
  queueSample(index, filtered, centered);
}

void queueStatus() {
  txSize = ECG_PRINT(tx, sizeof(tx), "STATUS,%s,%s\n", pendingStatus, sourceName());
  txSent = 0;
  pendingStatus = nullptr;
}

void setup() {
  Serial.begin(115200);
#if defined(ARDUINO_ARCH_ESP32)
  analogReadResolution(12);
  analogSetPinAttenuation(InputPin, ADC_11db);
#endif
  startSource(Source::Live);
}

void handleCommand(const char *command) {
  if (!strcmp(command, "l")) {
    startSource(Source::Live);
  } else if (!strcmp(command, "d")) {
    startSource(Source::Dataset);
  } else if (!strcmp(command, "w")) {
    streamSamples = !streamSamples;
  } else if (!strcmp(command, "p")) {
    plotMode = true;
  } else if (!strcmp(command, "c")) {
    plotMode = false;
  }
}

void readCommands() {
  // Read a complete command before changing modes.
  while (Serial.available()) {
    const int value = Serial.read();
    if (value == '\r' || value == '\n') {
      if (!commandOverflow && commandLength > 0) {
        commandText[commandLength] = '\0';
        handleCommand(commandText);
      }
      commandLength = 0;
      commandOverflow = false;
    } else if (commandLength < sizeof(commandText) - 1) {
      commandText[commandLength++] = value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
    } else {
      commandOverflow = true;
    }
  }
}

void loop() {
  readCommands();
  // Advance one sample per deadline. Discard missed slots rather than burst-sampling.
  const uint32_t now = micros();
  if (!datasetEnded && (int32_t)(now - nextSampleUs) >= 0) {
    const uint32_t missed = (now - nextSampleUs) / sampleUs;
    if (missed) {
      sampleIndex += missed;
      resetQrsLocator();
    }
    nextSampleUs += (missed + 1) * sampleUs;
    if (source == Source::Dataset && sampleIndex >= DatasetSamples) {
      sampleIndex = DatasetSamples;
      datasetEnded = true;
      pendingStatus = "dataset_end";
    } else {
      acquireSample(sampleIndex++);
    }
  }

  drainOutput();

  if (txSent != txSize) return;
  if (pendingStatus && !plotMode) {
    queueStatus();
    return;
  }
  if (!plotMode && haveOsbornReport) queueOsbornReport();
}

