#!/bin/bash
# Builds TwinPitch (AU + VST3 + Standalone) on macOS and installs the plugins
# into ~/Library/Audio/Plug-Ins automatically.
set -e
cd "$(dirname "$0")"

if ! xcode-select -p >/dev/null 2>&1; then
  echo "Installing Xcode command line tools — rerun this script when it finishes."
  xcode-select --install
  exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
  echo "CMake is missing. Install it with:  brew install cmake   (or from cmake.org)"
  exit 1
fi

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j"$(sysctl -n hw.ncpu)"

echo ""
echo "Done. Installed:"
ls -d ~/Library/Audio/Plug-Ins/Components/TwinPitch.component ~/Library/Audio/Plug-Ins/VST3/TwinPitch.vst3 2>/dev/null || true
echo "Standalone app: build/TwinPitch_artefacts/Release/Standalone/TwinPitch.app"

# make the Audio Unit visible to Logic / GarageBand right away
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aufx Twnp Drau | tail -3 || true
