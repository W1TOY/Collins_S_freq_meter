# Collins S-Line Frequency Meter: design notes (Rev J)

Schematic: `Collins_Freq_Meter.kicad_sch` (PDF: `Collins_Freq_Meter_schematic.pdf`).
ERC is clean (0 violations, KiCad 9.0.7). GPIO numbers match `fmaster/main/main.c`.

Rev J replaces the five TLV3501 single comparators with the **TLV3502** dual (SMD SOIC-8, same footprint). Each channel uses half A and parks half B. Rev I adds **JP6**, a 2-pin jumper in series with D2 (the 5 V feed to the DevKit VIN), so the board's 5 V can be disconnected while programming over USB. Rev H removes J8 (the R390 monitor header), renames the three extra inputs to **R390 IF1 / R390 IF2 / R390 VFO** (firmware CH2 / CH3 / CH4) and routes the board. Q1–Q3 now use the wide TO-92 footprint (2.54 mm pin pitch). Rev F added the CH3/CH4 inputs (J10 → GPIO25, J11 → GPIO33) and a 2N7000 PTT input stage on GPIO4. It removes the JP4/JP5 link jumpers and moves to a 100 × 150 mm board with an 82 × 141 mm mounting-hole pattern. Rev E added the CH2 AUX input (J9 → GPIO26) and returned U4 to the SMD 74LVC74A. Rev D uses through-hole parts wherever one exists and adds the 100 × 120 mm PCB placement. Rev C runs from a single 12 V supply through a 7805 and uses SMA jacks for J2–J5. Since Rev B, the schematic draws real wires inside each block. Net labels remain only for signals that go to another block: the ESP32 GPIOs, the headers, VBIAS and +3V3A. The ESP32 is now the DOIT ESP32 DevKit V1 (30 pin), and Y1 has a footprint.

## Signal path
| Signal | Source | Range | Board path | ESP32 |
|---|---|---|---|---|
| VFO (PTO) injection | 75S → 32S | 2.5–2.7 MHz | J2→J3 pass-through, tap C11/R4 → U2 TLV3502 (A) → R9 | GPIO27 (CH1) |
| HF crystal osc. injection ("IF" in code) | 75S → 32S | ~6.9–32.9 MHz | J4→J5 pass-through, tap C14/R10 → U3 TLV3502 (A) → JP1 (direct or ÷2 via U4A) → R15 | GPIO32 (CH5) |
| R390 IF1 | any source, single SMA J9 | up to ~30 MHz | tap C20/R22 → U7 TLV3502 (A) → R27 | GPIO26 (firmware CH2, `FREQ_390_IF1`) |
| R390 IF2 | any source, single SMA J10 | up to ~30 MHz | tap C23/R29 → U9 TLV3502 (A) → R34 | GPIO25 (firmware CH3, `FREQ_390_IF2`) |
| R390 VFO | any source, single SMA J11 | up to ~30 MHz | tap C26/R36 → U10 TLV3502 (A) → R41 | GPIO33 (firmware CH4, `FREQ_390_VFO`) |
| PTT | station keying line, J12 or J7 pin 5 | DC | R43 → Q3 2N7000 (D14 clamp) | GPIO4, low = TX |
| Reference | Y1 ECOC-7050 20 MHz OCXO | 20 MHz LVCMOS | R1 33 Ω | GPIO14 |
| Gate | GPIO13 LEDC → R16 1k | 2 Hz | loopback | GPIO12 |
| Self-test | GPIO15 LEDC 1 MHz → R17 → JP2 (open) | 1 MHz | loopback | GPIO2 |

Displayed carrier = HFO − VFO − 455 kHz (± BFO offset, see firmware notes).

