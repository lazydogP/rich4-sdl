#!/bin/sh
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DIR" || exit 1
mkdir -p saves logs
[ ! -f logs/launch.log ] || mv -f logs/launch.log logs/launch.previous.log
[ ! -f logs/console.log ] || mv -f logs/console.log logs/console.previous.log
[ ! -f logs/perf.log ] || mv -f logs/perf.log logs/perf.previous.log
exec 3>logs/launch.log
echo "started=$(date '+%Y-%m-%d %H:%M:%S %z')" >&3
echo "directory=$DIR" >&3
echo "system=$(uname -a)" >&3
printf 'arguments=' >&3
printf ' <%s>' "$@" >&3
printf '\n' >&3
grep -E 'MemTotal:|MemAvailable:|SwapTotal:' /proc/meminfo >&3 2>/dev/null
export LD_LIBRARY_PATH="$DIR/lib:/mnt/SDCARD/miyoo/lib:/config/lib:/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# The Mini's panel needs a 180-degree turn. Set to 0 for OS SDL builds that
# already turn the panel (e.g. an alternate MinUI SDL).
export RICH4_MIYOO_ROTATE="${RICH4_MIYOO_ROTATE:-1}"
# Optional temporary performance diagnostics. A small local debug.env can
# enable these switches on devices launched by a menu with no shell.
if [ -f debug.env ]; then
    . ./debug.env
fi
export RICH4_TEXT_BACKEND="${RICH4_TEXT_BACKEND:-ttf}"
export RICH4_PROFILE="${RICH4_PROFILE:-0}"
export RICH4_PROFILE_LOG="$DIR/logs/perf.log"
export RICH4_LOG_MEMORY="${RICH4_LOG_MEMORY:-0}"
if pgrep audioserver >/dev/null 2>&1; then
    for PADSP in /mnt/SDCARD/miyoo/lib/libpadsp.so /customer/lib/libpadsp.so; do
        if [ -f "$PADSP" ]; then
            export LD_PRELOAD="$PADSP${LD_PRELOAD:+:$LD_PRELOAD}"
            export SDL_AUDIODRIVER=dsp
            break
        fi
    done
fi
chmod +x ./rich4_sdl 2>/dev/null
echo "SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-default}" >&3
echo "SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-default}" >&3
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH" >&3
echo "LD_PRELOAD=${LD_PRELOAD:-}" >&3
echo "RICH4_MIYOO_ROTATE=$RICH4_MIYOO_ROTATE" >&3
echo "RICH4_PROFILE=$RICH4_PROFILE" >&3
echo "RICH4_TEXT_BACKEND=$RICH4_TEXT_BACKEND" >&3
./rich4_sdl --data-dir assets/original --music-dir assets/music \
    --input-config config/input.toml "$@" >logs/console.log 2>&1 &
GAME_PID=$!
echo "game_pid=$GAME_PID" >&3
trap 'kill -TERM "$GAME_PID" 2>/dev/null' TERM INT HUP
MEMORY_PID=
if [ "${RICH4_LOG_MEMORY:-0}" = 1 ]; then
    (
        while [ -r "/proc/$GAME_PID/status" ]; do
            echo "sample=$(date '+%s')"
            grep -E 'VmPeak:|VmSize:|VmHWM:|VmRSS:|VmSwap:' "/proc/$GAME_PID/status" 2>/dev/null
            awk '{print "minor_faults: " $10; print "major_faults: " $12}' "/proc/$GAME_PID/stat" 2>/dev/null
            grep -E 'MemAvailable:|SwapFree:' /proc/meminfo
            grep -E '^pswpin |^pswpout ' /proc/vmstat
            sleep 1
        done
    ) >logs/memory.log &
    MEMORY_PID=$!
fi
wait "$GAME_PID"
STATUS=$?
# A trapped signal can interrupt wait before the child has actually exited.
if kill -0 "$GAME_PID" 2>/dev/null; then
    wait "$GAME_PID"
    STATUS=$?
fi
[ -z "$MEMORY_PID" ] || kill "$MEMORY_PID" 2>/dev/null
echo "exit_status=$STATUS" >&3
echo "finished=$(date '+%Y-%m-%d %H:%M:%S %z')" >&3
grep -E 'MemAvailable:|SwapFree:' /proc/meminfo >&3 2>/dev/null
exit "$STATUS"
