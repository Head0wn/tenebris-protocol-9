# Quality Gate — « bon du premier coup »

Une build ne peut porter le nom de **Démo Site 47** que si tous les critères de ce document sont validés. Avant cela, elle reste une build interne de développement.

## Stabilité

- zéro crash reproductible connu ;
- zéro bug bloquant connu ;
- zéro objectif impossible à terminer ;
- zéro sauvegarde corrompue connue ;
- récupération correcte après fermeture brutale ;
- trois parties complètes consécutives sur la Release Candidate sans incident bloquant ;
- session prolongée d’au moins deux heures sans fuite mémoire critique.

## Fonctionnalité

- tous les boutons visibles fonctionnent ;
- tous les checkpoints ont été testés séparément ;
- Nouvelle partie, Continuer, Charger, Options, Crédits et Quitter fonctionnent ;
- pause, reprise, Alt+Tab et changement de périphérique fonctionnent ;
- clavier AZERTY/QWERTY et manette sont testés ;
- remappage, sensibilité, inversion verticale et champ de vision sont persistants ;
- les sous-titres restent synchronisés et identifient clairement le locuteur ;
- aucune interaction principale ne dépend d’un placement au pixel près.

## Présentation

- aucun placeholder visible ;
- aucun cube gris de blocage restant ;
- aucune texture, animation ou piste sonore temporaire ;
- aucun texte de développement, identifiant interne ou message serveur dans l’interface joueur ;
- aucune faute majeure dans les textes français ;
- aucun élément copiant directement l’interface d’un autre jeu ;
- crédits complets et exacts.

## Niveau et narration

- aucun passage permettant de sortir involontairement du niveau ;
- aucune collision bloquante connue ;
- aucun déclencheur narratif pouvant être sauté par un chemin normal ;
- dialogues naturels, lisibles et correctement attribués ;
- rythme testé auprès de joueurs n’ayant pas lu le roman ;
- objectif courant toujours compréhensible sans surcharger l’écran.

## Audio

- aucune coupure audible aux boucles ;
- mixage vérifié au casque et sur haut-parleurs ;
- volumes séparés : général, musique, effets, dialogues et ambiance ;
- plage dynamique compatible avec un mode nuit ;
- indices sonores critiques audibles sans être agressifs ;
- aucun effet joué plusieurs fois à cause d’un déclencheur doublé.

## Performance

La configuration cible exacte sera verrouillée avant l’optimisation finale. La Release Candidate devra au minimum respecter :

- 60 FPS stables en 1080p sur la configuration milieu de gamme cible ;
- aucun stutter majeur lors du streaming normal ;
- temps de chargement mesurés et documentés ;
- consommation mémoire mesurée sur chaque secteur ;
- options graphiques réellement fonctionnelles ;
- compilation des shaders gérée avant le début de la partie ou sans saccade perceptible.

## Distribution

- build Windows signée ou clairement identifiée comme build privée de test ;
- installation sur une machine Windows propre ;
- lancement sans outils de développement installés ;
- désinstallation propre ;
- sauvegardes stockées hors du dossier d’installation ;
- logs et rapports de crash séparés des données joueur ;
- archive ou installateur vérifié par somme de contrôle.

## Validation finale

La publication exige :

1. tous les tests automatiques au vert ;
2. une checklist manuelle signée ;
3. un rapport de performance ;
4. une liste vide de bugs bloquants et critiques connus ;
5. validation du parcours complet du début au générique.

Un défaut connu n’est pas masqué par une note de version. Il est corrigé ou la publication est reportée.
