# Données de rendu voxel

## Objectif

Cette phase transforme le résultat du greedy meshing en données directement exploitables par le renderer Vulkan, sans introduire de dépendance entre le monde voxel et l'API graphique.

Le module `Tenebris::Scene` est la frontière entre :

- la représentation logique des voxels ;
- la géométrie CPU produite par le mesher ;
- les futurs vertex/index buffers Vulkan ;
- la caméra et les matrices de scène.

## Format de vertex

`GpuVoxelVertex` occupe exactement 20 octets :

- position : trois flottants 32 bits ;
- normale : trois entiers signés 8 bits plus un octet réservé ;
- matériau : identifiant 16 bits plus 16 bits réservés.

Le format est stable, explicitement aligné sur 4 octets et vérifié à la compilation. Les octets réservés sont initialisés à zéro afin de conserver un contenu déterministe.

## RenderMesh

Un `RenderMesh` contient :

- les vertices convertis dans l'espace monde ;
- les indices 32 bits ;
- les limites spatiales ;
- la révision du chunk source.

La conversion rejette :

- une échelle nulle, négative ou non finie ;
- une origine non finie ;
- un index buffer non triangulaire ;
- tout index hors limites.

Le renderer pourra ainsi refuser une ressource invalide avant toute allocation GPU.

## Caméra

La caméra produit des matrices colonne-majeure compatibles avec Vulkan :

- vue main droite ;
- profondeur de 0 à 1 ;
- axe Y inversé dans la projection ;
- validation du champ de vision, de l'aspect et des plans de clipping ;
- rejet des directions nulles ou parallèles au vecteur vertical.

## Blockout Site 47

`buildSite47Blockout()` génère un premier environnement déterministe contenant :

- désert et dalle de fondation ;
- enveloppe du laboratoire ;
- entrée nord ;
- couloirs et cloisons ;
- Chambre 9 ;
- éclairages d'urgence ;
- première trace organique.

Il ne s'agit pas encore du décor final. Ce blockout sert de donnée de validation pour le pipeline complet et empêche le renderer d'être développé uniquement sur un triangle ou un cube artificiel.

## Garanties automatisées

La suite de tests vérifie :

- la validité des matrices ;
- le refus des configurations de caméra invalides ;
- la conservation des vertices, indices, matériaux et normales ;
- les limites spatiales après mise à l'échelle ;
- la validité des indices ;
- la présence des cinq matériaux du blockout ;
- la reproductibilité bit à bit du mesh ;
- un hash stable des données de rendu.

## Étape suivante

La phase suivante devra consommer `RenderMesh` dans `Tenebris::Renderer` afin de créer :

- vertex et index buffers ;
- mémoire GPU ;
- pipeline graphique ;
- depth buffer ;
- push constants ou uniform buffer pour la matrice caméra ;
- rendu réel du blockout Site 47.
