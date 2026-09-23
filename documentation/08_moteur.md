# 08 — Carte du moteur (RISE2.EXR, Phase 2 en cours)

> Analyse descending depuis le blitter (`1c819`) et le cluster de combat.
> Adresses = adresses linéaires Ghidra du programme `RISE2_EXR` (image plate à 0x10000).
> Graphe complet : `ANALYSIS/exr_callgraph.txt` ; liste des fonctions : `ANALYSIS/exr_functions.txt`.
> Les adresses ci-dessous concernent l’ancienne copie DOS, pas l’EXR de la Director’s Cut.

## Choix de l’audio de fond

Dans `FUN_150b4`, `DAT_6639c` choisit 0 = ambiance, 1 = musique numérique,
2 = musique CD. Le nom initial `BGA.MRW` se trouve à l’adresse linéaire 0x50863.
Le chemin ambiance conserve le préfixe `B` ; le chemin numérique le remplace
par `M` et choisit la lettre `A + (arène % 6)` : **MGA à MGF**.
`FUN_3c17c` puis `FUN_3be83` chargent la séquence MRS et la banque MRW associée.
Le manuel `README.FRA` et les entrées `DIGITAL MUSIC` / `CD STREAMED MUSIC`
de `OPTIONS.TXT` corroborent les deux sources musicales. Ce ne sont pas des MID.
L’importeur conserve ces sources et le choix audio ; leur lecture dans le port
reste à intégrer (document 11).

## La boucle de combat identifiée

| Fonction | Taille | Rôle |
|---|---|---|
| `FUN_137ce` | 1472 o | **Gestionnaire de round** : init (FUN_13d8e = intro), appelle la frame de combat `2163a` + rendu `165fb`, détection KO (`DAT_663A? == 0`), bonus +1000 pts si l'adversaire a encore ses **120 PV** (victoire sans dégât), mode arcade (attente touche via `FUN_3bbc8`), initialisation des deux joueurs (`FUN_20fb9` ×2) + physique ×6 |
| `FUN_2163a` | 1397 o | **Cœur : mise à jour d'une frame de combat** — boucle sur les 2 joueurs, lecture des entrées (mode `DAT_66366`), machine à états (`FUN_234fa`), physique (`FUN_21baf`), caméra (séparation des combattants, recul quand |Δx| ≥ 100), HUD |
| `FUN_25615` | 3340 o | Étape de frame (appelée en tête de 2163a) |
| `FUN_234fa` | 2354 o | **Machine à états des combattants** : correspondance entrées → ID de mouvement, enchaînements, transitions, gestion du stun |
| `FUN_2298b` | 140 o | Lissage des entrées (valeur actuelle → cible, max 4 pas/frame) |
| `FUN_21baf` | 2845 o | **Physique/collisions** (appelée 6× au démarrage de round, 2× par frame) |
| `FUN_35a7c` | 1084 o | **IA : réactions + contrôleur CPU** (appelle `FUN_35eb8`/`3785c`/`37db3`) |
| `FUN_35eb8` | 3111 o | Décision IA (la plus grosse du cluster IA) |
| `FUN_12cc6` | 1481 o | Chargement d'un match (initialise l'arène, appelle le loader robot `FUN_14c04`) |
| `FUN_15403` | 2082 o | Gestionnaire d'écran (sélection ?, masque de disponibilité par tableau 0x62998) |
| `FUN_1fa80` | 4402 o | **Moteur de texte** (rendu caractère par caractère, table à 0x6250F, déclenché par chaîne de caractères) — pas la logique de jeu |
| `FUN_20fb9` | 1665 o | Initialisation d'un joueur |
| `FUN_165fb` | 871 o | Rendu de la frame de combat (appelle la chaîne de dessin → `FUN_1c819`) |
| `FUN_1aa37` | 93 o | Wrapper blitter (prépare les paramètres puis `FUN_1c819`) |
| `FUN_381a9` | 1717 o | Étape post-physique (hit detection ? à confirmer) |
| `FUN_24a59` / `FUN_24e0d` | — | Effets/KO par joueur (appelés si non-KO) |
| `FUN_253a8` ×2 | ? | HUD par joueur |
| `FUN_22848` ×2 | ? | ? par joueur (barres de vie ?) |
| `FUN_3a0d2` ×4 | ? | Effets (4 occurrences par frame) |
| `FUN_244eb` ×3/joueur | ? | Projectiles ? (3 par joueur = slots ?) |
| `FUN_1b22a` | ? | Son KO (appelé à la détection de KO) |
| `FUN_13d8e` | ? | Intro de round |

