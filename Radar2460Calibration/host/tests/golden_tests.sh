#!/usr/bin/env bash
# Tests de sortie du décodeur de trames : host/tests/golden_tests.cpp rejoue des suites
# d'octets fixes sur une horloge virtuelle (le seul banc qui fait avancer millis(), donc
# qui teste l'abandon d'une trame interrompue) et la sortie série est comparée au
# caractère près à host/tests/expected/golden.txt.
# Usage, depuis Radar2460Calibration/ : host/tests/golden_tests.sh
#   UPDATE_EXPECTED=1 host/tests/golden_tests.sh   régénère la sortie de référence
# Lancement normal : docker build --target tests .
set -eu
cd "$(dirname "$0")/../.."
CXX="${CXX:-g++}"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

"$CXX" -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Ihost -I. host/main.cpp host/tests/golden_tests.cpp -o "$WORK/golden_tests"
"$WORK/golden_tests" > "$WORK/golden.txt"

if [ "${UPDATE_EXPECTED:-0}" = 1 ]; then
    cp "$WORK/golden.txt" host/tests/expected/golden.txt
    echo "  [MIS À JOUR] host/tests/expected/golden.txt"
elif diff -u host/tests/expected/golden.txt "$WORK/golden.txt"; then
    echo "  [OK] $(grep -c '^## ' host/tests/expected/golden.txt) cas, sortie identique à expected/golden.txt"
else
    echo "  [ÉCHEC] sortie différente de expected/golden.txt (diff ci-dessus)" >&2
    exit 1
fi
