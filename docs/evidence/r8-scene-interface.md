# R8.1 scene interface evidence

`app/scene.[ch]` is a C99, SDL-free scene protocol. Every scene exposes `enter`, `handle_command`, `update`, `build_view`, and `leave`. `Rich4SceneResult` is the only transition carrier and distinguishes `STAY`, `SWITCH`, and `QUIT`.

The scene registry contains five explicit objects: Menu, NewGame, Intro, Game, and Options. Options records the scene that opened it and returns there on Back. Menu/NewGame/Intro/Game transitions are command-driven; the SDL adapter may request a direct explicit transition while legacy event bodies are migrated incrementally.

`main_sdl.c` no longer assigns a new top-level `AppState` at transition sites. Each site calls `switch_app_scene`, which applies `rich4_scene_machine_transition` and accepts the resulting current scene only when the result is `SWITCH`. Existing local cleanup, audio, and drawing remain in place for behavioral compatibility and are future R8.2/R8.3 migration work.

`tests/unit/test_scene.c` covers the Menu → NewGame → Intro → Game sequence, Options return-to-caller, Back navigation, lifecycle counts, view construction, update, invalid target rejection, and Quit.

