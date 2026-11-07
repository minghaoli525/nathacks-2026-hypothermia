# EMG OLED Display Test

**Date:** November 7, 2026

## Goal
Display real-time raw EMG readings from the BioAmp EXG Pill on a 128×64 OLED screen using an Arduino UNO.

## Hardware
- Arduino UNO
- BioAmp EXG Pill
- 128×64 OLED Display
- Jumper wires

## Implementation
- Read analog EMG signals from pin A0.
- Sample the sensor at 20 Hz.
- Send raw readings to the Serial Monitor.
- Update the OLED display at 5 Hz.

## Current Status
Initial Arduino code completed.

OLED hardware display functionality is being tested.

## Next Steps
1. Verify OLED wiring and I²C address.
2. Confirm successful OLED initialization.
3. Display live EMG sensor readings.
4. Improve the display interface for the final prototype.
