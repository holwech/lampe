"""Plot the saved controlled comparisons (requires matplotlib)."""
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    root = Path(__file__).resolve().parent
    captures = [json.loads((root / "results" / f"{name}.json").read_text())
                for name in ["have-a-cigar-pcm-01", "have-a-cigar-pcm-02", "room-noise-pcm-01"]]
    methods = [("Lamp", "lamp-onsets", "lamp"),
               ("BTrack + current onsets", "lamp-onsets", "btrack"),
               ("BTrack + bass", "bass", "btrack"),
               ("BTrack + spectral", "complex-spectrum", "btrack")]
    colors = ["#64696f", "#4675a6", "#65844b", "#9b639b"]
    fig, (music, noise) = plt.subplots(1, 2, figsize=(12, 6), width_ratios=[1.15, 1])
    fig.subplots_adjust(left=.08, right=.97, top=.78, bottom=.25, wspace=.3)
    fig.suptitle("Better tempo estimates, but false beats remain", x=.08, y=.96,
                 ha="left", fontsize=19, fontweight="bold")
    fig.text(.08, .885, "Same microphone recordings, fixed processing settings; original lamp firmware restored.",
             fontsize=11, color="#555555")
    for i, ((label, method, tracker), color) in enumerate(zip(methods, colors)):
        values = [c["methods"][method]["default"][tracker]["near_reference_percent"] for c in captures[:2]]
        bars = music.bar([x + (i - 1.5) * .19 for x in range(2)], values, width=.17,
                         label=label, color=color)
        music.bar_label(bars, labels=[f"{v:.0f}%" for v in values], padding=4, fontsize=10)
        beats = captures[2]["methods"][method]["default"][tracker]["beat_events_including_startup"]
        bar = noise.bar(i, beats, color=color, width=.68)
        noise.bar_label(bar, padding=4, fontsize=11)
    music.set(xticks=[0, 1], xticklabels=["Have a Cigar 1", "Fresh passage 2"], ylim=[0, 115],
              yticks=[0, 25, 50, 75, 100], ylabel="Windows within approximate 120 ±4 BPM (%)")
    music.set_title("Tempo agreement after 10 s warm-up", loc="left", fontsize=12, pad=16)
    noise.set(xticks=list(range(4)), xticklabels=["Lamp", "Current\nonsets", "Bass", "Spectral"],
              ylim=[0, 80], ylabel="Beat events during 30 s of paused music")
    noise.set_title("Room-noise control · lower is better", loc="left", fontsize=12, pad=16)
    for ax in [music, noise]:
        ax.spines[["top", "right"]].set_visible(False)
        ax.grid(axis="y", alpha=.18)
        ax.set_axisbelow(True)
    fig.legend(*music.get_legend_handles_labels(), loc="lower left", bbox_to_anchor=(.07, .12),
               ncol=4, frameon=False, fontsize=10)
    fig.text(.08, .065, "Approximate tempo agreement is not beat-alignment accuracy. Passage 2 often reports 125 BPM, just outside the band.",
             fontsize=9, color="#555555")
    fig.text(.08, .035, "BTrack has no unlocked state. The lamp also produces false beats on this new noise recording. Real beat positions are unannotated.",
             fontsize=9, color="#555555")
    output = root.parents[1] / "docs" / "bpm-btrack-comparison.png"
    fig.savefig(output, dpi=160, facecolor="white")
    plt.close(fig)
    print(output)


if __name__ == "__main__":
    main()
