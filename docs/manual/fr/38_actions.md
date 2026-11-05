# Actions enregistrées et traitement par lots

Les actions sont des commandes d'édition ordonnées avec paramètres JSON. Elles diffèrent des nœuds d'exécution : une action rejoue des modifications du document ; un événement de graphe lance le flux d'une fonction. La catégorie Commandes décrit les 20 types pris en charge, leurs valeurs initiales et leurs limites.

Aide > Référence des actions ouvre ce guide. Dans l'Éditeur d'actions, sélectionnez une étape puis F1 ou Aide de la commande pour sa fiche. Commandes est aussi un filtre de recherche dans les lecteurs intégré et HTML.

![L'exécution explicite diffère d'une connexion d'image ou d'un aperçu direct.](media/execution_fr.png)

## Enregistrer puis affiner une action

1. Ouvrez Actions ou Éditeur d'actions dans Fenêtre. Créez une action avec un nom descriptif.
2. Lancez Enregistrer, effectuez des commandes prises en charge puis arrêtez. L'enregistrement ajoute des étapes ; il ne filme pas chaque geste.
3. Examinez la liste réelle. Dimensions, rotations, recadrages, filtres, opérations de calque, sélection, remplissage/effacement et opérations compatibles peuvent être représentés. Traits arbitraires, mouvements de pointeur et résultats IA externes ne sont pas automatiquement rejoués sous forme de pixels enregistrés.
4. Sélectionnez une étape pour lire description et paramètres. Les contrôles numériques acceptent la saisie. Paramètres avancés (JSON) applique immédiatement un objet valide ; un texte incomplet ne remplace pas les derniers paramètres valides.
5. Réordonnez, dupliquez ou désactivez les étapes. Copiez une action simple en lecture seule dans Mes actions avant modification.
6. Testez l'action entière sur une copie de travail. Vérifiez dimensions, calque actif, alpha et fichier écrit, pas seulement le message de fin.

Pour un filtre non destructif enregistré, les valeurs numériques ajustées sont finalisées à la prochaine étape enregistrée ou à l'arrêt. Les ressources courbe/LUT ne sont pas des données arbitraires du tableau numérique params. Gardez les effets et ressources utilisateur nécessaires sur l'installation destinataire.

## Ordre d'exécution et essais partiels

Une action entière ignore les étapes désactivées. Exécuter cette étape dans le menu contextuel lance volontairement l'étape même décochée. Exécuter jusqu'ici et Exécuter à partir d'ici respectent normalement les cases des étapes de la plage.

L'exécuteur attend compilation, lecture GPU ou export. Une erreur signalée interrompt les étapes suivantes, mais les modifications et fichiers antérieurs restent. Plusieurs commandes de calque incompatibles affichent plutôt un statut ou ne font rien sans interrompre. Une fin réussie ne prouve donc pas que chaque entrée possédait la pile attendue.

Les cibles dépendent de l'état : Nouveau calque et Dupliquer changent le calque actif ; Sélection limite les remplissages/filtres suivants. Un nom de calque enregistré n'est pas un sélecteur automatique, sauf commande le prévoyant explicitement. L'action entière n'est pas une transaction avec retour arrière automatique.

## Import, export et JSON

L'éditeur importe .sxactions ou JSON et exporte .sxactions. Il accepte un objet contenant un tableau actions ou une action unique contenant steps. Les noms déjà présents sont numérotés. Importer ajoute des actions sans les exécuter.

Cette action complète peut être enregistrée dans un fichier JSON puis importée. Elle adapte à 1024 × 1024 sans agrandir les petites images. Elle n'exporte qu'après ajout d'une étape Exporter ou choix d'une sortie finale de lot :

```json
{
  "name": "Practice - Fit inside 1024",
  "steps": [
    {"cmd":"image_size","enabled":true,"params":{"mode":"fit","w":1024,"h":1024,"enlarge":false,"interp":2}}
  ]
}
```

Chaque étape possède cmd, params et enabled. Identifiants, clés et valeurs d'énumération restent en anglais dans les deux éditions. Ajouter une étape fournit les valeurs d'usine ; un params vide importé utilise les valeurs de repli à l'exécution. Elles ne sont pas toujours identiques : une nouvelle étape Filtre choisit black_white destructif, tandis que destructive absent vaut false à l'exécution.

## Préparer un lot

