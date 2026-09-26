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
   firmware uses **115200 baud** for debug output.

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
WS2812B addressable RGB LEDs through the bundled FastLED 3.1.8 library. A digital
button/sensor on D2 cycles through lighting effects; A0 supplies analog audio for
sound-reactive brightness. The entry point, project libraries and build
configuration are all at the repository root.

The unused original TLC5940/capacitive-touch implementation has been removed;
its source remains available in Git history. No Git submodules are required.

[`platformio.ini`](platformio.ini) currently says `platform = atmelavr`,
`framework = arduino`, and `board = nanoatmega328`. **The photographed controller
is marked Pro Mini.** The Nano build target is the repository's existing setting,
not proof of the fitted board or its bootloader. Verify the controller's clock,
voltage variant and bootloader before changing that target or upload parameters.

### Where to make changes

| File or directory | Responsibility |
| --- | --- |
| [`src/main.cpp`](src/main.cpp) | Arduino `setup()` / `loop()`; initializes serial, selects the current effect, then updates the LEDs |
| [`lib/Lampe/`](lib/Lampe/) | LED buffer, FastLED setup, button handling, menu index, timing and amplitude decay |
| [`lib/Programs/`](lib/Programs/) | Effect implementations and the `selectProgram()` dispatch switch |
| [`lib/Mic/`](lib/Mic/) | Experimental filtering and beat detection |
| [`lib/Config/`](lib/Config/) | Older color/menu definitions; the active loop uses the numeric dispatch in `Programs.cpp` |
| [`lib/test/`](lib/test/) | Manual LED demo helpers; not an automated test suite and not called by the current entry point |
| [`lib/FastLED-3.1.8/`](lib/FastLED-3.1.8/) | Bundled third-party library; application effects belong in `Programs`, not here |

Hardware constants are in
[`lib/Lampe/Lampe.h`](lib/Lampe/Lampe.h):

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

### Building, uploading and observing

Use PlatformIO Core (`pio`). Run these commands from the repository root:

```sh
# Build the firmware.
pio run --environment nanoatmega328

# Find the FTDI serial port.
pio device list
```

Once the firmware builds, replace `YOUR_SERIAL_PORT` below with the
adapter's actual port (for example, `/dev/cu.usbserial-...` on macOS). Connect the
lamp as described above before uploading:

```sh
pio run --environment nanoatmega328 --target upload --upload-port YOUR_SERIAL_PORT
pio device monitor --port YOUR_SERIAL_PORT --baud 115200
```

Close the serial monitor before uploading. The monitor baud rate comes from
`Serial.begin(115200)` and is separate from the bootloader's upload speed. See the
PlatformIO references for [`pio run`](https://docs.platformio.org/en/latest/core/userguide/cmd_run.html)
and [serial monitoring](https://docs.platformio.org/en/latest/core/userguide/device/cmd_monitor.html).

### Current limitations and useful checks

This is experimental firmware; builds with a current toolchain have not yet been
verified. In particular:

- Platform/toolchain versions are not pinned. FastLED 3.1.8 is vendored in `lib/`.
- `Mic::detectBeat()` passes the `beat_times` array to `getBPM(uint32_t)`,
  which expects one period value. This is an existing type mismatch to investigate
  if the build fails in the microphone code, even though beat detection is not
  selected at runtime.
- There is no active CI configuration or automated test suite in this repository.

For firmware changes, build the project first, then verify on the actual
lamp with its separate 5 V supply: startup lighting, the available menu effects,
button/sensor behavior, and serial output at 115200 baud. Check sound response when
changing the audio code. Record which hardware and firmware revision were tested;
compilation alone does not validate wiring or light output.

The photos in [`docs/images/`](docs/images/) are JPEG copies of the six reference
photos supplied with this project. They document the physical Mini setup; source
files remain the reference for the firmware behavior described above.
