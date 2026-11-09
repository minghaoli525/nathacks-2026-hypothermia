#pragma once

#include <Arduino.h>

void beginEcg();
bool serviceEcg();
bool jWaveAlarmActive();

void startJWaveDemo();
bool lastBeatHadJWave();
bool simulatedJWavePresent();
int simulatedEcgSample(uint16_t phase, bool withJWave);
