# Réglages, espaces de travail et mises à jour

Shaderixel distingue réglages du document, préférences de l’application et ressources de bibliothèque. Enregistrer un projet ne copie pas toutes les préférences ni ses ressources externes.

## Interface et lecture

Affichage > Langue choisit la langue de l’interface ; une configuration neuve utilise l’anglais. Affichage > Thème choisit palette et densité confortable/compacte. Le manuel suit l’interface ou une langue indépendante pour la session. Sa Taille du texte change la lecture sans modifier l’échelle de l’éditeur.

Fenêtre contrôle les panneaux et espaces enregistrés. Ouvrez Peinture pour les pinceaux, 3D pour matériaux/rendu et IA pour les fournisseurs. Un panneau ouvert peut être caché derrière un onglet. Rétablissez l’onglet ou l’espace avant de le croire fermé. Un filtre de panneau peut aussi cacher les réglages jusqu’à son effacement.

## Mémoire de l’historique

Les réglages de mémoire ont trois budgets : retouches récentes sur GPU pour annulation rapide, maximum d’historique GPU et anciennes retouches compressées en RAM. Auto les déduit du matériel. Désactivez Auto sur une ligne pour saisir un budget manuel ; les limites affichées en Go sont enregistrées avec les préférences.

Réduire les budgets peut supprimer d’anciennes étapes d’annulation. Ce n’est pas une sauvegarde sur disque. Gardez des versions natives avant les grandes modifications et ne consacrez pas toute la VRAM à l’historique : surfaces, calculs intermédiaires, pinceaux et rendus en ont aussi besoin.

## Tablette et valeurs numériques

Les réglages tablette choisissent Windows Ink ou Wintab et la courbe de pression. Si Wintab ne s’initialise pas, Shaderixel utilise Windows Ink. Testez avec le périphérique réel ; un shader peut ignorer les canaux fournis. Les flottants modifiables acceptent point et virgule. Les entiers restent entiers et les contrôles désactivés ne se modifient pas.

## IA et pont local

Assistant IA configure exécutable/script Python compagnon, connecteur et modèle d’assistant. Le fournisseur d’images possède ses propres réglages de point d’accès, modèle et nom de variable d’environnement de clé. ComfyUI possède serveur, chemin de workflow, checkpoint, prompt négatif et échantillonnage. Ces configurations sont indépendantes.

Un champ de clé demandant une variable d’environnement conserve son nom ; configurez la vraie variable dans l’environnement de lancement. Ne placez pas de secret dans un workflow exporté, un nom de fichier ou une capture. L’autorisation d’écriture du pont contrôle les modifications des clients ; une connexion seule ne démarre pas un modèle de génération.

## Fichiers conservés et bibliothèques utilisateur

- shaderixel_settings.json près de l’exécutable conserve préférences d’interface, tablette, mises à jour, historique et IA. L’écriture passe par un fichier temporaire avant remplacement.
- shaderixel_layout_v12.ini près de l’exécutable conserve les panneaux ; les espaces nommés sont sous layouts/.
- shaderixel.log près de l’exécutable contient les diagnostics de l’exécution.
- Functions, Templates, SmartMaterials et Plugins utilisateur sont dans leurs sous-dossiers de %LOCALAPPDATA%\Shaderixel\. Les ressources livrées sont près de l’exécutable.
- Les téléchargements de mises à jour utilisent %LOCALAPPDATA%\Shaderixel\Updates.

Avant transfert ou réinitialisation, fermez l’application et copiez les fichiers en lieu sûr. Préservez séparément les bibliothèques. Renommez un fichier de réglages/disposition sauvegardé pour repartir à neuf puis restaurez le nécessaire. Réinitialiser les panneaux ne répare pas un graphe ni une ressource absente.

## États de mise à jour

1. Ouvrez Aide > Rechercher des mises à jour. Vérifier au démarrage et Inclure les versions alpha et préliminaires sont des préférences persistantes.
2. Vérifier maintenant contacte le canal GitHub public configuré. Un résultat à jour diffère d’une erreur réseau ou de vérification.
3. Télécharger récupère l’installateur adapté et le vérifie. Annuler le téléchargement arrête le transfert sans installer un fichier partiel.
4. Installateur vérifié et prêt active Installer et redémarrer. Shaderixel vérifie à nouveau puis suit le dialogue habituel enregistrer/fermer/annuler des documents.
5. Terminez l’installation et rouvrez l’application. Vérifiez la version dans À propos. Annuler la fermeture d’un travail modifié garde l’éditeur ouvert.

La vérification au démarrage n’installe pas sans interaction. Hors ligne, la version actuelle reste utilisable et la vérification peut être relancée. Les tout premiers installateurs sans mise à jour nécessitent un premier remplacement manuel.

[Dépannage et récupération](#recovery)
[Configuration IA](#ai)
[Répertoire des panneaux](#panels)
