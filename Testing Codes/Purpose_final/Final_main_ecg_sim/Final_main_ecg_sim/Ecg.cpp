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

#include "Ecg.h"
#include "Osborn.h"
#include "PeakWindow.h"

namespace {
// Fixed synthetic sample offsets from ECG_test4_sim. Store them in flash on the Uno.
constexpr uint16_t QrsStart = 49;
constexpr uint16_t JWaveStart = QrsStart + 20;
const int16_t SimulatedQrs[] PROGMEM = {
  -15, -40, -65, 0, 65, 180, 300, 180,
  65, -65, -195, -155, -115, -77, -40, -20
};
const uint8_t SimulatedJWave[] PROGMEM = {
  4, 15, 29, 40, 45, 40, 29, 15, 4
};
}

int simulatedEcgSample(uint16_t phase, bool withJWave) {
  int sample = 512;
  const uint16_t qrsIndex = phase - QrsStart;
  const uint16_t jWaveIndex = phase - JWaveStart;
  if (qrsIndex < sizeof(SimulatedQrs) / sizeof(SimulatedQrs[0])) {
    sample += static_cast<int16_t>(pgm_read_word(&SimulatedQrs[qrsIndex]));
  }
  if (withJWave && jWaveIndex < sizeof(SimulatedJWave)) {
    sample += pgm_read_byte(&SimulatedJWave[jWaveIndex]);
  }
  return sample;
}

namespace {
constexpr uint32_t SampleUs = 4000;
constexpr uint8_t sampleMs = 4;
constexpr uint8_t DatasetSampleMs = 8;
constexpr uint8_t rateScale = DatasetSampleMs / sampleMs;
constexpr uint8_t LiveRateScale = rateScale;
constexpr uint16_t samplesPerSecond = 250;
constexpr uint32_t JWaveDelaySamples = 10UL * samplesPerSecond;
bool demoStarted;
uint32_t demoStartIndex;
bool lastJWaveDetected;
// Define the minimum gap between detections and the time allowed to locate a QRS peak.
constexpr uint8_t RefractoryMs = 240;
constexpr uint8_t CandidateMs = 88;
constexpr uint16_t MaxCandidateGapMs = 4000;
constexpr float MinimumSlope = 2.0f;
constexpr float MinimumAmplitude = 5.0f;
constexpr uint8_t ReplayGain = 1;

uint32_t nextSampleUs;
uint32_t sampleIndex;
uint32_t continuousStart;
// Track the number of consecutive qualifying beats up to the four-beat threshold.
constexpr uint8_t RequiredJWaves = 4;
uint8_t consecutiveJWaves;
constexpr uint32_t JWaveHoldMs = 5000;
bool jWaveHeld;
uint32_t lastQualifiedJWaveMs;
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
uint32_t osbornAnchor;
uint32_t previousOsbornAnchor;

float biquad(float input, uint8_t section, float a1, float a2,
           float b0, float b1, float b2) {
  // Use the previous two internal samples for the feedback and feed-forward terms of this filter section.
  float *z = filterState[section];
  const float x = input - a1 * z[0] - a2 * z[1];
  const float y = b0 * x + b1 * z[0] + b2 * z[1];
  z[1] = z[0];
  z[0] = x;
  return y;
}

// Cascade four biquad sections to band-limit the signal used for locating QRS peaks.
float filterEcg(float input) {
  float y = biquad(input, 0, -0.49508698f, 0.09768299f, 0.03104488f, 0.06208975f, 0.03104488f);
  y = biquad(y, 1, -0.65689312f, 0.49491779f, 1.0f, 2.0f, 1.0f);
  y = biquad(y, 2, -1.97656072f, 0.97672230f, 1.0f, -2.0f, 1.0f);
  return biquad(y, 3, -1.99040690f, 0.99056496f, 1.0f, -2.0f, 1.0f);
}

// Clear signal history and detection state at startup or after a gap in sampling.
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
  pendingOsborn = false;
  consecutiveJWaves = 0;
  lastJWaveDetected = false;
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
  // Compare samples 16 ms apart.
  const uint8_t lag = 2 * rateScale;
  const float slope = fabsf(signal - previous[(index - lag) % (2 * LiveRateScale)]);
  previous[index % (2 * LiveRateScale)] = signal;

  const float noiseAlpha = 0.00904f;
  // Smooth the slope magnitude so the trigger follows a sharp change over several samples.
  envelope += 0.19377f * (slope - envelope);
  // Allow half a second for the signal to recover before looking for a new QRS complex.
  if (clippedRecently) {
    if (index - lastClipIndex <= samplesPerSecond / 2) return;
    clippedRecently = false;
  }

  float baseline, deviation;
  // Compare amplitude with a rolling reference window to find a deflection above local noise.
  const bool spike = peakWindow.observe(signal, MinimumAmplitude, baseline, deviation);
  if (spike) recentSpikeIndex = index;
  // Use the first two seconds to learn typical slope and noise before accepting beats.
  if (index - continuousStart < samplesPerSecond * 2UL) {
    typicalSlope = fmaxf(typicalSlope, envelope);
    noiseLevel += noiseAlpha * (fminf(envelope, 0.14f * typicalSlope) - noiseLevel);
    return;
  }

  const uint32_t sinceQrs = haveQrsAnchor ? index - lastQrsIndex : UINT32_MAX;
  // Follow the largest deflection until the candidate window closes, then test whether it qualifies.
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

