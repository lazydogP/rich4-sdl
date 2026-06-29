# Rich4 Font Hook

Small Windows hook launcher for observing the original game's GDI font usage.

It injects `font_hook.dll` into the target process, patches the target EXE import
table, and logs:

- `CreateFontA/W`
- `CreateFontIndirectA/W`
- `SelectObject(HFONT)`
- `TextOutA/W`
- `DrawTextA/W`

The log includes the requested `LOGFONT`, the selected HDC face reported by GDI,
and `TEXTMETRIC` charset/weight values.

## Build

The original Rich4 executable is expected to be 32-bit, so build the default x86
hook with a 32-bit MinGW compiler:

```powershell
.\build-font-hook.ps1
```

For a 64-bit target, use:

```powershell
.\build-font-hook.ps1 -Arch x64
```

## Run

```powershell
.\bin\font_hook_launcher.exe "D:\path\to\Rich4.exe"
```

Output is appended to:

```text
tools/font_hook/bin/font_hook.log
```

The launcher and DLL must have the same bitness as the target EXE. A 64-bit
launcher cannot inject into the original 32-bit game.
