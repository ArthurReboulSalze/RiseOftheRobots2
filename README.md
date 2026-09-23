# Rise 2 — retour dans l’arène

Un port communautaire expérimental de **Rise 2: Resurrection**, en C++17 et SDL2.
Le projet reconstruit les images, mouvements, collisions et sons à partir de **ta propre copie du jeu**.

Ce dépôt contient notre code, nos outils et nos notes de recherche. Il ne contient
ni jeu original, ni images, films, musique, exécutables DOS ou résultats de décompilation.
Les imports et conversions restent sur ton ordinateur. Projet indépendant, sans affiliation aux ayants droit.

## Préparer ton jeu

Il te faut **Python 3.12 ou plus récent**. Sous Windows, ouvre PowerShell dans ce dossier :

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\IMPORTER.cmd
```

Dans la fenêtre, ajoute tes sources, éventuellement un dossier de musique séparé,
puis clique sur **Importer et préparer les données**. Donne un nom différent à
chaque édition : un profil existant n’est jamais écrasé.

| Ce que tu possèdes | Comment l’importer |
|---|---|
| Un dossier de jeu | Sélectionne le dossier contenant les banques RBT, ou son parent. Le petit dossier installé qui ne contient que la configuration ne suffit pas. |
| Une archive ZIP | Sélectionne le ZIP ; les sous-dossiers sont reconnus. |
| Un fichier ISO | Sélectionne l’image ISO9660. La musique CD peut être ajoutée séparément. |
| Un BIN et son CUE | Sélectionne le **CUE**. Les données et pistes CD audio sont extraites avec leurs numéros. |
| Plusieurs disques | Ajoute chaque CUE/ISO. Le disque contenant les banques du jeu fournit la musique principale ; les bonus restent séparés. |
| Un CD physique sous Windows | Sélectionne la racine du lecteur. Les données sont copiées et les pistes audio lues via le lecteur. Ce chemin nécessite encore une validation matérielle. |

**Director’s Cut : le CD 1 suffit.** Il contient le jeu, 30 robots et les neuf
pistes musicales 02 à 10. Le CD 2 est un disque de bonus ; il n’est pas requis.
Cette édition servira de base aux prochaines extractions des films et contenus du jeu.

Pour monter ce disque avec l’outil ISO de Windows, extrais sa **piste de données** :

```powershell
.\.venv\Scripts\python.exe TOOLS/cue_to_iso.py --cue "D:/Disc 1/RISE2_DC_D1.CUE" --output LOCAL/directors-cut-cd1-data.iso
```

Cette ISO sert à tester l’accès aux données depuis un lecteur virtuel. Elle ne
contient pas les pistes audio 02 à 10 : pour tester la lecture CDDA depuis un
lecteur virtuel compatible, monte le **CUE/BIN** d’origine. L’importeur accepte
un lecteur virtuel de données même s’il n’expose aucune table de pistes audio.

L’outil lit les fichiers sans lancer l’installation ni les exécutables du jeu.
Il ne fournit pas de jeu et ne télécharge pas ses données.

### Musique : plusieurs possibilités

Tu peux fournir des WAV, MP3, FLAC, OGG ou M4A dans un dossier séparé, ou un ZIP
avec `--music`. Utilise les **numéros du CD** : `02.mp3`, `03.mp3`, … `10.mp3`.
`Piste 02.mp3` et `Track 02.wav` sont aussi reconnus. Les fichiers sont renommés
dans le profil sans réencodage ; tes originaux restent intacts. Les doublons et
numéros ambigus sont refusés plutôt que réaffectés.

Le jeu possède également une **musique numérique séquencée**, dans les six
couples `MGA` à `MGF` (`.MRS` + `.MRW`). Il ne s’agit pas de fichiers MIDI classiques.
L’importeur détecte ces banques et conserve les séquences et échantillons.

Le réglage `auto` choisit CD → musique numérique → effets d’ambiance → silence,
selon les données disponibles. Les choix explicites sont `cd`, `digital`,
`effects` et `off`. **C’est pour l’instant une configuration d’import** : la
lecture musicale et le séquenceur MRS ne sont pas encore intégrés au moteur du port.

### En ligne de commande

```powershell
# Dossier + musique séparée
.\.venv\Scripts\python.exe TOOLS/import_game.py --source "D:/Mes jeux/Rise2" --music "D:/Mes pistes" --output LOCAL/mon-jeu

# Director’s Cut : le premier disque seul suffit (BIN à côté du CUE)
.\.venv\Scripts\python.exe TOOLS/import_game.py --source "D:/Disc 1/RISE2_DC_D1.CUE" --output LOCAL/directors-cut

