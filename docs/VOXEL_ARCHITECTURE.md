# Architecture du monde voxel

## Objectif de la phase

Cette phase établit le format CPU du monde voxel TENEBRIS. Elle ne réalise encore ni upload GPU, ni texture, ni collision, ni streaming.

Le système est volontairement indépendant de Vulkan afin de pouvoir être testé intégralement en mode headless et réutilisé par l'éditeur, le serveur d'outils et les tests de validation.

## Chunk

Un `VoxelChunk` contient exactement `32 × 32 × 32`, soit 32 768 cellules.

Les coordonnées locales valides vont de `0` à `31` sur chaque axe.

La disposition mémoire est :

```text
index = x + 32 × (y + 32 × z)
```

`x` est donc l'axe contigu en mémoire.

Chaque cellule contient un `MaterialId` non signé sur 16 bits :

- `0` : air ;
- `1..65535` : matériau solide défini par la future bibliothèque de matériaux.

Le chunk maintient :

- un compteur de cellules solides en temps constant ;
- une révision 64 bits incrémentée uniquement lorsqu'une mutation change réellement les données.

Les écritures hors limites sont refusées. Les échantillonnages hors limites sont traités comme de l'air, ce qui permet au mesher de produire les frontières externes sans accès mémoire invalide.

## Format de mesh CPU

Le mesh produit contient deux tableaux :

- `VoxelVertex` ;
- indices 32 bits.

Un vertex stocke :

- une position locale entière entre `0` et `32` ;
- une normale signée par axe (`-1`, `0`, `1`) ;
- le matériau de la face.

Les coordonnées sur la limite `32` sont nécessaires pour représenter la face positive d'un chunk.

Chaque quad utilise quatre vertices et six indices. Les triangles suivent un winding cohérent avec la normale encodée.

## Greedy meshing

Le mesher balaie successivement les axes X, Y et Z.

Pour chaque plan séparant deux couches :

1. il compare les cellules des deux côtés ;
2. il crée une entrée de masque uniquement lorsqu'un côté est solide et l'autre vide ;
3. il fusionne les rectangles contigus ayant le même matériau et la même direction de normale ;
4. il émet un quad par rectangle.

Deux cellules solides adjacentes ne créent jamais de face interne, même si leurs matériaux diffèrent. En revanche, leurs faces externes coplanaires ne sont fusionnées que si le matériau est identique.

Le masque de travail est un tableau fixe de `32 × 32` cellules alloué une seule fois par génération. Aucun objet temporaire n'est alloué pour chaque voxel parcouru.

## Conventions d'axes

Le moteur utilise un repère droit :

- X : droite ;
- Y : hauteur ;
- Z : profondeur.

Pour un axe principal `d`, les deux axes du plan sont choisis cycliquement :

```text
u = (d + 1) modulo 3
v = (d + 2) modulo 3
```

Ainsi, le produit vectoriel `u × v` pointe toujours vers la direction positive de `d`. Les faces négatives inversent l'ordre de leurs vertices.

## Invariants testés

La suite automatisée valide notamment :

- les accès bornés ;
- le compteur de solides ;
- les révisions ;
- un chunk vide ;
- un bloc isolé ;
- deux blocs identiques contigus ;
- deux matériaux différents contigus ;
- un chunk totalement solide ;
- une cavité interne ;
- le winding de chaque triangle ;
- la déterminisme des vertices et indices.
