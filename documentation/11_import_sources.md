# 11 — Import des sources et préparation du dépôt

État au 23 septembre 2026. Le dépôt reste **local**, sans remote ni publication
GitHub. Les données du jeu, leurs conversions et les résultats de décompilation
ne font pas partie du dépôt. Le README racine décrit l’installation publique.

## Éditions et périmètre retenu

| Source vérifiée | Fichiers dans RISE2 | Robots RBT | ANI | Banques MRW / échantillons |
|---|---:|---:|---:|---:|
| Ancien dossier DOS | 996 | 28 | 3 | 69 / 1 142 |
| Director’s Cut, CD 1 | 1 122 | 30 | 105 | 71 / 1 179 |

Le **CD 1 suffit pour la Director’s Cut** : jeu et pistes musicales 02 à 10 sont
sur ce disque. Le CD 2 contient 275 fichiers de bonus (GIF, FLC, WAV, documents,
utilitaires), sans banques du jeu ; il n’est pas nécessaire. L’import simultané
des deux disques a été comparé au CD 1 seul : mêmes SHA-256 pour chaque fichier
du jeu et chaque piste principale, les bonus restant à part.

La Director’s Cut est la base choisie pour les **prochaines extractions**, avec
tous les contenus du jeu conservés. RBT2 et RBT3 ajoutent SHEEPMAN et BUNNYRABBIT.
Les 30 portraits et banques haute résolution sont pris en charge ; les deux
robots supplémentaires n’ont pas de banques RB4 ni de fonds AG dédiés dans ce CD.
Leur palette propre suffit aux indices utilisés par leurs sprites.

Les 105 ANI sont importés. LLOGO/END/ENL sont convertis en 161 images ; les 102
autres sont conservés pour la prochaine phase. L’import réussi ne signifie pas
que toutes les cinématiques ou tous les systèmes du jeu sont déjà jouables.

## Pipeline

1. `TOOLS/import_gui.py` ou `TOOLS/import_game.py` reçoit dossier, ZIP, ISO9660,
   CUE/BIN ou lecteur CD Windows. La détection cherche RBT0.ANR + RBT0.MVS.
2. Les archives et disques sont extraits dans un répertoire temporaire. La
   piste de données des BIN/CUE est lue directement, sans montage ni exécution.
   MODE1/2048, MODE1/2352 et MODE2/2352 Form 1 sont acceptés ; les limites viennent
   du CUE. Les INDEX 00/01 et PREGAP sont distingués.
3. Le dossier du jeu est copié dans `game/`, avec noms normalisés en majuscules
   et empreintes SHA-256. Deux éditions différentes ne sont jamais fusionnées.
   Les originaux restent inchangés ; un dossier installé réduit à la configuration
   est insuffisant. Les noms ambigus, chemins sortants et liens sont refusés.
4. La musique est copiée dans `music/` et indexée. Un dossier/ZIP donné avec
   `--music` a priorité ; sinon, les pistes du disque contenant le jeu sont
   sélectionnées. La musique d’un disque bonus ne remplace pas celle du jeu.
5. Les convertisseurs GGF, ANR, MVS, CL2, MRW, ANI connus, police UI et galerie
   construisent `EXTRACTED/`. Le lien atlas/MVS/CL2 est contrôlé pour chaque robot.
6. Le profil et son rapport deviennent disponibles seulement si tout réussit.
   Un profil existant n’est jamais écrasé. `--import-only` saute la conversion,
   tout en contrôlant les fichiers nécessaires au port.

Les trois fragments GGF A6B/A6G/A6M sont signalés par le convertisseur, sans
empêcher l’import : ils ne produisent pas une image complète exploitable.
Le reste des sources, même non encore interprété, est conservé dans le profil.

## Audio

Les pistes CD gardent leurs numéros **02 à 10**. Les BIN/CUE sont extraits en
WAV PCM 16 bits stéréo 44 100 Hz, sans réencodage avec pertes. Le rip des images
inspectées utilise l’ordre little-endian. Les gaps synthétiques du CUE ne sont
pas ajoutés aux offsets du BIN ; un INDEX 00 stocké est exclu de la piste voisine.

Les pistes fournies séparément peuvent être WAV, MP3, FLAC, OGG ou M4A.
Exemples acceptés : `02.wav`, `Piste 02.mp3`, `Track 02.flac`. Les doublons ou
numéros ambigus échouent. Un ISO de données seul ne contient pas le CD audio.
Les fichiers compressés sont conservés tels quels avec vérification de signature ;
l’importeur n’effectue pas une validation de décodage complète de chaque format.

La musique numérique d’origine utilise **MGA à MGF, MRS + MRW** : séquences et
échantillons, pas des fichiers MIDI classiques. Le manuel et `FUN_150b4` confirment
ce mode (documents 08/09). `auto` choisit CD, puis numérique, ambiance et silence
selon les sources disponibles. Les modes explicites sont `cd`, `digital`,
`effects`, `off` ; demander un mode absent échoue.

**Ces réglages sont enregistrés dans settings.json. La lecture musicale du port
et le séquenceur MRS restent à intégrer.** Les banques numériques sont préservées
même lorsqu’on choisit la musique CD.

