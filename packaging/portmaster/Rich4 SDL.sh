#!/bin/bash

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
GAMEDIR="$SCRIPT_DIR/rich4"
if [ ! -d "$GAMEDIR" ]; then
  echo "Rich4 payload directory not found: $GAMEDIR" > "$SCRIPT_DIR/rich4-launch.log"
  exit 1
fi
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -n "$controlfolder" ] && [ -r "$controlfolder/control.txt" ]; then
  : # Preserve the firmware launcher's explicit PortMaster location.
elif [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi
echo "PortMaster control folder: $controlfolder"
if [ ! -r "$controlfolder/control.txt" ]; then
  echo "PortMaster control.txt not found. Launch through the firmware's Ports menu."
  exit 1
fi
source "$controlfolder/control.txt"
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

if [ -d "/$directory/ports/rich4" ]; then
  GAMEDIR="/$directory/ports/rich4"
fi
cd "$GAMEDIR" || exit 1
printf 'Rich4 startup: CFW=%s arch=%s game=%s\n' "$CFW_NAME" "$DEVICE_ARCH" "$GAMEDIR"
getconf GNU_LIBC_VERSION 2>/dev/null || true

if [ "$DEVICE_ARCH" != "aarch64" ]; then
  pm_show_error "Rich4 SDL requires aarch64. Detected: $DEVICE_ARCH. See rich4/log.txt." --timeout 12
  pm_finish
  exit 1
fi
mkdir -p assets/original assets/music assets/fonts saves
if [ ! -f assets/fonts/rich4.ttc ]; then
  pm_show_error "Add your own CJK font as rich4/assets/fonts/rich4.ttc. See rich4/rich4.md." --timeout 12
  pm_finish
  exit 1
fi
if [ ! -f assets/original/Data.mkf ] && [ ! -f assets/original/DATA.MKF ] && [ ! -f assets/original/data.mkf ]; then
  pm_show_error "Add your own Rich4 V3.11 MKF files to rich4/assets/original. See rich4/rich4.md." --timeout 12
  pm_finish
  exit 1
fi

export LD_LIBRARY_PATH="$GAMEDIR/libs.aarch64:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
chmod +x ./rich4_sdl.aarch64
$GPTOKEYB "rich4_sdl.aarch64" -c "./rich4.gptk" &
pm_platform_helper "$GAMEDIR/rich4_sdl.aarch64"
./rich4_sdl.aarch64 --data-dir ./assets/original --music-dir ./assets/music --font ./assets/fonts/rich4.ttc
result=$?
echo "Rich4 exit status: $result"
if [ "$result" -ne 0 ]; then
  pm_show_error "Rich4 SDL exited with status $result. See rich4/log.txt for the loader or SDL error." --timeout 12
fi
pm_finish
exit "$result"
