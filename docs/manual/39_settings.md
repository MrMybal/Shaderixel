# Settings, workspaces and updates

Shaderixel keeps document settings, application preferences and library resources in different places. Saving a project does not copy every application preference or external asset.

## Interface and reading

View > Language selects the interface language; a fresh configuration defaults to English. View > Theme selects the palette and comfortable/compact density. The documentation language can follow the interface or use an independent session override. Its Text size changes reading size without changing the editor scale.

Window controls dockable panels and saved workspaces. Show the Painting group for brushes, 3D for material/render work and AI for provider settings. A panel can be open but hidden behind another tab. Restore the appropriate tab or workspace before assuming it has closed. Panel filters also hide matching controls until cleared.

## History memory

The History memory controls have three budgets: recent edits kept on the GPU for fast undo, maximum GPU history, and compressed older history in system RAM. Auto derives budgets from available hardware. Disable Auto on a row to enter a manual budget; limits are shown in GB and saved as application settings.

Reducing these budgets can discard old undo steps. They are not a disk backup system. Keep saved native versions before large changes, and do not assign all VRAM to history: document surfaces, shader intermediates, brushes and renders need memory too.

## Tablet and numeric controls

Tablet controls select Windows Ink or Wintab and the pressure response curve. If Wintab cannot initialize, Shaderixel uses Windows Ink. Test pressure with the actual device; a shader can ignore channels even when the device supplies them. Dot and comma decimals are accepted in editable floating-point controls. Integer controls stay integer, and disabled controls cannot be edited.

## AI and local bridge

The Assistant panel configures the companion Python executable/script, connector and assistant model. The image-generation provider has its own endpoint/model/credential-environment-variable settings. ComfyUI has its own server, workflow path, checkpoint, negative prompt and sampling settings. These configurations are independent.

An API-key field that asks for an environment variable stores its name; configure the actual variable in the environment used to launch the app. Do not put a secret in an exported workflow, filename or screenshot. The bridge's write permission controls client edits; a connection alone does not start an image-generation model.

## Saved files and user libraries

- shaderixel_settings.json beside the executable stores interface, tablet, update, history and AI preferences. Settings writes use a temporary file before replacement.
- shaderixel_layout_v12.ini beside the executable stores the current docking layout; named workspace layouts are under layouts/.
- shaderixel.log beside the executable contains diagnostics for that run.
- User Functions, Templates, SmartMaterials and Plugins are under %LOCALAPPDATA%\Shaderixel\ in their corresponding folders. Shipped resources live beside the executable.
- Update downloads use %LOCALAPPDATA%\Shaderixel\Updates.

Before transferring or resetting settings, close the app and copy the existing files somewhere safe. Preserve user libraries separately. Rename a backed-up layout/settings file to start fresh, then restore only what is needed. Resetting layout does not repair a document graph or missing source asset.

## Update states

1. Open Help > Check for updates. Check at startup and Include alpha and prerelease versions are persistent preferences.
2. Check now contacts the configured public GitHub release channel. A successful up-to-date result differs from a network or verification error.
3. Download update fetches the matching installer and verifies it. Cancel download stops that transfer; it does not install a partial file.
4. Verified installer ready enables Install and restart. Shaderixel verifies again, then uses the normal save/discard/cancel path for open documents.
5. Complete the installer and reopen the app. Check its version in About. Choosing Cancel while closing unsaved work keeps the editor open.

Checking at startup does not mean the app installs updates without interaction. An offline machine can keep using its current version and retry later. Very early installers without the updater require one manual upgrade to a later build.

[Troubleshooting and recovery](#recovery)
[AI configuration](#ai)
[Panel directory](#panels)