**Tap.** The 75S→32S coax runs straight through (IN→OUT). The meter sees the line through 1 nF + 1 kΩ, which loads it with about 1 kΩ / 3 pF.
- **Bias.** VBIAS is 1.65 V. R7 (1 MΩ) holds INN about 25 mV above INP, so the output stays quiet with no signal.
- **Sensitivity.** About 60 mVpp.
- **Protection.** Two 1N4148s per channel clamp INP to the rails: D3/D6 on the VFO channel, D4/D7 on the HFO channel.
  - D3 and D4 have anode to INP and cathode to +3V3A. D6 and D7 have anode to GND and cathode to INP.
  - Each 1N4148 adds about 2–4 pF, against about 1.5 pF for the old BAV99. With R4/R10 (1 kΩ) this rolls off around 20–30 MHz.
  - On the 10 m HFO injection, around 32 MHz, that costs roughly 2–5 dB. The comparator only needs about 60 mVpp, so the margin is still large.
  - If the 10 m bands ever count unreliably, reduce R10 to 470 Ω.
- **More sensitivity.** Reduce R7 offset (raise it to 2.2 MΩ) or R4 (to 470 Ω).

## Power (single 12 V input)
- **Input.** J1 takes 12 V DC (10–18 V, centre positive), e.g. the station 13.8 V supply. It draws about 1 A peak while the OCXO oven warms up (about 1 min), then about 0.35 A.
  - OCXO current (ECS datasheet): 750 mA warm-up, 200 mA steady.
- **Protection.**
  - D1 (1N5822, in series): reverse-polarity protection.
  - F1: radial PTC, 2 A hold.
  - D5: P6KE18CA TVS, for spikes on the 12 V line.
- **U6 is an L7805CV in TO-220** (1.5 A). C19 (330 nF) sits at its input, as the datasheet requires; C1 and C2 sit at its output.
  - A linear regulator adds no switching noise to the receiver, so FB2 was removed.
- **U6 needs a heatsink.** It drops (V<sub>in</sub> − 0.4 V − 5 V) times the load current. At 13.8 V in:
  - **Warm-up:** about 8.4 V × 1 A ≈ **8 W** for about 1 minute.
  - **Steady:** about 8.4 V × 0.35 A ≈ **3 W**.
  - Use a heatsink of **≤ 10 °C/W** (a TO-220 extrusion about 25 mm tall), or bolt the tab to a metal case with an insulating pad. The 7805 tab is GND, so a pad is only needed if the case isn't grounded.
  - Without enough heatsinking, the 7805 hits thermal shutdown during warm-up and the ESP32 resets.
  - **To halve the 7805's heat:** add a 3.3 Ω / 5 W resistor in series with its input. It drops about 3.3 V at 1 A and still leaves ≥ 2.5 V headroom at 12 V in.
- **U1 is an LD1117V33 in TO-220** (800 mA). It powers *only* the OCXO from the 7805 output.
  - TO-220 pinout is 1 = GND, 2 = OUT (tab), 3 = IN. That differs from the 7805 (1 = IN, 2 = GND, 3 = OUT), so do not swap the two parts.
  - **Current margin.** The OCXO peaks at 750 mA during warm-up, close to the LD1117's 800 mA rating. That is fine because its current limit is higher, but nothing else should go on this rail.
  - **Headroom.** 5 V in leaves 1.7 V across U1, above its about 1.2 V dropout at 800 mA.
  - **Heat.** About 1.3 W during warm-up and 0.34 W steady. A small clip-on heatsink or a copper area under the tab is enough. Its tab is OUT (3.3 V), so isolate it from a grounded case.
  - **Output capacitor.** C4 (10 µF electrolytic) is the minimum 10 µF the ST LD1117 datasheet asks for. Keep it right at the pins.
