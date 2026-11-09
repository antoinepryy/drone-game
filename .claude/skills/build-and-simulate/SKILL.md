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

- `SIM_FRAMES` (10) : nombre de trames avant arrêt.
- `SIM_PERIOD_MS` (300) : délai entre deux trames.
- `SIM_TARGETS` (2) : cibles par trame, de 1 à 3.
- `SIM_FAULTS` (0) : `1` intercale des trames défectueuses.

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

```bash
docker run --rm radar2460-calibration > avant.txt    # sur main
docker run --rm radar2460-calibration > apres.txt    # sur la branche
diff avant.txt apres.txt
```

Sur des trames valides, le diff doit être vide, sauf si le changement vise justement la sortie.

## Pièges

- Le flash USB de la carte ne marche pas depuis Docker Desktop sur Mac : exporter le binaire (`docker build --target firmware-export -o out .`) puis flasher depuis le Mac avec `esptool.py` à l'adresse `0x0`.
- `host/Arduino.h` n'implémente que ce que le croquis utilise. Une nouvelle fonction Arduino dans le croquis doit y être ajoutée, sinon la version ordinateur ne compile plus.