## Structure joueur (stride 0x97 = 151 octets ; p0 à `0x6620e`, p1 à `0x662A5`)

| Offset | Champ | Notes |
|---|---|---|
| +0x00 | état courant (u16) | 0x4B = état spécial (temporisé), < 10 = états de base, 0x2A, 0x14/0x40 = spécifiques IA |
| +0x04 | x (u16) | positions absolues dans l'arène |
| +0x06 | y (u16) | comparé à +0x46 pour « au sol » |
| +0x08 | frame dans l'état (u16) | |
| +0x16 | flags (byte) | bit 1 = KO ; bits 0x20/0x40 état |
| +0x2A | entrées (u16) | bits de direction/boutons ; masque 0x21 pendant certains états |
| +0x2E | valeur lissée | (analogique, 4 pas max) |
| +0x46 | au sol y (u16) | `DAT_66254` |
| +0x4E | pointeur records AIP (par état, 14 o) | octet+6 : bit 0x20 = « non interruptible » |
| +0x6C | verrou (u16) | empêche la transition |
| +0x6C+2 | compteur freeze | seuil 0x167F |
| +0x6E | compteur de stun | init 100, décrément 13 frames |
| +0x79 | indicateur 0/1 (KO ?) | vérifié == 1 dans le flux d'entrées |
| +0x83..97 | variables IA | réactions (états 0x0D/0x0E/0x19) |

## Données de combat trouvées dans la frame de combat

