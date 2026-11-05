# Documents, sauvegarde et modèles

Gardez un original natif pour l’édition et des exports distincts clairement nommés. Enregistrer sous change aussi la base des exports relatifs du graphe. Un exemple d’exercice livré s’ouvre comme copie non enregistrée : Enregistrer demande une destination.

Le mode Calques compose une pile ; le mode Nodal donne les connexions directes. La conversion des calques expose leur graphe généré. Le retour d’un graphe arbitraire vers les calques n’est proposé que si sa structure est représentable ; ne supposez pas toute conversion réversible. Enregistrez une version avant de changer de mode.

Le format natif .sxl conserve un document d'image éditable. Paint 3D utilise .sx3d. Une image aplatie destinée à la livraison ne remplace pas le projet de travail.

## Créer et ouvrir

Fichier > Nouveau choisit taille, précision et mode Calques ou Nodal. Les modèles fournissent une pile ou un graphe préparé. Nouveau projet Paint 3D démarre la peinture de maillage.

Ctrl+O ouvre une image ou un document compatible. Ctrl+Maj+O importe une image comme calque. Un dépôt peut ouvrir un projet, importer une image ou insérer une ressource selon son type et sa destination. Utilisez Fichier > Ouvrir pour charger toute la composition d'un exemple .sxl.

## Conserver le travail éditable

Ctrl+S enregistre ; Ctrl+Maj+S choisit un autre chemin. Le document modifié porte un indicateur ; sa fermeture propose enregistrer, abandonner ou annuler.

Enregistrez avant d'exécuter un outil utilisant des noms d'export relatifs : ils se résolvent à côté du document. Sans document enregistré, ils utilisent exports/ à côté de l'exécutable.

Un objet dynamique lié dépend toujours de son fichier source. Déplacer le document parent ne déplace pas cette source ; incorporez le contenu pour un échange autonome.

## Modèles et précision

Enregistrer comme modèle crée un point de départ réutilisable avec ses métadonnées. Un modèle démarre un document ; une fonction insère un nœud. Les modèles utilisateur sont dans %LOCALAPPDATA%\Shaderixel\Templates, ceux livrés dans templates/ à côté de l'application.

Utilisez 32 bits flottants pour les UV, distances signées et calculs hors de 0 à 1. L'aperçu peut saturer alors que les valeurs restent valides. Un document haute précision ne transforme pas un export PNG 8 bits en export haute précision.

[Export](#export)
[Objets dynamiques](#smart_objects)