# ZIP ou ISO sans rip musical ; sélection automatique de la musique numérique
.\.venv\Scripts\python.exe TOOLS/import_game.py --source "D:/jeu.zip" --music-mode auto --output LOCAL/autre-copie
```

`--import-only` copie, identifie et contrôle les sources sans reconstruire les
atlas. Le rapport `LOCAL/<profil>/import-report.json` contient les empreintes,
les pistes, le mode audio et les limites constatées. Un échec ne remplace jamais
un profil précédent. La conversion des images peut prendre plusieurs minutes.

```text
LOCAL/mon-jeu/                 # entièrement privé, ignoré par Git
  game/                       # données de jeu normalisées
  sources/                    # disques/images/archives extraits, bonus séparés
  music/                      # pistes CD 02, 03, … et manifest
  EXTRACTED/                  # PNG, atlas, WAV, JSON et galerie
  settings.json
  import-report.json
```

## Compiler et jouer

La compilation actuelle cible **Windows x64**, avec Visual Studio 2022
(outils C++ et SDK Windows) et CMake 3.20 ou plus. Les bibliothèques sont téléchargées
depuis leurs projets officiels, à des versions fixes et avec contrôle SHA-256 :

```powershell
.\.venv\Scripts\python.exe TOOLS/bootstrap_port.py
cmake -S PORT -B PORT/build -G "Visual Studio 17 2022" -A x64
cmake --build PORT/build --config Release
ctest --test-dir PORT/build -C Release --output-on-failure
.\PORT\build\Release\rotr2.exe --assets "$PWD/LOCAL/mon-jeu/EXTRACTED"
```

Le parcours actuel comprend les logos, le titre, la sélection des robots et un
premier combat. Entrée valide, Échap revient en arrière. Choix des robots :
flèches pour le joueur 1, A/D pour le joueur 2. En combat : flèches + J/K et W/A/S/D + I/O.
Le profil classique fournit 28 robots ; la Director’s Cut ajoute les deux banques bonus.

Le port est en construction : commandes et combat incomplets, options de menu
partielles, musique encore à intégrer. Seuls `LLOGO`, `END` et `ENL` sont actuellement
convertis automatiquement en vidéo ; les autres ANI et bonus sont conservés
dans le profil pour la suite du travail. La copie complète et une édition amputée
de ses films sont distinguées dans le rapport.

## Extraction et décompilation

L’import normal **convertit les données** pour notre moteur C++. Il n’a pas besoin
de Ghidra et ne transforme pas automatiquement l’exécutable DOS en un nouveau jeu.
Pour participer à la rétro-ingénierie, les outils d’analyse restent disponibles :

```powershell
# Image LE + rapport, dans le profil privé
.\.venv\Scripts\python.exe TOOLS/prepare_analysis.py --profile LOCAL/mon-jeu

# Facultatif : projet Ghidra isolé (Ghidra, JDK compatible et pyghidra à installer)
python TOOLS/prepare_analysis.py --profile LOCAL/mon-jeu --ghidra "C:/Outils/ghidra" --java-home "C:/Outils/jdk"
```

Les sorties d’analyse et de décompilation restent locales. Voir
[la documentation](documentation/README.md) et [les imports et leurs tests](documentation/11_import_sources.md).

## Développer sans données du jeu

```powershell
.\.venv\Scripts\python.exe -m unittest discover -s TOOLS/tests -v
.\.venv\Scripts\python.exe TOOLS/test_image_codecs.py
python TOOLS/audit_repo.py --staged
```

Après un import, `PORT/build/Release/rotr2_asset_smoke.exe LOCAL/mon-jeu/EXTRACTED`
vérifie aussi les menus et le chargement de tous les robots sans ouvrir de fenêtre.

Les tests automatisés fabriquent leurs propres petites images disque et leurs
propres échantillons ; aucun fichier original n’est nécessaire. L’audit vérifie
le contenu de l’index Git : seuls les chemins de code et de documentation prévus
sont admis. `SRC`, `LOCAL`, `EXTRACTED`, les captures, les projets Ghidra, les
builds et les dépendances téléchargées sont exclus.

Avant publication, il reste à valider le CD physique et à choisir la licence du
code du projet. Les noms et contenus du jeu restent ceux de leurs ayants droit.
Les dépendances conservent leurs licences respectives : SDL2/SDL2_image (zlib),
nlohmann/json (MIT), Pillow et pycdlib (voir leurs distributions).
