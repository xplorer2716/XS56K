# Essai sur le S5000 — éditeur de programme (session APP, 2026-10-17)

Cochez `[x]` au fur et à mesure. Sous un point, écrivez ce que vous voyez après `Remarque :` (une valeur, un écart, « OK »).
Chaque point a un code (F3, K5…) : citez-le pour qu'on en parle.

Tâches concernées : TASK-APP-040 à 042, 044 à 049, 051 (PLAN-APP-005 et PLAN-APP-006).

## 0. Avant de commencer

- [ ] **P1** Sauvegarder la mémoire du sampler.
- [ ] **P2** Créer ou charger un programme de test jetable, à 2 keygroups, et un sample de test. Aucune autre donnée.
- [ ] **P3** Ouvrir le programme dans l'éditeur. Ne **pas** envoyer le refresh des disques.
- [ ] **P4** Règle pour chaque essai : noter la valeur d'origine, la changer dans l'éditeur, regarder l'écran du sampler, remettre la valeur d'origine.

Remarque :

## 1. Filter (TASK-APP-045) et forme du type (TASK-APP-051)

- [KO] **F1** Keygroup 1 : Cutoff, Reso, Track et Atten correspondent à l'écran FILTER du sampler: ATTEN: 2026-10-17 21:50:43.835 [ERROR] [CTL] Model.cpp:64: Setting the atten of New Program  1: The sampler did not keep the value: filter attenuation was set to 1 dB but reads 0 dB for keygroup 1: C'est parce que les valeurs possibles sont 0,6,12,28,24,30 uniquement. le slider et les fonctions sous jacentes doivent refleter cela.
- [OK] **F2** Changer Cutoff dans l'éditeur : l'écran du sampler suit.
- [OK] **F3** Passer au keygroup 2 : on voit les valeurs du keygroup 2, pas celles du 1.
- [KO] **F4** Le type de filtre : une forme est dessinée dans la cellule, les flèches précédent et suivant fonctionnent: LE PROBLEME DU CONTROLE EST QUE LES FLECHE OUVRENT UNE DROP DOWN DE CHOIX, mais n'incrémentent ni ne décrementent. (c'est pour tous les controles de ce type dans l'application)
- [KO] **F5** Parcourir les 26 types : les 19 formes dessinées ressemblent à celles du manuel (EDIT PROGRAM, FILTER MODE, pp. 114-118). POUR LES FITRES avec "+" le sampler affiche un petit "+" au dessus de la fréquence de coupure
- [OK] **F6** Les 7 types sans forme montrent leur nom seul : LO<>HI, LO<>BAND, BAND<>HI, BI-NOTCH, BI-PEAK, BI-PHASE, VOWELISER.
- [OK] **F7** Après chaque changement de type, l'éditeur affiche le type que le sampler affiche.

Remarque :

## 2. Keygroup et ses 4 zones (TASK-APP-046)

- [KO] **K1** La grille des 4 colonnes se compare à la page ZONE du sampler : mêmes lignes, mêmes valeurs. => DONNER LES PHOTO
- [ ] **K2** Une zone sans sample affiche NONE.
- [ ] **K3** Choisir un sample pour la zone 2 : le sampler l'affiche, et « Read again » le confirme.
- [ ] **K4** Level, pan, tune, playback, mute et solo d'une zone : un changement chacun, lu sur le sampler.
- [ ] **K5** Vélocité : mettre la basse au-dessus de la haute est refusé ; l'inverse est accepté.
- [ ] **K6** Notes basse et haute (value boxes), semi et fine : un changement chacun, et la même règle d'ordre.
- [ ] **K7** Mute grp, FX, FX send, Xfade zones, Xfade keygroup : un changement chacun.
- [ ] **K8** Choisir NONE pour une zone est refusé (limite connue : la gateway ne sait pas retirer un sample).
- [ ] **K9** La grille est lisible : rien de coupé, rien qui se chevauche.

Remarque :

## 3. Ajout et suppression de keygroups (TASK-APP-047) — sur le programme jetable