## Analyse facultative

`prepare_analysis.py --profile LOCAL/<profil>` reconstruit l’image LE et son
rapport dans `LOCAL/<profil>/ANALYSIS/`. Avec `--ghidra` et `--java-home`, il
crée un projet Ghidra isolé (PyGhidra requis dans le Python qui lance la commande).
Le projet historique n’est pas remplacé. Pour réutiliser les scripts d’analyse :

```powershell
$env:RISE2_ANALYSIS = "$PWD/LOCAL/mon-jeu/ANALYSIS"
python TOOLS/exr_decompile_at.py <adresse-hex-de-cette-edition>
```

Le loader LE corrige les fixups aux frontières de pages, la dernière page et les
sélecteurs sans offset cible ; voir document 06. La préparation et l’import
Ghidra ont été exécutés sur les deux éditions. Les adresses historiques de
l’ancienne copie ne doivent pas être réutilisées aveuglément sur la Director’s Cut.

## Vérifications

| Cas | Résultat local |
|---|---|
| Dossier DOS + neuf MP3 séparés | Import et conversions complets ; sources musicales conservées sans réencodage |
| ZIP fabriqué depuis ce dossier | 996 fichiers identiques au dossier, SHA-256 comparés ; numérique choisi sans CD |
| ISO9660 fabriqué depuis ce dossier | Même comparaison sur les 996 fichiers ; numérique choisi sans CD |
| CUE/BIN Director’s Cut CD 1 seul | Import et conversions complets ; 30 robots, 105 ANI préservés, 9 pistes WAV |
| CUE/BIN CD 1 + CD 2 | Même jeu et musique principale que CD 1 seul ; bonus séparés |
| Tests synthétiques Python | 22 tests import/LE + 9 tests codecs réussis |
| Build Release / CTest | Compilation réussie, test fighter_frames réussi |
| Lecteurs C++ avec profils réels | Logos, titre, portraits et banques des 28 puis 30 robots chargés sans erreur |
| Checkout du seul index Git | Dépendances récupérées et vérifiées, compilation et tests réussis sans données embarquées |
| CD physique réel | **À faire** : aucun lecteur optique disponible sur la machine de test |

Les tests synthétiques fabriquent leurs fichiers sans données originales. Les
profils de test et leurs rapports se trouvent uniquement dans `LOCAL/`.
La validation locale a utilisé Python 3.14.5 ; la matrice CI 3.12/3.14 sera
exécutée après publication. La fenêtre Tkinter a été initialisée sans erreur ;
ce contrôle ne remplace pas un essai utilisateur de tous ses boutons.
Le lecteur physique utilise les appels Windows de lecture TOC/CDDA, avec contrôle
de longueur et trois tentatives en cas d’erreur. Les tests simulés ne remplacent
pas un essai avec disque et matériel réels ; il ne s’agit pas d’un rip sécurisé
avec correction de jitter ou comparaison AccurateRip.

Commandes de vérification :

```powershell
.\.venv\Scripts\python.exe -m unittest discover -s TOOLS/tests -v
.\.venv\Scripts\python.exe TOOLS/test_image_codecs.py
ctest --test-dir PORT/build -C Release --output-on-failure
.\PORT\build\Release\rotr2_asset_smoke.exe LOCAL/mon-jeu/EXTRACTED
python TOOLS/audit_repo.py --staged
```

`rotr2_asset_smoke` utilise un écran SDL caché et charge les logos, menus,
portraits et banques de tous les robots via les véritables lecteurs C++.
Il contrôle la sélection complète sans remplacer la comparaison visuelle au jeu DOS.
La CI prévue utilise uniquement des fixtures synthétiques et compile le port ;
elle ne télécharge aucune donnée du jeu.

## Contenu publiable

`.gitignore` exclut par défaut les nouveaux dossiers racine et autorise seulement
les emplacements de code, tests, modèles HTML et documentation. SRC, EXTRACTED,
LOCAL, ANALYSIS, projets Ghidra, captures, builds et dépendances sont exclus.
Le modèle HTML est versionné ; la galerie contenant les images et données ne l’est pas.
`audit_repo.py` contrôle le contenu complet de l’index Git : chemins, texte UTF-8,
fichiers binaires et motifs de secrets/médias embarqués. Il complète l’inspection
des fichiers ; ce n’est pas une analyse juridique ou un détecteur universel de secrets.

Les dépendances Windows sont récupérées par `bootstrap_port.py` depuis leurs
projets officiels, avec versions fixes et SHA-256. Rien dans l’import ne lance
les exécutables DOS. Les fichiers de décompilation restent privés.

Avant publication : effectuer l’essai matériel CD, choisir la licence de notre
code et revoir l’index. Aucun dépôt distant n’est créé à cette étape.

Références techniques : [pycdlib](https://clalancette.github.io/pycdlib/pycdlib-api.html),
[RAW_READ_INFO Windows](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddcdrm/ns-ntddcdrm-__raw_read_info),
[lecture CDDA Windows](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddcdrm/ni-ntddcdrm-ioctl_cdrom_raw_read).
