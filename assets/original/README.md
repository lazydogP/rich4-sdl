# assets/original

Place your legally owned original `*.mkf` files here.

This repository does not include or download commercial game assets.

Recommended layout:

```text
assets/original/
  Data.mkf
  Panel.mkf
  Map.mkf
  Effect.mkf
  Speaking.mkf
  Jump.mkf
  Help.mkf
```

Desktop and Miyoo builds, packaging and resource tests all use this directory
directly. No version subdirectory is required:

```sh
bin/rich4_sdl --data-dir assets/original
```

The SDL runtime checks this directory first. Legacy `V3.11` and `V2.01`
subdirectories remain fallback locations when a resource is absent here.