- **D2** feeds the DevKit VIN pin and the LCD. It also stops the 5 V rail and USB feeding each other while you program over USB.
- **JP6** (fitted by default) sits between D2 and VIN. D2 stops USB from feeding the 7805 and OCXO. In the other direction, the board's 5 V is kept off your PC's USB port only by the DevKit's own VBUS Schottky diode, and some clones omit it.
  - **Check your DevKit:** with it unpowered, put a meter on diode test, red lead on USB VBUS and black on VIN. About 0.2–0.4 V means the diode is present, so JP6 can stay fitted while programming.
  - **If it reads a short (no diode):** remove JP6 before plugging in USB with 12 V applied. The ESP32 and LCD then run from USB, and the OCXO stays powered from the 7805.
  - The PCB silkscreen next to JP6 reads "OPEN=USB PROG".
- **Comparators and prescaler** run on +3V3A: the DevKit's own 3V3 regulator, through FB1, plus 10 µF.
- **Comparators (U2, U3, U7, U9, U10) are TI TLV3502AID duals** (4.5 ns, push-pull, SOIC-8). There is one package per channel, which keeps the channels physically apart and avoids crosstalk between two RF inputs in one package.
  - Pinout: 1 OUTA, 2 −INA, 3 +INA, 4 V−, 5 +INB, 6 −INB, 7 OUTB, 8 V+. This differs from the TLV3501, so the board is rerouted.
  - **Unused half B:** +INB is tied to +3V3A and −INB to GND, so OUTB (pin 7) sits steadily high and is left unconnected. Don't leave unused inputs floating; they would oscillate and inject noise.
  - The TLV3502 has no shutdown pin, so the old SHDN-to-GND link is gone.
- **USB-only operation.** If only USB is connected, the OCXO is unpowered and the reference count is 0. The firmware must guard against that (see below).

## Connectors
- J1: 12 V DC barrel jack, 2.1 mm (CUI PJ-102AH), at the centre of the RF (top) edge.
- J2–J5, J9–J11: vertical PCB-mount SMA jacks (footprint `Connector_Coaxial:SMA_Amphenol_132134_Vertical`).
  - The footprint has a centre pin plus 4 ground legs on a 5.08 mm square. **Check this against your jacks.**
  - J2 and J4 come from the 75S; J3 and J5 go to the 32S.
  - The S-line uses RCA connectors, so use SMA-male-to-RCA-male cables (RG-174/RG-316).
- J9: R390 IF1 SMA input (GPIO26, net R390_IF1). It has the same comparator front end as CH1/CH5. It presents a high-impedance input (about 1 kΩ + 10 kΩ) unless R28 (51 Ω, not fitted by default) is installed as a 50 Ω termination.
- J10 / J11: R390 IF2 / R390 VFO SMA inputs (GPIO25 / GPIO33, nets R390_IF2 / R390_VFO). Same front end as J9. R35 / R42 (51 Ω, not fitted) are the optional terminations.
- J12: PTT input, 1×2 header (PTT_IN, GND). It is in parallel with J7 pin 5.
  - **JP9 open** (default): a keying *voltage* of 4–30 V on PTT_IN means TX. Q3 turns on and pulls GPIO4 low, which is TX in `main.c` as written.
  - **JP9 fitted:** R45 pulls PTT_IN up to +5 V for a *contact closure to ground* = TX. That inverts the sense (closure → Q3 off → GPIO4 high), so set `PTT_TX_LEVEL` to 1 in `main.c`.
  - D14 (12 V zener) and R43 (10 kΩ) / R44 (100 kΩ) keep the 2N7000 gate within ±20 V. R46 (10 kΩ) pulls GPIO4 up to 3.3 V.
- J6: 1×4 2.54 mm LCD header: GND, VCC (5 V), SDA, SCL. This matches the usual PCF8574 backpack. **Check your backpack's pin order before plugging in.**
- J7: mode selector / PTT (GND, SEL0, SEL1, SEL2, PTT_IN, 3V3). Pin 5 now goes into the Q3 PTT stage, not directly to GPIO4.
- JP3 (bridged by default): ties SEL0 to GND, so the board boots in S-line mode with no switch fitted.

