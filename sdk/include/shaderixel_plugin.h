/*
 * Shaderixel plugin SDK (C ABI, any compiler able to build a Windows x64 DLL).
 *
 * A native plugin is a DLL in a folder of the plugins directory
 * (<Shaderixel>/plugins/<Name>/ or %LOCALAPPDATA%/Shaderixel/Plugins/<Name>/) with a
 * plugin.json manifest:
 *
 *   { "id": "studio.example", "name": "Exemple", "version": "1.0",
 *     "description": "...", "dll": "example.dll" }
 *
 * The DLL exports one function:
 *
 *   SX_PLUGIN_EXPORT int sx_plugin_load(const SxHost* host, SxPlugin* plugin);
 *
 * called once on the main thread. It checks host->api_version, fills `plugin` (optional
 * unload callback) and registers what it brings: menu commands, dockable windows (panels),
 * canvas tools and operations. Everything runs on the main thread, between two frames.
 *
 * The plugin drives the document through the host's operations (the same ones the AI
 * assistant and the action editor use: shaderixel.document_info, layer_add, filter_add,
 * fill, paint_stroke, render, undo...): host->call(). The list with their JSON parameters is
 * returned by host->operations_json(). A plugin may register operations of its own: they
 * become available to the assistant (MCP), to actions and to other plugins.
 *
 * Strings are UTF-8. Strings passed to the host are copied; strings returned by the host
 * stay valid until the next host call.
 */
#ifndef SHADERIXEL_PLUGIN_H
#define SHADERIXEL_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#define SX_PLUGIN_API_VERSION 2  /* 2: pixel access, active layer, tools in the 3D view */

#if defined(_WIN32)
#define SX_PLUGIN_EXPORT __declspec(dllexport)
#else
#define SX_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

typedef void (*SxCommandFn)(void* user);
typedef void (*SxPanelFn)(void* user);  /* draws the window contents with host->ui (window already open) */
typedef void (*SxResultFn)(void* user, int ok, const char* result_json);  /* ok: 1, else result_json = {"error": "..."} */
/* An operation of the plugin: parameters as JSON, returns a JSON result (kept by the plugin until the next call). */
typedef const char* (*SxOperationFn)(void* user, const char* params_json);

/* Canvas tool events, in document pixels. */
enum SxToolEventType { SX_TOOL_PRESS = 1, SX_TOOL_DRAG = 2, SX_TOOL_RELEASE = 3, SX_TOOL_HOVER = 4, SX_TOOL_DRAW = 5 };
enum SxModifiers { SX_MOD_SHIFT = 1, SX_MOD_CTRL = 2, SX_MOD_ALT = 4 };
typedef struct SxToolEvent {
    int type;          /* SxToolEventType; SX_TOOL_DRAW: draw the overlay (host->draw_*), every frame the tool is active */
    float x, y;        /* document pixels */
    float pressure;    /* 0..1 (tablet), 1 with a mouse */
    int modifiers;     /* SxModifiers */
    float zoom;        /* screen pixels per document pixel */
    int view;          /* API 2: 0 canvas, 1 3D view (x, y = texture pixel under the cursor, -1 off the model; no overlay) */
} SxToolEvent;
typedef void (*SxToolFn)(void* user, const SxToolEvent* e);

/* Pixels (API 2). */
enum SxPixelFormat {
    SX_PIXELS_RGBA8 = 0,   /* 4 bytes per pixel, sRGB, straight alpha (like a PNG) */
    SX_PIXELS_RGBA32F = 1  /* 4 floats per pixel, linear, premultiplied alpha (as the layers are stored) */
};
typedef void (*SxPixelsFn)(void* user, int ok, int width, int height, const void* pixels);  /* pixels valid during the call */

