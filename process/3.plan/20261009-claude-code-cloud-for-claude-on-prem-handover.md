# Passation — branche `feature/MCP`, session du 2026-10-08 au 2026-10-09

Note de passation pour l'agent qui reprend ce travail. Ce n'est pas un artefact AGNOS (pas d'identifiant, ignoré par l'index) : elle complète
`CHECKPOINT-MCP-2026-10-08.md` (même dossier), qui reste le résumé court. Tout ce qui suit a été vérifié dans le dépôt le 2026-10-09 ; ce qui ne
l'a pas été est dit comme tel. Les identifiants cités (RQ, ADR, DEC, TASK) se retrouvent par `grep` dans `process/`.

## 1. État en un coup d'œil

| | |
|---|---|
| Branche | `feature/MCP` (suit `origin/feature/MCP`), tout est commité et poussé |
| Point de départ de la session | `e277aa5` (« mcp ») ; fin de session `87c4324` (les commits sont listés en 3) |
| Tests | `ctest` : **1076 sur 1076** (Linux, GCC 13.3, `Debug`), une fois la dernière tâche faite ; depuis, seul `AGENTS.md` a changé |
| Serveur | **56 outils sans option, 72 avec `--allow-disk`, 61 avec `--allow-front-panel`, 77 avec les deux** (vérifié par `ctest`) |
| Plan en cours | `process/3.plan/PLAN-MCP-005-remaining-sampler-functions.md` : tâches 044 à 054 faites, **055 bloquée (il faut le propriétaire)** |
| Jamais fait | aucun des nouveaux outils n'a tourné sur un vrai sampler ; tout est vérifié sur le sampler simulé |

Le propriétaire n'était pas disponible de la tâche 047 à la fin : tous les choix qu'il aurait pu trancher sont dans le champ `Assumptions` de la tâche
concernée, dans le plan. **Lisez-les avant de modifier un de ces comportements.**

## 2. Avant de toucher à quoi que ce soit

1. **Lire en entier** `.github/instructions/agnos-sw-eng.instructions.md` (382 lignes, étape 1 de « START SESSION »). Cette session n'en a lu que 150 lignes
   au début, ce qui a coûté des erreurs (voir 8). `AGENTS.md` le dit désormais.
2. Lire `process/_sessionstate/session.yaml` : il dit `platform: linux`, `unit_tests: true`. Les commandes shell s'écrivent donc en bash. Si votre
   plateforme diffère, réécrivez-le **avant** la première tâche.
3. Le propriétaire écrit en français et attend des réponses **concises** en français, qui vérifient les faits, ne disent jamais ce qui est inventé et
   avouent « je ne sais pas ». Préfixez toutes vos commandes shell par `rtk` (proxy qui condense les sorties ; voir 6).
4. Branche : le harnais cite une branche `claude/mcp-feature-branch-wrfgo0` qui **n'existe pas** sur le remote. Le propriétaire a demandé de travailler
   sur `feature/MCP` et seulement là. Ne poussez nulle part ailleurs et n'ouvrez pas de pull request sans demande explicite.
5. Règle de commit/push donnée par le propriétaire : **un commit par tâche**, un seul push à la fin. Un hook d'arrêt de session
   (`~/.claude/stop-hook-git-check.sh`) redemande « commit and push » dès qu'un fichier est modifié : j'ai alors commité et poussé, en le lisant
   comme un accord.

## 3. Ce qui a été fait, dans l'ordre

Environnement (tâches hors plan) : `rtk 0.49.0` installé depuis le `.deb` fourni par le propriétaire (`rtk init -g` a ajouté `@RTK.md` au `CLAUDE.md`
global ; le hook de `~/.claude/settings.json` **n'a pas pu être écrit**, le classificateur de permissions l'a refusé : tapez `rtk` à la main) ;
`libasound2-dev` installé avec `apt-get` (nécessaire à `juce_audio_devices`) ; `session.yaml` corrigé (`windows` → `linux`).

