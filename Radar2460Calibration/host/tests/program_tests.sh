#!/usr/bin/env bash
# Tests du programme complet : lance le binaire natif contre le radar simulé et vérifie
# sa sortie (code retour, nombre de trames, messages de rejet, sortie de référence).
# Usage : program_tests.sh <binaire> <dossier des sorties de référence>
#   UPDATE_EXPECTED=1 program_tests.sh ...   régénère les sorties de référence
# Lancement normal : docker build --target tests .
set -u

BIN=${1:?binaire radar-calibration}
EXPECTED=${2:?dossier expected/}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

failures=0
total=0

# run <nom> [VAR=valeur ...] : exécute le programme, stdout et stderr dans $WORK/<nom>.{out,err}
run() {
    local name=$1; shift
    env SIM_PERIOD_MS=0 "$@" timeout 10 "$BIN" >"$WORK/$name.out" 2>"$WORK/$name.err"
    echo $? >"$WORK/$name.code"
}

check() {   # check <description> <obtenu> <attendu>
    total=$((total + 1))
    if [ "$2" = "$3" ]; then
        echo "  [OK] $1"
    else
        echo "  [ÉCHEC] $1 : obtenu '$2', attendu '$3'"
        failures=$((failures + 1))
    fi
}

count() { grep -cF -- "$1" "$2" || true; }

golden() {  # golden <nom> : compare stdout à expected/<nom>.txt
    total=$((total + 1))
    if [ "${UPDATE_EXPECTED:-0}" = 1 ]; then
        cp "$WORK/$1.out" "$EXPECTED/$1.txt"
        echo "  [MIS À JOUR] $EXPECTED/$1.txt"
    elif diff -u "$EXPECTED/$1.txt" "$WORK/$1.out" >"$WORK/$1.diff"; then
        echo "  [OK] sortie identique à expected/$1.txt"
    else
        echo "  [ÉCHEC] sortie différente de expected/$1.txt :"
        sed 's/^/    /' "$WORK/$1.diff" | head -40
        failures=$((failures + 1))
    fi
}

echo "Programme : réglages par défaut (10 trames, 2 cibles)"
run default
check "code retour" "$(cat "$WORK/default.code")" 0
check "bannière" "$(count '=== LD2460 X / Y CALIBRATION ===' "$WORK/default.out")" 1
check "UART radar démarré" "$(count '[RADAR] UART started' "$WORK/default.out")" 1
check "trames décodées" "$(count '--- TARGETS ---' "$WORK/default.out")" 10
check "cibles affichées" "$(count 'Target → X=' "$WORK/default.out")" 20
check "lignes Y min / Y max" "$(count 'Y min = ' "$WORK/default.out")" 10
check "aucun rejet" "$(count '[RADAR]' "$WORK/default.out")" 1
golden default

echo "Programme : SIM_TARGETS=3, SIM_FRAMES=5"
run targets3 SIM_TARGETS=3 SIM_FRAMES=5
check "code retour" "$(cat "$WORK/targets3.code")" 0
check "trames décodées" "$(count '--- TARGETS ---' "$WORK/targets3.out")" 5
check "cibles affichées" "$(count 'Target → X=' "$WORK/targets3.out")" 15

echo "Programme : SIM_TARGETS hors bornes (0 -> 1, 9 -> 3)"
run targets0 SIM_TARGETS=0 SIM_FRAMES=4
run targets9 SIM_TARGETS=9 SIM_FRAMES=4
check "SIM_TARGETS=0 : 1 cible par trame" "$(count 'Target → X=' "$WORK/targets0.out")" 4
check "SIM_TARGETS=9 : 3 cibles par trame" "$(count 'Target → X=' "$WORK/targets9.out")" 12

echo "Programme : trames défectueuses (SIM_FAULTS=1, SIM_FRAMES=12)"
# 6 trames valides + 6 défauts : longueur 2 (x2), longueur 32767, trame sans cible,
# footer corrompu, F4 parasite. La trame sans cible et celle au F4 parasite sont valides.
run faults SIM_FAULTS=1 SIM_FRAMES=12
check "code retour" "$(cat "$WORK/faults.code")" 0
check "défauts envoyés (stderr)" "$(count '[SIM] fault:' "$WORK/faults.err")" 6
check "trames décodées" "$(count '--- TARGETS ---' "$WORK/faults.out")" 8
check "trame sans cible" "$(count 'No target' "$WORK/faults.out")" 1
check "longueurs rejetées" "$(count '[RADAR] invalid length dropped' "$WORK/faults.out")" 3
check "footer rejeté" "$(count '[RADAR] bad footer, frame dropped' "$WORK/faults.out")" 1
golden faults

echo
echo "$total vérifications du programme, $failures échec(s)"
[ "$failures" -eq 0 ]
