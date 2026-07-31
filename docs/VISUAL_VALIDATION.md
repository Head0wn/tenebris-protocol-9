# TENEBRIS 0.7.0 — Validation visuelle de Site 47

Cette build n'est pas présentée comme une démo jouable. Elle sert à valider la première image 3D produite par le moteur Vulkan propriétaire de TENEBRIS.

## Contrôles

- Maintenir le clic droit : capturer la souris et regarder autour de soi.
- `WASD` ou flèches : déplacer la caméra.
- `Q` / `E` ou `Ctrl` / `Espace` : descendre / monter.
- `Maj` : déplacement rapide.
- Molette : avancer ou reculer rapidement.
- `O` : activer ou désactiver l'orbite cinématique automatique.
- `R` : restaurer le cadrage initial.
- `Échap` : libérer la souris ; une seconde pression ferme la build.

## Validation attendue

1. La fenêtre s'ouvre sans écran noir permanent ni message d'erreur.
2. Le blockout voxel de Site 47 est visible et correctement cadré.
3. Les surfaces proches masquent correctement les surfaces situées derrière elles.
4. Les matériaux sable, béton, métal, éclairage d'urgence et contamination sont distincts.
5. La caméra ne traverse pas encore un système de collision : ce point appartient à la phase gameplay suivante.
6. Le redimensionnement de la fenêtre ne déforme pas l'image et ne provoque pas de crash.
7. L'orbite automatique reste fluide pendant au moins cinq minutes.

## Diagnostic

Lancer `TENEBRIS.exe --gpu-smoke-test` effectue une orbite automatique de 300 images puis ferme la fenêtre. Une exécution réussie affiche le nombre d'images, de dessins indexés et de triangles soumis.

En cas d'échec, conserver le texte de la console et préciser la carte graphique ainsi que la version du pilote.
