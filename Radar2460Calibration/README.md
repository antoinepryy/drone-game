# Radar2460Calibration

Croquis de calibration X/Y du radar LD2460 sur ESP32-C3 XIAO. Docker fournit toutes les dépendances (arduino-cli, cœur ESP32, compilateur) : rien à installer en local.

## Compiler et exécuter

```bash
docker build -t radar2460-calibration .
docker run --rm radar2460-calibration
```

Le `build` compile le vrai firmware ESP32-C3 et une version native du même croquis. Le `run` exécute la version native contre un radar LD2460 simulé (trames `F4 F3 F2 F1 … F8 F7 F6 F5`) et affiche la sortie série.

Variables du simulateur :

| Variable | Défaut | Rôle |
|---|---|---|
| `SIM_FRAMES` | 10 | Nombre de trames envoyées avant arrêt |
| `SIM_PERIOD_MS` | 300 | Délai entre deux trames |
| `SIM_TARGETS` | 2 | Cibles par trame (1 à 3) |
| `SIM_FAULTS` | 0 | `1` intercale des trames défectueuses (longueur 2 ou 32767, trame sans cible, footer corrompu, F4 parasite avant le header) |

```bash
docker run --rm -e SIM_FRAMES=20 -e SIM_TARGETS=3 radar2460-calibration
docker run --rm -e SIM_FAULTS=1 -e SIM_FRAMES=12 radar2460-calibration
```

## Récupérer le firmware

```bash
docker build --target firmware-export -o out .
```

`out/Radar2460Calibration.ino.merged.bin` se flashe à l'adresse `0x0` (le flash USB n'est pas faisable depuis Docker Desktop sur Mac, le faire depuis l'hôte avec `esptool.py`).

## Fichiers

- `Radar2460Calibration.ino`, `Config.h` : le croquis, inchangé.
- `host/` : couche Arduino minimale (`Serial` vers stdout) et simulateur radar, pour l'exécution native.
