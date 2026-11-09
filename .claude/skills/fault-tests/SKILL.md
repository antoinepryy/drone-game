---
name: fault-tests
description: Tester la réception des trames LD2460 face aux défauts d'une ligne série (bruit, coupure, longueur fausse). À utiliser avant et après toute modification du décodage des trames, et pour transformer un bug observé en test.
---

# Tests de trames défectueuses

Fichier : `Radar2460Calibration/host/tests/frame_tests.cpp`. Il inclut le croquis sans le modifier, lui envoie un flux d'octets par scénario (via `host/tests/test_support.h`) et compte les trames décodées. C'est l'une des trois suites lancées par `docker build --target tests .` : vue d'ensemble dans le skill `run-tests`.

## Lancer

```bash
cd Radar2460Calibration
docker build --target tests .
```

Compilé avec AddressSanitizer et UBSan : une lecture hors tampon arrête le build avec la ligne fautive. Le build échoue aussi si un scénario échoue. La même commande lance les tests unitaires et les tests du programme : lire la section « Tests de réception des trames » de la sortie.

Chaque ligne affiche un statut :

- `[OK]` : le nombre de trames attendu est retrouvé.
- `[ÉCHEC]` : régression, le build échoue.
- `[ÉCHEC CONNU]` : défaut connu, pas encore corrigé (`known = true`). Il ne fait pas échouer le build.
- `[CORRIGÉ (retirer known)]` : un défaut connu passe désormais. Le build échoue pour forcer à passer `known` à `false`, afin que le scénario protège le correctif.

## Ajouter un scénario

1. Écrire le flux d'octets qui reproduit le défaut avec les aides existantes : `targetsFrame(n, longueurForcée)`, `truncated(trame, octets)`, `badFooter(trame)`, `strayF4(trame)`. Pour des cibles aux coordonnées précises : `test::targetsFrame({{x_dm, y_dm}, …}, longueurForcée, type)`.
2. L'ajouter au tableau `scenarios` : nom, octets envoyés avant les trames valides, nombre de trames valides, nombre de trames attendues en sortie.
3. Le lancer sur le code actuel. S'il échoue, c'est un défaut : le marquer `known = true` dans la même PR que le test, ou le corriger dans la même PR.
4. Corriger, relancer, passer `known` à `false`.

Nombre attendu : les trames valides envoyées, plus le préfixe s'il est lui-même une trame valide (trame sans cible, F4 parasite avant un vrai header).

## Règles

- Un correctif du décodage n'est terminé que si tous les scénarios sont `[OK]`, y compris les anciens.
- Ne jamais baisser un nombre attendu pour faire passer un test.
- La simulation complète avec défauts (`SIM_FAULTS=1 SIM_FRAMES=12`) est vérifiée par les tests du programme, sortie comprise (`host/tests/expected/faults.txt`). Un nouveau défaut ajouté au simulateur (`makeFaultyFrame`) doit aussi être ajouté à `program_tests.sh` et aux sorties de référence (skill `run-tests`).

## Défauts connus à ce jour

Aucun. Les trois défauts d'origine (longueur corrompue mais plausible, trame tronquée, longueur hors format 11 + 4 × n) sont corrigés et leurs scénarios protègent le correctif :

- après un rejet, le croquis réanalyse les octets déjà reçus à partir du deuxième (`receiveRadarByte`), une trame valide qui commence dedans est retrouvée ;
- une trame de cibles dont la longueur ne vaut pas 11 + 4 × n est rejetée dès que la longueur est lue (`[RADAR] invalid data size dropped: N`) ;
- une trame interrompue plus de 50 ms est abandonnée (`[RADAR] frame timeout, dropped after N bytes`). Ce cas dépend du temps : il est testé par `host/tests/golden_tests.sh` (horloge virtuelle), pas par ce fichier où `millis()` vaut toujours 0.
