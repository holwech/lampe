# Lampe

A hobby RGB lamp project with Arduino firmware for animated colors and experiments
with sound-reactive lighting. The lamp in the photos is **Lampe Mini**: a ring of
addressable LEDs inside a rounded, frosted diffuser, with a Pro Mini controller in
the base. This is the repository's sole firmware project, maintained at the root
in [`src/`](src/) and [`lib/`](lib/).

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
4. Select the adapter's serial port for uploads or serial monitoring. The
   firmware logs the selected program at **115200 baud** on startup and mode changes.

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

This is a single PlatformIO project targeting Arduino on AVR. It controls 16
WS2812B addressable RGB LEDs through FastLED 3.10.5. A digital button/sensor on D2
cycles through lighting effects; A0 supplies analog audio for
sound-reactive brightness. No Git submodules are required. The unused original
TLC5940/capacitive-touch implementation remains available in Git history.

[`platformio.ini`](platformio.ini) retains `board = nanoatmega328`, the existing
bootloader target. **The photographed controller is marked Pro Mini.** A successful
build for the Nano target does not verify the fitted board's clock, voltage or
bootloader. Check the actual controller before changing upload parameters.

### Where to make changes

| File or directory | Responsibility |
| --- | --- |
| [`src/main.cpp`](src/main.cpp) | Starts serial, waits one second, calls `lampe.begin()` from `setup()`, and runs `lampe.update()` from `loop()` |
| [`lib/Lampe/Hardware.h`](lib/Lampe/Hardware.h) | Pin assignments, LED count, brightness and frame interval |
| [`lib/Lampe/`](lib/Lampe/) | Hardware initialization, LED buffer, input polling, per-effect state and frame scheduling |
| [`lib/Programs/ProgramMenu.cpp`](lib/Programs/ProgramMenu.cpp) | Single effect table defining dispatch, audio input needs and menu length |
| [`lib/Programs/Programs.cpp`](lib/Programs/Programs.cpp) | The eight lighting effects |
| [`lib/LampLogic/LampLogic.h`](lib/LampLogic/LampLogic.h) | Hardware-independent brightness arithmetic, audio envelope, debouncing and timing helpers |
| [`tests/`](tests/) | Regression tests for arithmetic, input handling, menu dispatch, timer rollover and rendered LED colors |
| [`experiments/beat-detection/`](experiments/beat-detection/) | Archived, unfinished beat detector; excluded from the firmware build |
| [`platformio.ini`](platformio.ini) | Pinned AVR platform, toolchain, Arduino core and FastLED dependency; downloaded libraries live under ignored `.pio/libdeps/` |

The global `Lampe` object only initializes its data. Hardware calls happen in
`begin()`, after Arduino has initialized its timers. Each loop polls the button
and, in the amplitude mode, takes one ADC sample. LED rendering is paced separately
at up to 120 frames per second, without a frame delay or blocking audio window.
Effect timers and the audio envelope reset when switching modes; the shared hue
and existing LED colors carry through transitions.

### Hardware settings and effects

| Setting | Current value |
| --- | --- |
| LED data | D3 |
| LED type / color order | `WS2812B` / `GRB`, configured in `Lampe::begin()` |
| LED count | 16; mirrored effects require an even count |
| Global brightness | 100 on FastLED's 0–255 scale |
| Frame interval | 8,333 microseconds (approximately 120 FPS) |
| Button/sensor input | D2, `INPUT`; HIGH while pressed, advance on a stable LOW release |
| Debounce interval | 20 ms, configurable in `LampLogic.h` |
| Audio input | A0; 10-bit ADC values from 0 to 1023 |
| Serial output | 115200 baud; selected program index on startup and changes |

The existing input wiring supplies the button's logic level; the firmware does
not enable an internal pull-up. Verify the input module's polarity and pulse
length when changing the debounce interval or wiring. The photos do not provide
a complete sensor schematic.

The lamp starts at program 0, with an initial rainbow in the LED buffer. Each
button release advances through all eight programs, then wraps back to 0:

| Index | Effect |
| --- | --- |
| 0 | Random blocks of color (`quarterBlink`) |
| 1 | Mirrored flowing colors (`flow`) |
| 2 | Sound-reactive red brightness (`amplitude`) |
| 3 | Rotating red and blue (`ambulance`) |
| 4 | Rotating changing hues (`ambulanceHue`) |
| 5 | Warm flickering colors (`fireplace`) |
| 6 | Northern lights (`northernLights`) |
| 7 | Rainbow (`rainbow`) |

To add an effect, implement it in `Programs.cpp`, declare it in `Programs.h`, and
add it to the table in `ProgramMenu.cpp`. Menu length is derived from that table.
Set its audio flag if it needs microphone sampling, and extend the dispatch test.

The amplitude effect collects peak-to-peak values over successive 50 ms windows.
It scales them using 32-bit arithmetic, decays the envelope by one level every
5 ms, and clamps the 2.5× red brightness gain to 255. Decay follows elapsed time,
so loop speed does not change its rate. There are no serial writes per sample.
Beat detection is a separate archived experiment, not an available mode.

