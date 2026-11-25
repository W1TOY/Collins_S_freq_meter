# Collins S-Line / R390 Frequency Meter

A digital frequency readout for the **Collins S-line** (75S receiver → 32S transmitter) and the **Collins R390** receiver. It has an ESP32 counter board and a 20×4 LCD.

The meter taps the radios' internal oscillator injection lines through high-impedance inputs. It counts each line against a 20 MHz OCXO reference, then calculates the operating (carrier) frequency and shows it, along with the raw oscillator frequencies.

```
W1TOY,  Collins S
IF :    14,455,123      <- HF crystal-oscillator injection (HFO)
VFO:    02,601,450      <- PTO / VFO injection
fc :    11,399,023      <- displayed carrier frequency
```

The repository contains:

| Path | Contents |
|---|---|
| [`fmaster/`](fmaster/) | ESP32 firmware (ESP-IDF v5.4, C) |
| [`PCB/Collins_Freq_Meter/`](PCB/Collins_Freq_Meter/) | KiCad 9 schematic and PCB, PDF plots, and [design notes](PCB/Collins_Freq_Meter/DESIGN_NOTES.md) (BOM, power, connector details) |
| [`CMakeLists.txt`](CMakeLists.txt), [`idf.ps1`](idf.ps1), [`idf.cmd`](idf.cmd), [`.vscode/`](.vscode/) | Lets you build, flash and monitor the firmware from the repository root |

---

## How it works

**Inputs.** Each RF input goes through a 1 nF + 1 kΩ tap into a TLV3502 high-speed comparator. The comparator squares the signal up into 3.3 V logic for the ESP32.
- The two S-line lines (VFO and HFO) are **pass-through**: the coax from the 75S goes in, and straight out to the 32S. The meter only adds about 1 kΩ / 3 pF of load.
- Sensitivity is about 60 mVpp.

**Counting.** The ESP32 has one hardware pulse counter (PCNT) per input, so every channel is measured at the same time over the same gate.
- The gate is a 2 Hz signal the ESP32 generates itself. It is high for 0.484 s of each 0.5 s period.
- It is looped back from GPIO13 to GPIO12 and enables every counter.
- On the gate's falling edge an interrupt reads and clears all counters.
- **Result:** a new reading every 0.5 s, with about 2 Hz resolution.

**Calibration.** One channel counts a 20 MHz ECS ECOC-7050 OCXO.
- The ratio *nominal 20 MHz / measured* corrects every other channel for the error of the ESP32's own crystal, which sets the gate length.
- The factor is only updated while the reference reads within ±0.1 % of 20 MHz. While the OCXO is unpowered or still warming up, the last good factor is kept (1.0 at power-up).

**Displayed frequency.**

| Mode | Formula |
|---|---|
| Collins S-line | `fc = HFO − VFO − 455 kHz ± 1.35 kHz` |
| Collins R390 | `fc = IF1 − IF2 − VFO − 455 kHz` |