## Parts needing attention before milling
1. **Y1 footprint** (`Collins_Freq_Meter:ECS_ECOC-7050_7.0x5.0mm`) follows datasheet Fig. 2.
   - **Pad sizes:** corner pads 1.28×0.94 mm, small pads 0.76×1.28 mm, pad 1 0.76×1.60 mm, overall height 5.46 mm.
   - **Pitch:** 1.27 mm, taken from the bottom-view dimension (3.14 mm across 0.6 mm pads). The Fig. 2 drawing itself is slightly off-scale.
   - **Top view:** VCC (9) top-left, OUT (5) top-right, GND (4) bottom-right, long pad 1 bottom-left. This matches the ECS test-circuit drawing and the pin-1 dot.
   - **DNC pads** (1, 2, 3, 6, 7, 8, 10) have copper for soldering, but nothing is routed to them.
   - Print the footprint 1:1 and lay a part on it before milling.
2. **DevKit V1 footprint** (`Collins_Freq_Meter:ESP32-DevKit-V1_30pin_2x15_P2.54mm_W25.4mm`) is 2× 1×15 sockets with rows 25.4 mm apart. Pitch, row spacing and the 28 × 51.5 mm board size were confirmed from your board.
   - **Pin numbers** follow your pinout picture: 1–15 run down the left side (EN to VIN), and 16–30 run up the right side (3V3 to IO23).
   - **Silkscreen outline:** its position along the board's length is approximate.
   - **Left row:** EN, VP, VN, D34, D35, D32, D33, D25, D26, D27, D14, D12, D13, GND, VIN.
   - **Right row:** D23, D22, TX0, RX0, D21, D19, D18, D5, TX2, RX2, D4, D2, D15, GND, 3V3.

3. **SMD parts for CNC milling:**
   - U2/U3/U7: SOIC-8, 1.27 mm pitch.
   - U4: SOIC-14, 1.27 mm pitch.
   - Y1: OCXO.
   - Use a 0.1 mm V-bit or 0.2 mm end mill around them. Everything else is through-hole.
4. **Layout:**
   - Keep each tap trace (J2→J3, J4→J5) short.
   - Put C11/R4 (C14/R10) right at the comparator.
   - Use a solid ground pour on the bottom.
   - Keep the OCXO away from U1 and away from draughts. It is an oven, so a small thermal slot around it helps.

## PCB (`Collins_Freq_Meter.kicad_pcb`)
Board size is **100 × 150 mm**. The 100 mm short edge at the top is the RF / cable edge. A placement plot is in `Collins_Freq_Meter_placement.pdf`.

**Mounting holes:** M3, on an **82 × 141 mm** pattern centred on the board, at (9, 4.5), (91, 4.5), (9, 145.5) and (91, 145.5) mm from the top-left corner.

**Done:**
- Board outline and mounting holes.
- Every footprint placed, with its net assigned. The board is linked to the schematic, so *Update PCB from Schematic* keeps the placement.
- A GND pour outline on B.Cu.
- Silkscreen labels for the connectors and the 7805 heatsink area.

**DRC:**
- No courtyard overlaps, clearance errors or schematic-parity errors.
- What remains is the unrouted connections (the ratsnest) and cosmetic silkscreen overlaps.

**Placement (top to bottom):**
- **Top strip (y 0–17 mm), between the two top holes:**
  - J1 (12 V) sits at the centre of the top edge, with its plug opening flush with the edge.
  - Right of J1 is the input protection: D1, F1, D5, C18.
  - Left of J1 the strip is free.
- **7805 (U6), bottom right:** U6 sits at (69, 139.5) mm with its tab facing up. C19 is on its input side, and C1/C2 on the output side.
  - The silkscreen marks a **30 × 11 mm heatsink area** on the tab side (x 54–84, y 125.5–137). That's room for an extruded TO-220 heatsink of about 30 mm width and ≤ 10 °C/W, 20–30 mm tall.
  - Route the protected 12 V (after D5/C18 in the top strip) down the board edge to U6 with a ≥ 1.0 mm track. Return 5 V to D2/C3 (DevKit VIN), the LCD and the LD1117 (U1).
  - The LD1117 (U1, next to the OCXO) needs no heatsink: about 1.3 W for 1 minute, then 0.34 W. There is about 13 × 7 mm free on its tab side if you want a small clip-on.
