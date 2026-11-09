# Radar LD2460

Croquis Arduino de calibration X/Y du radar LD2460 sur carte ESP32-C3 XIAO, compilé et exécuté dans Docker.
Sert de support pour une démonstration de versionning (git, branches, pull requests, SemVer, changelog).

## Démarrer

Prérequis : Docker Desktop.

```bash
cd Radar2460Calibration
docker build -t radar2460-calibration .
docker run --rm radar2460-calibration
```

Le `build` compile le vrai firmware ESP32-C3 et une version du même croquis qui tourne sur l'ordinateur. Le `run` exécute cette version contre un radar LD2460 simulé.

Détails et options : [`Radar2460Calibration/README.md`](Radar2460Calibration/README.md). Guide pas à pas de toutes les commandes : [`COMMANDES.md`](COMMANDES.md).

## Structure

```
Radar2460Calibration/
  Radar2460Calibration.ino   croquis (lecture et décodage des trames radar)
  Config.h                   broches et options de la carte
  host/                      couche Arduino simulée et faux radar, pour l'exécution sur ordinateur
  Dockerfile                 compilation du firmware et de la version ordinateur
.vscode/tasks.json           commandes lançables depuis VS Code
COMMANDES.md                 guide des commandes pour débutant
CHANGELOG.md                 historique des versions
```

## Historique

Les versions 0.1.0 à 0.3.0 (tags `v0.1.0` à `v0.3.0`) contenaient une application de pilotage de drone en C/SDL2, retirée depuis. Elle reste consultable avec `git switch --detach v0.3.0`.
