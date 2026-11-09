# Hypothermia Risk Detection — natHACKS 2026

A wearable biosensing prototype for exploring early warning signals associated with cold exposure and potential hypothermia risk.

Built during the 64-hour **natHACKS 2026** competition (November 6–9, 2026) at the University of Alberta, Edmonton.

> **Research and hackathon prototype only.** This project does not measure core body temperature, diagnose hypothermia, or replace medical assessment or emergency care.

---

## The Problem

People exposed to cold conditions may not immediately recognize when they are becoming unsafe. Our team explored whether a small wearable system could monitor multiple physiological and environmental signals, display them in real time, and trigger a local warning when a configurable combination of conditions is met.

Rather than relying on a single measurement metric, this prototype system integrates data such as muscle activity (EMG), electrocardiogram (ECG) waveform characteristics, heart rate, and ambient temperature to improve the accuracy of hypothermia detection.

---

## Programs We Built

| Component | Purpose | Current implementation |
| :--- | :--- | :--- |
| **EMG — electromyography** | Observe muscle electrical activity, potentially including activity associated with shivering | Live analog input from a BioAmp EXG Pill; raw-value threshold checks |
| **ECG — electrocardiography** | Explore ECG waveform features, including an experimental J-wave/Osborn-wave heuristic | ECG sampling and heuristic detection in the integrated experimental sketch |
| **Heart rate / SpO₂** | Display optical pulse measurements and compare heart rate with a starting baseline | DFRobot blood-oxygen sensor integration |
| **Surrounding temperature** | Indicate cold environmental conditions | MPL3115A2 temperature sensor |
| **Pressure / altitude** | Provide additional environmental readings | MPL3115A2 pressure and altitude functions |
| **OLED display** | Show readings on a compact, watch-like screen | 128 × 64 I²C OLED with button-controlled pages |
| **Buzzer / vibration** | Provide audible and tactile feedback | Digital outputs driven by the prototype's alert rule |

> The current code reads surrounding (ambient) temperature, not skin temperature or core body temperature.

### Experimental alert logic

The live-input integrated sketch, [`Final-Main.ino`](Testing%20Codes/Purpose_final/Final-Main/Final-Main.ino), turns on the buzzer and vibration output when all of these conditions hold:

1. The experimental ECG detector has flagged at least one qualifying J-wave-like beat and its five-second hold is still active.
2. A valid heart-rate reading is at least 15 BPM below the initial calibrated baseline.
3. The raw EMG reading on A0 is below 450 or above 650.
4. A recent ambient-temperature reading is below 10 °C.

These values were chosen for prototype testing. A raw EMG threshold alone cannot distinguish involuntary shivering from ordinary voluntary muscle movement, and a waveform heuristic is not a clinical ECG interpretation.

| Path | Contents |
| :--- | :--- |
| [`Testing Codes/Purpose_final/Final-Main/`](Testing%20Codes/Purpose_final/Final-Main/) | Integrated experimental code using live EMG, ECG, pulse and environmental sensor inputs, an OLED, and warning outputs |
| [`Testing Codes/Purpose_final/Final_main_sim/`](Testing%20Codes/Purpose_final/Final_main_sim/) | Demo-oriented integrated code: live EMG; simulated J-wave flag, temperature and heart-rate values; optional live SpO₂ and barometer pages |
| [`FinalProject/FinalProject/`](FinalProject/FinalProject/) | Earlier OLED/sensor integration sketch; its EMG display value is currently a generated counter, not a live EMG sample |
| [`Testing Codes/EMG_Test/`](Testing%20Codes/EMG_Test/) | Standalone EMG acquisition, OLED display, buzzer, vibration, and experiment notes |
| [`Testing Codes/ECG_TEST/`](Testing%20Codes/ECG_TEST/) | ECG prototypes, recorded text samples, plots, and waveform-analysis experiments |
| [`Testing Codes/HR_oximeter/`](Testing%20Codes/HR_oximeter/) | Heart-rate and blood-oxygen module test |
| [`Testing Codes/altimeter_baro_mode/`](Testing%20Codes/altimeter_baro_mode/) | Temperature, pressure, and altitude sensor test |
| [`Hardware/`](Hardware/) and [`docs/`](docs/) | Hardware and documentation notes |

