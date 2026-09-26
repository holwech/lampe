# Archived beat detection experiment

These files preserve the unfinished beat detector from commit `fae3df8`. They
are outside PlatformIO's `src/` and `lib/` directories and are not compiled or
selectable in the lamp firmware. `BeatPrograms.cpp` preserves its old effect
wrappers. They refer to the old `Lampe` API and are reference material, not a
standalone build target.

The active sound-reactive effect uses `lib/LampLogic/BeatTracker.h`, independently
of this archived experiment. Current research and physical-input comparisons are
in [the BTrack benchmark](../btrack-benchmark/).

Before reviving beat detection, address the existing problems:

- Pass a measured interval (not an array pointer) to BPM conversion, use
  `60000UL / periodMs`, and handle zero intervals.
- Reset the interval timer on each detected beat and the sample counter at the
  intended cadence. Store durations separately from BPM values.
- Replace the unbounded sampling loops with incremental processing and a timeout.
- Remove per-sample serial printing, which interferes with the sample rate.
- Reset filter state when starting a new detection run, and define how the
  resulting tempo drives a light effect without blocking button handling.

Validate against recorded audio and the real microphone before integrating it.
