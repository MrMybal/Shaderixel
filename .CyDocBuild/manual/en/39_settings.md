---
id: settings
title: Settings, workspaces and updates
description: Shaderixel keeps document settings, application preferences and library resources in different places.
section: Handbook
order: 38
parent: workspace
revision: 238818a8c222359e
---

Shaderixel keeps document settings, application preferences and library resources in different places. Saving a project does not copy every application preference or external asset.

## Interface and reading {#interface-and-reading}

View > Language selects the interface language; a fresh configuration defaults to English. View > Theme selects the palette and comfortable/compact density. Application help links use the interface language. The CyDoc language selector can switch English/French independently, and browser zoom changes reading size without changing the editor scale. Reading positions and bookmarks use local storage when available.

Window controls dockable panels and saved workspaces. Show the Painting group for brushes, 3D for material/render work and AI for provider settings. A panel can be open but hidden behind another tab. Restore the appropriate tab or workspace before assuming it has closed. Panel filters also hide matching controls until cleared.

## History memory {#history-memory}

The History memory controls have three budgets: recent edits kept on the GPU for fast undo, maximum GPU history, and compressed older history in system RAM. Auto derives budgets from available hardware. Disable Auto on a row to enter a manual budget; limits are shown in GB and saved as application settings.

Reducing these budgets can discard old undo steps. They are not a disk backup system. Keep saved native versions before large changes, and do not assign all VRAM to history: document surfaces, shader intermediates, brushes and renders need memory too.

## Tablet and numeric controls {#tablet-and-numeric-controls}

Tablet controls select Windows Ink or Wintab and the pressure response curve. If Wintab cannot initialize, Shaderixel uses Windows Ink. Test pressure with the actual device; a shader can ignore channels even when the device supplies them. Dot and comma decimals are accepted in editable floating-point controls. Integer controls stay integer, and disabled controls cannot be edited.

## AI and local bridge {#ai-and-local-bridge}

In AI Assistant, select a chat provider and model. The local MCP server, its permissions and saved conversations have their own controls. The image provider and ComfyUI each keep their own connection and generation settings.

In MCP server, select the access profile. Free is the default and uses the local address alone. Local controlled uses the address and checks editing and file access permissions. Token only requires a Bearer token and allows all exposed operations. Protected requires a token and checks both permissions. The controlled profiles apply file access permission to generic menu and input automation, because either can open a file command.

When an API-key field asks for an environment variable, enter its name and set its value in the environment used to launch Shaderixel. Keep the value out of exported workflows, filenames and screenshots.

## Saved files and user libraries {#saved-files-and-user-libraries}

- shaderixel_settings.json beside the executable stores interface, tablet, update, history and AI preferences. Settings writes use a temporary file before replacement.
- shaderixel_layout_v12.ini beside the executable stores the current docking layout; named workspace layouts are under layouts/.
- shaderixel.log beside the executable contains diagnostics for that run.
- User Functions, Templates, SmartMaterials and Plugins are under %LOCALAPPDATA%\Shaderixel\ in their corresponding folders. Shipped resources live beside the executable.
- Update downloads use %LOCALAPPDATA%\Shaderixel\Updates.

Before transferring or resetting settings, close the app and copy the existing files somewhere safe. Preserve user libraries separately. Rename a backed-up layout/settings file to start fresh, then restore only what is needed. Resetting layout does not repair a document graph or missing source asset.

## Update states {#update-states}

1. Open Help > Check for updates. Check at startup and Include alpha and prerelease versions are persistent preferences.
2. Check now contacts the configured public GitHub release channel. A successful up-to-date result differs from a network or verification error.
3. Download update fetches the matching installer and verifies it. Cancel download stops that transfer; it does not install a partial file.
4. Verified installer ready enables Install and restart. Shaderixel verifies again, then uses the normal save/discard/cancel path for open documents.
5. Complete the installer and reopen the app. Check its version in About. Choosing Cancel while closing unsaved work keeps the editor open.

Checking at startup does not mean the app installs updates without interaction. An offline machine can keep using its current version and retry later. Very early installers without the updater require one manual upgrade to a later build.

[Troubleshooting and recovery](doc:recovery)
[AI configuration](doc:ai)
[Panel directory](doc:panels)
