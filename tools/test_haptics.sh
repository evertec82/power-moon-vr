#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
B=${B:-build-tests}
mkdir -p "$B"
clang -O2 tools/vibration_test.c -o "$B/vibration-test.exe"
"$B/vibration-test.exe"
clang -O2 -Iruntime -Ithird_party/openxr/include tools/vr_haptics_test.c -lpthread -o "$B/vr-haptics-test.exe"
"$B/vr-haptics-test.exe"
clang -O2 -Iruntime -Ithird_party/openxr/include tools/gamepad_haptics_test.c -lSDL3 -lpthread -o "$B/gamepad-haptics-test.exe"
"$B/gamepad-haptics-test.exe"
clang -O2 tools/vr_input_test.c -o "$B/vr-input-test.exe"
"$B/vr-input-test.exe"
clang -O2 -Ithird_party/openxr/include tools/vr_hud_test.c -o "$B/vr-hud-test.exe"
"$B/vr-hud-test.exe"

clang -O2 tools/vr_prediction_test.c -o "$B/vr-prediction-test.exe"
"$B/vr-prediction-test.exe"
clang -O2 tools/vr_bloom_test.c -o "$B/vr-bloom-test.exe"
"$B/vr-bloom-test.exe"
clang -O2 -Iruntime tools/vr_lighting_test.c -o "$B/vr-lighting-test.exe"
for mode in no-lens-flare trace desktop missing unset invalid; do "$B/vr-lighting-test.exe" "$mode"; done
