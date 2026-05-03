#!/usr/bin/env bash
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_SRC="${SRC_DIR}/build-anime-smoother/AnimeSmoother.ofx.bundle"
if [[ ! -d "${PLUGIN_SRC}" ]]; then
  PLUGIN_SRC="${SRC_DIR}/build-anime-line/AnimeSmoother.ofx.bundle"
fi
if [[ ! -d "${PLUGIN_SRC}" ]]; then
  PLUGIN_SRC="${SRC_DIR}/build-universal/AnimeSmoother.ofx.bundle"
fi
if [[ ! -d "${PLUGIN_SRC}" ]]; then
  PLUGIN_SRC="${SRC_DIR}/build/AnimeSmoother.ofx.bundle"
fi
PLUGIN_DEST_DIR="/Library/OFX/Plugins/AnimeSmoother"
PLUGIN_DEST="${PLUGIN_DEST_DIR}/AnimeSmoother.ofx.bundle"
OLD_PLUGIN_DEST_DIR="/Library/OFX/Plugins/MLAAEdgeSmoother"
OLD_LINE_PLUGIN_DEST_DIR="/Library/OFX/Plugins/AnimeLineSmoother"

if [[ ! -d "${PLUGIN_SRC}" ]]; then
  echo "Plugin bundle not found: ${PLUGIN_SRC}"
  echo "Build it first:"
  echo "  /Applications/CMake.app/Contents/bin/cmake -S . -B build-anime-smoother -DOFX_SUPPORT_ROOT=/Users/miyatatoshihide/openfx"
  echo "  /Applications/CMake.app/Contents/bin/cmake --build build-anime-smoother"
  exit 1
fi

sudo mkdir -p "${PLUGIN_DEST_DIR}"
sudo rm -rf "${PLUGIN_DEST}"
sudo rm -rf "${OLD_PLUGIN_DEST_DIR}"
sudo rm -rf "${OLD_LINE_PLUGIN_DEST_DIR}"
sudo ditto "${PLUGIN_SRC}" "${PLUGIN_DEST}"
sudo xattr -dr com.apple.quarantine "${PLUGIN_DEST}" 2>/dev/null || true
sudo codesign --force --deep --sign - "${PLUGIN_DEST}" >/dev/null
rm -f "${HOME}/Library/Application Support/Blackmagic Design/DaVinci Resolve/OFXPluginCacheV2.xml" 2>/dev/null || true

echo "Installed:"
echo "  ${PLUGIN_DEST}"
echo
echo "Restart Autograph or DaVinci Resolve, then look for Anime Smoother under Filter/Anime."
