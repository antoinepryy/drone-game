---
name: build-and-simulate
description: Compiler le croquis radar LD2460 et l'exécuter contre le radar simulé dans Docker. À utiliser pour vérifier un changement du croquis, reproduire un comportement ou montrer la sortie série.
---

# Compiler et simuler

Tout se passe dans `Radar2460Calibration/`, avec Docker. Rien d'autre à installer.

## Compiler

```bash
cd Radar2460Calibration
docker build -t radar2460-calibration .
```

Le build produit deux choses : le vrai firmware ESP32-C3 (via `arduino-cli`) et une version ordinateur du même croquis (`host/main.cpp` inclut le `.ino`). Si le firmware ne compile pas, le build échoue : un changement qui passe sur l'ordinateur mais pas sur la carte est donc détecté.

La taille du firmware s'affiche dans la sortie de l'étape `arduino-cli compile`. La noter avant et après un changement.

## Exécuter

```bash
docker run --rm radar2460-calibration
```

Variables du simulateur (`host/radar_sim.cpp`) :

- `SIM_FRAMES` (10) : nombre de trames avant arrêt, `0` pour tourner sans fin.
- `SIM_PERIOD_MS` (300) : délai entre deux trames.
- `SIM_TARGETS` (2) : cibles par trame, de 1 à 3 (1 à 8 avec `SIM_SCENARIO=walk`).
- `SIM_FAULTS` (0) : `1` intercale des trames défectueuses.
- `SIM_SCENARIO` : `walk` pour des personnes qui entrent, traversent et repartent (boucle de 60 s), au lieu de cibles qui oscillent.
- `SIM_SPEED` (1) : avec `walk`, facteur de vitesse des déplacements.

Un changement du scénario `walk` modifie `host/tests/expected/walk.txt` et les comptages de `program_tests.sh` : mettre à jour les deux (skill `run-tests`).

Interface temps réel : `docker compose up -d --build`, puis http://localhost:8080.

```bash
docker run --rm -e SIM_FRAMES=20 -e SIM_TARGETS=3 radar2460-calibration
docker run --rm -e SIM_FAULTS=1 -e SIM_FRAMES=12 radar2460-calibration
```

Les messages `[SIM] fault: …` (sur stderr) annoncent chaque défaut envoyé. Le croquis doit répondre par un rejet (`[RADAR] … dropped`) puis lire la trame valide suivante.

## Lire la sortie

Une trame valide donne :

```
--- TARGETS ---
Target → X=1.20 m | Y=3.50 m
Y min = 3.50 m | Y max = 3.50 m
```

Compter les blocs `--- TARGETS ---` donne le nombre de trames décodées. Comparer ce nombre aux trames valides envoyées : un écart signifie que des trames sont perdues.

## Vérifier qu'un changement ne modifie pas la sortie

C'est automatique : `docker build --target tests .` compare la sortie du programme à `host/tests/expected/default.txt` et `faults.txt` (skill `run-tests`). Pour comparer à la main un autre réglage du simulateur :

```bash
docker run --rm radar2460-calibration > avant.txt    # sur main
docker run --rm radar2460-calibration > apres.txt    # sur la branche
diff avant.txt apres.txt
```

Sur des trames valides, le diff doit être vide, sauf si le changement vise justement la sortie.

## Pièges

- Le flash USB de la carte ne marche pas depuis Docker Desktop sur Mac : exporter le binaire (`docker build --target firmware-export -o out .`) puis flasher depuis le Mac avec `esptool.py` à l'adresse `0x0`.
- `host/Arduino.h` n'implémente que ce que le croquis utilise. Une nouvelle fonction Arduino dans le croquis doit y être ajoutée, sinon la version ordinateur ne compile plus. Les tests utilisent le même `Arduino.h` : si `available()`, `read()`, `begin()` ou `millis()` changent de signature, mettre à jour `host/tests/test_support.h`.
- Après un changement, lancer les tests (`docker build --target tests .`), pas seulement la simulation.