1. Ouvrez Fichier > Automatisation > Traitement par lots ou la commande de l'Éditeur d'actions.
2. Ajoutez les actions dans l'ordre voulu. Sans action, un format final réalise une conversion simple. Sans action ni sortie finale, il n'y a rien à lancer.
3. Choisissez Dossier ou Documents ouverts. Le dossier accepte sous-dossiers et filtre d'extensions, par exemple png jpg. Documents ouverts utilise ceux présents au démarrage du lot.
4. Réglez format final, destination, motif de nom et gestion des fichiers existants. Une destination distincte accessible facilite l'inspection.
5. Testez sur un petit ensemble. Lancez puis examinez journal, fichiers et compteurs, y compris éléments ignorés/en erreur.

Les entrées de dossier sont ouvertes, traitées puis fermées ; leurs modifications en mémoire ne sont conservées que si une sortie/export les écrit. Documents ouverts fonctionne autrement : les actions modifient directement ces documents avec leur historique et ils restent ouverts.

## Noms, dossiers et conflits

Les jetons restent littéraux quelle que soit la langue :

- {nom} : nom du document/fichier sans extension.
- {n} : numéro d'entrée à partir de un ; {n3} : le même sur trois chiffres.
- {l}, {h} : largeur et hauteur courantes lors de l'évaluation du nom.
- {dossier} : nom du dossier source ; {ext} : extension initiale sans point.
- {date} : date courante au format AAAAMMJJ.

Ainsi {nom}_{n3}_{l}x{h} peut produire brick_001_512x256 après redimensionnement. L'aperçu peut utiliser les tailles initiales avant traitement ; le nom final utilise les dimensions courantes. Les caractères interdits dans un nom sont remplacés par des tirets bas.

Recréer les sous-dossiers s'applique aux entrées de dossier avec récursion et destination fournie. Une destination vide utilise les dossiers source/document ; un document non enregistré utilise le dossier des réglages.

La sortie finale propose Remplacer, Ignorer ou Ajouter un numéro. Si elle désigne le fichier source lui-même, l'implémentation numérote une copie au lieu de remplacer la source. Cette protection et la politique choisie ne couvrent pas les étapes Exporter internes : elles peuvent remplacer directement leur propre destination.

## Sortie finale et étapes Exporter

Format Aucun signifie que seules les étapes Exporter écrivent. Un élément sans telle étape peut correctement indiquer traité (aucun fichier écrit).

Un format final écrit après la chaîne. Si les actions contiennent aussi Exporter, les deux types de sorties peuvent être produits. Destination et nom du lot peuvent remplacer dossier/nom de ces étapes ; leurs suffixes sont toujours ajoutés. Évitez de donner le même nom à l'export interne et à la sortie finale, sauf remplacement volontaire.

Les identifiants de format JSON sont minuscules. SXL conserve la structure native, PSD les calques compatibles ; un raster ordinaire représente la composition visible. Le JPEG n'a pas d'alpha et la transparence restante devient noire. L'étape Aplatir ne fait rien avec un seul calque ; elle ne garantit donc pas à elle seule un fond JPEG blanc.

## Annulation, erreurs et validation

Annuler attend la fin de l'action en cours avant d'arrêter le lot ; cela n'annule ni modifications ni fichiers terminés. Un fichier en échec est journalisé et les entrées suivantes peuvent encore être traitées. Consultez le journal plutôt que considérer le compteur comme preuve du résultat visuel.

En cas de résultat inattendu, vérifiez cible active, sélection, visibilité, identifiants d'effets, dimensions, champs facultatifs, sortie finale et destination réelle. Une copie native est nécessaire pour conserver calques/graphes éditables après une livraison raster.

## Référence des commandes

[Taille de l'image](#command:image_size)
[Adapter et recadrer au centre](#command:fit_crop)
[Taille du canevas](#command:canvas_size)
[Rotation ou symétrie](#command:rotate)
[Rotation d'angle libre](#command:rotate_arbitrary)
[Recadrer selon la sélection](#command:crop_selection)
[Appliquer un filtre](#command:filter)
[Nouveau calque de pixels](#command:new_layer)
[Dupliquer le calque](#command:duplicate_layer)
[Convertir en calque de pixels](#command:rasterize_layer)
[Fusionner vers le bas](#command:merge_down)
[Fusionner les visibles](#command:merge_visible)
[Étape Aplatir](#command:flatten)
[Étape Sélection](#command:select)
[Étape Remplir](#command:fill)
[Étape Effacer](#command:erase)
[Propriétés du calque](#command:layer_props)
[Étape Exporter](#command:export)
[Étape Photo RAW](#command:photo_raw)
[Opération enregistrée](#command:op)

[Fonctions et événements de graphe](#automation)
[Calques et différents types de bake](#tools)
[Export et livraison](#export)
