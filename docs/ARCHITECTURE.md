# Architecture du moteur TENEBRIS

## Principe

Le moteur est propriétaire et spécialisé pour TENEBRIS. Il ne cherche pas à devenir un moteur généraliste. Chaque module doit répondre à un besoin réel de la démo Site 47 ou du jeu complet.

L’utilisation de bibliothèques bas niveau éprouvées pour la fenêtre, l’audio, la physique ou le décodage de fichiers ne remet pas en cause cette propriété. Le moteur conserve la responsabilité de l’architecture, du rendu, des voxels, du streaming, de l’éditeur, du gameplay, de la narration et des sauvegardes.

## Pile technique cible

- C++20 ;
- CMake ;
- Vulkan pour le rendu ;
- SDL3 pour la fenêtre, les entrées et les périphériques ;
- Jolt Physics pour les collisions et la physique ;
- miniaudio pour la sortie et le mixage audio ;
- Lua pour les scripts de mission et de narration ;
- Dear ImGui réservé aux outils internes ;
- Windows 10/11 comme plateforme joueur initiale.

Les dépendances externes seront intégrées une à une, épinglées à une version précise et accompagnées d’un test minimal avant leur utilisation dans le jeu.

## Modules prévus

```text
engine/
  core/            cycle de vie, temps, événements, tâches, fichiers, logs
  platform/        fenêtre, entrées, périphériques, système
  renderer/        Vulkan, matériaux, éclairage, ombres, post-traitement
  voxel/           chunks, maillage, LOD, streaming, contamination
  physics/         collisions, requêtes, personnages, objets physiques
  audio/           bus, spatialisation, musique, dialogues, ambiance
  animation/       squelettes, états, blending, événements d’animation
  world/           scènes, entités, composants, sérialisation
  narrative/       dialogues, scripts, objectifs, cinématiques
  gameplay/        joueur, armes, inventaire, preuves, exposition
  ai/              perception, navigation, comportements, escouade
  save/            profils, checkpoints, migration, récupération
tools/
  editor/          niveau, éclairage, voxels, triggers, prévisualisation
game/
  site47/          contenu et logique propres à la démo
```

## Monde voxel hybride

Les environnements utilisent une grille voxel pour la structure, la destruction locale et la contamination. Les personnages, armes et objets complexes utilisent des meshes voxel riggés afin de préserver des animations fluides et des silhouettes crédibles.

Principes techniques :

- chunks chargés de façon asynchrone ;
- maillage fusionné pour éviter une face par bloc ;
- niveaux de détail pour les longues distances ;
- occlusion et frustum culling ;
- destruction limitée aux surfaces prévues par le level design ;
- sauvegarde des seules modifications ;
- données de collision générées séparément du rendu ;
- propagation de contamination pilotée par des champs et des règles, pas par des millions d’entités.

## Règles d’architecture

- aucune dépendance du moteur vers le contenu Site 47 ;
- aucune allocation importante dans la boucle critique sans justification ;
- systèmes déterministes lorsque nécessaire aux tests et sauvegardes ;
- erreurs récupérables remontées explicitement ;
- logs structurés sans texte technique dans l’interface joueur ;
- formats de données versionnés ;
- chaque module possède au moins un test de démarrage et d’arrêt ;
- les dépendances circulaires sont interdites.

## Ordre d’intégration

1. fondation, tests et CI ;
2. plateforme et fenêtre ;
3. rendu Vulkan minimal ;
4. monde voxel et caméra FPS ;
5. physique et interactions ;
6. audio ;
7. outils de niveau ;
8. narration et sauvegardes ;
9. gameplay Site 47 ;
10. finition et validation.
