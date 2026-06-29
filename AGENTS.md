# Agent Instructions

- Do not invent gameplay, UI, animation, resource mapping, or rendering behavior.
- Document only original-game behavior; omit incorrect conclusions and repair history.
- For Rich4 behavior, first verify against IDA Pro MCP output, `rich4/asm`, `rich4/csrc`, or `rich4/docs`.
- When the original behavior is not yet known, stop at investigation or leave the feature unimplemented; do not add placeholder behavior that looks like a real implementation.
- Before editing, identify the original function address, resource index, chunk index, global/state field, or reference file that justifies the change.
- Do not infer an MKF archive from a resource number alone. Resolve the handle used at the original call site first. In this executable, `0x48A0E4` is the `Data.mkf` handle and `0x48A05C` is the `Panel.mkf` handle (initialized by `0x4015D6`).
- If an implementation is only a temporary debug aid, name it as such in code and do not present it as original-game behavior.
- Prefer small, reversible changes. Avoid broad rewrites that would need to be deleted if the original behavior later proves different.
- For builds and tests, use `python scripts/make_minimal.py <target>` from this directory. Long-running targets may add `--progress`; normal output stays minimal. Use `--raw-output` only for machine checks that must parse Make output. Prefer the narrowest relevant target first. Do not use `-B` or `--always-make` in normal mode; use `--allow-force-rebuild` only when intentionally validating a clean rebuild or investigating the build graph.
