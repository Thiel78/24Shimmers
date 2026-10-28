# 24 Shimmers

A lucha libre shimmer reverb for macOS (VST3 + Audio Unit), built with JUCE.

![24 Shimmers](docs/screenshot.png)

Inspired by the Strymon BigSky shimmer: a dense, modulated reverb whose tail keeps
climbing in pitch. The pitch shifters sit *inside* the reverb's feedback network,
so every trip round the loop lifts part of the tail by the chosen interval.

## Controls

| On the mask | |
|---|---|
| **Shimmer** (left eye) | How much of the tail cascades up into the next interval. The tail length is kept intact at any setting. |
| **Decay** (right eye) | Reverb time, 0.5 s to 60 s. |
| **Freeze** (mouth) | Holds the current tail forever and ignores new input. |
| **Mix** (belt) | Dry/wet balance. |

| El Ring (the room) | |
|---|---|
| **Size** | Room size. |
| **Pre-Delay** | Gap before the reverb starts, up to 500 ms. |
| **Tone** | How fast the highs die away compared to the lows (dark to bright). |
| **Low Cut** | High-pass on the reverb and the shimmer path, to keep the low end clean. |

| El Cielo (the shimmer) | |
|---|---|
| **Shift 1 / Shift 2** | Two pitch voices: ±4th, ±5th, ±1 oct, +1 oct +5th, ±2 oct. |
| **Blend** | Balance between Shift 1 and Shift 2. |
| **Mod** | Slow chorus-like modulation of the reverb lines. |

Double-click any knob to reset it.

## Getting the plugin

Every push to `main` builds the plugin on GitHub's macOS servers as a universal
binary that runs on Apple Silicon and Intel.

1. Open the repository's **Actions** tab, click the latest green
   **Build macOS plugin** run, and download **24-Shimmers-mac** at the bottom.
   You can also push a tag such as `v0.1.0` to get the zip attached to a GitHub
   Release.
2. Unzip it (it contains `24-Shimmers-mac.zip`; unzip that too).
3. Right-click `install.command` and choose **Open**, or follow `INSTALL.txt`.
4. In Ableton Live, go to **Settings → Plug-Ins**, enable *Use VST3 Plug-In
   System Folders* and/or *Use Audio Units*, then **Rescan**. The plugin
   appears under **Plug-Ins → Thiel → 24 Shimmers**.

The plugin is ad-hoc signed rather than signed with a paid Apple Developer ID.
That's fine for your own Mac; the installer removes the download quarantine.

## Building locally

You need Xcode (or the Command Line Tools) and CMake. JUCE is downloaded
automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 8
./build/EngineTest
```

The plugins end up in `build/TwentyFourShimmers_artefacts/Release/`. There is
also a Standalone app there for trying the plugin without a DAW.

## Layout

```
Source/ShimmerEngine.h      DSP: pre-delay, diffusers, 8-line FDN, granular pitch shifters (plain C++)
Source/PluginProcessor.*    JUCE wrapper and parameters (Parameters.h)
Source/PluginEditor.*       UI: knobs, freeze mouth, mix belt, sparkles
Source/MaskArt.*            Vector artwork for the mask and ring
Source/LuchaLookAndFeel.*   Knob and belt drawing
tests/EngineTest.cpp        Offline DSP checks (decay time, loudness, shimmer, freeze, stability, CPU)
tests/UISnapshot.cpp        Renders the UI to PNG (-DSHIMMER_BUILD_SNAPSHOT=ON)
```