- **S-line BFO correction:** the ±1.35 kHz is the offset of the S-line carrier crystals (453.650 kHz USB, 456.350 kHz LSB).
  - The firmware assumes **LSB below 8 MHz** (+1.35 kHz) and **USB above** (−1.35 kHz), the usual sideband for each band.
  - If you run the opposite sideband, the reading is off by 2.7 kHz (see [Known limitations](#known-limitations)).
- **R390 formula:** carried over from the prototype and not yet verified.

### Inputs and I/O

| Connector | Signal | Range | ESP32 | Firmware channel |
|---|---|---|---|---|
| J2 → J3 (SMA, pass-through) | 75S VFO / PTO injection → 32S | 2.5–2.7 MHz | GPIO27 | CH1 |
| J4 → J5 (SMA, pass-through) | 75S HF crystal-oscillator injection → 32S | ~6.9–33 MHz | GPIO32 | CH5 (direct, or ÷2 via JP1) |
| J9 (SMA) | R390 IF1 | ≤ ~30 MHz | GPIO26 | CH2 |
| J10 (SMA) | R390 IF2 | ≤ ~30 MHz | GPIO25 | CH3 |
| J11 (SMA) | R390 VFO | ≤ ~30 MHz | GPIO33 | CH4 |
| Y1 (on board) | 20 MHz OCXO reference | 20 MHz | GPIO14 | CH0 |
| JP2 (open) | 1 MHz self-test loopback, GPIO15 → GPIO2 | 1 MHz | GPIO2 | CH6 |
| J12 / J7 pin 5 | PTT sense (through a 2N7000 stage) | DC | GPIO4 | — |
| J7 pins 2–4 | Mode selector SEL0 / SEL1 / SEL2 (active low) | — | GPIO18 / 19 / 21 | — |
| J6 | 20×4 LCD, PCF8574 I2C backpack at 0x27 (via 2N7000 level shifter) | — | GPIO16 SDA / GPIO17 SCL | — |
| J1 | 12 V DC (10–18 V), centre positive | — | — | — |

The S-line uses RCA connectors, so you need SMA-male-to-RCA-male cables (RG-174 or RG-316).

---

## Operating instructions

### 1. Connect

1. **S-line:** break the two injection cables between the 75S and the 32S and route them through the meter.
   - 75S **VFO out** → **J2**, then **J3** → 32S VFO in.
   - 75S **HFO out** → **J4**, then **J5** → 32S HFO in.
2. **R390 (optional):** connect IF1 → **J9**, IF2 → **J10** and VFO → **J11**.
   - These inputs are high impedance.
   - Fit R28 / R35 / R42 (51 Ω) only if the source needs a 50 Ω termination.
3. **PTT (needed only for mode 2):** connect the station keying line to **J12** (or J7 pin 5).
   - **JP9 open** (default): a keying *voltage* of 4–30 V on PTT_IN means TX.
   - **JP9 fitted:** a *contact closure to ground* means TX. You must also set `PTT_TX_LEVEL` to `1` in [`main.c`](fmaster/main/main.c) and rebuild.
4. **LCD:** plug it into **J6** (GND, VCC 5 V, SDA, SCL). Check your backpack's pin order first.
5. **Power:** 12 V DC (the station 13.8 V supply is fine) into **J1**.
   - It draws about 1 A for the first minute while the OCXO oven warms up, then about 0.35 A.
   - **The 7805 needs a heatsink** of 10 °C/W or better.

### 2. Choose the mode

The mode is read from a selector switch on J7 (SEL0–SEL2; pull the selected line to GND). It is checked every 200 ms, so you can change it while the meter is running.

| Selector | Mode | Display |
|---|---|---|
| SEL0 (GPIO18) | **0: Collins S-line** (75S → 32S) | header `Collins S`; IF = HFO, VFO = VFO, fc = S-line carrier |
| SEL1 (GPIO19) | **1: Collins R390** | header `Collins R390`; IF = IF1, VFO = VFO, fc = R390 frequency |
| SEL2 (GPIO21) | **2: R390 receive / 32S transmit** | follows PTT: shows the R390 display on receive and the S-line display while transmitting |
| none | — | `select mode`; frequencies are still logged on the serial port |

- **JP3 is bridged by default.** It grounds SEL0, so the board starts in S-line mode with no switch fitted.
  - SEL0 takes priority over the other positions, so **remove JP3 when you fit a selector switch**. Otherwise the meter stays in S-line mode.

### 3. Power up and read

- The display shows the header, then a fresh reading every 0.5 s.
- **During the first minute or so,** the OCXO is still warming up. Frequencies are then only as accurate as the ESP32's own crystal, about ±20 ppm, until the reference settles.
- Once the reference is in range, the serial log shows `reference OK (… Hz) - calibration active`.
- If the reference drops out of range, the log shows `reference … Hz out of range - keeping the last calibration factor`.

### 4. Serial monitor

The firmware prints every channel twice a second: the raw value and the OCXO-calibrated value.

```
f[0]=19999987 cal=20000000  f[1]=2601449 cal=2601450  ...  f[6]=1000001 cal=1000001
```

| Index | Channel |
|---|---|
| 0 | reference |
| 1 | 75S VFO |
| 2 | R390 IF1 |
| 3 | R390 IF2 |
| 4 | R390 VFO |
| 5 | 75S HFO |
| 6 | self-test |

Use **ESP32: Monitor** in VS Code or `.\idf monitor` (see below). Press **Ctrl+]** to leave the monitor.

### 5. Self-test

Fit **JP2**. This loops the ESP32's internal 1 MHz test output (GPIO15) back into channel 6 (GPIO2). `f[6]` should then read about 1,000,000 Hz in the serial log, which confirms the gate and counter chain without any radio connected.

### USB-only operation

The ESP32 can run from its USB port alone, for programming or bench tests. In that case the OCXO is unpowered: the reference reads 0 and calibration stays off.

**JP6** links the board's 5 V to the DevKit's VIN pin. Some DevKit clones lack the USB-VBUS protection diode. On those, **remove JP6 before connecting USB while 12 V is applied**, so the board's 5 V can't feed back into your PC. The [design notes](PCB/Collins_Freq_Meter/DESIGN_NOTES.md#power-single-12-v-input) explain how to check your DevKit.

---

## Building and flashing the firmware

The build has been tested on Windows with ESP-IDF v5.4 and the Espressif VS Code extension. Everything runs from the **repository root**; you never need to `cd` into `fmaster/`.

- The root [`CMakeLists.txt`](CMakeLists.txt) points ESP-IDF at `fmaster/main` and `fmaster/sdkconfig`.
- Build output goes to `fmaster/build/`.

### Prerequisites

- [ESP-IDF v5.4](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/get-started/).
- VS Code with the [ESP-IDF extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension) (optional, but gives the status-bar buttons).
- **Machine-specific paths:** [`.vscode/settings.json`](.vscode/settings.json) and [`idf.ps1`](idf.ps1) hold the paths of the original development machine. Change these to match your install:
  - `C:\Users\email\esp\v5.4\esp-idf` (ESP-IDF)
  - `C:\Users\email\.espressif` (tools)
  - the serial port, `COM3`

### Option A: VS Code

Open the repository folder (**File → Open Folder**). Then use any of these:
- **ESP-IDF status-bar buttons:** set target, choose port, menuconfig, full clean, build, flash, monitor.
- **Ctrl+Shift+B** to build.
- **Terminal → Run Task**: ESP32 Build, Flash, Flash + Monitor, Monitor, Menuconfig, Full Clean.

### Option B: command line

From PowerShell or cmd in the repository root:

```powershell
.\idf build                  # compile
.\idf flash monitor          # flash, then open the serial monitor
.\idf -Port COM5 flash       # use a different serial port (default COM3, or set ESPPORT)
.\idf menuconfig             # ESP-IDF configuration
.\idf fullclean              # delete the build
```

`idf.ps1` loads the ESP-IDF environment by itself, so no special "ESP-IDF terminal" is needed.

### Firmware options

These are at the top of [`fmaster/main/main.c`](fmaster/main/main.c):

| Define | Default | Purpose |
|---|---|---|
| `FREQ_REF_OSCILLATOR_HZ` | `20000000.0` | Nominal OCXO frequency. For best absolute accuracy, put the value you measured against a good standard here (the OCXO is within ±500 ppb when delivered). |
| `HFO_PRESCALE` | `1` | Set to `2` if JP1 is moved to ÷2 (the HFO input then goes through the 74LVC74A). |
| `PCNT_GLITCH_FILTER_NS` | `0` | Counter glitch filter. Leave it off: a 30 ns filter blocks the 20 MHz reference and any HFO above about 16 MHz. |
| `PTT_TX_LEVEL` | `0` | GPIO4 level that means TX. `0` for voltage keying (JP9 open), `1` for contact closure (JP9 fitted). |
| `ENABLE_LCD_DISPLAY` | defined | Comment it out to run without an LCD (serial log only). |
| `show_moving_average` / `num_moving_average` | `0` / `2` | Optional moving average over the last N readings (N up to 16). |

The LCD's I2C address is set in [`fmaster/main/i2c-lcd.c`](fmaster/main/i2c-lcd.c). It is `0x27` for a PCF8574; use `0x3F` for a PCF8574A.

---

## Building the board

The PCB is a 100 × 150 mm, two-sided board laid out for CNC milling. Almost every part is through-hole. The SMD exceptions are the TLV3502 comparators (SOIC-8), the 74LVC74A (SOIC-14) and the OCXO.

- [Schematic (PDF)](PCB/Collins_Freq_Meter/Collins_Freq_Meter_schematic.pdf)
- [Placement (PDF)](PCB/Collins_Freq_Meter/Collins_Freq_Meter_placement.pdf)
- [Top copper (PDF)](PCB/Collins_Freq_Meter/Collins_Freq_Meter_route_top.pdf)
- [Bottom copper (PDF)](PCB/Collins_Freq_Meter/Collins_Freq_Meter_route_bottom.pdf)
- [Top silkscreen (PDF)](PCB/Collins_Freq_Meter/Collins_Freq_Meter_silk_top.pdf)

The [design notes](PCB/Collins_Freq_Meter/DESIGN_NOTES.md) have the full BOM, power budget, heatsinking, connector pinouts and footprint checks. Two things matter on a milled (unplated) board:
- each of the 22 vias needs a wire soldered through it;
- through-hole pads with tracks on both sides must be soldered on both sides.

---

## Known limitations

- **S-line sideband.** The ±1.35 kHz BFO correction is chosen by frequency (LSB below 8 MHz), not by the 75S sideband switch. Running the opposite sideband reads 2.7 kHz off.
- **R390 formula.** `IF1 − IF2 − VFO − 455 kHz` is carried over from the prototype and still needs checking against the R390 frequency scheme. IF2 is measured, but it has no line of its own on the LCD; it appears only in the serial log.
- **Mode names in the serial log.** Mode 2 is labelled `Reserved` there, although it is the R390-receive / 32S-transmit mode.
