"""Copy the bilingual product READMEs and their local illustrations into a package."""
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
README_FILES = ('README.md', 'README.fr.md', 'VERSION.md', 'VERSION.fr.md')
README_IMAGES = ('assets/logo.png', 'website/media/ui-toolkit.webp') + tuple(
    f'website/media/{name}-{language}.webp'
    for name in ('layers', 'effect-lab', 'material')
    for language in ('en', 'fr')
)


def copy_readme(destination):
    destination = Path(destination)
    for relative in README_FILES + README_IMAGES:
        source = ROOT / relative
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        if relative.endswith('.md'):
            # In a package the repository's examples/ documents live in Exemples/2D.
            text = target.read_text(encoding='utf-8')
            text = text.replace('](examples/', '](Exemples/2D/')
            text = text.replace('](docs/ASSISTANT_MCP.fr.md)', '](Assistant-MCP.fr.md)')
            target.write_text(text, encoding='utf-8', newline='\n')
