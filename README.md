# Drone Pilot

Prototype C (SDL2) : une fenêtre graphique pour piloter un drone simulé.
Sert de support pour une démonstration de versionning (git, tags, SemVer, changelog).

## Compiler et lancer

Prérequis : un compilateur C, CMake 3.16+, SDL2 (`brew install sdl2` / `apt install libsdl2-dev`).

```bash
cmake -S . -B build
cmake --build build
./build/drone_pilot
ctest --test-dir build      # tests unitaires
```

## Commandes (clavier AZERTY, flèches aussi acceptées)

| Touche | Commande |
|--------|----------|
| T | Décollage |
| L | Atterrissage |
| Z / S | Avancer / reculer |
| Q / D | Gauche / droite |
| A / E | Rotation gauche / droite |
| R / F | Monter / descendre |
| H | Stationnaire |
| P | Relancer le script |
| Espace | Arrêt d'urgence |
| Échap | Quitter |

## Scripts de commandes

Le drone peut suivre une mission écrite dans un fichier texte :

```bash
./build/drone_pilot missions/carre.txt
```

Une commande par ligne, avec une durée optionnelle en secondes :

```
# commentaire
TAKEOFF          # attend que le drone soit en vol
UP 1.5
FORWARD 1.2      # avancer pendant 1,2 s
YAW_RIGHT 1      # 90° vers la droite
WAIT 0.5
LAND             # attend que le drone soit posé
```

Commandes : `TAKEOFF`, `LAND`, `UP`, `DOWN`, `FORWARD`, `BACKWARD`, `LEFT`, `RIGHT`,
`YAW_LEFT`, `YAW_RIGHT`, `HOVER`, `EMERGENCY`, `WAIT`. Sans durée, les mouvements durent 1 s.
Le clavier reste actif pendant le script ; un arrêt d'urgence l'interrompt.

## Structure

```
src/command.[ch]  liste des commandes, conversion nom <-> commande
src/script.[ch]   lecture et exécution des scripts de mission
src/font.[ch]     police bitmap intégrée
src/drone.[ch]    modèle et simulation du drone (sans SDL, testable)
src/render.[ch]   dessin SDL (carte, drone, jauges)
src/main.c        fenêtre, boucle principale, clavier
tests/            tests unitaires (vol, scripts)
missions/         exemples de scripts
```

## Version

Le numéro de version est défini à un seul endroit : `project(... VERSION x.y.z)` dans `CMakeLists.txt`.
Il est affiché dans le titre de la fenêtre et dans la sortie des tests.
