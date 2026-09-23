# 02 — Plan de travail & suivi d'avancement

> Ce document est le tableau de bord du projet. Chaque phase est mise à jour (statut, résultats) au fur et à mesure.

## Phases

| # | Phase | Statut | Livrables |
|---|---|---|---|
| 0 | Mise en place (workspace, doc, chaîne d'outils, boot test DOSBox) | ✅ terminée (2026-09-22) | arborescence, docs, chaîne headless validée, boot test OK jusqu'au combat |
| 1 | Formats de données & extraction d'assets | 🔄 GGF, sprites, MVS, MRW, CL2 et 3 ANI décodés ; Director’s Cut CD 1 retenu pour la suite | Ancienne copie : 28 robots, 1 142 WAV ; Director’s Cut : 30 robots, 1 179 WAV, 105 ANI dont 102 à convertir |
| 2 | Rétro-ingénierie de `RISE2.EXR` (loader LE + analyse Ghidra) | 🔄 Import LE et décodeurs d'images validés ; logique de jeu à poursuivre | `06_analyse_binaire.md`, exports assembleur, validation x86 |
| 3 | Décision d'architecture du port | ✅ réimplémentation C++/SDL2 adoptée | `PORT/`, `10_port.md` |
| 4 | Développement du port | 🔄 logos, titre, sélection de 28 ou 30 robots et combat initial en 640x400 ; HUD, transitions MVS et CL2+dégâts câblés | `PORT/`, `10_port.md` |
| 5 | Validation & packaging | 🔄 Import et dépôt local ; pas de publication GitHub avant validation | `README.md`, `11_import_sources.md`, tests synthétiques |

## Détail des phases

### Phase 0 — Mise en place

- [x] Arborescence de travail (`TOOLS/`, `ANALYSIS/`, `EXTRACTED/`, `PORT/`, `documentation/`)
- [x] Documentation initiale (README, overview, plan, environnement, journal)
- [x] PyGhidra installé (scripts Python headless Ghidra 12)
- [x] Verrous du projet Ghidra `ROTR2_Reverse` libérés (GUI fermée)
- [x] Import + analyse Ghidra du lanceur `RISE2.EXE` (19 Ko)
- [x] Comprendre la logique de patching du lanceur sur `RISE2.EXR` (cible 0x52421 identifiée ; patch in-memory/idempotent confirmé dynamiquement)
- [x] Test de démarrage du jeu original sous DOSBox — **validé jusqu'au combat en cours** (LOADER vs DEADLIFT)

### Phase 1 — Formats de données & extraction

Acquis au 22 septembre 2026 :

- [x] Géométrie des GGF : 400×200, 800×400 ; A6K isolé en 640×400.
- [x] 185 GGF extraits ; 3 fragments invalides identifiés, sans données d'image.
- [x] ANL/ANR : table d'offsets et segments de pixels décodés ; 126 banques / 20 969 frames validées.
- [x] 28 robots dans les deux résolutions + portraits : 18 522 frames sur 139 planches.
- [x] Transparence, positions et indices de palette conservés pour le port.
- [x] Galerie locale, contrôles d'atlas et comparaisons avec le code x86.
- [x] Associer les images MVS aux planches RBT et aux boîtes CL2 (atlas = image MVS + 1).
- [x] Décoder les trois ANI présents : LLOGO, END, ENL (161 images) ; la distribution est explicitement amputée de ses cinématiques (`ENERGY.NFO`).
- [ ] Finaliser les cadences et transitions AIP/MVS/STS, ainsi que les séquences MRS.
- [x] Import du CD 1 Director’s Cut : 30 robots et 105 ANI conservés, musique CD extraite.
- [ ] Export contextuel des autres banques ANR, scripts CTL et polices ; conversion des 102 ANI supplémentaires.

Plan d'action :
1. Écrire un catalogueur : stats par type de fichier, magic numbers, entropie (détecter compression LZH — cf. `RSPC050.LZH`).
2. Parsers réalisés : `.GGF`, `.PAL`, `.ANL`/`.ANR`, `.MVS`, `.MRW` audio et `.CL2`. À compléter : `.MRS` (séquences audio), `.CTL`, `.STS`, `.DAT` et `RISE2.CFG`.
3. Croiser avec les routines de chargement du binaire (Phase 2) comme vérité terrain.
4. Extraction complète vers `EXTRACTED/` (PNG par frame avec palettes, WAV, JSON de métadonnées).

### Phase 2 — Rétro-ingénierie

1. **Réalisé** : parser LE en Python (header, object table, fixup pages, point d'entrée EIP:CS).
2. **Réalisé** : import headless Ghidra x86:32 à la base réelle **0x10000**, application des fixups, auto-analyse.
3. Marquer runtime Watcom + DOS/4GW comme bibliothèque (réduction bruit décompilé).
4. Cartographie : boucle de jeu, entrées, IA (Superviseur/droides), machines à états des combattants, tables de coups/dégâts, rendu VESA 640×400 double-buffered, audio, loaders de fichiers (ancrage : `ERRORS.TXT`).

### Phase 3 — Architecture du port (choix réalisé)

| Option | Description | Avantages | Inconvénients |
|---|---|---|---|
| A (recommandée) | Réimplémentation moteur moderne (C++ + SDL2) consommant assets extraits + logique RE | Port propre, durable, portable | Effort logique de jeu |
| B | Décompilation → recompilation avec shims vidéo/audio/OS | Fidélité maximale | Très lourd, code décompilé dur à maintenir |
| C | Wrapper DOSBox préconfiguré | Rapide | Pas un vrai port |

Choix actuel : option A, C++17/SDL2, Windows d'abord, résolution logique 640×400.
La fidélité des animations, collisions et commandes reste à valider contre le jeu DOS.

### Phase 4 — Développement

Squelette → pipeline assets → états de jeu (titre, sélection, arène, HUD, rounds) → combat (tables RE) → IA → audio.

### Phase 5 — Validation

Comparaison side-by-side avec captures DOSBox, checklist fonctionnalités (robots, modes, options setup), packaging.

Priorité actuelle : terminer les contrôles d’import et de dépôt local (document 11),
puis reprendre les extractions et films sur le CD 1 Director’s Cut. Pas d’upload
GitHub avant validation, notamment du lecteur CD physique. La lecture MRS et
musique CD reste ensuite à intégrer avec le reste du gameplay (document 10).