### Building and testing

Build dependencies are pinned in [`requirements-dev.txt`](requirements-dev.txt)
and `platformio.ini`:

| Dependency | Version |
| --- | --- |
| PlatformIO Core | 6.2.0 |
| Atmel AVR platform | 5.3.0 |
| Arduino AVR core | 1.8.8 (framework package 5.4.0) |
| AVR GCC | 7.3.0 (package 1.70300.191015, as recommended by the AVR platform) |
| FastLED | 3.10.5 |

PlatformIO downloads FastLED automatically; no third-party source needs to be
copied into `lib/`. The first dependency installation needs internet access.
Ordinary `chain` dependency discovery avoids the expensive preprocessor scan
of FastLED's optional modules. Files using FastLED directly should include
`<FastLED.h>` themselves so each application library declares that dependency.

Set up the build tool in a virtual environment from the repository root:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements-dev.txt
```

Run the tests and build:

```sh
# Install the libraries used by both the tests and firmware.
pio pkg install --environment nanoatmega328

# Requires Python 3 and a C++17 compiler (Clang or GCC); no lamp is needed.
python scripts/test.py

# Compile firmware for the existing bootloader target.
pio run --environment nanoatmega328

# Find the FTDI serial port.
pio device list
```

The test runner uses the host C++ compiler with warnings treated as errors and
undefined-behavior checks. Set `CXX` to select a compiler. Tests cover brightness
limits, audio sampling and decay, debounce and one-click behavior, dispatch to
all eight effects, index wrapping, and 32-bit timer rollover. Rendering tests
compile the real `Programs.cpp` with FastLED's host headers and color algorithms.
They check color conversion, fading, saturation, mirrored/opposite LED patterns,
effect timing, and repeatable 15-second frame sequences for all eight effects.
The amplitude rendering trace covers silence; audio input arithmetic is covered
separately by the logic tests. These host checks do not exercise LED signal timing.

The test adapter in `tests/fastled_colors.cpp` includes a small set of upstream
implementation files; review those paths and intentional color changes when
upgrading FastLED. If using a custom PlatformIO library directory, pass
`python scripts/test.py --fastled-dir /path/to/FastLED`.
Firmware compilation also checks representative brightness values using AVR's
actual integer widths. The [CI workflow](.github/workflows/ci.yml) installs the
pinned dependencies and runs both test suites and the AVR build on pushes and
pull requests.

### FastLED upgrade notes

The 3.1.8 → 3.10.5 upgrade also updates PlatformIO and the Arduino AVR core.
The default firmware build uses **458 bytes of static RAM** and **8,482 bytes of
flash**, compared with 391 and 8,162 bytes before the upgrade. Static RAM figures
exclude runtime stack/heap use. The configured board has 2,048 bytes of RAM and
30,720 bytes of application flash.

A clean AVR build emits upstream warnings in unused optional FastLED modules.
The affected functions are discarded from the linked firmware.

FastLED's modern HSV saturation curve makes the startup rainbow, rainbow effect
and northern lights brighter than the old library's output. For example,
`CHSV(0, 240, 255)` now produces RGB `(255, 1, 1)` instead of `(240, 0, 0)`.
This is the upstream color behavior; the effect logic, timing, global brightness
and LED wiring settings are retained. Rendering fingerprints record the new
behavior so subsequent dependency changes are visible in tests.

FastLED 3.10.5 also includes host and WebAssembly backends. The host rendering
tests establish that our effects can run outside the Arduino. A future local
simulator can use the same C++ effects with the upstream
[FastLED web compiler](https://github.com/zackees/fastled-wasm), with simulated
time/button/audio inputs. The 3D dashboard and live serial frame streaming are
not implemented yet; browser compilation remains a separate integration step.

### Uploading

Replace `YOUR_SERIAL_PORT` with the adapter's actual port (for example,
`/dev/cu.usbserial-...` on macOS). Connect the separate 5 V supply and FTDI as
described above before uploading:

```sh
pio run --environment nanoatmega328 --target upload --upload-port YOUR_SERIAL_PORT
pio device monitor --port YOUR_SERIAL_PORT
```

Close the serial monitor before uploading. `monitor_speed = 115200` is already
configured; it is separate from the bootloader's upload speed. See the PlatformIO
references for [`pio run`](https://docs.platformio.org/en/latest/core/userguide/cmd_run.html)
and [serial monitoring](https://docs.platformio.org/en/latest/core/userguide/device/cmd_monitor.html).

### Hardware verification

The cleanup and library upgrade were checked with host tests and an AVR build.
This firmware has not yet been flashed to or tested on the physical lamp. In
particular, check that:

- The lamp starts normally using its separate 5 V supply.
- One button press/release advances one effect, including northern lights and
  rainbow, and the menu wraps after the eighth effect.
- Button input remains responsive in the audio mode, and sound produces smooth
  red brightness without wrapping at high levels.
- Effect colors, timing and transitions look right on the actual LED strip.

Record the hardware and firmware revision tested. The JPEGs in
[`docs/images/`](docs/images/) are the six supplied reference photos and document
the physical Mini setup.