  // Reject deflections within the refractory period so one beat does not count twice.
  if (sinceQrs < (RefractoryMs / DatasetSampleMs) * rateScale) return;
  if (sinceQrs > 2 * samplesPerSecond) typicalSlope = fmaxf(MinimumSlope * 4, 0.99850f * typicalSlope);
  // Combine an absolute floor, the local noise level, and the typical beat slope into one trigger.
  const float threshold = fmaxf(MinimumSlope, fmaxf(3.0f * noiseLevel, 0.30f * typicalSlope));
  // Open a candidate only when a strong slope occurs close to an amplitude spike.
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

void analyzeOsborn(uint32_t index) {
  // Wait until the raw sample ring contains the post-QRS part of the beat.
  if (!pendingOsborn || index - osbornAnchor < (Osborn::After + 1) * rateScale) return;
  pendingOsborn = false;
  // Reject a window without enough raw samples or with recent ADC clipping.
  const uint8_t requiredSamples = (Osborn::WindowSize + 1) * rateScale + 1;
  const bool osbornUnreadable = morphologySamples < requiredSamples ||
                     (clippedRecently && index - lastClipIndex < requiredSamples);
  if (osbornUnreadable) {
    lastJWaveDetected = false;
    consecutiveJWaves = 0;
    return;
  }

  // A centered three-tap smoother avoids the causal band-pass delay used by
  // the beat detector. Analyze morphology on an 8 ms grid.
  float beat[Osborn::WindowSize];
  const uint32_t first = osbornAnchor - Osborn::Before * rateScale;
  for (uint8_t i = 0; i < Osborn::WindowSize; ++i) {
    const uint32_t at = first + i * rateScale;
    beat[i] = (morphology[(at - rateScale) % MorphologyRingSize] +
               2.0f * morphology[at % MorphologyRingSize] +
               morphology[(at + rateScale) % MorphologyRingSize]) * 0.25f;
  }
  // Search the smoothed beat for a distinct post-QRS bump. A beat without one resets the streak.
  const Osborn::Features osbornFeatures = Osborn::locate(beat, 3.0f * ReplayGain);
  lastJWaveDetected = osbornFeatures.peak >= 0;
  if (!lastJWaveDetected) {
    consecutiveJWaves = 0;
    return;
  }

  // Start a new streak if the previous qualifying beat is too old.
  if (osbornAnchor - previousOsbornAnchor > MaxCandidateGapMs / sampleMs) {
    consecutiveJWaves = 0;
  }
  if (consecutiveJWaves < RequiredJWaves) ++consecutiveJWaves;
  // Once four consecutive beats qualify, each further qualifying beat refreshes
  // the hold. A missed or invalid beat clears the streak but leaves the hold alone.
  if (consecutiveJWaves == RequiredJWaves) {
    lastQualifiedJWaveMs = millis();
    jWaveHeld = true;
  }
  previousOsbornAnchor = osbornAnchor;
}

void acquireSample(uint32_t index) {
  // Feed synthetic ADC counts through the same detector as the live sketch.
  const uint32_t elapsedSamples = demoStarted ? index - demoStartIndex : index;
  const bool withJWave = demoStarted && elapsedSamples >= JWaveDelaySamples;
  const int raw = simulatedEcgSample(elapsedSamples % samplesPerSecond, withJWave);
  const int centered = raw - 512;
  // Treat samples near either ADC limit as clipped and invalidate the current beat and streak.
  if (raw <= 2 || raw >= 1021) {
    if (!clippedRecently) {
      peakWindow.reset(rateScale);
      recentSpikeIndex = 0;
    }
    consecutiveJWaves = 0;
    clippedRecently = true;
    lastClipIndex = index;
    candidate = haveQrsAnchor = pendingOsborn = false;
  }
  // Store the raw signal for shape analysis and use the filtered copy for beat timing.
  morphology[index % MorphologyRingSize] = centered;
  if (morphologySamples < MorphologyRingSize) ++morphologySamples;
  const float filtered = filterEcg(centered);
  locateQrsAnchor(filtered, index);
  analyzeOsborn(index);
}

}  // namespace

void beginEcg() {
  nextSampleUs = micros();
  resetQrsLocator();
}

bool serviceEcg() {
  const uint32_t now = micros();
  // Use a signed difference between microsecond timestamps to handle timer wraparound.
  if (static_cast<int32_t>(now - nextSampleUs) < 0) return false;
  const uint32_t missed = (now - nextSampleUs) / SampleUs;
  // Discard missed sample slots and restart detection on the next continuous stretch.
  if (missed) {
    sampleIndex += missed;
    continuousStart = sampleIndex;
    resetQrsLocator();
  }
  nextSampleUs += (missed + 1) * SampleUs;
  acquireSample(sampleIndex++);
  return true;
}

bool jWaveAlarmActive() {
  // Expire the streak if qualifying beats stop arriving, even when no new QRS is detected.
  if (consecutiveJWaves && sampleIndex - previousOsbornAnchor > MaxCandidateGapMs / sampleMs) {
    consecutiveJWaves = 0;
  }
  // Unsigned elapsed time keeps the five-second hold valid across millis() rollover.
  if (jWaveHeld && millis() - lastQualifiedJWaveMs >= JWaveHoldMs) {
    jWaveHeld = false;
  }
  return jWaveHeld;
}

void startJWaveDemo() {
  demoStartIndex = sampleIndex;
  demoStarted = true;
  continuousStart = sampleIndex;
  resetQrsLocator();
}

bool lastBeatHadJWave() {
  return lastJWaveDetected;
}

bool simulatedJWavePresent() {
  return demoStarted && sampleIndex - demoStartIndex >= JWaveDelaySamples;
}
