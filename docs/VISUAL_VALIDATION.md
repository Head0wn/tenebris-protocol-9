# TENEBRIS 0.8.0 — Validation de la composition Site 47

Cette build n'est pas présentée comme une démo jouable. Elle remplace le premier cube technique par une composition architecturale volontaire, destinée à vérifier que Site 47 est immédiatement identifiable avant le travail avancé sur les matériaux, la brume, la pluie et l'éclairage cinématique.

## Ce qui doit être visible

- une implantation irrégulière dans le désert, et non un bâtiment cubique fermé ;
- une façade endommagée avec une entrée tactique centrale clairement dimensionnée ;
- un couloir intérieur lisible depuis l'extérieur ;
- une aile droite ouverte en coupe pour révéler la profondeur de la scène ;
- la Chambre 9 au fond, avec sa cage de confinement ;
- une traînée organique reliant la cage à l'entrée ;
- plusieurs masses architecturales : aile de service, auvents, poutres, contreforts et tour de ventilation ;
- des barres d'éclairage d'urgence suffisamment grandes pour être lisibles.

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
2. Le cadrage initial montre une vue trois-quarts, plus basse et plus proche de l'entrée.
3. La silhouette du laboratoire est fragmentée et les hauteurs sont variées.
4. L'entrée ne paraît plus minuscule par rapport à la façade.
5. Le couloir, la Chambre 9 et la contamination restent visibles pendant l'orbite.
6. Les surfaces proches masquent correctement les surfaces situées derrière elles.
7. Les matériaux sable, béton, métal, éclairage d'urgence et contamination sont distincts.
8. Le redimensionnement de la fenêtre ne déforme pas l'image et ne provoque pas de crash.
9. L'orbite automatique reste fluide pendant au moins cinq minutes.

## Limites assumées de cette passe

Cette version vérifie la composition, l'échelle et la lecture des volumes. Elle ne contient pas encore les textures PBR, les lumières locales rouges et cyan, les ombres avancées, la brume volumétrique, la pluie, les particules, les accessoires détaillés ni le contrôleur joueur avec collisions.

## Diagnostic

Lancer `TENEBRIS.exe --gpu-smoke-test` effectue une orbite automatique de 300 images puis ferme la fenêtre. Une exécution réussie affiche le nombre d'images, de dessins indexés et de triangles soumis.

En cas d'échec, conserver le texte de la console et préciser la carte graphique ainsi que la version du pilote.
