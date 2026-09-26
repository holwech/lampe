# Lampe

A hobby RGB lamp project with Arduino firmware for animated colors and experiments
with sound-reactive lighting. The lamp in the photos is **Lampe Mini**: a ring of
addressable LEDs inside a rounded, frosted diffuser, with a Pro Mini controller in
the base. Its firmware lives in [`lampe-mini/`](lampe-mini/).

## For humans: connecting the lamp

**The lamp needs its separate 5 V power supply to light up, even when the FTDI
adapter is connected to USB.** Connect the lamp's normal power lead to the socket
in the base. The FTDI adapter provides the programming/serial connection; in the
photographed setup its VCC lead is deliberately disconnected from the controller.

### FTDI wiring

Remove the diffuser to reach the blue Pro Mini's six-pin programming header. Use
the pin labels and the close-up below to orient the connectors. With the boards
facing as in the photo (Pro Mini reset button on the right, programming header on
the left), the Pro Mini header runs from `GRN` at the top to `BLK` at the bottom:

| Pro Mini header, top to bottom in the photo | FTDI adapter connection | Purpose |
| --- | --- | --- |
| `GRN` / DTR | `DTR` | Automatic reset for uploading |
| `TXO` | `RX` / `RXD` | Controller transmit to adapter receive |
| `RXI` | `TX` / `TXD` | Adapter transmit to controller receive |
| `VCC` | **Leave disconnected** | Use the lamp's separate supply |
| `GND` | `CTS` | Grounded CTS in the pictured six-pin arrangement |
| `BLK` / GND | `GND` | Common ground between the lamp and adapter |