| Commit | Contenu |
|---|---|
| `c66c344` | TASK-MCP-046 : `set_midi_setting`, `set_midi_filter` |
| `3a58fff` | `session.yaml` : plateforme `linux`, `unit_tests: true` |
| `070ac8f` | 046 : mots, valeurs et bornes des outils MIDI en tables et constantes nommées (la règle « pas de littéral dupliqué » de la DoD avait été oubliée) ; `Verification` raccourci |
| `f55b827` | 046 : **canaux MIDI = numéros du sampler, 0 à 31** (choix du propriétaire) ; le critère de `RQ-MCP-047` « canal 17 refusé » devient « canal 32 refusé » (amendé dans `FTR-MCP-005`) |
| `9d5e21f` | TASK-MCP-047 : lister, sélectionner, renommer les song files, set lists et scenelists (8 outils) |
| `e3ae3e8` | TASK-MCP-048 : `delete_song_file`, `delete_set_list`, `delete_scenelist` |
| `e6337cd` | TASK-MCP-049 : genres `song_file`, `set_list`, `scenelist` pour `save_memory_item`, `save_all_memory_items`, `load_file` |
| `f95237b` | TASK-MCP-050 : `delete_all_programs`, `delete_all_samples`, `delete_all_multis` |
| `b4979b6` | TASK-MCP-051 : `clear_sampler_memory` |
| `1102d80` | TASK-MCP-052 : carte d'effets (5 outils) |
| `5ead81c` | TASK-MCP-053 : `--allow-front-panel` et 5 outils de touches |
| `5f2f895` | TASK-MCP-054 : README, comptes vérifiés par `ctest`, section Sécurité réécrite |
| `1225f39` | note de reprise `CHECKPOINT-MCP-2026-10-08.md` + ligne de `METRICS_LOG.md` |
| `87c4324` | `AGENTS.md` : renvoi vers le fichier d'instructions AGNOS, à lire en entier |

Ce fichier de passation est le commit suivant.

## 4. Les outils ajoutés, par tâche

- **046 — réglages MIDI** (`set_midi_setting` : `program_change`, `multi_select`, `multi_select_channel`, `external_apm_controller`, `aftertouch` ; `set_midi_filter` :
  `note_on`/`aftertouch`/`wheels`/`volume`, canal, `allow`/`ignore`). La section 04 n'a pas de Get : les réponses disent que la valeur précédente
  n'a pas pu être lue et ne peut pas être remise.
