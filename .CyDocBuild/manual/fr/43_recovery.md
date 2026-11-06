---
id: recovery
title: Diagnostiquer, récupérer et gérer les performances
description: Commencez par le contrôle le plus simple qui distingue les causes probables.
section: Manuel
order: 42
parent: troubleshooting
sourceRevision: 24d1f4d8bc1febe1
---

Commencez par le contrôle le plus simple qui distingue les causes probables. Préservez le fichier natif avant les essais destructifs. Annulation, instantanés, exports et versions natives ont des rôles différents.

```image
{"id": "recovery", "src": "assets/manual/recovery_fr.png", "alt": "Vérifier vue, cible, connexions et résultats conservés avant export", "controls": "minimal"}
```

## Image vide ou inattendue {#empty-or-unexpected-image}

1. Choisissez Image finale, une surimpression normale et le bon canal de matériau.
2. Inspectez visibilité, opacité, fusion, Blend If, écrêtage, masques et limites du canevas.
3. En mode nodal, remontez depuis la sortie choisie vers une image non vide. Vérifiez ports multiples et entrées optionnelles.
4. Distinguez ressource de nœud .sxfn et exemple complet .sxl. Une surface dans une fonction sans pixels démarre vide.
5. Un outil peut montrer l’entrée directe avant exécution. Lancez l’événement pour conserver un résultat, puis relancez après modification ou effacez les résultats pour revenir au direct.

## Peinture ou transformation inactive {#nothing-paints-or-transforms}

Vérifiez cible déverrouillée, vignette image/masque, sélection, couleur, opacité/débit et vue. Convertissez le contenu procédural en pixels ou peignez sur un nouveau calque. Dimensions géométriques d’une forme, cadre transformé d’un calque et canevas sont distincts. Les champs de pixels acceptent une saisie décimale ; les dimensions d’une image matricielle finissent entières.

Si la pression est constante, contrôlez backend/pilote puis un pinceau qui l’utilise réellement. Une souris ne fournit pas tous les canaux. Un réglage numérique inactif peut être désactivé par Auto, une liaison ou le mode du document.

## Compilation ou ressources absentes {#shader-compilation-or-missing-resources}

Ouvrez Édition > Erreurs de shaders et shaderixel.log. Vérifiez identifiant stable, types, syntaxe et nombre d’entrées. Recharger un fichier renommé ne répare pas ses anciennes références. Une fonction d’un autre poste peut dépendre d’un effet utilisateur ou d’une texture externe.

En cas de coutures entre tuiles, vérifiez voisinage, UV et halo. Pour les franges sombres, vérifiez la prémultiplication. Pour des UV/SDF étranges, choisissez la précision flottante et évitez transfert colorimétrique ou bornage involontaires. Corrigez les erreurs de compilation avant de comparer aperçu et export.

## Mémoire et lenteur {#memory-and-slow-work}

Le stockage croît comme largeur × hauteur × canaux × octets par composante, multiplié par calques, masques, intermédiaires et résultats conservés. Doubler chaque dimension produit environ quatre fois plus de pixels. L’historique GPU partage aussi la VRAM.

Réduisez aperçu, rayons coûteux, taille/densité des touches, échantillons de rendu ou résolution de simulation pendant les réglages. Gardez les exigences finales pour l’export. Les tuiles limitent certaines allocations mais ne rendent pas bon marché tout shader lisant l’image entière. Utilisez Auto ou des budgets d’historique raisonnables ; les réduire peut oublier les anciennes étapes.

Effacez les résultats obsolètes pour revenir au calcul direct. Les sources IA/upscale complètes peuvent dépasser l’aperçu. Le rendu CPU est une alternative de présentation, pas une commande exécutant tous les shaders image sur le processeur.

## Récupération des fichiers et de l’environnement {#file-and-environment-recovery}

- Enregistrez une nouvelle version native avant UV, fusion, aplatissement ou réinitialisation complexes.
- Annuler une fermeture/mise à jour garde le travail ouvert ; un export terminé ne prouve pas l’enregistrement du projet.
- Restaurez/reliez ou incorporez une ressource liée absente. Réinitialiser les panneaux ne restaure pas des pixels.
- Fermez l’application avant de renommer réglages/disposition sauvegardés. Préservez séparément projets et bibliothèques.
- Une requête IA échouée peut laisser l’entrée visible ; contrôlez serveur, modèle, JSON API et messages avant d’exporter comme résultat fini.

## Rapport de problème utile {#a-useful-bug-report}

Indiquez version, GPU/backend/pilote, dimensions/précision, étapes exactes, résultat attendu/obtenu et lignes utiles du journal. Réduisez une copie au plus petit graphe ou empilement en erreur. Incluez les ressources utilisateur nécessaires ou un exemple procédural. Retirez secrets et fichiers privés. Une capture peut masquer la sortie ou la cible choisie : décrivez-les explicitement.

[Réglages et mises à jour](doc:settings)
[Création de shaders](doc:shader-authoring)
[Index de dépannage](doc:troubleshooting)