- `DAT_000685cc/685d4` : tables de pointeurs par joueur (résolution `ptr[player] + state*4 - 0xC` = pointeurs auto-référentiels relogés par `FUN_23e97` (MVS/STS)).
- `DAT_00068524 + player*4 + move_id*0x14` : **table des mouvements AIP (N2 extra)**, indexée par ID de coup.
- `DAT_000685dc + player*0x10` : **scripts d'entrées encodés en octets** (échappements -1/-2, groupes terminés par -1, u16 < 0x80 = transitions) — les enchaînements (combos).
- `DAT_70314 + player*0x3C` : 3 slots de **projectiles** par joueur (0x14 o chacun, `>>16 == -10000` = libre).
- `DAT_65FE0` : état du clavier (bits joueurs 1 : bits 0–5, joueur 2 : bits 6–11).
- `DAT_65fd6/65fda` : bornes de l'arène (recadrage des positions).
- 0x66212 - 0x662a7 : distance x entre combattants (< 0x3D = 61 px = à portée → les coups de près ajoutent +0x40 à l'ID de coup).
- Points de score : `DAT_66230 + 0x97*p` (+1000 en victoire parfaite).

## Ce qui reste à cartographier

- `FUN_35eb8` (3111 o) : la prise de décision IA (l'essentiel de la difficulté du jeu).
- `FUN_21baf` (2845 o) : la physique/collision (lire en détail, relier aux CL2).
- `FUN_25615` (3340 o) : l'étape de frame (graphe d'états).
- `FUN_381a9` : hit detection ? `FUN_244eb` (projectiles ?), `FUN_253a8/22848` (HUD).
- Les entrées/sorties des états : le tableau de correspondance état → animation vient des AIP (u16[1] = index d'animation, confirmé par la lecture `state*0xE + ...`).
- Séquences MRS (déclenchement et pitch des échantillons MRW ; voir doc 09) et le timer de round (0x96 = 150 ?).

## Méthode reproductible

```bash
python TOOLS/exr_callgraph.py          # graphe d'appels (529 fonctions)
python TOOLS/exr_decompile_at.py <hex> # décompilation ciblée -> ANALYSIS/funcs/fn_<addr>.c
python TOOLS/exr_disasm_fn.py <hex>    # assembleur ciblé -> ANALYSIS/funcs/asm_fn_<addr>.txt
```

Graphe inverse : `ANALYSIS/exr_callgraph.txt` (format `callee -> callers`).
## Effets et périphériques du combat (session 8, suite)

| Fonction | Rôle |
|---|---|
| `FUN_253a8` ×2 | **Recul/mur** : déplace x de `DAT_6624e×2-1` selon l'orientation ; aux bornes d'arène (0x65fd6/0x65fda) transfère l'élan à l'ADVERSAIRE (mécanique de mur/poussée) ; si en l'air et état < 0x50, plancher de recul 10 |
| `FUN_244eb` ×3/joueur | **Projectiles** : slots `0x70314 + p*0x3C + s*0x14` ; -10000 = libre, -999 = suppression ; script : pointeur (`0x70320`) avancé de 6 o/frame ; vx à +0 (signé par flag bit 0), vy à +2 ; flag bit 1 = table de collision alternative (+0x87a0) ; bornes ±0x17C (380 px) ; sous 0x50 frames de vie si flag bit 0 = 0 |
| `FUN_22848` ×2 | **Effets/particles par joueur** : 5 slots de 12 o (`0x6852C + p*0x3C`) : [type u16][compteur][index][..][valeur +A] ; deux tables de 8 pointeurs (0x62702/0x62722) selon type < 0x10 ou >= 0x10 ; avance l'index dans la table pointée |
| `FUN_3a0d2` ×4 | **Défilement du décor** : 4 canaux de 14 o (`0x65c42`) : pointeur `0x65c4e` avancé de 6 o/frame ; dx à +2 -> `0x65c46`, dy à +4 -> `0x65c48` ; -1 = canal inactif ; nourri par les 4 sous-tables des CTL |
| `FUN_39fde/22fc9/3c595/3c28c` | sons/réactions de hit/KO |

## Graphe d'états + chaîne de dégâts (session 8)

### `FUN_25615` = mise à jour d'orientation (uniquement)

Si x0 < x1 : joueur 0 face droite (flag bit0 = 0), joueur 1 face gauche (bit0 = 1) ; inversement si
x0 > x1. Le retournement n'est appliqué que dans les états listés (0, 2, 3, 6, 0x10, 0x12, 0x16,
0x1A, 0x20, 0x21, 0x22/0x23, 0x3C, 0x3D, 0x3E, 0x4A, 0x4B, 0x4C, 0x4D, 0x4F) ; paires de
retournement : 0x22<->0x23, 0x4C<->0x4D, 0x1A<->0x21 (avec FUN_26321 = redémarrage d'anim) ;
transitions de retournement : 0x10 -> 0x45, autres -> 0x44.

### Table des états connus (ID de `struct+0x00`)

| ID | Rôle |
|---|---|
| 0x00 | debout (idle) |
| 0x02 / 0x03 | accroupi (bas / relevé ?) |
| 0x06 | ? (mouvement) |
| 0x0D / 0x0E | étourdissement / à terre (réactions IA) |
| 0x10 | relevé (knockdown recovery) |
| 0x12 | saut (?) |
| 0x14 | coup reçu (hitstun) |
| 0x16 | marche ? |
| 0x19 | assommé (dizzy) |
| 0x1A / 0x21 | marche avant/arrière (paire de retournement) |
| 0x20 | garde accroupie ? |
| 0x22 / 0x23 | marche vers/loin (paire de retournement) |
| 0x2A | en l'air / chute |
| 0x38 / 0x34 | réactions spéciales (exclues du recul) |
| 0x3C / 0x3D / 0x3E | mouvements |
| 0x40 / 0x41 / 0x42 | coups spéciaux (bloquent le lissage d'entrée) |
| 0x44 / 0x45 | retournement |
| 0x4A / 0x4C / 0x4D / 0x4F | coups multi-phases (paires 0x4C/0x4D) |
| 0x4B | état spécial temporisé (charge, durée de stun 13) |
| 0x54 | coup final ? (0x78 de dégât si round 15) |
| 0x58 | **coup SUPER** (jauge pleine 0x18 = 24) + sans collision |

Règles : `(état & 0xF) < 10` = interruptible ; `> 0x4F` = états spéciaux (défense 0x50) ;
octet +6 du record AIP bit 0x20 = non-interruptible.

### `FUN_38b72` = application du hit (la chaîne complète)

1. **Jauge de SUPER** (`struct+0x70` = 0x66272) : +2 par coup reçu (x2 si l'attaquant n'est pas
   étourdi), plafond 0x18 = 24 -> débloque l'état 0x58 (le super).
2. **Score** : +500 à `DAT_66230 + victime*0x97` par coup encaissé (non-KO).
3. **Combos** : même état + même octet 66235 en moins de 0x25 (37) frames -> compteur
   `DAT_66293` ++ (combo affiché).
4. **Dégâts** : base = octet 4 de la boîte d'attaque (signé : négatif = coup spécial/absorption) ;
   `_DAT_70594 = base × multiplicateur(_DAT_705aa, défaut 1)` ; `final = (_70594 × défense) >> 13`
   où défense = `DAT_66237[victime]&3` (0..2) ou 0x50 (états > 0x4F) / 0x32 (config bit 6) ;
   **÷8 si l'attaquant est étourdi (états 4/0x14)** ; plancher 1.
5. **Santé** : `struct+0x2C` (mot à 0x6623A) -= dégâts (mode `DAT_62b46==1`) ; <= 0 -> KO
   (`FUN_3c28c`) sinon `FUN_3c595` (réaction).
6. **Hitstun** : freeze victime `DAT_6627a` += `_DAT_7059c` ; recul/anti-repet :
   `DAT_6624e + attaquant*0x97` = max(défaut/10+4, table `PTR_62b00[anim]`) ; 5 si en l'air.
7. **Réactions** : flag victime `66225 |= 4` ; sons `FUN_39fde/22fc9/3c595/3c28c` ;
   état de son : `DAT_62b48[type_box]` (0x1B si attaquant libre, 0x2B si en l'air, 0x14 si étourdi).

## IA (session 6)

### Structure de décision — `FUN_35a7c` (réactions) + `FUN_35eb8` (décision, 3111 o)

**Blocs IA par joueur** :
- `DAT_70518 + p*0x28` : bloc IA par joueur (file d'actions : index courant à +2, plan de coups) ;
- `DAT_704BA + p*0x28` : entrées IA courantes (écrites dans `DAT_66234` = l'input du joueur simulé) ;
- `DAT_66243` : timer d'action IA ; `DAT_66240` : entrée ponctuelle.

**Config par joueur** — `FUN_22957` renvoie un pointeur par joueur : byte[0] = type,
byte[1] = difficulté (0–3 ; l'IA utilise des roulettes différentes si > 1, ×0x1E = 30 pour les niveaux forçés).

**Décision `FUN_35eb8`** (simplifiée) :
1. Jauge `DAT_66268 + p*0x97` : +4/iteration max 0x8C (140) pendant les états 4/0x14, -1 sinon
   (une charge d'« énergie/rage » IA, seuil de réaction 0x50).
2. Distance aux combattants : |x0 - x1| :
   - repli actif (`DAT_66243 < 0`) : compare à la distance du record de mouvement courant
     (`DAT_66241 + p*0x97 >> 16`) -> si franchie, reset du timer.
   - **réaction à l'attaque adverse** : si distance < 0xA1 (161 px) ET l'adversaire attaque
     (flag 0x10) ET l'IA n'est pas déjà en riposte : roll `FUN_3781a() % 100` comparé au seuil
     `DAT_62ae5 + personnalité*2` (par robot) ; si réussi et en mesure -> `FUN_376c4` (riposte).
3. **Modulation dynamique de la table AIP** : selon l'approche/repli (`DAT_62af9 > 0/< 0`),
   le jeu écrit dans les records AIP courants : durée (+8) = valeur (+6) × 125 % (tier 1)
   ou 111 % (autres) en approche ; × 80 % / 90 % en repli — l'IA ajuste son rythme selon la distance.
4. Décision finale : `FUN_36d20` (avance/attaque ?) sinon `FUN_36bb8`.
5. `FUN_3785c` : exécution de la file (délai d'action, états 0x25/0x2A, sortie vers 66234).
6. `FUN_37db3` : sélecteur d'action avec difficulté (roll % 90 si difficulté > 1).

**Paramètres de personnalité IA** : table `DAT_62ae5 + char*2` (seuils de riposte), config `FUN_22957`.
Les états 0x0D/0x0E (étourdissement ?) et 0x19 déclenchent des réactions spéciales (`FUN_37db3`).

## Physique — `FUN_21baf` (2845 o), lecture détaillée

- `DAT_66285 + p*0x97` : compteur de frames d'animation de l'état courant ; dépassement ->
  `DAT_62742 + anim*2` (table de durées) + avancement (`FUN_3776e` ×3 + `FUN_226cc`) ;
- `DAT_6627A` : compteur freeze post-coup (décrément de 0x20, reset sous 0x21) ;
- `DAT_6626C` : compte à rebours ; `DAT_66270` : verrou décrémenté si record AIP octet+1 < 1 ;
- **Avancement du combattant** : `DAT_685cc + p*4 + (anim>>16)*4` -> pointeur vers la table de
  mouvement de l'animation ; vitesse via `DAT_66237 (+ bit 0-1) * 3 + 3) / (DAT_66256 >> 16)`
  -> index 0..2 -> triplets (dx, dy, ...) sur le pointeur (±3 dwords) ;
  x += 2 × delta signé par l'orientation (flag bit 0) ;
- Énergie : `DAT_66238+2 -= DAT_66299` (coût par frame, stocké par `DAT_6629b`), si < 0 ->
  clamp 0 + flag `DAT_66398 = 1` ;
- Flags : bit 7 (0x80) = en l'air ? ; copie depuis le record AIP : octet+6 -> `DAT_6625a`,
  octet+5 -> `DAT_6623e` ; si octet+1 > 1 -> verrou + `DAT_703a0` (stride 9).

## Collisions CL2 — `FUN_15d92` + données

- Les `?.CL2` sont **par robot** (RB/RG/RS... = lettre du robot ; magic `CLL5` ou `CLL6` selon version) ;
  les `BGANIM*.CL2` sont des tables de décor quasi vides (ex. BGANIM1 = magic + 0x19 + zéros).
- Lecteur : lit le fichier, joueur 2 = +0x10F4 dans le buffer partagé `DAT_65bd8` ;
  `count` (u16 lu au 2e chunk -> `DAT_65bf8`) ; pour chaque entrée : record 3 octets
  (a,b,c) + blocs de 5/6 octets (a×5 + b×6 + c×5 + 3) ; table de pointeurs construite à la volée.
- Données observées : records avec tags ASCII ('F','f','b' = types de boîtes ?) et
  triplets de coordonnées (ex. `4a 58 0a 0e 00 05 46 66 11 36`) — les boîtes de collision
  des attaques par frame. À convertir en données de port au moment de l'implémentation.

## Collisions CL2 — FORMAT FINAL (session 7, validé sur les 49 CL2 robots)

### Lecture (`fn_15d92`, assembleur exact)

```
header 16 o : [4B magic "CLL5"/"CLL6"][4B ??][u16 version @8][u16 @10][u16 count @12][u16 @14]
records dès l'offset 16 ; record : [b0][b1][b2] puis
   b0 blocs de 5 o + b1 blocs de 6 o + b2 blocs de 5 o ; avance = 3 + b0*5 + b1*6 + b2*5
buffer : pointeurs par frame (500 entrées max) à 0x65bd8 (+0x43d0 joueur 2), records à +0x7d0.
```

### Sémantique des boîtes (validée par `fn_3885e`, le test boîte-vs-boîte)

- **b0 = boîtes d'ATTAQUE** (5 o) `[x, y, w, h, dégât(char)]` — rares (34-88 par robot = frames de coup) ;
- **b1 = boîtes de CORPS/défense** (6 o) `[x, y, w, h, p4, partie(5=corps générique)]` — 2/frame
  typiquement (tête + torse), ~310 frames sur RG ;
- **b2 = boîte unique** (5 o) `[x, y, w, h, tag]` — 1 par frame presque toujours (poussée/empreinte,
  usage exact à confirmer avec `fn_38b72`) ;
- échelle : x ×4, y ×2 (le jeu convertit en pixels d'arène) ; overlap X et Y ; le hit le plus
  profond (somme des x retenue) gagne ; état 0x58 = pas de collision ;
- test mutuel : `fn_381a9` appelle `fn_3885e(rec_p0, rec_p1)` deux fois (deltas opposés) pour les
  deux orientations ; projectiles : 3 slots/joueur (tables 0x70314/0x70350, 0x14 o) avec table de
  pointeurs alternative à `+0x87a0` (flag bit 1) ; collisions décor : 4 canaux CTL vs joueurs,
  records à `+0x9850` (les BGANIM*.CL2 chargés là) ; `DAT_705b6` = mode (0 = corps-corps, 1 = proj-joueur,
  2 = proj-proj, 3/4 = décor-joueur).

### Résultat de conversion (`EXTRACTED/data/cl2/`)

- 49/51 consommation exacte ; BGANIM1/Z : 25 records puis 75 o supplémentaires (section secondaire) ;
  BMANIMB : magic variante « 6LLC » (conservé tel quel) ;
- **28 CL2 robots (R0–RZ) : 9 204 frames, 1 119 boîtes d'attaque, 11 170 boîtes de corps,
  7 091 boîtes uniques** ; JSON par robot avec boîtes nommées et offsets de records ;
- `R*.CL2` = lettre robot (base36, même alphabet que les assets) ; count ≈ nb de frames d'animation
  (RG = 469).

### Correction structure joueur

Le mot à +0x02 = **index d'animation courant** (indexe les tables de pointeurs CL2 et 0x685cc) ;
le mot à +0x00 = ID d'état. (Le dword 6620C = (anim<<16)|état.)
