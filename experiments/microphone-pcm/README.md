# Temporary, regular microphone capture

This standalone diagnostic firmware records the existing lamp's ADC0/A0 at a
nominal **4 kHz, 10-bit resolution**. It does not run light programs. LED output
stays latched while capturing so WS2812 transmission cannot interrupt sampling.
The capture script restores a supplied normal firmware image in `finally`, even
when recording or the diagnostic upload fails. Restore is verified by AVRDUDE.
A process kill, lost USB connection or power failure can still require a manual
restore. This is measurement equipment, not a new dashboard capture mode.

Timer1 compare B auto-triggers the ADC; its clock is 16 MHz / 8 / 500. The ADC
clock is 125 kHz; a discarded initial conversion primes it. Its completion ISR
clears the trigger flag and places samples in a 128-entry ring. It never prints,
allocates memory or drives LEDs. The foreground loop sends 24 samples per 63-byte
CRC-protected packet at 115200 baud (10,500 bytes/s, below the 11,520 byte/s 8N1
limit). Ring overflow, bad checksums, missing packets and invalid ADC values cause
the recorder to fail, rather than producing an apparently continuous waveform.

Build the normal image first, preserve it, then build the diagnostic. Disconnect
the browser's serial connection before recording. The board is the existing
16 MHz ATmega328P with the Nano/old-bootloader upload configuration.

```sh
uv run --locked pio run -e nanoatmega328
cp .pio/build/nanoatmega328/firmware.hex /tmp/lampe-before-pcm.hex
uv run --locked pio run --project-dir experiments/microphone-pcm
uv run --locked python experiments/microphone-pcm/test_capture.py
uv run --locked python experiments/microphone-pcm/capture.py --port YOUR_SERIAL_PORT --seconds 75 --track 'Track and artist' --output captures/new-pcm --firmware experiments/microphone-pcm/.pio/build/nanoatmega328/firmware.hex --restore-firmware /tmp/lampe-before-pcm.hex
```

Reconnect the dashboard after restoration and select the desired light program.
If automatic restoration fails, run the normal uploader with the preserved image:

```sh
uv run --locked pio pkg exec --package platformio/tool-avrdude -- avrdude -N -p m328p -c arduino -P YOUR_SERIAL_PORT -b 57600 -D -U flash:w:/tmp/lampe-before-pcm.hex:i
```

Outputs are `adc.csv` (sample index and unmodified ADC reading), `serial.bin`
and `metadata.json`. Metadata records firmware hashes, validity, restoration,
sample count, input range, nominal sample rate and a host-rate sanity check.
The source impedance, microphone frequency response and anti-alias response are
not calibrated. This diagnostic intentionally changes sampling and LED activity;
results must be validated again under normal lamp operation before deployment.

Protocol: `LP`, version 1, count 24, uint16 sample rate, uint16 sequence, uint32
first sample index, uint16 dropped count, 24 uint16 ADC readings, CRC-8/0x07.
All multibyte fields are little endian. A nonzero dropped count invalidates the
stream; sample indices must not be interpreted as continuous after an overflow.
The firmware uses 450 bytes of static RAM and 2,448 flash bytes in this build.

See the [ATmega328P ADC auto-trigger documentation](https://ww1.microchip.com/downloads/en/devicedoc/atmel-7810-automotive-microcontrollers-atmega328p_datasheet.pdf)
and the [measured comparison](../btrack-benchmark/).