The standalone [`EMG_OLED_Buzzer_Vibration.ino`](Testing%20Codes/EMG_Test/EMG_OLED_Buzzer_Vibration/EMG_OLED_Buzzer_Vibration.ino) uses a different EMG threshold range: below 350 or above 680. That difference reflects testing iterations; do not assume every sketch uses the same alert settings.

---

## Hardware

- Arduino UNO for the integrated Final-Main build
- BioAmp EXG Pill analog biosensing hardware and appropriate electrodes/connections for EMG and ECG channels
- 128 × 64 SSD1306-compatible I²C OLED display (configured at `0x3C`)
- DFRobot I²C heart-rate / blood-oxygen sensor (configured at `0x57`)
- MPL3115A2 environmental pressure / altitude / temperature sensor (I²C address `0x60`)
- Buzzer module, vibration-motor module, push button, breadboard and jumper wires
- USB cable and computer for Arduino programming

### Pin map for Final-Main

| Arduino UNO pin | Connection |
| :--- | :--- |
| `A0` | EMG analog output |
| `A1` | ECG analog output |
| `A2` | Button input (the sketch treats an analog reading below 300 as pressed) |
| `D8` | Buzzer module control input and Vibration module control input |
| `A4 / SDA` | Shared I²C data line for OLED, oximeter and MPL3115A2 |
| `A5 / SCL` | Shared I²C clock line for OLED, oximeter and MPL3115A2 |

> Connect sensor grounds together and follow each module's documentation for power voltage, level compatibility, electrode placement and safe operation. Use a suitable motor driver/module; do not drive an unprotected motor directly from an Arduino I/O pin. The A2 button logic assumes the team's analog-input wiring; other button circuits may require a code change.

---

## Software & Dependencies

- **Arduino IDE** and the **Arduino AVR Boards** package (select Arduino Uno)
- **U8g2** for the integrated OLED interface
- **Adafruit BusIO** for `Adafruit_I2CDevice.h`
- **DFRobot_RTU** for the DFRobot blood-oxygen driver
- **Wire** and **SoftwareSerial**, provided by the Arduino core

The integrated sketch folders already include local `.cpp` / `.h` copies of the MPL3115A2 and DFRobot BloodOxygen drivers. Keep these files together with each `.ino` sketch. The earlier `FinalProject` and standalone EMG OLED sketches also require **Adafruit GFX** and **Adafruit SSD1306**.

---

## How to Set Up and Run

1. Clone or download the repository:
   ```bash
   git clone [https://github.com/minghaoli525/nathacks-2026-hypothermia.git](https://github.com/minghaoli525/nathacks-2026-hypothermia.git)
   ```
2. Open Arduino IDE. Install the required libraries using **Tools → Manage Libraries** (and the Arduino AVR board package using **Boards Manager**).
3. Assemble the hardware using the pin map above. Verify the sensor supply voltages and common ground before powering the circuit.
4. For the integrated live-input experimental version, open:
   ```text
   Testing Codes/Purpose_final/Final-Main/Final-Main.ino
   ```
   Keep `Ecg.cpp`, `Ecg.h`, `Osborn.h`, `PeakWindow.h`, and the included sensor-driver files in the same sketch folder.
5. Select **Arduino Uno** and the correct USB port, then click **Verify** and **Upload**.
6. Keep the oximeter finger measurement steady while the sketch attempts to collect a 15-second heart-rate baseline. If too few valid samples arrive, baseline collection repeats. Press the button to cycle through EMG, ambient temperature, pressure, altitude, heart rate and SpO₂ pages.
7. Observe the OLED and the local buzzer/vibration outputs. The full alert may not trigger under ordinary conditions because the combined rule is intentionally restrictive and experimental.

### Repeatable demonstration mode

