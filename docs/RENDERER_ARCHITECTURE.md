# Architecture du renderer Vulkan

## Rôle

`Tenebris::Renderer` est la couche de rendu propriétaire de TENEBRIS. SDL fournit uniquement la fenêtre, les événements et le chargement du Vulkan Loader. Le moteur crée et possède directement l'instance Vulkan, la surface, le périphérique, la swapchain et toutes les ressources GPU.

Cette phase ne contient aucun code de gameplay, aucun shader de production et aucun contenu du Site 47.

## Chargement des fonctions

Le renderer est compilé avec `VK_NO_PROTOTYPES`. Il ne lie pas directement l'exécutable à une bibliothèque Vulkan :

1. SDL charge le Vulkan Loader avant la création de la fenêtre ;
2. le renderer récupère `vkGetInstanceProcAddr` via SDL ;
3. les fonctions globales sont chargées ;
4. après création de l'instance, les fonctions d'instance sont chargées ;
5. après création du périphérique, les fonctions de périphérique sont chargées.

Une fonction obligatoire absente interrompt immédiatement l'initialisation avec un message exploitable.

## Propriété et ordre de destruction

Le propriétaire unique des ressources est `RendererSystem::Impl`.

Ordre de création :

1. instance ;
2. debug messenger optionnel ;
3. surface SDL ;
4. sélection du GPU et des files ;
5. périphérique logique ;
6. command pool et command buffers ;
7. swapchain, image views, render pass et framebuffers ;
8. sémaphores et fences.

Ordre de destruction :

1. attente du périphérique ;
2. synchronisation ;
3. command pool ;
4. ressources dépendantes de la swapchain ;
5. périphérique ;
6. surface ;
7. debug messenger ;
8. instance.

La surface est toujours détruite avant la fenêtre SDL. Le jeu appelle donc `renderer.shutdown()` avant `platform.shutdown()`.

## Swapchain

La swapchain utilise :

- `VK_FORMAT_B8G8R8A8_SRGB` quand il est disponible ;
- le color space sRGB non linéaire ;
- Mailbox pour la présentation quand il est disponible, sinon FIFO ;
- deux images en vol par défaut ;
- un render pass sans pipeline qui efface l'image avec la couleur de fond TENEBRIS.

Cette première sortie GPU volontairement minimale valide toute la chaîne réelle de présentation sans masquer les erreurs derrière une bibliothèque de rendu externe.

## Redimensionnement et minimisation

Les retours `VK_ERROR_OUT_OF_DATE_KHR` et `VK_SUBOPTIMAL_KHR` déclenchent une reconstruction complète des ressources dépendantes de la swapchain.

Une fenêtre de taille nulle place le renderer en état `Suspended`. Aucune soumission GPU n'est réalisée tant qu'une taille de framebuffer valide n'est pas retrouvée.

## Synchronisation

Chaque frame en vol possède :

- un sémaphore d'acquisition ;
- un sémaphore de présentation ;
- une fence CPU.

Une table `imagesInFlight` empêche la réutilisation d'une image de swapchain encore associée à une frame précédente.

## Validation

Les builds Debug demandent `VK_LAYER_KHRONOS_validation`. La couche n'est activée que lorsqu'elle est réellement installée sur la machine. Le renderer reste utilisable sur un poste joueur ne possédant pas le SDK Vulkan.

Les erreurs et avertissements de validation sont envoyés dans les logs techniques, jamais dans l'interface joueur.

## Tests actuels

La CI valide :

- la compilation Debug et Release sous Windows et Linux ;
- le smoke test du runtime ;
- 500 cycles d'initialisation, frame headless et destruction du renderer.

La validation GPU réelle, les redimensionnements répétés et les messages des validation layers nécessitent une machine Windows équipée d'un pilote Vulkan et sont traités comme un gate distinct avant la fermeture complète de la phase.
