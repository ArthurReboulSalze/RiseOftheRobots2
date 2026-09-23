# 10 — Le port : ossature, build et état (passation)

> Document de passation (mis à jour le 2026-09-23). État du dossier `PORT/` après correction de l'index des images et des séquences MVS.

## Chaîne de build

- Compilateur : **MSVC 14.44** (VS2022 Community) via le générateur CMake
  `"Visual Studio 17 2022"`. CMake VS : `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`.
- Dépendances téléchargées dans `PORT/thirdparty/` : **SDL2 2.30.11 (VC)**, **SDL2_image 2.8.4 (VC)**,
  **nlohmann/json v3.11.3** (`json.hpp`).
  Sur un nouveau checkout : `python TOOLS/bootstrap_port.py` les récupère auprès
  des projets officiels et vérifie leurs empreintes. Voir le README racine.

```powershell
# depuis la racine du projet ; CMake VS n'est pas sur le PATH de cette session
$cmakeVs = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctestVs = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $cmakeVs -S PORT -B PORT/build -G 'Visual Studio 17 2022' -A x64
& $cmakeVs --build PORT/build --config Release
& $ctestVs --test-dir PORT/build -C Release --output-on-failure
# fermer l'exe avant de recompiler, sinon LNK1104 (fichier verrouillé)
& '.\PORT\build\Release\rotr2.exe' --assets 'K:\Projects\RiseOftheRobots2\EXTRACTED'
```

⚠️ L'exe copie `SDL2.dll`/`SDL2_image.dll` à côté de lui (POST_BUILD). Les assets sont lus
depuis `--assets` (défaut `..\EXTRACTED` relatif au cwd).

## Ce qui fonctionne

- **Parcours d'accueil initial** : lecture des 39 images de `LLOGO.ANI`, puis
  titre `MAINSCR.GGF`, choix de deux robots avec les portraits `VSFACE`, puis
  combat chargé depuis les banques RBT/MVS/CL2 choisies. Flèches = joueur 1,
  A/D = joueur 2, Entrée = combat, Échap = retour ; `I` relance les logos au
  titre. Les temps de lecture et la police de menu sont provisoires. Les
  cinématiques longues ont été retirées de l’ancienne copie (`ENERGY.NFO`). Les
  105 ANI de la Director’s Cut sont désormais disponibles dans son profil privé ;
  102 restent à convertir, voir [09_formats_restants.md](09_formats_restants.md).
- **Sélection selon l’édition** : 28 portraits dans l’ancienne copie, 30 dans la
  Director’s Cut. RBT2 = SHEEPMAN, RBT3 = BUNNYRABBIT. Ces deux banques ont leurs
  MVS/CL2 et utilisent la palette propre du robot ; RB42/RB43 et AG2/AG3 sont absents.
- **Fenêtre 1280x800** (logique **640x400** = résolution native haute). Les assets RB4
  320x200 sont extraits ; le choix du mode VGA n'est pas encore câblé dans le port.
- **Deux combattants hi-res face à face** (miroir par `facing`) ; simulation à 15 Hz
  et rendu synchronisé à l'écran. Cette cadence est un réglage provisoire après
  essai visuel, pas encore une mesure du jeu DOS.
- **Rendu animé** : atlas RGBA (`EXTRACTED/sprites/RBT0/` : manifest + pages) + banque de
  mouvements MVS (`EXTRACTED/data/mvs/RBT0.json`). L'image MVS `i` utilise la frame
  **`i+1` de l'atlas** (l'entrée 0 est vide) et le record **`i` du CL2**. Le marqueur
  `end` de chaque séquence ne correspond à aucune image.
- **Transitions MVS actives** : clavier -> masques 6 bits -> table de transitions de l'état courant,
  avec la règle de comparaison de `fn_234fa` (si le masque contient des bits de marche 0x21,
  la comparaison est restreinte à ces bits). La marche avant (masque 0x01 -> m8) et arrière
  (0x20 -> m9) sont déclenchées par la direction physique selon l'orientation ; relâcher
  la touche revient à l'état neutre. Les profils de déplacement MVS de m8/m9 sont vides
  pour RBT0 : leur animation joue actuellement sur place. L'ancien « pas ±3 x2 » concernait
  les états 2/3, pas la marche.
- **Placement stable des sprites** : l'origine de chaque recadrage est rapportée à une ancre
  fixe du canevas source. Le miroir est calculé autour de la même ancre, ce qui supprime
  le décalage ajouté par les largeurs et hauteurs variables des images.
- **CL2 chargé** (`EXTRACTED/data/cl2/R0.json`, 140 frames) : boîtes d'attaque/corps par frame,
  conversion écran = x*4 (ancré au centre, `cl2_ref_x = 294`) et y*2 (y absolu du canevas,
  calé sur le sol ~312).
- **HUD** : barres de vie 120 PV (verte), jauges de super (jaune, max 24), flash rouge de hit.

## État du code (`PORT/src/`)

