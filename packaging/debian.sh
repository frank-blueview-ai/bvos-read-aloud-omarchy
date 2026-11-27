#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname -- "$0")/.."
version=$(jq -er '.version' manifest.json)
root=$(mktemp -d)
trap 'rm -rf -- "$root"' EXIT
arch=$(dpkg --print-architecture)

install -Dm755 build/standalone/bvos-read-aloud "$root/usr/bin/bvos-read-aloud"
install -Dm644 packaging/bvos-read-aloud.desktop "$root/usr/share/applications/bvos-read-aloud.desktop"
install -Dm644 assets/double-oval.svg "$root/usr/share/icons/hicolor/scalable/apps/bvos-read-aloud.svg"
install -Dm644 LICENSE "$root/usr/share/doc/bvos-read-aloud/copyright"
mkdir -p "$root/usr/lib/bvos-read-aloud" "$root/DEBIAN" artifacts
cp -a dist/bvos-speech "$root/usr/lib/bvos-read-aloud/speech-worker"

cat > "$root/DEBIAN/control" <<CONTROL
Package: bvos-read-aloud
Version: $version
Architecture: $arch
Maintainer: Blue View <https://github.com/frank-blueview-ai/bvos-read-aloud-omarchy>
Section: utils
Priority: optional
Depends: ca-certificates, libqt6gui6, libqt6multimedia6, libqt6qml6, libqt6quick6, libqt6quickcontrols2-6, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-templates, qml6-module-qtquick-window, qml6-module-qtqml-workerscript, qt6-qpa-plugins, libgomp1
Recommends: qt6-wayland, fonts-noto-core
Description: Offline desktop text reader with synchronized highlighting
 BvOs Read Aloud reads copied or pasted text using Kokoro. Its full-screen
 reader supports word, sentence, and paragraph highlighting.
CONTROL
dpkg-deb --root-owner-group --build "$root" "artifacts/bvos-read-aloud_${version}_${arch}.deb"