- **SMA row (y ≈ 22.5 mm), all vertical jacks:** J2 VFO IN, J3 VFO OUT, J9 R390 IF1, J10 R390 IF2, J11 R390 VFO, J4 HFO IN, J5 HFO OUT.
  - The pitch is about 14.5 mm.
  - The optional 51 Ω terminations sit just below: R28 under J9, and R35/R42 standing between the VFO and HFO blocks.
- **Under the SMA row:**
  - Left: the VFO front end (C11, R4, D3/D6, U2…), with the OCXO block below it (U1 LD1117, C4–C7, Y1, R1).
  - Right: the HFO front end and the ÷2 prescaler (U3, U4, JP1).
- **AUX band (y ≈ 66–92 mm):** the R390 IF1, IF2 and VFO front ends side by side. Each block is a resistor column, a diode/capacitor column and its TLV3502.
- **ESP32 band:** the DevKit, with USB at the left edge. To its right are the analog bias (FB1, R2, R3, C8–C10), D2/C3/JP6 (5 V to VIN), the loopbacks (R16, R17, JP2), the level shifter (Q1/Q2, R18–R21) and J6 LCD on the right edge.
- **Bottom:** the PTT stage (R43–R46, D14, Q3, JP9). On the bottom edge are JP3, J12 PTT IN and J7 MODE/PTT.

**Routing (done, double-sided):**
- **Router:** Freerouting 2.5.0, via `routing/Collins_Freq_Meter.dsn` → `.ses` (log in `routing/freerouting.log`).
- **Result:** all 263 connections routed, 0 violations, **22 vias**.
- **KiCad DRC:** 0 errors and 0 schematic-parity issues. Only cosmetic silkscreen overlaps remain.
- **CNC rules**, stored in the project's net classes:

  | Class | Track | Clearance | Via | Nets |
  |---|---|---|---|---|
  | Default | 0.5 mm | 0.4 mm | 1.6 mm / 0.8 mm drill | everything else |
  | Power | 0.8 mm | 0.4 mm | 1.6 mm / 0.8 mm drill | 12 V chain (J1–D1–F1–D5/C18–U6), U6 5 V out, +5V, OCXO 3.3 V (U1 out) |

  - The router necks down to 0.38–0.5 mm only where it enters the 1.27 mm-pitch SMD pads (SOIC-8/-14, OCXO).
- **Edge keep-back:** tracks stay at least 1 mm from the board edge (the router was given an outline inset by 1 mm), so the outline cut can't nick them.
- Plots: `Collins_Freq_Meter_route_top.pdf`, `Collins_Freq_Meter_route_bottom.pdf`.

**Ground pour (filled):**
- B.Cu GND pour covers about 9,940 mm² of the 15,000 mm² board.
- Settings: 0.4 mm clearance, 0.4 mm minimum width, 0.4 mm thermal gap with 0.6 mm spokes. Unconnected islands are removed.
- These GND pads connect **solid** (no thermal relief) because bottom tracks box them in: C3, C16, D11, J7 pin 1, J12 pin 2 and U5 pin 14 (ESP32 GND).
- **DRC on the filled board:** 0 errors, 0 thermal or island warnings. Only cosmetic silkscreen overlaps remain.
- If you move anything, press **B** in KiCad to refill.

