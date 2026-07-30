# TENEBRIS — Validation Vulkan Windows

Ce paquet est une **build technique interne du moteur**, pas la démo jouable de Site 47.

Il sert uniquement à vérifier que le renderer propriétaire TENEBRIS peut créer une instance Vulkan, sélectionner le GPU, présenter des images, reconstruire sa swapchain et libérer proprement ses ressources sur une véritable machine Windows.

## Prérequis

- Windows 10 ou Windows 11 64 bits ;
- pilote graphique NVIDIA, AMD ou Intel à jour ;
- carte graphique compatible Vulkan 1.0 au minimum ;
- PowerShell 5.1 ou supérieur.

Le Vulkan SDK n'est pas requis pour la build Release. Le chargeur Vulkan est fourni par le pilote graphique.

## Test simple

Double-cliquez sur `TENEBRIS.exe`.

Une fenêtre sombre intitulée **TENEBRIS — Le Protocole 9** doit apparaître et rester stable. Fermez-la avec la croix ou `Alt+F4`.

## Test automatisé recommandé

Dans le dossier extrait, ouvrez PowerShell puis lancez :

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\validate-vulkan.ps1 -Cycles 10
```

Chaque cycle ouvre une fenêtre, présente 300 images puis ferme proprement le moteur. Le script s'arrête immédiatement si un processus retourne une erreur.

## Résultat attendu

Le terminal doit terminer avec :

```text
Validation réussie : 10 cycles ...
```

Chaque exécution du moteur doit également indiquer le nombre d'images présentées et le nombre de reconstructions de swapchain.

## Vérifications manuelles

Pendant un lancement normal de `TENEBRIS.exe` :

1. redimensionnez rapidement la fenêtre plusieurs fois ;
2. minimisez-la puis restaurez-la ;
3. déplacez-la entre deux écrans si plusieurs moniteurs sont disponibles ;
4. fermez-la avec la croix ;
5. relancez-la et fermez-la avec `Alt+F4`.

Aucun crash, écran figé durable, fenêtre fantôme ou message d'erreur ne doit apparaître.

## En cas d'échec

Conservez :

- le texte complet affiché dans PowerShell ;
- le numéro du cycle ayant échoué ;
- le modèle exact de la carte graphique ;
- la version du pilote ;
- la version de Windows.

Ne présentez pas cette build au public : elle ne contient encore ni monde voxel, ni interface, ni gameplay.
