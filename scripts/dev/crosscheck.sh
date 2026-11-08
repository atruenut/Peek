#!/usr/bin/env bash
# Developer convenience for Linux/macOS hosts: cross-compile Peek with MinGW-w64 to catch
# compile and link errors. This mirrors build.bat. It does NOT run the program.
# Usage: scripts/dev/crosscheck.sh [x64|arm64] [release|debug]
set -euo pipefail
cd "$(dirname "$0")/../.."
ARCH="${1:-x64}"; CONFIG="${2:-release}"
case "$ARCH" in
  x64) P=x86_64-w64-mingw32- ;;
  arm64) P=aarch64-w64-mingw32- ;;
  *) echo "unknown arch $ARCH"; exit 1 ;;
esac
CXX="${CXX:-${P}g++}"; RC="${RC:-${P}windres}"
OUT="build/$ARCH"; mkdir -p "$OUT/obj"
COMMON=(-std=c++20 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -Wall -Wextra -Isrc -Iresources)
if [ "$CONFIG" = debug ]; then OPT=(-O0 -g -DPEEK_DEBUG); LINKOPT=(); else OPT=(-O2 -DNDEBUG -ffunction-sections -fdata-sections); LINKOPT=(-s -Wl,--gc-sections); fi
"$RC" -O coff -I resources resources/peek.rc -o "$OUT/obj/peek_res.o"
OBJS=()
while IFS= read -r f; do
  o="$OUT/obj/$(basename "${f%.cpp}").o"
  echo "  $f"
  "$CXX" "${COMMON[@]}" "${OPT[@]}" -c "$f" -o "$o"
  OBJS+=("$o")
done < <(find src -name '*.cpp' | sort)
"$CXX" -municode -mwindows -static -static-libgcc -static-libstdc++ "${LINKOPT[@]}" "${OBJS[@]}" "$OUT/obj/peek_res.o" -o "$OUT/Peek.exe" \
  -lcomctl32 -ldwmapi -lole32 -loleaut32 -lshell32 -lshlwapi -lcomdlg32 -lrstrtmgr -lwbemuuid -lpsapi -ladvapi32 -lgdi32 -luser32 -luuid -luxtheme -lshcore -lcomdlg32 -luuid
echo "Built $OUT/Peek.exe ($(stat -c %s "$OUT/Peek.exe") bytes)"
