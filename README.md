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
| Espace | Arrêt d'urgence |
| Échap | Quitter |

## Structure

```
src/command.[ch]  liste des commandes, conversion nom <-> commande
src/drone.[ch]    modèle et simulation du drone (sans SDL, testable)
src/render.[ch]   dessin SDL (carte, drone, jauges)
src/main.c        fenêtre, boucle principale, clavier
tests/            tests unitaires de la logique de vol
```

## Version

Le numéro de version est défini à un seul endroit : `project(... VERSION x.y.z)` dans `CMakeLists.txt`.
Il est affiché dans le titre de la fenêtre et dans la sortie des tests.
