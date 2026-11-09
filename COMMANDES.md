# Guide des commandes

Toutes les commandes utilisables dans ce dépôt, pas à pas. Chaque bloc se copie-colle tel quel dans un terminal.

Le dépôt contient un programme Arduino pour carte ESP32-C3 qui lit un radar LD2460 (`Radar2460Calibration/`). Il est compilé et exécuté dans Docker : pas besoin de la carte ni des outils Arduino pour l'essayer.

---

## 0. Avant de commencer

### Ouvrir un terminal dans VS Code

1. Ouvrir le dossier du projet : menu **File > Open Folder…**, choisir `demo-versionning`.
2. Ouvrir le terminal intégré : menu **Terminal > New Terminal** (raccourci `` Ctrl+` ``).
3. Le terminal s'ouvre directement dans le dossier du projet. Vérifier :

   ```bash
   pwd
   ```

   La réponse doit se terminer par `demo-versionning`.

> Les commandes de ce guide partent toujours de la racine du projet. Si vous êtes perdu, revenez-y avec `cd` suivi du chemin du dossier.

### Lancer une commande sans la taper : les tâches VS Code

Les commandes principales sont aussi enregistrées comme **tâches VS Code** (fichier `.vscode/tasks.json`) :

1. `Cmd+Shift+P` (Mac) ou `Ctrl+Shift+P` (Windows/Linux).
2. Taper `Run Task` puis Entrée.
3. Choisir la tâche dans la liste, par exemple « Radar : exécuter ».

`Cmd+Shift+B` lance directement la compilation.

### Installer les outils (une seule fois)

Sur Mac, avec [Homebrew](https://brew.sh) :

```bash
brew install git
```

Puis installer **Docker Desktop** depuis https://www.docker.com/products/docker-desktop et le lancer. La baleine doit apparaître dans la barre des menus.

Vérifier que tout répond :

```bash
git --version
docker --version
```

Chaque commande doit afficher un numéro de version. Une erreur `command not found` signifie que l'outil n'est pas installé.

---

## 1. Radar LD2460 (programme ESP32)

Le programme est fait pour une carte ESP32-C3. Docker contient tous les outils nécessaires : rien à installer à part Docker. Toutes les commandes se tapent **dans le dossier `Radar2460Calibration/`** :

```bash
cd Radar2460Calibration
```

### 1.1 Compiler

```bash
docker build -t radar2460-calibration .
```

Cette commande compile deux choses :

- le vrai firmware pour la carte ESP32-C3 ;
- une version du même programme qui tourne sur l'ordinateur, branchée sur un faux radar.

Le premier lancement prend plusieurs minutes (téléchargement des outils ESP32). Les suivants prennent quelques secondes.

### 1.2 Exécuter

```bash
docker run --rm radar2460-calibration
```

Le programme affiche les positions des cibles détectées par le faux radar, puis s'arrête après 10 trames. `--rm` supprime le conteneur à la fin.

### 1.3 Régler le faux radar

Ajouter des `-e VARIABLE=valeur` avant le nom de l'image :

```bash
docker run --rm -e SIM_FRAMES=20 -e SIM_TARGETS=3 radar2460-calibration
```

| Variable | Défaut | Rôle |
|---|---|---|
| `SIM_FRAMES` | 10 | Nombre de trames avant l'arrêt (0 = sans fin, `Ctrl+C` pour arrêter) |
| `SIM_TARGETS` | 2 | Nombre de cibles par trame (1 à 3, ou 1 à 8 avec `SIM_SCENARIO=walk`) |
| `SIM_PERIOD_MS` | 300 | Délai entre deux trames, en millisecondes (0 = le plus vite possible) |
| `SIM_FAULTS` | 0 | `1` pour envoyer aussi des trames défectueuses |
| `SIM_SCENARIO` | (aucun) | `walk` : jusqu'à 8 personnes entrent, traversent le champ et repartent, au lieu de cibles qui oscillent |
| `SIM_SPEED` | 1 | Avec `walk` : vitesse des déplacements (2 = deux fois plus vite) |

### 1.4 Tester la robustesse

```bash
docker run --rm -e SIM_FAULTS=1 -e SIM_FRAMES=12 radar2460-calibration
```

Une trame sur deux est défectueuse : longueur fausse, trame vide, fin de trame abîmée, octet parasite. Le programme doit les rejeter avec un message `[RADAR] ...` et continuer normalement.

### 1.5 Voir les cibles en direct (interface graphique)

```bash
docker compose up -d --build
```

Puis ouvrir http://localhost:8080 dans le navigateur. On y voit :

- **le plan vu de dessus** : le radar en bas, son champ de vision, les cibles en couleur avec leur traînée, et la **zone de danger** en rouge (par défaut tout le côté X > 0, à droite du radar). En passant la souris sur le plan, un réticule affiche les coordonnées X/Y du point visé ;
- **le panneau de droite** : les coordonnées X/Y de chaque cible, l'état de la zone (présence, libre depuis X s, sûre après 10 s), le Y min/max imprimé par le programme et les compteurs de trames ;
- **l'historique en bas** : la distance Y de chaque cible sur les 30 dernières secondes, soulignée en rouge quand la cible était dans la zone de danger.

En haut de la page, deux curseurs : **Personnes** (1 à 8 personnes qui se déplacent) et **Vitesse** (×0,5 à ×3). Puis la case « Trames défectueuses », **Pause** pour figer l'image et **Effacer les traces**. Dans le panneau, les curseurs « Réglage de la zone de danger » (X min, X max, Y min, Y max) déplacent la zone en direct ; **Revenir à X > 0** remet la zone par défaut.

Les réglages Personnes, Vitesse et Trames défectueuses sont communs à tous ceux qui ont la page ouverte. La zone de danger, elle, est propre à chaque navigateur.

Pour arrêter :

```bash
docker compose down
```

Après une modification du programme (`.ino`) ou de l'interface (`gui/`), relancer `docker compose up -d --build` puis recharger la page.

### 1.6 Récupérer le firmware pour la carte

```bash
docker build --target firmware-export -o out .
ls out
```

Le dossier `out/` contient les fichiers `.bin`. Pour flasher la carte (branchée en USB, hors Docker) :

```bash
pip install esptool
esptool.py --chip esp32c3 write_flash 0x0 out/Radar2460Calibration.ino.merged.bin
```

---

## 2. Git

Le sujet de la démo : chaque modification passe par une branche, un commit et une pull request.

### 2.1 Se situer

```bash
git status                  # fichiers modifiés, branche actuelle
git log --oneline -10       # les 10 derniers commits
```

### 2.2 Récupérer les dernières modifications

```bash
git fetch                   # télécharge sans rien modifier
git pull                    # met à jour la branche actuelle
```

Toujours faire `git fetch` avant de comparer sa branche avec `origin/main` : sans lui, Git compare avec une copie ancienne.

### 2.3 Proposer une modification

```bash
git switch main
git pull
git switch -c feat/ma-fonctionnalite    # 1. créer une branche
# ... modifier les fichiers ...
git add -A                               # 2. préparer tous les fichiers modifiés
git commit -m "feat: ma fonctionnalité"  # 3. enregistrer
git push -u origin feat/ma-fonctionnalite   # 4. envoyer sur GitHub
gh pr create                             # 5. ouvrir une pull request
```

`gh` est l'outil en ligne de commande de GitHub (`brew install gh`, puis `gh auth login`). On peut aussi ouvrir la pull request depuis le site : GitHub propose un bouton « Compare & pull request » juste après le `push`.

Préfixes de message utilisés ici : `feat:` (nouveauté), `fix:` (correction), `perf:` (performance), `test:`, `docs:`, `ci:`, `chore:` (maintenance).

### 2.4 Revenir à un état précédent (pour le consulter)

Chaque commit a un identifiant, affiché par `git log --oneline` (par exemple `6cf44c9`) :

```bash
git switch --detach 6cf44c9    # le code tel qu'il était à ce commit
ls                             # ici, on retrouve l'ancienne application drone (src/, missions/…)
git switch main                # revenir au présent
```

Rien n'est perdu : un fichier supprimé aujourd'hui reste dans les commits plus anciens.

---

## 3. En cas de problème

| Symptôme | Cause probable | Solution |
|---|---|---|
| `command not found: docker` (ou `git`) | Outil non installé | Voir « Installer les outils » |
| `Cannot connect to the Docker daemon` | Docker Desktop n'est pas lancé | Lancer Docker Desktop, attendre la baleine fixe |
| `Unable to find image 'radar2460-calibration'` | L'image n'a pas encore été construite | Lancer d'abord `docker build -t radar2460-calibration .` dans `Radar2460Calibration/` |
| http://localhost:8080 ne répond pas | L'interface n'est pas lancée, ou le port 8080 est pris | `docker compose ps` dans `Radar2460Calibration/` ; si le port est pris, changer `"8080:8080"` en `"8081:8080"` dans `compose.yaml` et ouvrir http://localhost:8081 |
| La page affiche « déconnecté » | Le conteneur s'est arrêté | `docker compose logs gui` puis `docker compose up -d` |
| `docker build` très long | Premier téléchargement des outils ESP32 | Normal la première fois, ensuite c'est en cache |
| `failed to read dockerfile` | La commande n'est pas lancée dans le bon dossier | `cd Radar2460Calibration` puis relancer |
