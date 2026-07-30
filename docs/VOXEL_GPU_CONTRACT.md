# Contrat GPU voxel

## Rôle

Le renderer Vulkan ne doit jamais deviner la validité ou la taille d'un mesh. `Tenebris::Scene` prépare désormais un `GpuUploadPlan` complet avant toute allocation ou copie.

Cette frontière protège le renderer contre :

- les index buffers incomplets ;
- les indices hors limites ;
- les dépassements de compteurs 32 bits ;
- les multiplications de tailles qui débordent ;
- les régions de staging non alignées ;
- les uploads répétés d'une révision inchangée.

## Organisation du staging

Le staging initial regroupe la géométrie dans cet ordre :

1. vertices à l'offset zéro ;
2. padding jusqu'à un alignement de 16 octets ;
3. indices 32 bits ;
4. padding final jusqu'à 16 octets.

Le plan conserve la révision du `RenderMesh`. Le futur cache GPU comparera cette valeur à la dernière révision uploadée.

Une scène totalement vide est valide : elle produit zéro octet et aucun draw. Une scène qui contient uniquement des vertices ou uniquement des indices est rejetée.

## Interface vertex

Le format `GpuVoxelVertex` est verrouillé à la compilation :

| Location | Donnée | Format Vulkan prévu | Offset |
|---|---|---|---:|
| 0 | position | `VK_FORMAT_R32G32B32_SFLOAT` | 0 |
| 1 | normale | `VK_FORMAT_R8G8B8A8_SNORM` | 12 |
| 2 | matériau | `VK_FORMAT_R16_UINT` | 16 |

Stride : 20 octets.

Les offsets sont vérifiés par `static_assert`. Toute modification involontaire de l'ABI bloque la compilation.

## Push constants

`VoxelPushConstants` occupe 80 octets :

- matrice vue-projection : 64 octets ;
- direction de lumière normalisée et intensité ambiante : 16 octets.

Cette taille reste sous la garantie minimale Vulkan de 128 octets.

## Matériaux Site 47

La première palette définit :

0. air ;
1. sable ;
2. béton ;
3. acier ;
4. éclairage d'urgence ;
5. contamination organique.

Le rouge d'urgence et la matière organique ont une réponse émissive distincte. Les shaders de référence sont stockés dans `assets/shaders/voxel.vert` et `assets/shaders/voxel.frag`.

## Validation automatique

Les tests contrôlent :

- les scènes vides ;
- les meshes incomplets ;
- les indices hors limites ;
- les tailles exactes des vertex/index buffers ;
- l'absence de chevauchement ;
- l'alignement des offsets et de la taille finale ;
- le draw indexé ;
- la conservation de la révision ;
- la normalisation de la lumière ;
- le clamp de l'ambiance ;
- la palette Site 47 ;
- l'ABI des vertices et push constants.

## Prochaine intégration

Le renderer devra consommer le plan sans recalculer ses tailles, créer les buffers de staging et device-local, copier les données, puis enregistrer un `vkCmdDrawIndexed` fondé sur `IndexedDrawPlan`.
