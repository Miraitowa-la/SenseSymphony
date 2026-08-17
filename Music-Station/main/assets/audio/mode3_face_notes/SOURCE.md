# Mode 3 Acoustic Grand Piano samples

Source: University of Iowa Electronic Music Studios, Musical Instrument Samples database:
https://theremin.music.uiowa.edu/MISPiano.html

- Instrument: Steinway & Sons Model B grand piano
- Performer: Evan Mazunik
- Original format: 44.1 kHz, 16-bit stereo AIFF
- Dynamic layer: `mf`
- Mapping: low C3-B3, mid C4-B4, high C5-B5
- Generated: 2026-07-17

Each output is generated deterministically by `tools/build_mode3_piano.py`: stereo
average downmix, DC removal, attack detection at -45 dB relative to the source
peak, a 5 ms pre-attack lead, 0.55 s crop, polyphase resampling to 16 kHz,
normalization to 0.82 full scale, and a 40 ms linear fade-out. Outputs are
16-bit mono PCM WAV files.

The MuseScore 1 handbook describes its older Steinway SoundFont based on the
University of Iowa Musical Instrument Samples as public domain:
https://musescore.org/en/print/book/export/html/51