/* Immediate-mode widgets, inside a panel callback. Return 1 when the value changed / the button was pressed. */
typedef struct SxUi {
    void (*text)(const char* text);
    void (*text_disabled)(const char* text);
    void (*text_wrapped)(const char* text);
    int (*button)(const char* label);
    int (*checkbox)(const char* label, int* value);
    int (*slider_float)(const char* label, float* value, float min, float max, const char* format);
    int (*slider_int)(const char* label, int* value, int min, int max);
    int (*input_text)(const char* label, char* buffer, int buffer_size);
    int (*color_edit)(const char* label, float rgba[4]);
    int (*combo)(const char* label, int* current, const char* items_zero_separated); /* "A\0B\0C\0" */
    void (*same_line)(void);
    void (*separator)(const char* title); /* title may be NULL */
    void (*tooltip)(const char* text);     /* for the previous widget, when hovered */
    void (*push_id)(int id);
    void (*pop_id)(void);
    int (*collapsing_header)(const char* label);
    float (*content_width)(void);          /* available width, pixels */
    void (*set_next_width)(float pixels);  /* width of the next widget (negative: from the right edge) */
} SxUi;

typedef struct SxHost {
    int api_version;               /* SX_PLUGIN_API_VERSION of the host */
    const char* app_version;
    const SxUi* ui;

    void (*log)(const char* message);    /* shaderixel.log */
    void (*status)(const char* message); /* status bar */

    /* Registration (during sx_plugin_load, or later). */
    void (*add_command)(const char* menu_path, SxCommandFn fn, void* user);   /* "Mon plugin/Faire quelque chose" under the Plugins menu */
    void (*add_panel)(const char* title, SxPanelFn fn, void* user);           /* dockable window, listed in Plugins > Fenêtres */
    void (*add_tool)(const char* name, const char* help, SxToolFn fn, void* user);  /* canvas tool, listed in Plugins > Outils and the toolbar */
    void (*add_operation)(const char* id, const char* description, const char* input_schema_json, int writes, SxOperationFn fn, void* user);

    /* Operations (host and plugins): the result comes through `done` (now, or a few frames later). */
    void (*call)(const char* operation, const char* params_json, SxResultFn done, void* user);
    const char* (*operations_json)(void);    /* [{"id","description","input"}...] */

    /* Active document. */
    int (*document_size)(int* width, int* height);   /* 0 without a document */
    const char* (*workspace_dir)(void);              /* folder for the plugin's temporary files */
    void (*open_panel)(const char* title, int open);  /* show / hide a plugin window */
    void (*select_tool)(const char* name);

    /* Tool overlay (SX_TOOL_DRAW), document pixels; color 0xAABBGGRR. */
    void (*draw_line)(float x0, float y0, float x1, float y1, unsigned color, float thickness);
    void (*draw_circle)(float x, float y, float radius, unsigned color, float thickness);
    void (*draw_rect)(float x0, float y0, float x1, float y1, unsigned color, float thickness);
    void (*draw_text)(float x, float y, unsigned color, const char* text);

    /* Advanced, optional: the host's Dear ImGui context, for plugins built against the same ImGui
       version (host->imgui_version); with it a panel may use ImGui directly (ImGui::SetCurrentContext). */
    void* (*imgui_context)(void);
    const char* imgui_version;

    /* ---- API 2 (check host->api_version >= 2) ---- */
    unsigned (*active_layer)(void);  /* id of the active layer (0: none) */
    /* Reads a rectangle of a pixel layer (layer 0: the active paint target; w or h <= 0: the whole layer).
       The result comes through `done` a frame later. */
    void (*pixels_read)(unsigned layer, int x, int y, int w, int h, int format, SxPixelsFn done, void* user);
    /* Writes a rectangle of a pixel layer (one undo step named undo_label). Returns 1 on success. */
    int (*pixels_write)(unsigned layer, int x, int y, int w, int h, int format, const void* pixels, const char* undo_label);
} SxHost;

typedef struct SxPlugin {
    void (*unload)(void* user); /* optional: before the DLL is freed (reload, exit) */
    void* user;
} SxPlugin;

typedef int (*SxPluginLoadFn)(const SxHost* host, SxPlugin* plugin);

#ifdef __cplusplus
}
#endif
#endif /* SHADERIXEL_PLUGIN_H */