| Fichier | Contenu |
|---|---|
| `main.cpp` | parcours accueil/choix/combat, simulation fixe 15 Hz, rendu vsync, entrées 2 joueurs (p1 flèches+J/K, p2 A/D/W/S+I/O -> masques relatifs à l'orientation), update_facing (fn_25615 simplifiée), apply_movement (pas x2 signé par l'orientation), collisions att/corps + dégâts (fn_381a9/38b72 simplifiés), HUD |
| `frontend.h/.cpp`, `roster.h`, `ui.h/.cpp` | logos, titre, sélection de 28 ou 30 robots selon les portraits importés, police provisoire et chargement des banques |
| `assets.h/.cpp` | `Assets` : `load_atlas(bank)` (manifest + pages RGBA), `load_video(name)` (PNG ANI), `load_ggf(name)`, `load_mvs(bank)` (JSON), `load_cl2(robot_letter)` (JSON) |
| `fighter.h/.cpp` | `Fighter` : x/y/facing/move_id/seq_pos/speed_level/health/super_meter/flash/cl2 ; `step(inputs)` (transitions + avance de séquence, retour à l'attente après marche, sans afficher le marqueur `end`) ; `frame_rect()` (ancre stable du sprite) ; `get_boxes()` (boîtes écran de la frame courante) |

## Tâches en cours / à faire (pour la reprise)

1. **Calibrer le coup** : le punch (masque 0x10 -> m7) n'a pas encore été confirmé à l'écran —
   portée des boîtes d'attaque x*4-294 ≈ 98 px devant l'ancre ; à 200 px d'écart le coup rate de
   ~40 px. Deux pistes : avancer avant de frapper (test mécanique OK), ou vérifier l'échelle
   (x*4 vs x*2) avec une vue debug des boîtes (touche F1 à ajouter).
2. **États de réaction** : recevoir un coup doit faire passer la victime en hitstun (état 0x14 ?),
   puis relevé — via les transitions MVS ou le code (FUN_22fc9) ; KO (santé 0) non géré.
3. **Re-arm des coups** : actuellement un hit par frame d'attaque active (`hit_move` reset chaque
   frame) — à remplacer par le re-arm original (re-arm au changement d'état + freeze post-coup
   `DAT_6627a`).
4. **Déplacement de marche** : les séquences m8/m9 de RBT0 ont des profils de déplacement MVS
   vides ; retrouver dans le moteur DOS comment la position x avance sous entrée de marche.
   Vérifier ensuite le signe des pas pour facing=-1 (fn_21baf applique ±2 selon l'orientation).
5. **Caméra** : l'arène défile (bornes 0x65fd6/0x65fda) — caméra à ajouter quand le fond rentre.
6. **Sons** : jouer les WAV extraits (`EXTRACTED/audio/mrw/`, 8 bits 11025 Hz) sur les hits
   (SDL_QueueAudio) ; pitch des séquences MRS à intégrer ensuite.
7. **Fonds** : GGF AG (800x400, déjà en PNG dans `EXTRACTED/ggf/`) + banques ANR décor + scroll CTL.
8. **Menus/flow** : comparer la sélection provisoire au jeu DOS, brancher les modes
   et rounds/KO/timer ; poursuivre les cinématiques depuis le profil Director’s Cut
   (CD 1 seul). **IA** ensuite (35a7c/35eb8).

Les nouveaux profils se lancent avec `--assets LOCAL/<profil>/EXTRACTED`.
Le raccourci bureau historique utilise encore `EXTRACTED/` : le passage de nos
extractions de référence à la Director’s Cut est l’étape suivante demandée.
L’import configure les modes musicaux mais le port ne les lit pas encore.

## Découvertes moteur utiles pour la suite (doc 08)

- Dégâts validés : 6-10 par coup (boîtes CL2), 120 PV -> 12-20 coups par round.
- Le point d'ancrage des boîtes CL2 = le centre du combattant (référence canevas ~x=294) ;
  y*2 = absolu (le corps s'étend jusqu'au sol 312).
- Certaines frames ANR sont vides dans les données d'origine (1 199 au total) ;
  le marqueur de fin MVS, lui, ne doit pas produire de frame vide.
- La correspondance banque sprites <-> MVS <-> CL2 : `RBT0`/`RBT0`/`R0` (lettre du robot).

## Correction du 23 septembre 2026

Le rendu et les séquences ont été corrigés : `image MVS 0 → atlas frame 1`,
`image MVS 0 → CL2 record 0`, et le marqueur `end` n'est plus une frame affichée.
L'export MVS lit les sept pointeurs dans l'ordre de ses variantes, puis traite les
champs `+1C..+1F` comme quatre octets. Les 30 JSON ont été régénérés. Le log des
transitions imprime désormais l'ancien état et l'état cible. La correspondance
visuelle et la cadence exactes restent à comparer au jeu DOS après ce changement.
Build Release et test `fighter_frames` passés le 23 septembre 2026. Les vérifications
de données couvrent les 30 banques MVS, leurs 8 640 séquences et l'alignement
atlas/CL2 des 28 robots.

Après essai du port, la simulation a été ramenée de ~60 à 15 pas/s : les entrées MVS
répétées restent visibles pendant plusieurs pas, comme dans les données. Les variations
de taille du recadrage ne déplacent plus l'ancre ; le miroir utilise aussi cette ancre.
Le retour à l'état neutre après relâchement des directions est assuré pour les états
de marche désignés par la table MVS. Les flèches et A/D correspondent désormais aux
directions à l'écran même quand les joueurs changent de côté. Ces corrections ont
été compilées en Release et couvertes par `fighter_frames` ; la sensation et la
cadence exactes restent à valider visuellement face au jeu DOS.
