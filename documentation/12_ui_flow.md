# 12 — Spécification UI du port (anglais) — flux de référence

> Références : `documentation/captures/reference/01..07*.png` (captures de l'original fournies
> par l'utilisateur, 2026-09-26). Le port est **en anglais uniquement** (les écrans originaux
> fournis sont en français ; traduire les libellés).

## 1. Menu principal (réf. 01)

Écran : logo **RESURRECTION / RISE 2** (titre original), liste verticale centrée.

Port — libellés anglais :

```
START            (original : DEMARRAGE)
KEY MAPPING      (remplace INSTALLATION — écran de configuration des touches)
HIGH SCORE
CREDITS
QUIT
```

Navigation : flèches haut/bas, Entrée = valider (identique à l'original : `↑↓ = MOVE`,
`↵ = SELECT`, `F1 = ONLINE HELP`, `Esc = PREVIEW MENU` en pied d'écran).

## 2. Écran KEY MAPPING (réf. 02 — remplace l'écran INSTALLATION original)

L'original ouvre l'« INSTALLATION » (setup complet : LANGUE / COMMANDES / SON / VIDEO /
OPTIONS DE JEU / OPTIONS RESET / AIDE / QUIT / CONTINUE). Le port remplace ce menu par
**KEY MAPPING** seul : la page de liaison des touches, directement accessible depuis le
menu principal.

Contenu à reproduire (d'après l'original, traduit) :

```
PLAYER 1  KEYBOARD                    PLAYER 2  KEYBOARD
  UP:    <touche>     01: <touche>     (P1)   UP:  <touche>   01: <touche>
  DOWN:  <touche>     02: <touche>            DOWN: ...
  LEFT:  <touche>     03: <touche>
  RIGHT: <touche>
```

- 10 entrées par joueur (4 directions + 01/02/03 = PUNCH 1-3, et P1/P2/P3 = KICK 1-3
  — cf. OPTIONS.TXT : `up, down, left, right, punch1, punch2, punch3, kick1, kick2, kick3`).
- Deux colonnes : PLAYER 1 et PLAYER 2, chacun KEYBOARD (joystick plus tard).
- Action en bas de l'écran : « CONFIGURE KEYBOARD/JOYSTICK CONTROLS » (l'original :
  « CONFIG. COMMANDES CLAV./JOYSTICK ») = mode d'affectation (toucher une touche pour la
  lier à l'entrée sélectionnée).
- Les liaisons lues/écrites dans `RISE2.CFG` (46 x u16 : scancodes + options — les
  scancodes visibles dans la capture P1 : HAUT/BAS/GAUCHE/DROITE + T/R/Y…).
- Retour : Esc / l'entrée QUIT du sous-menu -> menu principal.

## 3. Sélection des personnages (réf. 03 + 04)

- **Grille de portraits** : 20 panneaux visibles (2 rangées), vignettes des robots.
- **Cadre de sélection** : rouge pour le joueur 1, bleu pour le joueur 2 (réf. 04).
- Zone basse : le robot sélectionné s'anime en grand sur la plate-forme (nom affiché sous
  le robot, ex. WAR).
- **Robots masqués** : les robots non débloqués ne sont PAS dans la grille — en continuant à
  se déplacer **au-delà du bord gauche ou droit de la grille**, on « sort » du panel et les
  robots masqués s'affichent. À reproduire tel quel (défilement circulaire élargi).
- Valider un robot -> le cadre passe à la couleur du joueur et le robot reste affiché ;
  en 2 joueurs, les deux choix côte à côte (réf. 04 : WAR + DETAIN).

## 4. Écran VS (réf. 05)

- Fond sombre à motif, les deux robots en silhouette grand format face à face, grand **VS**
  au centre, **pastille circulaire type camembert** en haut (indicateur de chargement),
- noms des robots en bas à gauche/droite (WAR / DETAIN).
- Transite automatiquement vers le combat une fois le « chargement » terminé.

## 5. Combat in-game (réf. 06)

HUD (les deux barres sont EN HAUT) :

- **Noms** aux extrémités (WAR à gauche, DETAIN à droite), compteur de score 00000000 au
  centre de chaque barre.
- **Barres de vie vertes** symétriques (se vident vers le centre), dégradé vert->rouge.
- **Chrono** central rouge (85, 76… décompte).
- **Jauges de super** : pastilles jaunes sous chaque nom (gauche).
- Arène : fond défilant (ville), sol texturé.

## 6. Menu pause in-game (réf. 07 — touche ECHAP)

Port — libellés anglais (l'original : CONTINUER MATCH / F9 CALIBRER JOYSTICKS / F10 QUITTER MATCH) :

```
CONTINUE MATCH        (reprendre)
F9  CALIBRATE JOYSTICKS
F10 QUIT MATCH        (retour au menu principal)
```

Texte superposé en rouge sur le combat figé.

## Notes d'implémentation pour le port

- Tous les écrans = résolution native 640x400 (rendu x2).
- Les graphismes des écrans existent dans les assets : logo du titre (GGF : famille
  A4x/AGx — le panneau menu), portraits de sélection (VSFACE/V4FACE, 28+28), les arènes
  (GGF AG* 800x400). Le texte = moteur de texte (`FUN_1fa80`, police CHRSET).
- Le clavier = scancodes DOS (les valeurs de RISE2.CFG) — mapper scancodes -> SDL scancodes
  pour l'écran KEY MAPPING et le jeu.
- Les robots masqués : mécanisme de déblocage à retrouver (high score ? arcade ?) — pour
  l'instant reproduire le comportement de défilement étendu (sortir de la grille révèle
  les masqués).