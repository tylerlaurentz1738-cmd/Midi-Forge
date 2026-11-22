# MIDI Forge

An FL Studio plugin (VST3) that writes MIDI for chords, arp, lead, brass, pad, 808, kick, hi-hats, clap, snare, snare roll, open hat and perc. All the parts follow the same chords.

It comes trained on 225 MIDI files (the @starboyrob June pack). It learns which notes, rhythms, leaps and hi-hat rolls tend to follow each other, then writes new patterns. Every click of **New** or **New idea** gives a different pattern. Press **Train on my MIDI** to teach it more of your own files.

## 1. Build it on GitHub (no installs needed)

1. On github.com, create a new repository. Making it **Private** is a good idea.
2. Open the repository and click **Add file → Upload files**. Drag in **everything inside this folder**, including the `.github` folder, then click **Commit changes**.
   - If the `.github` folder doesn't upload, go to the **Actions** tab, click **set up a workflow yourself**, paste in the contents of `.github/workflows/build.yml` and commit it.
3. Go to the **Actions** tab. A run called **Build MIDI Forge** starts by itself and takes about 10–20 minutes.
4. When it shows a green tick, open the run. Under **Artifacts**, download **MIDI-Forge-Windows** and unzip it.

## 2. Install in FL Studio

1. Copy the **`MIDI Forge.vst3`** folder into `C:\Program Files\Common Files\VST3\`.
2. In FL Studio, open **Options → Manage plugins**, click **Find installed plugins**, then turn on the star next to **MIDI Forge**.
3. Add it from the channel rack: **+ → MIDI Forge**.

`MIDI Forge.exe` is the same thing as a standalone app, if you want to try it outside FL.

## 3. Use it

- Pick a **style** (Trap, WLR, Yeat, Hyperpop, Melodic, Memphis, Plugg), a key, scale and BPM.
- **New idea** writes a whole new beat. **New** on a part rewrites only that part. The kick and 808 are linked and always hit together.
- **Play** previews a part with simple built-in sounds. **Play all** plays everything that has **Mix** turned on.
- **Drag any piano roll** straight into FL's playlist or piano roll. **Save** saves one part. **Export all** writes the whole kit to a folder.
- **808 slides:** FL can't import slide notes from MIDI, so slide notes come in at velocity 100 (about 79% in FL). Select those notes in the piano roll and make them slide notes.
- **Drums** are all on C5 by default, so they play any one-shot at normal pitch. Switch to **GM drum map** for drum kits.

## 4. Teach it your own MIDI

Click **Train on my MIDI** and pick MIDI files or whole folders. It sorts them by file name:

| File name starts with | Learned as |
|---|---|
| `808`, `kick` | 808 / kick groove |
| `hh`, `hihat` | hi-hats and rolls |
| `oh`, `open hat` | open hats |
| `clap` / `snare` / `perc` | that drum |
| `(style) name 150bpm` | a melody for that style. A new style name adds a new style to the list. |
| anything else | a melody for the style that's selected now |

What you teach it is saved to `%APPDATA%\MIDI Forge\user_model.txt` and added on top of the factory training. **Reset training** clears it.

## For developers

- `Source/core`: the engine (MIDI read/write, the trainable model, the generators). Plain C++17 with no JUCE.
- `Source/plugin`: the JUCE plugin (processor, preview synth, white/grey UI).
- `tools/train.cpp` rebuilds `tools/factory_model.txt` from folders of MIDI, and `tools/embed.py` turns that into `Source/core/DefaultModel.h`.
- `tools/test.cpp` is a stress test for the engine.
- To build locally, install Visual Studio 2022 with C++ and CMake, then run `cmake -B build` and `cmake --build build --config Release`.
