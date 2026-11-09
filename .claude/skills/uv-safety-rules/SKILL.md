---
name: uv-safety-rules
description: Règles de sécurité pour la future commande de la lampe UV par le radar. À utiliser dès qu'un changement touche au relais, aux seuils de tension, à la durée de vie de la lampe ou à la décision de présence.
---

# Sécurité de la lampe UV

Les UV germicides sont dangereux pour les yeux et la peau. Le radar sert à couper la lampe dès qu'une personne est présente. Une erreur de code ici ne se voit pas à l'écran : elle expose quelqu'un.

## État actuel

`Config.h` prévoit déjà la partie sécurité, mais le croquis ne l'utilise pas encore :

- relais bistable sur `PIN_RELAY_A` (3) et `PIN_RELAY_B` (4) ;
- perte d'alimentation sous `POWER_LOSS_THRESHOLD_V` (4,5 V), retour au-dessus de `POWER_RESTORE_THRESHOLD_V` (4,75 V) ;
- durée de vie de la lampe : `LAMP_MAX_HOURS` (9000 h), `LAMP_MAX_CYCLES` (10 000 cycles) ;
- sortie d'erreur seulement après `RADAR_SAFE_DELAY_MS` (10 s) sans détection ;
- `SAFETY_NO_AUTO_RESTART` et `SAFETY_RADAR_CLEAR_REQUIRED` à 1.

## Règles

1. **Lampe éteinte par défaut.** Au démarrage, après un redémarrage et dans tout état non prévu, le relais commande l'arrêt.
2. **Absence de donnée = présence.** Pas de trame valide depuis un délai donné (radar débranché, ligne coupée, trames toutes rejetées) : on considère qu'il y a quelqu'un et on coupe. Une trame rejetée ne vaut jamais « zone vide ».
3. **Pas de rallumage automatique après une erreur** tant que `SAFETY_NO_AUTO_RESTART` vaut 1. Le rallumage demande une action humaine et `RADAR_SAFE_DELAY_MS` sans détection.
4. **Ne jamais affaiblir une valeur de sécurité** (délai plus court, seuil plus bas, drapeau à 0) sans accord écrit du responsable du produit, noté dans la PR.
5. **Relais bistable : il garde sa position sans courant.** Après une perte d'alimentation, ne pas supposer son état : le commander explicitement à l'arrêt au retour du courant.
6. **Chaque cas de sécurité a son test** dans `host/tests/` (skill `fault-tests`) : radar muet, trames corrompues en continu, perte puis retour d'alimentation, détection pendant le fonctionnement.
7. **Revue humaine obligatoire.** Une PR qui touche à ces points le dit dans son titre et n'est pas fusionnée sans relecture par une personne.

## Ce qu'il faut écrire dans la PR

- Les règles ci-dessus concernées et comment elles restent respectées.
- Les scénarios de test ajoutés et leur résultat.
- Ce qui n'a pas pu être testé sur l'ordinateur (relais réel, mesure de tension réelle) et doit l'être sur la carte.
