#!/usr/bin/env bash
# Install the Sasayaku GNOME Shell extension into the user's extensions dir.
set -euo pipefail

UUID="sasayaku@wmeddie.github"
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$HERE/$UUID"
DEST_DIR="$HOME/.local/share/gnome-shell/extensions/$UUID"

rm -rf "$DEST_DIR"
mkdir -p "$(dirname "$DEST_DIR")"
cp -r "$SRC_DIR" "$DEST_DIR"

# Compile the GSettings schema in place so the hotkey setting is available.
glib-compile-schemas "$DEST_DIR/schemas"

echo "Installed $UUID -> $DEST_DIR"
echo
echo "Next steps:"
echo "  1) Log out and back in  (Wayland loads new extensions only at login)."
echo "  2) gnome-extensions enable $UUID"
echo "  3) Start the daemon:  $HERE/../build/src/sasayaku-daemon"
echo "  4) Press Ctrl+Alt+Space (or use the top-bar mic menu) to dictate."