**Silkscreen (tidied):**
- Each connector on the RF edge has one combined label: J2 VFO IN, J3 VFO OUT, J9 R390 IF1, J10 R390 IF2, J11 R390 VFO, J4 HFO IN, J5 HFO OUT, J1 12V IN. Their separate reference text is hidden on the silkscreen, but kept on F.Fab.
- Every other reference designator is placed clear of outlines, pads and the board edge. Some smaller parts use 0.8 / 0.65 mm text.
- **KiCad DRC: 0 violations, 0 unconnected pads, 0 footprint errors**, silkscreen included.
- Plot: `Collins_Freq_Meter_silk_top.pdf`.

**Still to do in KiCad:**
1. **Vias:** each of the 22 vias needs a wire soldered through and onto both sides (or a via rivet), because CNC-milled boards are not plated.
2. **Pads used as layer changes:** through-hole pads with tracks on both sides must be **soldered on both sides**. That applies to resistor, header, DIP and TO-220 pads where the router changed layer, and to the ESP32 socket pins (use headers whose plastic lets you solder the top).

## Display level shifter (Q1/Q2 2N7000, TO-92)
It isn't needed for the display to *work*, as your direct connection showed. It is there for the ESP32's electrical ratings:
- **Without it:** the PCF8574 backpack pulls SDA/SCL up to **5 V**, typically with 4.7 kΩ. Each ESP32 pin then sits at 5 V, above its 3.6 V absolute maximum, and leaks about 0.3 mA into its protection diode.
  - This usually survives, which is why it works on the bench.
  - It is out of spec, and it pushes current into the ESP32's 3V3 rail.
- **The alternative** is cutting the backpack's pull-ups and pulling up to 3.3 V. That leaves the PCF8574 below its 3.5 V V<sub>IH</sub> spec at 5 V supply.
- **Recommendation:** keep Q1/Q2. That's 2 TO-92s and 4 resistors.
- **To build without it:** leave out Q1/Q2/R18–R21 and bridge each 2N7000's source and drain pads (pins 1 and 3) with a wire.
- **2N7000 vs BSS138:**
  - Gate drive is only 3.3 V, and the 2N7000's gate threshold can be as high as 3.0 V (typically 2.1 V). That still leaves far more current than the 1 mA needed to pull the bus low through a 4.7 kΩ pull-up.
  - If one ever misbehaves, try another 2N7000 or a BS170. These swap in directly with the same S-G-D pinout.
  - TO-92 pinout, flat face toward you: 1 = S, 2 = G, 3 = D.

