# assets/music

Place user-provided OGG, WAV or MID music files here. For each track the
runtime tries OGG first, then WAV, then MID. Uppercase extensions are also
accepted. MID is attempted only when SDL_mixer reports an available MIDI
backend; platforms without one continue to use OGG/WAV.

The settings panel retains the original eight-entry CD-track model:

```text
Original/track02.ogg          through Original/track09.ogg
MultiverseJourney/track02.ogg through MultiverseJourney/track09.ogg
```

The same paths accept `.wav` and `.mid`. `Original` contains the base-game
songs, while `MultiverseJourney` contains the eight replacement songs.
Changing the bank does not change maps or other resources.

Original `sub_4549CF` reads the fixed MIDI table at `0x47E793`; its high
`0x8000` flag is cleared before lookup. Shared scene music maps as follows;
each numbered stem accepts OGG/WAV/MID in that order:

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

The original filenames (`MIDI01.MID`, `Midi01.mid`, `midi01.mid`, etc.) are
also accepted beside numbered tracks. The same format preference applies
to those names: `MIDI01.ogg` takes priority over `MIDI01.mid`.

The V3.11 map playlist at `0x47E773`, used by `0x454D91` and `0x454BCC`, is
`RICH08`, `RICH16` through `RICH22`. Their original names are accepted in
`MultiverseJourney/`, or as a fallback in the music root directory. This
playlist does not fill a missing `Original` bank.

macOS builds enable SDL_mixer's native MIDI backend and use Apple's system
synthesizer. Windows and other platforms use the MIDI backends available in
their SDL_mixer build; software synthesizers may require locally configured
instrument banks. The project does not bundle those banks.
