# Arduino Hardware Development

This folder contains Arduino-related development work for our natHacks 2026 Hypothermia Risk Detection project.

Arduino was used during prototyping to collect physiological sensor signals, test hardware components, and implement real-time feedback systems.

## 1. Hardware Components

The Arduino-based experiments involved the following components:

| Component | Purpose |
|---|---|
| Arduino UNO | Microcontroller for sensor testing and data processing |
| BioAmp EXG Pill | Collects electrical muscle activity (EMG) and supports ECG experiments |
| OLED Display | Displays real-time sensor readings |
| TMB 12A0 Buzzer | Provides audible alerts |
| Open-Smart Vibration Motor Module | Provides vibration feedback |
| Breadboard and Jumper Wires | Connect electronic components |

## 2. EMG Signal Monitoring

Electromyography (EMG) measures electrical activity produced by skeletal muscles.

We used the BioAmp EXG Pill to collect EMG signals and observe changes in muscle activity.

During testing, we compared readings under different conditions, including:

- Relaxed muscle conditions
- Voluntary muscle contractions
- Physical exercise at the gym

Larger fluctuations were observed during muscle contractions.

These experiments helped us investigate the potential use of EMG signals in detecting muscle activity associated with shivering.

However, EMG activity alone cannot reliably distinguish cold-induced shivering from voluntary muscle movement.

## 3. Real-Time OLED Display

An OLED display was integrated into the Arduino prototype to show sensor readings in real time.

The display allows users to monitor EMG values directly without relying on a computer's Serial Monitor.

## 4. Buzzer and Vibration Feedback

We developed a threshold-based feedback system.

When the EMG reading falls outside a predefined range, the Arduino can activate an audible buzzer and a vibration motor.

During one stage of testing, the thresholds were configured as follows:

| EMG Reading | System Response |
|---|---|
| Below 350 | Alert triggered |
| 350–680 | Normal threshold range |
| Above 680 | Alert triggered |

These thresholds were experimental values selected during prototype testing.

They are not clinically validated thresholds for detecting shivering or hypothermia.

## 5. Development Process

Our Arduino development followed several stages:

**Stage 1: EMG Data Collection**
- Connected the BioAmp EXG Pill to Arduino.
- Collected raw EMG measurements.
- Observed sensor readings through the Serial Monitor.

**Stage 2: OLED Integration**
- Connected an OLED display.
- Displayed EMG readings in real time.

**Stage 3: Buzzer Integration**
- Added a buzzer to the Arduino circuit.
- Implemented threshold-based audible alerts.

**Stage 4: Vibration Motor Integration**
- Added a vibration motor module.
- Implemented vibration feedback using EMG threshold conditions.

**Stage 5: Experimental Testing**
- Conducted muscle contraction experiments.
- Tested EMG behavior during gym exercises.
- Observed changes in sensor output.

## 6. Software Requirements

Arduino experiments may require:

- Arduino IDE
- Adafruit GFX Library
- Adafruit SSD1306 Library
- Wire Library

Required libraries depend on the specific Arduino sketch.

## 7. Technical Challenges

During development, we encountered several challenges:

- Selecting suitable EMG threshold values.
- Maintaining stable sensor readings.
- Integrating the OLED display and alert components.
- Distinguishing voluntary muscle contractions from potential shivering.
- Adapting components and code for different microcontrollers.

## 8. Future Improvements

Potential improvements include:

- Implementing signal filtering and calibration.
- Using more advanced EMG signal processing.
- Reducing false alerts caused by voluntary muscle movement.
- Integrating EMG with heart rate and temperature measurements.
- Improving wearable hardware integration.

## 9. Project Status

Arduino-based sensor monitoring, display, and feedback features were developed and tested during natHacks 2026.

The prototype demonstrates preliminary hardware functionality, but further validation is required before it can reliably identify cold-induced shivering or hypothermia risk.
