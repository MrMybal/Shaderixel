# Créer des effets et presets réutilisables

Un effet est un shader .slang ; une fonction de graphe est du JSON .sxfn. Les formats diffèrent même si leur édition est visuelle.

![Éditeur de code et paramètres exposés du shader.](media/shader_editor_fr.png)

## Shaders

Le nom du fichier .slang est son identifiant stable : minuscules, chiffres et soulignements. Traduire les libellés ne doit pas changer cette identité.

Définissez float4 Effect(EffectContext ctx). Les métadonnées //@name, //@category et //@param décrivent nom et réglages. //@generator déclare Draw ; //@blend une fusion. Lire les voisins exige un //@halo adapté au rendu par tuiles.

## Fonctions

subinput/suboutput définissent les broches ; les variables exposées deviennent des contrôles par instance. kind vaut subgraph pour le direct et tool pour les événements/actions.

Tous les effets référencés doivent exister. Les entrées suivent l'ordre réel, images puis paramètres en broches. Choisissez le bon port des sorties multiples et évitez les cycles.

Enregistrez un exemple créé dans l'éditeur avant d'éditer son JSON pour conserver le schéma actuel.

## Emplacements et vérification

Les fonctions source sont dans library/functions ; installées dans functions ; utilisateur dans %LOCALAPPDATA%\Shaderixel\Functions. Les effets sont dans shaders/effects. Modifiez une copie des effets livrés protégés.

Shaderixel.exe --test-effects compile les effets puis se ferme. Examinez [test-effects] dans shaderixel.log. Compiler un shader ne prouve pas le câblage ou l'export d'un outil.

docs/PRESETS.md décrit le schéma détaillé ; docs/PLUGINS.md les API. La référence embarquée décrit les réglages livrés, pas les modifications d'un document.

[Éditeur d'effets](#effect_editor)
[Automatisation](#automation)