## BOM (all through-hole except U2, U3, U4, U7, U9, U10, Y1)
| Ref | Part | Footprint |
|---|---|---|
| U2, U3, U7, U9, U10 | TI TLV3502AID dual comparator (half B unused) | SOIC-8 (SMD) |
| Y1 | ECS ECOC-7050-20.000-BN-TR OCXO (SMD only) | 7.0 × 5.0 mm, 10 pad |
| U4 | 74LVC74A. Only needed if JP1 is moved to ÷2 | SOIC-14 |
| U5 | DOIT ESP32 DevKit V1, 30 pin, on 2× 1×15 female headers | 2× 1×15, 25.4 mm rows |
| U6 | L7805CV + heatsink ≤ 10 °C/W | TO-220 vertical |
| U1 | ST LD1117V33 | TO-220 vertical |
| Q1, Q2, Q3 | 2N7000 (legs bent to 2.54 mm) | TO-92 wide |
| D14 | BZX55C12 (12 V zener, 0.5 W) | DO-35 |
| D1 | 1N5822 (3 A Schottky; carries the full ~1 A input current) | DO-201AD |
| D2 | 1N5819 | DO-41 |
| D3, D4, D6–D13 | 1N4148 | DO-35 |
| D5 | P6KE18CA TVS | DO-15 |
| F1 | Radial PTC, 2 A hold (e.g. Bourns MF-RHT200, Littelfuse RXEF200) | radial, 5.1 mm |
| FB1 | Axial ferrite bead (or 1–2 turns through a ferrite bead on a wire) | axial, 10.16 mm |
| C1 | 100 µF 16 V electrolytic | radial D6.3 P2.5 |
| C18 | 47 µF 35 V electrolytic | radial D6.3 P2.5 |
| C2, C3, C4, C6, C8, C9 | 10 µF 16 V electrolytic | radial D5 P2.0 |
| C5, C7, C10, C12, C13, C15–C17, C21, C22, C24, C25, C27, C28 | 100 nF ceramic | disc, 5 mm pitch |
| C11, C14, C20, C23, C26 | 1 nF C0G/NP0 ceramic, 100 V | disc, 5 mm pitch |
| C19 | 330 nF ceramic, 50 V | disc, 5 mm pitch |
| R1–R46 | ¼ W metal film (R44 = 100 k) | axial, 10.16 mm |
| R8, R14, R26, R33, R40 | 220 k, **DNP** (optional hysteresis) | axial |
| R28, R35, R42 | 51 Ω, **DNP** (optional 50 Ω terminations on J9/J10/J11) | axial |
| JP1 | 1×3 pin header + jumper shunt (1-2 = direct, 2-3 = ÷2) | 2.54 mm |
| JP2 | 1×2 pin header, open (self-test) | 2.54 mm |
| JP3 | 1×2 pin header + shunt (S-line mode) | 2.54 mm |
| JP6 | 1×2 pin header + shunt (5 V → VIN; remove for USB programming if the DevKit has no VBUS diode) | 2.54 mm |
| JP9 | 1×2 pin header, open (fit for ground-closure PTT) | 2.54 mm |
| J1 | 2.1 mm DC barrel jack (CUI PJ-102AH) | THT |
| J2–J5, J9–J11 | SMA female, PCB vertical, 4-leg | 5.08 mm leg square |
| J6 | 1×4 2.54 mm header (LCD) | THT |
| J7 | 1×6 2.54 mm header | THT |
| J12 | 1×2 2.54 mm header (PTT IN) | THT |
| H1–H4 | M3 mounting holes, 82 × 141 mm pattern | 3.2 mm |
| LCD | 20×4 HD44780 with PCF8574 I2C backpack, address 0x27 | — |

## Firmware changes needed for this board (`fmaster/main/main.c`)
1. **Reference frequency.** Change `freq_ref_oscillator = 9999975.0` to `20000000.0`. Better still, use the value you measure once the OCXO is warm: initial tolerance is ±500 ppb, i.e. ±10 Hz.
2. **Glitch filter.** `max_glitch_ns = 30` rejects any pulse shorter than about 25–30 ns. That blocks the 20 MHz reference (25 ns half period) and every HFO frequency above about 16 MHz. Set it to about 10 ns, or remove the filter.
3. **HFO ÷2.** If JP1 is moved to ÷2, multiply channel 5 (`FREQ_75S_IF`) by 2.
4. **Missing reference.** Guard the calibration against a zero or implausible reference count (OCXO unpowered or still warming). Today `calibration_factor` becomes infinite.
5. **The ±1350 Hz "empirical correction" is the S-line BFO offset.** The carrier crystals are 453.650 kHz (USB) and 456.350 kHz (LSB), i.e. 455 kHz ∓ 1.35 kHz. It depends on the sideband switch, not on the band. The current `< 8 MHz` rule only matches the default sideband per band.
6. **R390 inputs.** R390 IF1 / IF2 / VFO (GPIO26 / 25 / 33 = firmware CH2 / CH3 / CH4) come from J9 / J10 / J11. The R390 mode in `main.c` already reads them through `FREQ_390_IF1/IF2/VFO`.
   - **PTT** (GPIO4) is low = TX with JP9 open, as `main.c` expects.
   - With JP9 fitted (ground-closure PTT), set `PTT_TX_LEVEL` to 1.
7. **Unused PCNT unit.** `NUM_FCNT_CHANNELS` is 8, but `gpio_num_input_freq[]` has 7 entries, so unit 7 counts GPIO0. This is harmless but unintended.
