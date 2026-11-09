---
name: run-tests
description: Lancer et faire évoluer les tests automatisés du radar LD2460 (tests unitaires des fonctions du croquis, tests de réception des trames, tests du programme complet contre le radar simulé, tests du décodeur de l'interface). À utiliser après toute modification du croquis, du simulateur, de la couche host/ ou de gui/, avant d'ouvrir une PR, et quand un test échoue en CI.
---

# Tests automatisés

Une seule commande lance les cinq suites, depuis `Radar2460Calibration/` :

```bash
docker build --target tests .
```

Le build échoue si une suite échoue. La CI GitHub (`.github/workflows/tests.yml`) lance la même commande sur chaque PR et sur `main`, plus la compilation du firmware ESP32-C3.

## Les suites

| Suite | Fichier | Ce qu'elle vérifie |
|---|---|---|
| Unitaires | `host/tests/unit_tests.cpp` | `formatDm`, `processFrame`, `loop`, `setup` appelées une par une, sortie comparée au caractère près |
| Réception des trames | `host/tests/frame_tests.cpp` | nombre de trames retrouvées après un défaut de ligne série (skill `fault-tests`) |
| Programme | `host/tests/program_tests.sh` | binaire complet contre le radar simulé (défaut, 3 cibles, bornes de `SIM_TARGETS`, défauts, scénario `walk` à 8 personnes, mode sans fin) : code retour, comptages, rejets, sortie identique à `host/tests/expected/*.txt` |
| Sortie du décodeur | `host/tests/golden_tests.sh`, `host/tests/golden_tests.cpp` | suites d'octets fixes rejouées sur une horloge virtuelle (seul banc où `millis()` avance, donc seul à tester l'abandon d'une trame interrompue) : sortie série identique à `host/tests/expected/golden.txt` |
| Décodeur de l'interface | `gui/test_serial_parser.py`, `gui/test_program_output.py` | `SerialParser` sur des lignes choisies, puis sur les sorties de référence du vrai programme : si le format série change, l'interface le voit ici |

Les suites C++ sont compilées avec `-Werror`, AddressSanitizer et UBSan : une lecture hors tampon ou un débordement d'entier arrête le build avec la ligne fautive. Elles incluent le croquis tel quel ; `host/tests/test_support.h` remplace le radar par un flux d'octets choisi par le test (`test::feed`), capture la sortie série (`test::capture`) et construit des trames (`test::targetsFrame`).

## Lire un échec

- Unitaire : `[ÉCHEC] ligne N : expression`, suivi de la valeur obtenue et de la valeur attendue.
- Trames : `[ÉCHEC] scénario : x/y trames` (voir `fault-tests` pour `ÉCHEC CONNU` et `CORRIGÉ`).
- Programme : `[ÉCHEC] description : obtenu '…', attendu '…'`, ou un `diff -u` entre la sortie de référence et la sortie obtenue.
- Décodeur : sortie `unittest` (`FAIL: test_…`) dans l'étape `gui-tests`. Pour itérer sans Docker : `cd gui && python3 -m unittest -v`.

## Ajouter un test unitaire

1. Écrire une fonction `testXxx()` dans `unit_tests.cpp` avec `CHECK_EQ(chaîne, attendu)` ou `CHECK_INT(entier, attendu)`.
2. L'ajouter à la liste `tests` de `main()` avec un nom lisible.
3. Pour `loop()`, passer par `runLoop(octets)` ; pour `processFrame`, par `process(trame)`.
4. Une nouvelle fonction du croquis arrive avec son test unitaire dans la même PR.

## Sorties de référence du programme

`host/tests/expected/golden.txt` se régénère avec `UPDATE_EXPECTED=1 host/tests/golden_tests.sh`, mêmes règles que ci-dessous.

`host/tests/expected/default.txt`, `faults.txt` et `walk.txt` sont la sortie exacte du programme (réglages par défaut, `SIM_FAULTS=1 SIM_FRAMES=12`, `SIM_SCENARIO=walk SIM_TARGETS=8 SIM_FRAMES=600`, une boucle complète de 60 s). Le décodeur de l'interface les relit aussi. Si un changement modifie la sortie **volontairement**, les régénérer puis relire le diff :

```bash
docker build --target host -t radar2460-host .
docker run --rm -v "$PWD/host/tests:/t" -e UPDATE_EXPECTED=1 radar2460-host \
  /t/program_tests.sh /out/radar-calibration /t/expected
git diff host/tests/expected/
```

Le diff fait partie de la PR et s'explique dans sa description. Si le format des lignes change, adapter `gui/serial_parser.py` dans la même PR : `test_program_output.py` échouera sinon.

## Règles

- Ne jamais régénérer les sorties de référence ni modifier une valeur attendue pour faire passer un test sans avoir compris pourquoi la sortie a changé.
- Un nouveau réglage du simulateur (`SIM_…`) ou un nouveau scénario arrive avec son bloc dans `program_tests.sh`.
- Un changement de `gui/serial_parser.py` arrive avec son test dans `gui/test_serial_parser.py`.
- Un bug corrigé devient d'abord un test qui échoue, puis passe.
- Ne pas annoncer « tests OK » sans avoir lu la dernière ligne de chaque suite (`0 test(s) en échec`, `0 échec(s)`).