- [ ] **A1** Add… : le dialogue s'affiche, avec un champ pour le nombre.
- [ ] **A2** 0 ou une valeur trop grande est refusée, le champ prend la couleur « en attente ». Annuler n'envoie rien.
- [ ] **A3** Add… avec 2 : le sélecteur passe de 2 à 4 et le sampler indique 4 keygroups.
- [ ] **D1** Delete sur le keygroup 3 : la confirmation nomme le keygroup et le programme.
- [ ] **D2** Annuler ne supprime rien.
- [ ] **D3** Confirmer : il reste 3 keygroups, côté éditeur comme côté sampler.
- [ ] **D4** Avec un seul keygroup, Delete est grisé ou refusé.

Remarque :

## 4. LFO (TASK-APP-048)

- [ ] **L1** LFO 1 : rate, delay, depth, wave, sync, Wheel, Touch correspondent à l'écran du sampler.
- [ ] **L2** LFO 2 : rate, delay, depth, wave, retrigger, clock sync, division.
- [ ] **L3** La division (cellule plus large) : les libellés sont lisibles et ce sont ceux du sampler.
- [ ] **L4** Un changement par LFO : le sampler suit.

Remarque :

## 5. Modulation (TASK-APP-049)

- [ ] **M1** Les 7 groupes sont là : Pitch, Filter, Keygroup amp, Program amp, Pan, LFO 1, LFO 2.
- [ ] **M2** Pour chaque groupe, les entrées, les listes de sources et les montants se comparent au sampler.
- [ ] **M3** Les titres « Source » et « Amount » et la portée (program ou keygroup) sont lisibles.
- [ ] **M4** Un montant de keygroup (Filter par exemple), changé sur le keygroup 1, ne bouge pas sur le keygroup 2.
- [ ] **M5** Un montant de programme (Program amp, Pan) est un seul réglage pour tout le programme.
- [ ] **M6** Router AUX ENV vers Filter input 1, montant à 50, puis relire.

Remarque :

## 6. Envelopes (TASK-APP-044, en cours)

- [ ] **E1** Glisser un point d'enveloppe à la souris (Amp, Filter, Aux) : la valeur change sur le sampler.
- [ ] **E2** Changer de keygroup avec le sélecteur : les enveloppes changent.
- [ ] **E3** Les paramètres vélocité, depth et key : les retrouver sur les écrans du sampler et vérifier qu'ils correspondent.
- [ ] **E4** Les clics sur les onglets : Keygroup, Envelopes, Filter, LFO, Modulation, Program.

Remarque :

## 7. Éditeur de sample (TASK-APP-040 à 042)

- [ ] **S1** Sur un sample de test : poser le loop end puis le loop start, relire les deux valeurs.
- [ ] **S2** Un loop end sous le loop start est refusé, et la boîte revient à la valeur du modèle.
- [ ] **S3** Les quatre bornes se bornent entre elles : start, loop start, loop end, end.
- [ ] **S4** Déplacer le loop end : le sampler déplace le loop start en gardant la longueur de boucle (noter ce qu'on voit).

Remarque :

## 8. Infobulle (TASK-APP-022)

- [ ] **T1** Dans le navigateur, laisser le pointeur sur un chemin de dossier ou un nom de fichier long : l'infobulle donne le texte entier.

Remarque :

## 9. Fin

- [ ] **Z1** Toutes les valeurs d'origine sont remises.
- [ ] **Z2** Programme jetable supprimé, ou sauvegarde rechargée.
- [ ] **Z3** Les 7 formes de filtre manquantes (illustrations) m'ont été données, ou sont notées pour plus tard.

Remarque :

## Décisions à prendre (pas besoin du sampler)

- [ ] **O1** L'ordre des onglets : Keygroup, Envelopes, Filter, LFO, Modulation, Program (TASK-APP-002).
- [ ] **O2** Faire le checkpoint de session, puis TASK-APP-050 « tous les keygroups » ? Ou 050 d'abord ?
