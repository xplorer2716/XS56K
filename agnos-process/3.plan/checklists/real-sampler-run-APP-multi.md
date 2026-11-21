# Essai sur le S5000 — éditeur de multi (session APP, 2026-11-01)

Cochez `[x]` au fur et à mesure. Sous un point, écrivez ce que vous voyez après `Remarque :` (une valeur, un écart, « OK »).
Chaque point a un code (C1, P2…) : citez-le pour qu'on en parle.

Tâches concernées : TASK-APP-062 à 067 (PLAN-APP-008, ADR-APP-006). Exigences : RQ-APP-100 à RQ-APP-104.

## 0. Avant de commencer

- [ ] **P1** En mémoire : deux programmes de test (par exemple PAD et BASS), rien d'important. Aucun multi important en mémoire.
- [ ] **P2** L'éditeur est connecté au S5000.

Remarque :

## 1. Créer un multi (TASK-APP-065)

- [ ] **C1** Clic droit dans la vue mémoire, « Create a multi… », nom `XSTEST`. Le multi apparaît sous « Multis », sélectionné ; l'éditeur de multi s'affiche.
- [ ] **C2** Sur l'écran du S5000 : le multi courant est `XSTEST`. Noter son nombre de parts (32, 64 ou 128) : ___
- [ ] **C3** Recréer `xs-test` : refus avec la raison, rien n'est créé.

Remarque :

## 2. L'onglet « Parts » (TASK-APP-063)

- [ ] **T1** Le sélecteur en haut à droite liste « Part 1 of N » … « Part N of N », puis « All parts ». N est le nombre noté en C2.
- [ ] **T2** Les valeurs de la part 1 (niveau, pan, canal MIDI, notes basse et haute…) sont celles de l'écran MULTI du S5000.
- [ ] **T3** Choisir la part 2, régler le niveau à 70 : la valeur s'affiche, et le S5000 montre 70 pour la part 2, la part 1 inchangée.
- [ ] **T4** Note haute de la part 2 à 60, puis note basse à 72 : refus avec la plage, rien n'est envoyé.
- [ ] **T5** Choisir « All parts », mettre Mute à ON : toutes les parts sont muettes sur le S5000. Remettre à OFF.
- [ ] **T6** Toujours sur « All parts » : le niveau affiche « VARIES » si les parts n'ont pas le même niveau.
- [ ] **T7** Combien de temps prend la lecture avec « All parts » (avec 32 parts ou plus) ? ___ s. Est-ce acceptable ?

Remarque :

## 3. Programme d'une part et numéro de programme (TASK-APP-064)

- [ ] **G1** Part 2 : choisir PAD dans « Program ». Le S5000 montre PAD sur la part 2 ; le sélecteur affiche « Part 2 of N - PAD ».
- [ ] **G2** Part 2 : choisir NONE. La part 2 ne joue plus rien sur le S5000 ; PAD est toujours en mémoire. Aucune question n'est posée.
- [ ] **G3** Sur « All parts », changer le programme : refus « choose one part », rien n'est envoyé.
- [ ] **G4** « Number » (Program change) affiche OFF pour un multi neuf. Régler 5 : le S5000 affiche 5. Revenir à OFF : le S5000 affiche OFF.
- [ ] **G5** Changer sur le S5000 la part 3 vers BASS, puis « Read again » : l'éditeur affiche BASS pour la part 3.

Remarque :

## 4. Fin

- [ ] **F1** Supprimer `XSTEST` depuis la vue mémoire (clic droit, « Delete… », confirmation).

Remarque :
