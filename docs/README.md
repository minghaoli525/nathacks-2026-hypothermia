# Project Documentation

This folder contains the technical documentation, experimental records, hardware research, and presentation materials developed during natHacks 2026.

Our project focuses on developing a wearable prototype that monitors physiological and environmental signals to identify potential signs associated with hypothermia risk.

## 1. Hardware Setup

Our prototype explores the integration of several sensors and electronic components:

- **Arduino UNO / ESP32:** Microcontrollers used for sensor testing, data collection, and prototype development.
- **BioAmp EXG Pill:** Used to collect EMG and ECG signals.
- **Temperature Sensors:** Used to monitor skin and surrounding temperature.
- **Heart Rate Sensor:** Used to monitor heart rate and related physiological data.
- **OLED Display:** Displays real-time sensor readings.
- **Buzzer and Vibration Motor:** Provide feedback when predefined alert conditions are triggered.

Hardware documentation includes component selection, circuit setup, PCB research, and wearable housing considerations.

## 2. Experiments and Testing

During natHacks 2026, our team conducted experiments to evaluate sensor performance and develop the monitoring system.

### EMG Testing
- Collected electrical muscle activity using the BioAmp EXG Pill.
- Observed changes in EMG signals during voluntary muscle contractions.
- Tested the EMG sensor during physical activity at the gym.
- Experimented with threshold-based detection.
- Developed OLED display, buzzer, and vibration feedback prototypes.

### ECG Testing
- Experimented with ECG signal collection using the BioAmp EXG Pill.
- Investigated signal accuracy and reliability.
- Explored methods for visualizing ECG measurements.

### Additional Sensor Testing
- Explored heart rate and oxygen saturation measurements.
- Tested environmental sensing components.
- Investigated skin and surrounding temperature monitoring.

## 3. Technical Challenges

Several challenges were encountered during development:

1. **Microcontroller Compatibility:** Adapting hardware and code between Arduino UNO and ESP32 platforms.
2. **ECG Signal Accuracy:** Obtaining stable and reliable physiological measurements.
3. **EMG Threshold Selection:** Determining appropriate thresholds while accounting for normal voluntary muscle movement.
4. **Data Visualization:** Converting raw sensor outputs into understandable graphical representations.
5. **Hardware Integration:** Combining multiple sensing and feedback components into a wearable prototype.

## 4. Presentation

This project was developed for natHacks 2026.

Presentation materials include:

- Project overview and motivation
- Hardware architecture
- Sensor testing and experimental results
- Technical challenges
- Prototype demonstration
- Future improvements

## 5. References

Technical references include:

- Arduino documentation
- ESP32 documentation
- BioAmp EXG Pill documentation
- Sensor datasheets
- Hardware component specifications

BioAmp EXG Pill documentation:
https://docs.upsidedownlabs.tech/hardware/bioamp/bioamp-exg-pill/index.html

## 6. Project Limitations

This project is an experimental prototype developed during a hackathon.

Although it explores physiological signals associated with cold exposure, it does not directly measure core body temperature or provide a clinically validated diagnosis of hypothermia.

Further testing, calibration, and validation would be required before the system could be considered reliable for real-world health monitoring.