- **047 — listes** : `list_song_files`, `select_song_file`, `rename_song_file` ; `list_scenelists`, `select_scenelist`, `rename_scenelist` ;
  `list_set_lists`, `rename_set_list` (par nom : le sampler n'a pas de set list courante). Les noms sont lus un à un.
- **048 — suppressions** : `delete_song_file`, `delete_scenelist` (l'élément courant, `confirm` = son nom exact) ; `delete_set_list` (`name` + `confirm`).
- **049 — disque** : voir ci-dessus. `memoryNames()` lit aussi les trois listes ; la réponse d'un chargement ne parle d'elles que si elles ont changé.
- **050 — tout supprimer** : `confirm` = le nombre d'éléments (entier ≥ 1, un texte est refusé).
- **051 — `clear_sampler_memory`** : `confirm` = programmes + samples + multis ; les song files, set lists et scenelists ne sont pas comptés.
- **052 — effets** : `get_fx_board`, `set_fx_channel_mute`, `set_fx_module` (type et/ou `enabled`), `get_fx_parameter`, `set_fx_parameter`. Les plages sont celles de la table 25
  de la spec, écrites une fois dans `FxCatalogue` ; la règle EB20 (seuls les modules 2 et 3 des canaux 0 et 1 changent de type) est appliquée par l'outil.
- **053 — touches** : `press_key`, `hold_key`, `release_key`, `turn_data_wheel`, `send_ascii_key`, **uniquement avec `--allow-front-panel`** ; toutes déclarées destructives.
- **054** : le README de `juce/mcp` et celui de la racine annoncent les quatre comptes ci-dessus ; `CheckReadmeCoversTools.cmake` les compare à ce que le serveur liste.

## 5. Où est quoi

Unités de la passerelle (`juce/mcp/src/`), une par famille, chacune propriétaire exclusive de ses appels sensibles :

| Fichier | Rôle |
|---|---|
| `SamplerGateway.cpp` (1830 lignes) | cœur, programmes/échantillons/multis, **Delete ALL** et **Clear Sampler Memory** (`deleteAllMemoryItems`, `clearSamplerMemory`) |
| `SamplerGatewayDisk.cpp` | disques, chargements, sauvegardes (genres de sauvegarde étendus en 049) |
| `SamplerGatewayLists.cpp` | song files, set lists, scenelists : lister, choisir, renommer, supprimer |
| `SamplerGatewayFx.cpp` | carte d'effets |
| `SamplerGatewayKeys.cpp` | touches de la face avant (**seul fichier autorisé à nommer les primitives §20**) |
| `FxCatalogue.cpp` + `include/mcp/FxCatalogue.hpp` | tables 24 et 25 en données, règle EB20 |
| `Tools.cpp` (3663 lignes) | tous les outils ; `makeAllTools` les assemble ; sections : MIDI (tables `Word<T>`, `meaningOf`, `wordList`, `wordArray`), listes, delete-all/clear, effets, touches |
| `ServerOptions.cpp`, `server/main.cpp` | options de lancement, dont `--allow-front-panel` |

`Tools.cpp` et `SamplerGateway.cpp` sont devenus gros ; les scinder serait raisonnable mais n'a pas été demandé.

Conventions suivies, à garder : gateway renvoie `Outcome<T>` ; chaque appel AKM passe par `await<...>(waitFor(n), ...)` et `explain(...)` ; l'unité sensible
accède à la session par `session()` (le type `Connection` est privé à `SamplerGateway.cpp`) ; constantes nommées, jamais de littéral dupliqué ; chaque
fonction ou outil porte le commentaire de traçabilité `[RQ-…, ADR-…]` ; les outils valident tout **avant** d'envoyer.

### Garde-fou de sécurité des sources

`juce/tests/CheckNoDestructiveCalls.cmake` (test `mcp_sources_call_no_destructive_primitive`) interdit, dans `juce/mcp`, tout nom de primitive AKM contenant
`create|delete|rename|save|load|clear|eject|format`, sauf dans la liste blanche par fichier (variables `allowed_calls_<fichier>`), et interdit les primitives
de touches (`key_pattern`) hors de `SamplerGatewayKeys.cpp`. Trois **tests négatifs** prouvent qu'il échoue : dossiers
`juce/tests/mcp/forbidden_call_fixture` (suppression), `forbidden_key_fixture` (touche), `forbidden_eject_fixture` (éjection) — fichiers jamais compilés. Éjecter ou
formater un disque reste interdit partout (décision du propriétaire du 2026-10-07).

### Sampler simulé et bancs de test

- `juce/tests/support/` : `SimulatedSampler` modélise désormais, en plus, la sauvegarde/le chargement des trois genres (extensions `.MID`, `.SET`, `.SCN` et tailles
  = **valeurs provisoires**). Les trois champs `loads…Named` de `FileRecord` sont placés **à la fin** de la structure : les tests construisent `FileRecord` par position
  (un champ inséré au milieu casse la compilation).
- `juce/tests/mcp/ToolRig.hpp` : `ToolRig(bool withDisk = false, bool withFrontPanel = false)`. `DiskRig.hpp` pour le disque. `SimulatedServer.cpp` (le serveur simulé) est
  amorcé avec 3 song files, 2 set lists, 2 scenelists et une carte EB20.
- Fichiers de tests ajoutés : `MidiSetupToolsTests`, `NamedListToolsTests`, `NamedListDeleteToolsTests`, `DiskNamedListTests`, `BulkDeleteToolsTests`, `ClearMemoryToolTests`,
  `FxCatalogueTests`, `FxToolsTests`, `FrontPanelToolsTests` (et des cas dans `ServerOptionsTests`).
- Étiquettes Catch2 utiles : `[midi]`, `[namedlists]`, `[deletelists]`, `[disk][save]`, `[bulkdelete]`, `[clearmemory]`, `[fx]`, `[fxcatalogue]`, `[frontpanel]`, `[options]`.

## 6. Construire, tester, régénérer

```
rtk cmake -S juce -B juce/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DCMAKE_CXX_FLAGS=-Wno-dangling-reference
rtk cmake --build juce/build -j"$(nproc)"
rtk ctest --test-dir juce/build -j4 > /tmp/ctest.log 2>&1; grep -E "ctest:|Failed|Not Run" /tmp/ctest.log
juce/build/tests/xs56k_mcp_tests "[fx]"          # un sous-ensemble
```

- `-Wno-dangling-reference` : faux positif de GCC 13 dans `juce/tests/akm/RealSamplerSuiteTests.cpp` (fichier non touché ; la CI utilise `ubuntu-22.04`, GCC 11).
  Le propriétaire a décidé de **le garder en local** plutôt que de corriger les 4 sites. Ne le mettez pas dans les fichiers CMake du dépôt.
- `rtk` condense les sorties : `rtk ctest … | grep` peut ne rien afficher. Redirigez dans un fichier puis lisez-le (ou `rtk proxy <cmd>`).
- Le répertoire `juce/build` est ignoré par git et propre au conteneur : il faut le recréer. JUCE, Catch2 et nlohmann/json sont téléchargés par CMake dans
  `juce/build/_deps` (réseau via le proxy de la session) ; le premier build complet prend plusieurs minutes.
- **Conversations scriptées** (`juce/tests/mcp/conversations/`) : après un changement délibéré, régénérez les sorties attendues depuis `juce/tests` :
  `../build/tests/xs56k_mcp_server_simulated < mcp/conversations/simulated_session.jsonl > mcp/conversations/simulated_session.expected.jsonl` et
  `../build/tests/xs56k_mcp_server_simulated --allow-disk --allow-disk-refresh --allow-front-panel < mcp/conversations/simulated_disk_session.jsonl > mcp/conversations/simulated_disk_session.expected.jsonl`,
  puis **lisez la différence ligne à ligne** (j'ai chaque fois vérifié par script que seules les lignes attendues changeaient). Mettez à jour `-DEXPECT_LINES=` dans
  `juce/tests/CMakeLists.txt` (160 et 58 aujourd'hui). N'insérez jamais au milieu d'une conversation un appel qui change un état dont les appels suivants dépendent
  (par exemple supprimer tous les multis) : placez-le avant l'appel `server/discover` final.
- Index AGNOS : `bash .github/skills/agnos-index/scripts/build-index.sh` avant chaque commit ; ne jamais éditer `process/INDEX.idx.md` à la main.
- Format de commit : `<type>(MCP): ADR-MCP-005/TASK-MCP-0NN <description> (HOL -Human on the loop)` suivi des deux lignes `Co-Authored-By: …` et `Claude-Session: …`
  que le harnais impose.

## 7. Choix qui s'écartent de la spécification ou de l'énoncé d'une tâche

Le détail et la justification sont dans `Assumptions` du plan ; à relire par le propriétaire.

- **Renommage vers un nom déjà pris** (047, 048) : refusé par le serveur avant tout envoi, au lieu de « la réponse est celle du sampler » (le simulateur accepte les doublons, la
  spec est muette). Renommer un élément à son propre nom est accepté.
- **Sélection d'un nom ou d'une position inconnus** (047) : la commande est envoyée et l'ERREUR 04 du sampler est traduite ; le critère disait « rien n'est envoyé ». Pour une set list
  inconnue, rien n'est envoyé (résolue d'après la liste).
- **Canal MIDI** (046) : numéros 0 à 31 (1A = 0 … 16B = 31), la réponse ajoute le nom (`3 (= 4A)`) ; 17 est valide, le premier refusé est 32.
- **Extensions des fichiers sauvegardés** (049) : inconnues ; `.MID`, `.SET`, `.SCN` sont des marques de substitution ; les outils trouvent un fichier par le nom de l'élément, sans extension.
- **Charger un fichier dont l'élément existe déjà** (049) : ajoute un second élément du même nom (comme le simulateur pour un programme) ; comportement réel inconnu.
- **`confirm` des outils de masse** (050, 051) : entier ≥ 1 ; « 3 » en texte est refusé ; un sampler qui ne contient rien n'envoie rien.
- **Total de `clear_sampler_memory`** (051) : programmes + samples + multis seulement ; le vrai sampler peut aussi vider song files, set lists et scenelists (non observé).
  Aucun délai propre à la commande (la primitive AKM n'en prend pas) : le délai de commande de la session s'applique.
- **Effets** (052) : plages de la table 25 imposées (alors que la spec dit que le sampler juge) ; règle EB20 appliquée plus strictement que le simulateur ; `none` n'est pas proposé comme type à poser ;
  valeurs brutes (une vitesse de 15 vaut 1,5).
- **Touches** (053) : cinq outils tous déclarés destructifs ; une touche tenue est relâchée à la fermeture ordonnée de la session (pas si le serveur est tué) ; les réponses répètent que le sampler
  « ne fait que mettre en file ».
- **Deux anciens tests changés** (050) : `SampleToolsTests.cpp` et `MultiToolsTests.cpp` affirmaient l'absence de `delete_all_*` ; `RQ-MCP-051` l'exige maintenant.
- **`AGENTS.md` ne donne aucun compte d'outils** (054) : il avait été allégé ; les comptes sont dans les deux README, vérifiés par `ctest`.

## 8. Pièges rencontrés — à ne pas refaire

- Lire le fichier d'instructions AGNOS **en entier** (partiel au début : session.yaml, `unit_tests`, index, liste des conflits oubliés).
- Écrire d'abord les tests et les voir échouer ; **recalculer à la main les comptes** attendus avant de lancer (4 des 6 « erreurs de récupération » de la session étaient des attentes
  de test fausses : nombre de lectures après un `set`, un `rotary speaker` avec espace accepté, etc.).
- Ne pas laisser un README devenir faux : la section « Sécurité » de `juce/mcp/README.md` affirmait que tout ce qui a été ajouté était « jamais fait ».
- Les commandes `rtk` + redirection : si une sortie est vide alors qu'elle ne devrait pas l'être, relancer avec `rtk proxy`.
- `explain(...)` : son quatrième argument ajoute « Use select_<objet> first » ; pour un objet dont l'outil porte un autre nom (`select_song_file`) construisez votre propre phrase.
- Une valeur ajoutée à un `enum class` ou à une structure agrégée de test peut casser l'initialisation positionnelle ailleurs : cherchez les `Struct{…}` avant d'insérer un champ.

## 9. Ce qu'il reste à faire, dans l'ordre

1. **TASK-MCP-055 (bloquée : le propriétaire doit être présent)** — exécution réelle des nouveaux outils sur le S5000, un par un, sur des données de test qu'il prépare (voir la liste
   dans la tâche), compte rendu dans `process/2.architecture/OBSERVATIONS-RQ-MCP-012-real-sampler.md`, mise à jour du tableau « ce qui a été essayé » du README et correction du sampler simulé.
   Exceptions : la carte d'effets (il n'en a pas), `clear_sampler_memory` (sauf décision), les touches (seulement des touches inoffensives qu'il observe). Elle doit trancher : extensions des trois genres,
   refus ou non d'un doublon au renommage, réponse à « quel est le courant » quand aucun n'est choisi, effet de Clear Sampler Memory sur les trois listes, durée de cette commande.
   **Ne jamais envoyer** le rafraîchissement de la liste de disques, `clear_sampler_memory`, un `delete_all_*` ni une touche au sampler du propriétaire sans son accord (`AGENTS.md`).
2. Revue par le propriétaire des ADR-MCP-002 à 005 (toujours « Proposed ») et des choix de la section 7.
3. Boucle d'amélioration du process proposée en fin de session, **non lancée** : il faut que le propriétaire donne le nombre maximal de lignes à ajouter. Le thème « lire le fichier en entier » est
   réglé par le renvoi ajouté à `AGENTS.md` ; restent deux thèmes — recalculer à la main les comptes d'un test avant de le lancer ; consigner dans `Assumptions` dès la planification tout écart entre un critère et le
   simulateur ou la spec.
4. Non vérifié : la bibliothèque AKM couvre-t-elle toutes les fonctions de la spec, et les outils toutes celles de la bibliothèque ? Aucune comparaison point par point n'a été faite
   (voir TASK-MCP-054, hypothèse 4). Le README ne prétend pas le contraire.
5. **`CHANGELOG.md` est périmé et contient des affirmations devenues fausses** (vérifié le 2026-10-09) : sa section `[Unreleased]` décrit « 32 tools without any option » et dit que le serveur
   « never deletes everything, never clears the memory, never formats or ejects ». Elle n'a pas été touchée par cette session, alors que `AGENTS.md` demande d'y noter les changements visibles. À mettre à jour
   (56 / 72 / 61 / 77 outils, Delete ALL et Clear Sampler Memory désormais offerts avec leur `confirm` compté, les touches derrière `--allow-front-panel`, l'éjection et le formatage toujours exclus).
6. Hors périmètre de cette branche mais noté : le hook `rtk` de `settings.json` reste à installer par le propriétaire.

## 10. Références

`process/3.plan/PLAN-MCP-005-remaining-sampler-functions.md` (tâches, `Verification`, `Assumptions`) · `process/1.requirements/FTR-MCP-005-remaining-sampler-functions.md` (RQ-MCP-046 à 057) ·
`process/2.architecture/ADR-MCP-005-remaining-sampler-functions.md` (DEC-MCP-028 à 034) · `process/3.plan/CHECKPOINT-MCP-2026-10-08.md` · `process/_sessionstate/METRICS_LOG.md` (ligne du 2026-10-08) ·
`juce/mcp/README.md` (référence utilisateur) · `AGENTS.md` (consignes projet).
