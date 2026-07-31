# TENEBRIS 0.9.0 — Validation de la mise en scène Site 47

Cette build n'est pas présentée comme une démo jouable. Elle doit transformer le blockout architectural 0.8.0 en une scène de staging horror plus crédible, avec une hiérarchie visuelle, des volumes techniques, des dégâts et une contamination racontant clairement l'incident.

## Ce qui change depuis la 0.8.0

- le terrain ne forme plus un socle rectangulaire continu ;
- l'approche bétonnée est fracturée et ponctuée de débris ;
- la façade possède plusieurs hauteurs, des ruptures, des renforts exposés et un poste de contrôle effondré ;
- le sas d'entrée est plus profond et comporte un seuil, un auvent et plusieurs couches de structure ;
- le couloir contient des cadres, consoles, alcôves, chemins de câbles et éclairages localisés ;
- l'aile de service contient des armoires, machines, toitures basses et conduites ;
- l'aile droite reste ouverte en coupe et reçoit une véritable tour de ventilation ;
- la Chambre 9 possède une passerelle technique, des postes d'observation, un anneau de maintenance et une cage plus imposante ;
- la contamination possède un noyau dense, une propagation au sol, des remontées murales et des foyers secondaires ;
- le shader 0.9.0 assombrit l'acier et la contamination tout en réservant l'émission forte aux seules lampes d'urgence ;
- le béton, l'acier, l'éclairage d'urgence et la matière organique sont davantage séparés par le contraste.

## Contrôles

- Maintenir le clic droit : capturer la souris et regarder autour de soi.
- `WASD` ou flèches : déplacer la caméra.
- `Q` / `E` ou `Ctrl` / `Espace` : descendre / monter.
- `Maj` : déplacement rapide.
- Molette : avancer ou reculer rapidement.
- `O` : activer ou désactiver l'orbite cinématique automatique.
- `R` : restaurer le cadrage initial.
- `Échap` : libérer la souris ; une seconde pression ferme la build.

## Captures nécessaires

1. Une vue trois-quarts avant depuis le cadrage initial.
2. Une vue intérieure depuis le sas, orientée vers la Chambre 9.
3. Une vue trois-quarts arrière ou latérale montrant la coupe, les toitures et la tour de ventilation.

## Validation attendue

1. La fenêtre s'ouvre sans écran noir permanent ni message d'erreur.
2. La première image ne ressemble ni à une boîte, ni à une maquette posée sur un plateau uniforme.
3. Le sas tactique est immédiatement identifiable et correctement dimensionné.
4. L'approche, les dégâts et le poste de contrôle donnent une échelle humaine à l'extérieur.
5. Le couloir présente plusieurs plans de profondeur et des éléments techniques reconnaissables.
6. La Chambre 9 et sa cage constituent le point focal au fond de l'axe principal.
7. La contamination possède une origine, une propagation et plusieurs ramifications lisibles.
8. Les éclairages rouges restent ponctuels et ne forment plus de grandes bandes de couleur debug.
9. La matière organique reste bordeaux très sombre et ne peut pas être confondue avec un luminaire rouge.
10. Le terrain, les ailes, les toitures et la tour produisent une silhouette irrégulière pendant l'orbite.
11. Les surfaces proches masquent correctement les surfaces situées derrière elles.
12. Le redimensionnement de la fenêtre ne déforme pas l'image et ne provoque pas de crash.
13. L'orbite automatique reste fluide pendant au moins cinq minutes.

## Limites assumées de cette passe

Cette version valide le staging, l'échelle et la séparation des matériaux. Elle ne contient pas encore les textures PBR, les lumières locales avec diffusion réelle, les ombres avancées, la brume volumétrique, la pluie, les particules, les accessoires narratifs finaux ni le contrôleur joueur avec collisions.

## Diagnostic

Lancer `TENEBRIS.exe --gpu-smoke-test` effectue une orbite automatique de 300 images puis ferme la fenêtre. Une exécution réussie affiche le nombre d'images, de dessins indexés et de triangles soumis.

En cas d'échec, conserver le texte de la console et préciser la carte graphique ainsi que la version du pilote.
