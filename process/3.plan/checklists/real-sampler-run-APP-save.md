# Essai sur le S5000 — sauvegarde sur disque (session APP, 2026-11-01)

Cochez `[x]` au fur et à mesure. Sous un point, écrivez ce que vous voyez après `Remarque :` (une valeur, un écart, « OK »).
Chaque point a un code (S3, N2…) : citez-le pour qu'on en parle.

Tâches concernées : TASK-APP-055 à 061 (PLAN-APP-007, ADR-APP-005). Exigences : RQ-APP-043 à RQ-APP-047.

## 0. Avant de commencer

- [ ] **P1** Un disque MS-DOS ou FAT32 **de test**, avec un dossier vide, par exemple `XS56KTEST`. Rien d'important sur ce disque.
- [ ] **P2** En mémoire : un programme de test qui utilise 1 ou 2 samples, et rien d'autre d'important.
- [ ] **P3** Dans le navigateur de l'éditeur : choisir le disque et ouvrir `XS56KTEST`. Ne **pas** envoyer le refresh des disques.

Remarque :

## 1. Accès (TASK-APP-060)

- [ ] **A1** Sans disque choisi, « File > Save… » est grisé, et Ctrl+S affiche la raison dans la barre d'état.
- [ ] **A2** Avec le disque choisi, « File > Save… » ouvre la boîte de dialogue. Elle propose l'élément sélectionné (ou le programme courant si rien n'est sélectionné) et affiche le disque et le dossier.
- [ ] **A3** Le clic droit sur un élément de la vue mémoire, puis « Save… », ouvre la même boîte sur cet élément.

Remarque :

## 2. Sauvegarder (TASK-APP-056, 057, 059)

- [ ] **S1** Le programme seul : le fichier `.AKP` apparaît dans `XS56KTEST`, et le compte rendu dit « Saved to … : NOM.AKP ».
- [ ] **S2** Le programme avec ses dépendances : les `.WAV` de ses samples apparaissent aussi et le compte rendu les cite.
- [ ] **S3** Tous les samples : un seul envoi, tous les `.WAV` apparaissent.
- [ ] **S4** Toute la mémoire : les fichiers de chaque type présent apparaissent. Notez la durée.
- [ ] **S5** Sur l'écran du sampler, les noms des fichiers sont ceux que l'éditeur affiche.

Remarque :

## 3. Check names (TASK-APP-055, 058) — le point le plus important

- [ ] **N1** Avec check names coché, sauvegarder de nouveau le programme seul : la question « Replace the file? » cite `NOM.AKP`.
- [ ] **N2** Répondre **Skip** : rien n'est envoyé, le fichier garde sa taille et sa date, et le compte rendu le dit « Skipped ».
- [ ] **N3** Répondre **Cancel** : rien n'est envoyé.
- [ ] **N4** Répondre **Replace** : le fichier est remplacé.
- [ ] **N5** **Inconnu à ce jour** : tous les samples de nouveau, avec check names coché, puis répondre **Skip**. On envoie alors le drapeau de la spec « skip if file exists ». Notez ce que fait le S5000 : les fichiers sont-ils passés sans erreur, ou y a-t-il une erreur, et laquelle (texte de la barre d'état) ? Le simulateur suppose une erreur, la spec dit « skip ».
- [ ] **N6** Même chose avec le programme **avec ses dépendances**, quand les `.WAV` existent déjà mais pas le `.AKP` (supprimez le `.AKP` d'abord). Aucune question ne doit être posée : le `.AKP` est écrit et les `.WAV` sont passés, ou bien une erreur s'affiche. Notez ce qui se passe.
- [ ] **N7** Avec check names décoché : aucune question, les fichiers sont remplacés. À la réouverture de la boîte, la case est toujours décochée.

Remarque :

## 4. Disque non MS-DOS (TASK-APP-056)

- [ ] **D1** Si vous avez un disque Akai, un CD ou un autre format : l'éditeur refuse et dit que le sampler ne sauvegarde que sur un disque MS-DOS. Rien n'est envoyé.

Remarque :
