# Documentation — Portage de Rise of the Robots 2 (RISE 2: Resurrection, 1996)

Index de la documentation du projet. Tous les documents sont en français.

**État actuel : import local dossier/ZIP/ISO/BIN-CUE, 28 ou 30 robots selon édition,
MVS/CL2/MRW et trois ANI convertis ; ossature C++/SDL2 en cours.**
La Director’s Cut devient la référence des prochaines extractions : son CD 1
contient le jeu, 105 ANI et les pistes 02–10 ; le CD 2 n’est pas nécessaire.
Lire [11_import_sources.md](11_import_sources.md) pour les imports,
[10_port.md](10_port.md) pour l'état du port et
les limites actuelles, puis [07_decodage_images.md](07_decodage_images.md) pour
les formats d'images. Ouvrir `EXTRACTED/index.html` pour la galerie des 28 robots,
de leurs planches et des 185 GGF.

| Document | Contenu |
|---|---|
| [01_overview.md](01_overview.md) | Vue d'ensemble : objectifs, inventaire des sources, constats clés |
| [02_plan_de_travail.md](02_plan_de_travail.md) | Plan de travail par phases, avec suivi d'avancement |
| [03_environnement.md](03_environnement.md) | Outils, chemins, commandes (Ghidra headless, DOSBox, Python) |
| [04_journal.md](04_journal.md) | Journal de développement (chronologique, mis à jour à chaque session) |
| [05_formats_donnees.md](05_formats_donnees.md) | Formats de fichiers de données du jeu (au fil des découvertes) |
| [06_analyse_binaire.md](06_analyse_binaire.md) | Rétro-ingénierie des binaires (lanceur, RISE2.EXR) |
| [07_decodage_images.md](07_decodage_images.md) | Décodage des images (résultats validés — Astra) |
| [08_moteur.md](08_moteur.md) | Carte du moteur : boucle, états, IA, collisions, dégâts |
| [09_formats_restants.md](09_formats_restants.md) | MRS/CHRSET, musique numérique et CD ; trois ANI convertis, 102 supplémentaires conservés dans la Director’s Cut |
| [10_port.md](10_port.md) | **Le port** : build, ossature, état, tâches (passation) |
| [11_import_sources.md](11_import_sources.md) | Sources privées, import, pistes audio, éditions et vérifications avant publication |

Dossiers de travail :

- `SRC/` — sources d'origine (NE PAS MODIFIER)
- `LOCAL/<profil>/` — nouveaux imports privés : `game/`, `music/`, `EXTRACTED/`, rapports et analyse facultative
- `ANALYSIS/` — sorties brutes d'analyse (décompilé Ghidra, exports)
- `TOOLS/` — scripts (parsers Python, scripts Ghidra)
- `EXTRACTED/` — assets convertis (PNG, WAV, métadonnées)
- `PORT/` — code du port moderne
- `Ghidra_Project/` — projet Ghidra initial (`ROTR2_Reverse`)
- `ANALYSIS/exr_proj/rotr2_exr/` — projet Ghidra de `RISE2_EXR`, ouvert par `TOOLS/ghidra_session.py`
