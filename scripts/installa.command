#!/bin/bash
# Installa Collateral Effects FTG-1 nelle cartelle di sistema dei plugin
# (/Library/Audio/Plug-Ins) e rimuove la quarantena. Chiede la password di amministratore.
cd "$(dirname "$0")"
set -e
VST3="/Library/Audio/Plug-Ins/VST3"
AU="/Library/Audio/Plug-Ins/Components"
echo "Installazione di FTG-1 (serve la password di amministratore)..."
sudo mkdir -p "$VST3" "$AU"
sudo rm -rf "$VST3/FTG-1.vst3" "$AU/FTG-1.component" "/Applications/FTG-1.app"
sudo cp -R "FTG-1.vst3" "$VST3/"
sudo cp -R "FTG-1.component" "$AU/"
sudo cp -R "FTG-1.app" "/Applications/"
sudo xattr -dr com.apple.quarantine "$VST3/FTG-1.vst3" "$AU/FTG-1.component" "/Applications/FTG-1.app" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo ""
echo "Collateral Effects FTG-1 installato:"
echo "  VST3: $VST3/FTG-1.vst3"
echo "  AU:   $AU/FTG-1.component"
echo "  App:  /Applications/FTG-1.app"
echo "Riavvia la DAW per vederlo."
