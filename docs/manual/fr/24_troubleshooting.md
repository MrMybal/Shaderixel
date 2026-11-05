# Dépannage, mises à jour et assistance

[Diagnostic, récupération et performances détaillés](#recovery)
[Réglages et mises à jour vérifiées](#settings)

Gardez une copie éditable. Vérifiez vue et cible avant de modifier la source.

## Image vide ou contrôles absents

- Vérifiez visibilité, opacité, masque, écrêtage et fusion.
- Revenez à Image finale dans Mode de vue.
- Connectez du contenu à la sortie du graphe.
- Peignez le calque vide ou vérifiez l'alpha du générateur.
- Un .sxfn est un nœud, pas un exemple complet ; ouvrez le .sxl.
- Effacez les filtres de panneau si des contrôles sont cachés.

## Peinture et shaders

La cible doit être déverrouillée et peignable. Cliquez la vignette de l'image pour peindre la couleur, pas le masque. Retirez une sélection involontaire. Convertissez en pixels le texte/procédural/forme/objet dynamique si nécessaire.

Consultez Édition > Erreurs de shaders et shaderixel.log. Un effet renommé/supprimé casse ses références. Une lecture de voisins exige un halo ; les champs de données exigent précision et interprétation adaptées.

## IA et mises à jour

Vérifiez serveur, adresse, modèle et workflow API. Voir l'entrée n'est pas voir un résultat calculé.

Aide > Rechercher des mises à jour consulte GitHub. Le panneau règle la recherche au démarrage et l'inclusion des préversions. Le téléchargement est vérifié avant Installer et redémarrer. Les documents modifiés passent par enregistrer/abandonner/annuler ; annuler garde l'éditeur ouvert. Une recherche hors ligne peut être relancée.

Le premier installateur du 2026-11-03 n'a pas de mise à jour intégrée ; installez une alpha ultérieure une fois. Le canal public n'exige pas de compte GitHub.

## Assistance et aide hors ligne

Indiquez version, D3D12/Vulkan, GPU/pilote, étapes et projet minimal. Vérifiez les informations privées avant de partager journaux/captures. Dépôt : github.com/MrMybal/Shaderixel.

Le lecteur CyDoc, les textes, scripts et médias sont distribués ensemble dans docs/manual/. Conservez ce dossier complet près de l’exécutable. Réparez ou réinstallez Shaderixel si les fichiers d’aide manquent. La lecture ne demande aucune connexion Internet.

[Prise en main](#getting_started)
[IA](#ai)
