# TENEBRIS — Le Protocole 9

Jeu d’horreur narratif en voxel cinématographique développé par **VX GAMES** avec un moteur propriétaire spécialisé.

## Objectif actuel

Construire une démo commerciale complète du prologue **Site 47** : une expérience solo à la première personne, stable, finie et testée, servant de première présentation publique de TENEBRIS.

Le dépôt suit une règle simple : les prototypes techniques restent internes. Une version n’est présentée comme « démo » qu’après validation de tous les critères de qualité définis dans [`docs/QUALITY_GATE.md`](docs/QUALITY_GATE.md).

## État du projet

**Phase 0 — fondations du moteur.**

Le dépôt contient actuellement l’architecture de base, le processus de compilation, les tests automatiques et la documentation de production. Il ne contient pas encore une version jouable représentative de la démo.

## Compiler

Prérequis :

- CMake 3.28 ou supérieur ;
- compilateur compatible C++20 ;
- Windows 10/11 ou Linux pour les outils et la CI.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

L’exécutable de fondation peut être lancé en test rapide :

```bash
./build/game/tenebris --headless-smoke-test
```

Sous Visual Studio, l’exécutable se trouve généralement dans `build/game/Debug/tenebris.exe`.

## Documentation

- [`docs/PRODUCT_VISION.md`](docs/PRODUCT_VISION.md) — vision du jeu ;
- [`docs/DEMO_SCOPE.md`](docs/DEMO_SCOPE.md) — contenu verrouillé de la démo ;
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — architecture du moteur ;
- [`docs/QUALITY_GATE.md`](docs/QUALITY_GATE.md) — conditions obligatoires de publication ;
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — ordre de production.

## Statut juridique

Code source et contenus propriétaires. Tous droits réservés à VX GAMES et aux ayants droit de TENEBRIS.