For a bench demonstration of the output logic, open:

```text
Testing Codes/Purpose_final/Final_main_sim/Final_main_sim.ino
```

This version uses live EMG from A0 but sets the J-wave flag to true, temperature to 8 °C, starting heart rate to 80 BPM, and current heart rate to 60 BPM. These are simulated presentation inputs, not measurements from the wearer. When EMG falls outside the demo range of 450–650, the alert outputs can be exercised. Pressure, altitude and SpO₂ pages still depend on connected hardware where available.

For the simplest stand-alone EMG test, open:

```text
Testing Codes/EMG_Test/EMG_OLED_Buzzer_Vibration/EMG_OLED_Buzzer_Vibration.ino
```

This sketch reads raw EMG on A0 at approximately 20 samples per second, prints to Serial at `115200` baud, displays the value on the OLED, and activates the buzzer/vibration outside its 350–680 test range.

---

## Testing and What We Learned

- Developed separate test sketches for EMG, ECG, the optical heart-rate / oxygen sensor, and the MPL3115A2 environmental sensor.
- Integrated live sensor acquisition and a multi-page wearable-style display in an Arduino-based experimental build.
- Added buzzer and vibration feedback and iterated on EMG thresholds.
- Recorded example ECG data and generated resting/active waveform plots in [`Testing Codes/ECG_TEST/`](Testing%20Codes/ECG_TEST/).
- Investigated differences between Arduino UNO and ESP32 ADC behavior, unreliable ECG sensing, EMG threshold selection, and conversion of sensor data into readable visualizations.

### Current limitations

- **No clinical validation:** this prototype cannot confirm hypothermia, infer a person's core temperature, or reliably identify true shivering.
- **ECG reliability:** the experimental J-wave detection logic may be affected by electrode contact, motion, noise and sampling constraints.
- **EMG specificity:** exercise and intentional muscle contractions can also produce large EMG changes.
- **Temperature type:** Temperature sensors cannot determine whether a person has hypothermia simply by measuring their core temperature.
- **Code maturity:** several sketches are experiments or demos. A successful integrated upload and end-to-end hardware performance should be verified on the exact board and wiring used for judging.

---

## Presentation and Demonstration

- **Team presentation** — Google Slides — https://docs.google.com/presentation/d/1Yt2TOdoYgUkpzP7vBjieTFo87q6CACHg/edit?slide=id.p1#slide=id.p1
- [ECG resting plot](Testing%20Codes/ECG_TEST/ecg_plot_rest.png) and [ECG active plot](Testing%20Codes/ECG_TEST/ecg_active_plot.png)
- [EMG muscle contraction plot](Testing%20Codes/EMG_Test/EMG_grapth.png)

The following video demonstrates our EMG prototype
responding to muscle contractions during physical activity.

https://github.com/user-attachments/assets/c28b7ae1-a820-41c1-ace1-010922f671f8

The following video demonstrates a simulation of our system changing display through different operating modes.

https://github.com/user-attachments/assets/45dedb1c-2814-4a9d-b9b2-5040b88335b5



---

## Team

- Nathan Lin
- Brian Wei
- Ethan Bui
- Khai
- Leon Li
- Jinze Cai

This was a collaborative interdisciplinary project developed for natHACKS 2026. Contributions included biosignal experimentation, embedded programming, sensor integration, hardware assembly, interface design, testing, and presentation preparation.

---

## Sources and Acknowledgments

- **Upside Down Labs** — BioAmp EXG Pill documentation and reference code
- **Adafruit** — MPL3115A2 library
- **DFRobot** — Blood Oxygen Sensor library
- **U8g2** display library
- **Adafruit GFX** and **Adafruit SSD1306**
- **Network for Applied Technology** — natHACKS

Third-party library code is included in some sketch folders; original copyright and license notices are retained in the source files. Thanks to the natHACKS organizers, mentors, and hardware providers.

> **Safety disclaimer:** For education and proof-of-concept testing only. Never use this device to make medical decisions or delay seeking assistance for possible cold-related illness.
