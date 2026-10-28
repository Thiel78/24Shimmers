#!/bin/bash
# Installs 24 Shimmers (VST3 + Audio Unit) for the current user and clears the
# download quarantine so macOS lets Ableton load it.
set -e
cd "$(dirname "$0")"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

rm -rf "$VST3_DIR/24 Shimmers.vst3" "$AU_DIR/24 Shimmers.component"
cp -R "24 Shimmers.vst3" "$VST3_DIR/"
cp -R "24 Shimmers.component" "$AU_DIR/"

xattr -dr com.apple.quarantine "$VST3_DIR/24 Shimmers.vst3" "$AU_DIR/24 Shimmers.component" 2>/dev/null || true

# Make macOS notice the new Audio Unit.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "24 Shimmers installed:"
echo "  $VST3_DIR/24 Shimmers.vst3"
echo "  $AU_DIR/24 Shimmers.component"
echo
echo "Restart Ableton Live (or rescan plug-ins) and look under Plug-Ins > Thiel."