The visible gap between the three upper and two lower connectors is the unused
`VCC` pin. `GRN` and `BLK` are board markings, not the colors of these jumper wires.
Match signals if using another adapter, since its pin order may differ. The
[Arduino Pro Mini schematic](https://www.arduino.cc/en/uploads/Main/Arduino-Pro-Mini-schematic.pdf)
shows the programming header's ground connections and capacitive reset circuit.

1. Disconnect power while attaching the jumpers according to the table.
2. Match the adapter's logic voltage to the fitted Pro Mini. The photos identify
   the board as a Pro Mini/ATmega328P, but do not establish its voltage variant.
3. Plug the FTDI adapter's USB cable into the computer and connect the lamp's
   **separate 5 V supply**. Keep FTDI VCC disconnected with this arrangement.
4. Select the adapter's serial port for uploads or serial monitoring. Both
   firmware variants use **115200 baud** for debug output.

<a href="docs/images/ftdi-wiring.jpg"><img src="docs/images/ftdi-wiring.jpg" alt="Close-up of the Pro Mini programming header and FTDI adapter, showing the disconnected VCC pin" width="600"></a>

*Programming connector close-up. Click any photo to see it at a larger size.*

### What the hardware looks like

| Inside the lamp | Separate power connection |
| --- | --- |
| [![Open lamp showing the LED ring, controller and FTDI wiring](docs/images/lampe-mini-open.jpg)](docs/images/lampe-mini-open.jpg) | [![Side of the base with the separate power lead plugged in and LEDs lit](docs/images/lampe-mini-power.jpg)](docs/images/lampe-mini-power.jpg) |

The LED strip follows the rim of the base, around the controller and a small
prototype board with screw terminals. The removable diffuser spreads the light
from the ring. The white lead supplies the lamp separately from the FTDI's black
USB cable.

| Assembled, purple | Assembled, blue | View from above |
| --- | --- | --- |
| [![Lampe Mini glowing purple](docs/images/lampe-mini-purple.jpg)](docs/images/lampe-mini-purple.jpg) | [![Lampe Mini glowing blue](docs/images/lampe-mini-blue.jpg)](docs/images/lampe-mini-blue.jpg) | [![Illuminated diffuser viewed from above](docs/images/lampe-mini-overhead.jpg)](docs/images/lampe-mini-overhead.jpg) |

If the FTDI is visible to the computer but the lamp stays dark, first check the
separate 5 V supply. Adapter activity alone does not confirm that the lamp is
powered. For upload problems, check the selected port, crossed TX/RX connections,
common ground, and DTR connection before changing firmware settings.

## Project context for developers and agents

This repository contains **two separate PlatformIO projects** targeting Arduino
on AVR. Choose the source tree for the hardware being worked on; each has its own
`platformio.ini`, `src/`, and `lib/`, including different classes named `Lampe`.

| | Lampe Mini: `lampe-mini/` | Original Lampe: repository root |
| --- | --- | --- |
| Hardware model in the code | 16 WS2812B addressable RGB LEDs | Five RGB light groups driven through TLC5940 |
| LED library | Vendored FastLED 3.1.8 | `Tlc5940` Git submodule |
| Input | Digital button/sensor on D2; analog audio on A0 | Capacitive touch inputs, using `CapacitiveSensor` submodule |
| Program control | Numeric menu, advanced on button release | Explicit startup, menus, programs and off states |
| Relation to the photos | Matches the pictured LED-strip lamp | Separate design; not the photographed LED-strip wiring |

Both configuration files currently say `platform = atmelavr`,
`framework = arduino`, and `board = nanoatmega328`. **The photographed controller
is marked Pro Mini.** The Nano build target is the repository's existing setting,
not proof of the fitted board or its bootloader. Verify the controller's clock,
voltage variant and bootloader before changing that target or upload parameters.

### Lampe Mini: where to make changes

| File or directory | Responsibility |
| --- | --- |
| [`lampe-mini/src/main.cpp`](lampe-mini/src/main.cpp) | Arduino `setup()` / `loop()`; initializes serial, selects the current effect, then updates the LEDs |
| [`lampe-mini/lib/Lampe/`](lampe-mini/lib/Lampe/) | LED buffer, FastLED setup, button handling, menu index, timing and amplitude decay |
| [`lampe-mini/lib/Programs/`](lampe-mini/lib/Programs/) | Effect implementations and the `selectProgram()` dispatch switch |
| [`lampe-mini/lib/Mic/`](lampe-mini/lib/Mic/) | Experimental filtering and beat detection |
| [`lampe-mini/lib/Config/`](lampe-mini/lib/Config/) | Older color/menu definitions; the active Mini loop uses the numeric dispatch in `Programs.cpp` |
| [`lampe-mini/lib/test/`](lampe-mini/lib/test/) | Manual LED demo helpers; not an automated test suite and not called by the current entry point |
| [`lampe-mini/lib/FastLED-3.1.8/`](lampe-mini/lib/FastLED-3.1.8/) | Bundled third-party library; application effects belong in `Programs`, not here |

Hardware constants are in
[`lampe-mini/lib/Lampe/Lampe.h`](lampe-mini/lib/Lampe/Lampe.h):

| Setting | Current value |
| --- | --- |
| LED data | D3 (`DATA_PIN`) |
| LED type / color order | `WS2812B` / `GRB` |
| LED count | 16 |
| Global brightness | 100 on FastLED's 0–255 scale |
| Frame pacing | `FRAMES_PER_SECOND = 120`; blocking effects can run more slowly |
| Button/sensor input | D2, configured as `INPUT`; a HIGH-to-LOW transition advances the menu |
| Audio input | A0 (`analogRead(0)` in the amplitude effect and microphone code) |

The Mini starts at menu index 0. The current button cycle reaches indices **0–5**:
`quarter_blink`, `flow`, `amplitude_sensor`, `ambulance`, `ambulance_hue`, and
`fire_place`. Although named `NUM_MENU_OPTIONS`, the value `5` is used as the
highest menu index. `selectProgram()` also implements `northern_lights` at 6 and
`rainbow` at 7, but the current button cycle cannot reach them. `setup()` calls
`rainbow()` once before the normal loop.

The amplitude effect samples A0 for 50 ms and drives red brightness from the
peak-to-peak signal with a decay helper. Beat detection is experimental and is
not selected by the current menu. A0 and D2 are firmware expectations; the photos
do not expose enough internal wiring to serve as a complete sensor schematic.

### Original Lampe

The root [`src/main.cpp`](src/main.cpp) initializes the TLC5940 and dispatches
states from [`lib/State/`](lib/State/). [`lib/Programs/`](lib/Programs/) implements
the touch menus, random colors, flowing colors, a dimmed flow, and a single-light
program with adjustable tempo. Touch input 0 controls the active menus: a short
click cycles options, a longer click selects, and a hold exits a program or turns
the lamp off from a menu.

[`lib/Lampe/`](lib/Lampe/) defines five light groups and capacitive sensors with
D2 as the shared send pin and D4–D8 as receive pins. Each RGB group occupies nine
TLC channels: three red, three green and three blue. `setLight()` maps 0–255 color
values through a 12-bit brightness lookup table; the five groups span channels
0–44. Preserve that mapping and check the TLC library's chain configuration when
working on this hardware. Root microphone integration is unfinished and disabled
in the entry point.

### Building, uploading and observing

Use PlatformIO Core (`pio`). From the repository root, these commands select the
two projects explicitly:

```sh
# Build Lampe Mini (the pictured lamp).
pio run --project-dir lampe-mini --environment nanoatmega328

# Initialize the original lamp's library submodules, then build it.
git submodule update --init --recursive
pio run --project-dir . --environment nanoatmega328

# Find the FTDI serial port.
pio device list
```

Once the selected firmware builds, replace `YOUR_SERIAL_PORT` below with the
adapter's actual port (for example, `/dev/cu.usbserial-...` on macOS). Connect the
lamp as described above before uploading:

```sh
pio run --project-dir lampe-mini --environment nanoatmega328 --target upload --upload-port YOUR_SERIAL_PORT
pio device monitor --port YOUR_SERIAL_PORT --baud 115200
```

For original Lampe hardware, use `--project-dir .` in the upload command. Close
the serial monitor before uploading. The monitor baud rate comes from
`Serial.begin(115200)` and is separate from the bootloader's upload speed. See the
PlatformIO references for [`pio run`](https://docs.platformio.org/en/latest/core/userguide/cmd_run.html)
and [serial monitoring](https://docs.platformio.org/en/latest/core/userguide/device/cmd_monitor.html).

### Current limitations and useful checks

This is experimental firmware, and a successful build on a current toolchain has
not been established by this documentation update. In particular:

- The root libraries `lib/CapacitiveSensor` and `lib/Tlc5940` are submodules and
  need initialization after cloning. Platform/toolchain versions are not pinned.
- Mini's `Mic::detectBeat()` passes the `beat_times` array to `getBPM(uint32_t)`,
  which expects one period value. This is an existing type mismatch to investigate
  if the build fails in the microphone code, even though beat detection is not
  selected at runtime.
- The `.travis.yml` files are commented-out templates. There is no active
  automated test suite in this repository.
- Root debug output calls the consuming `click()` / `longClick()` accessors;
  account for that when investigating missed touch events.

For firmware changes, build the affected project first, then verify on the actual
lamp with its separate 5 V supply: startup lighting, the available menu effects,
button/touch behavior, and serial output at 115200 baud. Check sound response when
changing the audio code. Record which hardware and firmware tree were tested;
compilation alone does not validate wiring or light output.

The photos in [`docs/images/`](docs/images/) are JPEG copies of the six reference
photos supplied with this project. They document the physical Mini setup; source
files remain the reference for the firmware behavior described above.
