#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
[ -f libnpeg_plugin.so ] || { echo 'Run make first' >&2; exit 1; }
# VLC honors VLC_PLUGIN_PATH to load additional plug-ins without system install.
# The 'demux' folder mirrors a normal VLC modules layout.
TARGET="$HOME/.local/lib/vlc/plugins/demux"
mkdir -p "$TARGET"
install -m 0644 libnpeg_plugin.so "$TARGET/libnpeg_plugin.so"
echo "Installed $TARGET/libnpeg_plugin.so"
echo "Start VLC with:"
echo "VLC_PLUGIN_PATH=\"$HOME/.local/lib/vlc/plugins\" vlc /path/to/video.npeg"
echo "If you use another VLC install (Flatpak, Snap), build inside that runtime."
