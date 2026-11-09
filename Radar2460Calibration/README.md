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
| `SIM_FRAMES` | 10 | Nombre de trames envoyées avant arrêt (0 = sans fin) |
| `SIM_PERIOD_MS` | 300 | Délai entre deux trames |
| `SIM_TARGETS` | 2 | Cibles par trame (1 à 3, ou 1 à 8 avec `SIM_SCENARIO=walk`) |
| `SIM_FAULTS` | 0 | `1` intercale des trames défectueuses (longueur 2 ou 32767, trame sans cible, footer corrompu, F4 parasite avant le header) |
| `SIM_SCENARIO` | | `walk` : boucle de 60 s où jusqu'à 8 personnes entrent, traversent, s'arrêtent et repartent ; le côté X > 0 reste vide 21 s par boucle (8 personnes, vitesse ×1) |
| `SIM_SPEED` | 1 | Avec `walk` : facteur de vitesse des déplacements |

```bash
docker run --rm -e SIM_FRAMES=20 -e SIM_TARGETS=3 radar2460-calibration
docker run --rm -e SIM_FAULTS=1 -e SIM_FRAMES=12 radar2460-calibration
```

## Tests automatisés

```bash
docker build --target tests .
```

Lance cinq suites, le build échoue si l'une d'elles échoue :

- **unitaires** (`host/tests/unit_tests.cpp`) : `formatDm`, `processFrame`, `loop` et `setup` appelées une par une ;
- **réception des trames** (`host/tests/frame_tests.cpp`) : flux de trames défectueuses (longueur fausse, footer corrompu, coupure de ligne, F4 parasite), nombre de trames décodées ;
- **programme** (`host/tests/program_tests.sh`) : le binaire complet contre le radar simulé (dont le scénario `walk` et le mode sans fin), sortie comparée à `host/tests/expected/` ;
- **sortie du décodeur** (`host/tests/golden_tests.sh`) : suites d'octets fixes sur une horloge virtuelle (trame tronquée, longueur fausse, radar coupé au milieu d'une trame…), sortie série comparée à `host/tests/expected/golden.txt` ;
- **décodeur de l'interface** (`gui/test_*.py`) : `SerialParser` sur des lignes choisies et sur les sorties de référence du vrai programme.

Les suites C++ tournent sous AddressSanitizer et UBSan. La CI GitHub lance la même commande sur chaque PR. Mode d'emploi : [`.claude/skills/run-tests`](../.claude/skills/run-tests/SKILL.md) et [`.claude/skills/fault-tests`](../.claude/skills/fault-tests/SKILL.md).

## Visualisation en temps réel

```bash
docker compose up -d --build     # puis http://localhost:8080
```

Le conteneur `gui` lance le croquis (version ordinateur, faux radar en mode `walk`, 10 trames/s) et lit sa sortie série telle que la carte l'imprimerait. La page affiche le plan vu de dessus (Y à l'horizontale, X à la verticale positif vers le haut, radar à gauche) avec un réticule de coordonnées sous la souris, les coordonnées des cibles, l'état de la zone de danger (par défaut X > 0 ; délai de 10 s de `RADAR_SAFE_DELAY_MS`) et l'historique de la position X de chaque cible. Des curseurs règlent le nombre de personnes (1 à 8), leur vitesse et les bornes de la zone ; une case active les trames défectueuses.

Les couleurs suivent chaque personne d'une trame à l'autre (association au plus proche, 1 m maximum) : le radar, lui, ne numérote pas les cibles. Le champ de vision dessiné (±60°, 8 m) est indicatif.

Tests du décodeur de sortie série : `cd gui && python3 -m unittest -v`.

## Récupérer le firmware

```bash
docker build --target firmware-export -o out .
```

`out/Radar2460Calibration.ino.merged.bin` se flashe à l'adresse `0x0` (le flash USB n'est pas faisable depuis Docker Desktop sur Mac, le faire depuis l'hôte avec `esptool.py`).

## Fichiers

- `Radar2460Calibration.ino`, `Config.h` : le croquis. Après une trame rejetée, il réanalyse les octets reçus à partir du deuxième, pour retrouver une trame valide qui commencerait dedans ; une trame interrompue plus de 50 ms est abandonnée (`[RADAR] frame timeout`).
- `host/` : couche Arduino minimale (`Serial` vers stdout) et simulateur radar, pour l'exécution native.
- `host/tests/` : tests automatisés et sorties de référence.
- `gui/` : interface de visualisation (serveur Python sans dépendance, page HTML, décodeur de la sortie série et ses tests).
- `compose.yaml` : lance l'interface.
