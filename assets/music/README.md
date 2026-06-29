# assets/music

Place user-provided or converted OGG/WAV music files here.

The settings panel keeps the original eight-entry CD-track model, with two
separate banks:

```text
Original/track02.ogg          through Original/track09.ogg
MultiverseJourney/track02.ogg through MultiverseJourney/track09.ogg
```

`Original` contains the base-game songs. `MultiverseJourney` contains the
eight replacement songs from the Multiverse Journey version. The bank switch
does not change maps or other game resources.

The M1 SDL_mixer audio preview currently uses a provisional interpretation of
the original `playmid_4549cf(0)` main menu call: `MIDI01.MID` maps to
`track10.ogg`, with `track10.wav` as a fallback.

The new game setup screen follows `init_new_game()` in the decompiled code:
`playmid_4549cf(0x8001)` clears the high bit and uses MIDI table index 1, so
it maps to `MIDI02.MID`, currently `track11.ogg` or `track11.wav`.

This mapping still needs end-to-end validation after the menu-to-game flow,
map preview, and later scene music paths are working.

Shared converted scene-music mapping:

```text
MIDI01.MID   -> track10.ogg
MIDI02.MID   -> track11.ogg
MIDI03.MID   -> track12.ogg
MIDI04.MID   -> track13.ogg
MIDI05.MID   -> track14.ogg
MIDI06.MID   -> track15.ogg
MIDI07.MID   -> track16.ogg
MIDI08.MID   -> track17.ogg
MIDI09.MID   -> track18.ogg
MIDI10.MID   -> track19.ogg
MIDI11.MID   -> track20.ogg
MIDI12.MID   -> track21.ogg
MIDI13.MID   -> track22.ogg
MIDI14-1.MID -> track23.ogg
MIDI14-2.MID -> track24.ogg
MIDI15.MID   -> track25.ogg
MIDI16.MID   -> track26.ogg
```

Real-time MIDI playback is not part of the project target.
