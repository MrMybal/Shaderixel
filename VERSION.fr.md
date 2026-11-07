# Shaderixel — notes de version

[English](VERSION.md) · [Français](VERSION.fr.md) · [README](README.fr.md)

## 0.1.0-alpha.20261106 · Alpha

Publiée le **2026-11-06** pour **Windows x64**. Cette version expérimentale est
destinée aux essais et aux retours.

[Publication et fichiers SHA-256](https://github.com/MrMybal/Shaderixel/releases/tag/v0.1.0-alpha.20261106)
· [Installeur](https://github.com/MrMybal/Shaderixel/releases/download/v0.1.0-alpha.20261106/Shaderixel-0.1.0-alpha.20261106-windows-x64-setup.exe)
· [ZIP portable](https://github.com/MrMybal/Shaderixel/releases/download/v0.1.0-alpha.20261106/Shaderixel-0.1.0-alpha.20261106-windows-x64.zip)

### Changements de cette alpha

La publication du 2026-11-06 met à jour la documentation CyDoc embarquée et en ligne :

- Éditions anglaise/française complètes, avec identifiants de pages et sections communs.
- Navigation par tâches, recherche en haut de page, thèmes et lecture mémorisée.
- Illustrations locales, galeries et graphes de fonctions interactifs.
- Signatures, paramètres et exemples des API publiques C/Python.
- F1 contextuel et aide des outils, pinceaux, commandes et nœuds sélectionnés.
- Projets guidés, compatibilité des anciens liens et lecteur hors ligne complet.

Les améliorations des alphas précédentes restent incluses : saisie numérique,
dimensions fractionnaires en pixels ou pourcentage, conversion en calques de
pixels modifiables, bibliothèques UI UV/UI Toolkit et mises à jour GitHub avec
vérification de l’installeur. Elles précèdent cette mise à jour documentaire.
Le README et la vitrine peuvent évoluer indépendamment de l’installeur publié ;
une modification de présentation ne crée pas une nouvelle version binaire.

### Problèmes connus et limites documentées

Les notes publiées ne fournissent pas de liste détaillée de bogues. Consultez le
[suivi des problèmes](https://github.com/MrMybal/Shaderixel/issues) pour les défauts
signalés. Les contraintes documentées ci-dessous ne forment pas une liste exhaustive :

- **Plateformes** : Windows x64, à partir de Windows 10 version 2004. D3D12 et
  Vulkan demandent chacun un GPU et un pilote compatibles. Aucun paquet Linux
  ou macOS n’est publié.
- **Projets alpha** : fonctions et formats natifs peuvent évoluer. Conservez
  les originaux modifiables et des exports de livraison séparés.
- **Développement photo** : les formats d’appareil pris en charge dépendent du
  décodeur disponible. Les profils XML d’objectifs sont externes et non livrés.
  Les cartes photo-vers-PBR estiment les propriétés de surface depuis une image.
- **IA** : ComfyUI demande un serveur actif, des modèles installés et des workflows
  API compatibles. Les presets spécialisés de profondeur/PBR exigent un workflow
  fourni ; ils n’installent ni modèles, ni nœuds supplémentaires.
- **Données UV/SDF** : utilisez un document flottant 32 bits et la bonne
  interprétation des données. Les états de texture UI sont des valeurs explicites ;
  les événements des widgets restent gérés par l’interface hôte.
- **Animation** : les images partagent le contenu des calques. Les poses peintes
  distinctes demandent des calques séparés ou des objets dynamiques indépendants.
  La vidéo est échantillonnée à 30 images/s ; le GIF impose aussi ses contraintes
  de durée et de couleur.
- **Unreal Engine** : les sources du connecteur doivent être compilées pour
  l’éditeur cible. Aucun plugin précompilé par version de moteur n’est inclus.
- **Aide hors ligne** : conservez `Documentation.html` et le répertoire `DocData/`
  complet ensemble, dans le dossier `Documentation/` de l’application.

Consultez [le dépannage](https://mrmybal.github.io/Shaderixel/Documentation.html#/fr/troubleshooting),
[la configuration IA](https://mrmybal.github.io/Shaderixel/Documentation.html#/fr/ai),
[l’animation](https://mrmybal.github.io/Shaderixel/Documentation.html#/fr/animation)
et [l’intégration Unreal](https://mrmybal.github.io/Shaderixel/Documentation.html#/fr/unreal-link).

### Mettre à jour et signaler un problème

Utilisez **Aide > Rechercher des mises à jour...** en incluant les versions alpha.
La recherche au démarrage suit la préférence enregistrée ; téléchargement et
installation restent des actions explicites. Le premier installeur du 2026-11-03
demande une première mise à niveau manuelle pour activer les mises à jour.

Signalez les problèmes dans le [suivi de Shaderixel](https://github.com/MrMybal/Shaderixel/issues)
en précisant version, moteur D3D12/Vulkan, GPU/pilote, étapes de reproduction et,
si possible, un projet minimal. Vérifiez les fichiers et journaux avant de les joindre.
