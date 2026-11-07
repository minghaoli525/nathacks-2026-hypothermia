/*
   HeartRateDetection
   https://github.com/upsidedownlabs/BioAmp-EXG-Pill

   Upside Down Labs invests time and resources providing this open source code,
   please support Upside Down Labs and open-source hardware by purchasing
   products from Upside Down Labs!

   Copyright (c) 2021 Moteen Shah moteenshah.02@gmail.com
   Copyright (c) 2024 Aryan Prakhar aryanprakhar1010@gmail.com
   Copyright (c) 2021 Upside Down Labs - contact@upsidedownlabs.tech

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#pragma once

#include <stdint.h>
#include <math.h>
#include <string.h>

// Adapted from the 16-sample mean/deviation trigger in the BioAmp
// Heart_Rate_Detection example. Keep a delayed reference window so a
// rising QRS does not immediately raise its own detection threshold.
class PeakWindow {
 public:
  static constexpr uint8_t BaseSize = 16;

  void reset(uint8_t rateScale) {
    size = BaseSize * rateScale;
    memset(samples, 0, sizeof(samples));
    memset(means, 0, sizeof(means));
    memset(variances, 0, sizeof(variances));
    slot = seen = 0;
    sum = sumSquares = 0.0f;
  }

  bool observe(float sample, float minimum, float &baseline, float &deviation) {
    baseline = means[slot];
    deviation = fabsf(sample - baseline);
    const bool spike = seen == 2 * size &&
                       deviation >= minimum &&
                       deviation * deviation > 64.0f * variances[slot];
    // Limit the influence of spikes on later noise estimates.
    const float retained = spike ? baseline : sample;
    const float outgoing = samples[slot];
    sum += retained - outgoing;
    sumSquares += retained * retained - outgoing * outgoing;
    samples[slot] = retained;
    means[slot] = sum / size;
    variances[slot] = fmaxf(0.0f, sumSquares / size - means[slot] * means[slot]);
    if (++slot == size) slot = 0;
    if (seen < 2 * size) ++seen;
    return spike;
  }

 private:
  float samples[2 * BaseSize];
  float means[2 * BaseSize];
  float variances[2 * BaseSize];
  uint8_t size;
  float sum;
  float sumSquares;
  uint8_t slot;
  uint8_t seen;
};
