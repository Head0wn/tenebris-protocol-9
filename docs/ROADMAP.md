# Roadmap de production

La roadmap utilise des portes de validation. Une phase n’est pas considérée terminée parce que le code existe, mais parce que ses tests, sa documentation et ses critères de sortie sont validés.

## Phase 0 — Fondation

Livrables :

- structure CMake ;
- cycle de vie de l’application ;
- tests automatiques ;
- CI Windows et Linux ;
- conventions de code ;
- vision, périmètre et Quality Gate.

Sortie : compilation propre en Debug et Release sur les deux plateformes de CI.

## Phase 1 — Plateforme Windows

Livrables :

- fenêtre SDL3 ;
- boucle d’événements ;
- clavier, souris et manette ;
- gestion du plein écran et du redimensionnement ;
- logs structurés ;
- gestion propre des erreurs de démarrage.

Sortie : application graphique stable pendant deux heures, ouverture et fermeture répétées sans fuite critique.

## Phase 2 — Rendu Vulkan

Livrables :

- instance, périphérique et swapchain ;
- synchronisation ;
- pipeline de base ;
- gestion du redimensionnement ;
- ressources GPU versionnées ;
- capture d’erreurs de validation en build développeur.

Sortie : scène de test rendue sans erreur de validation connue sur la configuration cible.

## Phase 3 — Monde voxel

Livrables :

- chunks ;
- palette de matériaux ;
- maillage fusionné ;
- streaming asynchrone ;
- caméra FPS ;
- éclairage initial ;
- sauvegarde des modifications locales.

Sortie : parcours test complet sans trou visible, collision incorrecte ni stutter majeur.

## Phase 4 — Vertical slice technique

Livrables :

- une section extérieure du Site 47 ;
- sas et premier couloir ;
- lampe, arme et terminal ;
- pluie de poussière, brouillard et lumière d’urgence ;
- premier personnage voxel riggé ;
- premier dialogue radio.

Sortie : identité visuelle validée avant la production massive des assets.

## Phase 5 — Outils de production

Livrables :

- éditeur de niveau ;
- placement de voxels et props ;
- éclairage ;
- triggers ;
- navigation IA ;
- prévisualisation narrative ;
- validation automatique des niveaux.

Sortie : création d’une salle jouable sans modifier le code du moteur.

## Phase 6 — Gameplay complet de la démo

Livrables :

- armes ;
- inventaire ;
- preuves ;
- exposition ;
- escouade ;
- trois menaces ;
- objectifs ;
- checkpoints ;
- sauvegardes ;
- options et accessibilité.

Sortie : démo jouable du début à la fin avec assets intermédiaires uniquement en build interne.

## Phase 7 — Production finale Site 47

Livrables :

- environnements définitifs ;
- personnages et animations définitifs ;
- cinématiques ;
- audio final ;
- textes finalisés ;
- interface définitive ;
- générique.

Sortie : contenu verrouillé, aucun placeholder visible.

## Phase 8 — Alpha interne

Objectifs : corriger les ruptures de progression, les sauvegardes, les collisions, la compréhension et les problèmes de performance majeurs.

Sortie : zéro bug bloquant connu.

## Phase 9 — Bêta privée

Objectifs : tester le rythme, l’horreur, l’ergonomie, les périphériques, les configurations matérielles et l’accessibilité.

Sortie : zéro bug critique connu et validation du Quality Gate hors distribution finale.

## Phase 10 — Release Candidate

Objectifs : installation propre, performance finale, trois parcours complets consécutifs, contrôle des crédits, sommes de contrôle et rapport de validation.

Sortie : publication uniquement après validation intégrale de `QUALITY_GATE.md`.
