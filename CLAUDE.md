# CLAUDE.md

Croquis Arduino de calibration X/Y du radar LD2460 sur carte ESP32-C3 XIAO (`Radar2460Calibration/`). Il est compilé et exécuté dans Docker, contre un radar simulé : la carte n'est pas nécessaire pour travailler.

## Commandes

Toutes depuis `Radar2460Calibration/` :

```bash
docker build -t radar2460-calibration .                      # firmware ESP32-C3 + version ordinateur
docker run --rm radar2460-calibration                        # exécution contre le radar simulé
docker run --rm -e SIM_FAULTS=1 -e SIM_FRAMES=12 radar2460-calibration
docker build --target tests .                                # tests de réception des trames (ASan + UBSan)
docker build --target firmware-export -o out .               # binaire à flasher
```

## Règles

- Le même fichier `.ino` tourne sur la carte et sur l'ordinateur (`host/` simule la couche Arduino). Ne jamais écrire de code qui ne marche que sur l'ordinateur : pas de `std::`, pas d'allocation dynamique dans `loop()`, pas de calcul flottant (l'ESP32-C3 n'a pas d'unité flottante, tout est en décimètres entiers).
- Une modification du décodage des trames passe par les tests (`docker build --target tests .`) avant et après le changement. Un nouveau cas de trame défectueuse devient d'abord un scénario de test.
- Le projet pilotera une lampe UV. Toute modification qui touche au relais, aux seuils de tension ou à la décision « présence / pas de présence » suit `.claude/skills/uv-safety-rules`.
- Les sorties série gardent leur format (`Target → X=… m | Y=… m`), pour que les sorties restent comparables d'une version à l'autre.
- Le dépôt sert aussi de démonstration de versionning : une branche et une pull request par sujet, des commits au format Conventional Commits (`fix(radar): …`).

## Skills du projet

- `build-and-simulate` : compiler, lancer la simulation, lire la sortie.
- `fault-tests` : ajouter et lancer les tests de trames défectueuses.
- `uv-safety-rules` : règles de sécurité pour la lampe UV.
