# Framework Runtime Monitor — Detailed Spec

> Spec id: `framework-runtime-monitor`  
> Source proposal: `openspec/changes/framework-runtime-monitor/proposal.md`  
> Targets: Godot 4.7-beta (engine module, not GDExtension)

This document is the single source of truth for the design of the `game_framework` runtime monitor. It is referenced by all sub-capabilities in the change.

---

## 1. Design Principles

### 1.1 Why a runtime monitor (instead of relying on Godot's built-in Debugger)?

Godot 4.7's built-in Debugger (`editor/debugger/`) is excellent for engine-level inspection: Profiler, Network, Audio, Mono, Vulkan. But for **business-level state** (Activity stack, MVVM bindings, Service registry, Resource loading queue), it provides no entry point:

- Activity 状态只能通过 `ActivityManager` 的 6 个只读 API（阶段 10 留下的）+ `debug_overlay.gd` 在游戏场景内 Ctrl+Shift+`` ` 临时打开；
- ServiceRegistry / ResourceManager / BindingEngine 完全没有 UI 入口；
- 文档查看与运行时状态分离，没法交叉跳转；
- 后续 plugin（FsmKit / EventKit / PoolKit / ResKit / IMC 等）各自需要一个调试面板入口，但没有统一注册中心。

参考截图（YokiFrame v2.0 Preview 风格）的三栏布局 + 顶栏 + 左侧菜单：

- **左侧菜单**：plugin 列表（框架 / 文档 / 工具：FsmKit、EventKit、PoolKit、ResKit 等可扩展）
- **顶栏**：连接状态指示 + 主题切换 + 语言切换 + 刷新
- **中部三栏**：活动对象列表 + 当前对象详情 + 状态图 / 矩阵
- **右侧栏**：事件洞察 + 转换历史时间线

### 1.2 Why engine module (not GDExtension)?

- 与 `enhanced_input`（GDExtension）/ `game_framework`（engine module）的现状对齐；
- 已有 `editor_bind_plugin.cpp` 是 `#ifdef TOOLS_ENABLED` 的 EditorInspectorPlugin，模式可复用；
- Editor 窗口需要直接用 `EditorInterface` / `EditorHelp`，engine module 路径更直接。

### 1.3 Why both Editor window AND Runtime Overlay?

- **Editor 窗口**：开发期 F5 调试，方便 — 与 SceneTreeDock / InspectorDock 平级；
- **Runtime Overlay**：release build 现场调试用（很多团队 QA 流程需要 release 包调试）；
- 两者共享 panel 注册表，避免双份代码。

### 1.4 Why panel-per-page (left sidebar) instead of tabs?

截图明确显示左侧菜单 + 三栏布局。tab 切换会让中部三栏（活动对象 / 详情 / 时间线）被压扁，不适合 Activity 这类有"栈 + 详情 + 历史"三要素的视图。

---

## 2. Architecture Overview

### 2.1 Four-layer responsibility

| Layer | Responsibility | Main classes |
|---|---|---|
| **Host** | Window/Overlay container, top bar (connection/refresh/theme/i18n) | `MonitorPlugin`, `MonitorWindow`, `MonitorOverlay` |
| **Panel** | Single feature panel (lifecycle, UI render, throttled refresh) | `MonitorPanel` + 6 built-in panels |
| **Registry** | Static registration table + data source abstraction + factory Callable | `Monitor` singleton |
| **Data Source** | Each module exposes its own snapshot API (read-only side channel) | `MonitorDataSource` interface + managers' `get_snapshot()` |

### 2.2 Module placement

```
modules/game_framework/monitor/
├── monitor.h/cpp                    # Static registry singleton
├── monitor_data_source.h            # Data source abstraction
├── monitor_panel.h/cpp              # Panel abstract base class
├── monitor_window.h/cpp             # Editor main window
├── monitor_plugin.h/cpp             # EditorPlugin
├── monitor_sidebar.h/cpp            # Left navigation
├── monitor_statusbar.h/cpp          # Top bar
├── monitor_overlay.h/cpp            # Runtime overlay (Stage 7)
├── panels/
│   ├── framework_panel.h/cpp        # Application overview
│   ├── activity_panel.h/cpp         # Activity stack visualization
│   ├── service_panel.h/cpp          # ServiceRegistry view
│   ├── resource_panel.h/cpp         # ResourceManager loading state
│   ├── mvvm_panel.h/cpp             # BindingEngine + ObservableProperty
│   └── docs_panel.h/cpp             # doc_classes browser + md viewer
├── doc_classes/                     # Monitor.xml + 6 panel XML
└── tests/
    ├── test_monitor_registry.cpp
    └── test_monitor_panel_lifecycle.cpp
```

### 2.3 Dependency direction

```
MonitorPlugin  ─►  MonitorWindow  ─►  MonitorSidebar / MonitorStatusbar
                       │                       │
                       ▼                       ▼
                  MonitorPanel  ◄──────  Monitor (registry)
                       │
                       ▼
                MonitorDataSource  ◄────  Application → Managers
```

---

## 3. Core Interfaces

### 3.1 `MonitorDataSource` (pure C++ interface)

```cpp
// monitor_data_source.h
class MonitorDataSource {
public:
    virtual ~MonitorDataSource() {}
    virtual Dictionary get_snapshot() const = 0;
};
```

**Not** exposed to GDScript (lives in `<monitor_data_source.h>`, not registered with ClassDB). Each Manager implements this interface:

- `ActivityManager : public MonitorDataSource`
- `ServiceRegistry : public MonitorDataSource`
- `ResourceManager : public MonitorDataSource`
- (Future) `BindingEngine : public MonitorDataSource`

### 3.2 `MonitorPanel` abstract base class

```cpp
// monitor_panel.h
class MonitorPanel : public VBoxContainer {
    GDCLASS(MonitorPanel, VBoxContainer);

protected:
    bool _is_active = false;
    Application *_app = nullptr;

    // GDVIRTUAL hooks — GDScript subclasses override via has_method + call()
    GDVIRTUAL0(_refresh)                          // 30fps throttled
    GDVIRTUAL1(_bind_data_source, Application*)  // called once on app bind
    GDVIRTUAL0(_on_activated)                    // panel first shown
    GDVIRTUAL0(_on_deactivated)                  // panel hidden
    GDVIRTUAL0R(String, _get_panel_id)           // default returns class_name

public:
    void set_active(bool p_active);
    bool is_active() const;

    void bind_data_source(Application *p_app);
    void refresh();
    void on_activated();
    void on_deactivated();
    virtual String get_panel_id() const;  // C++ override; default reads class name

    void _notification(int p_what);  // NOTIFICATION_THEME_CHANGED + NOTIFICATION_PROCESS
};
```

**Duck-typed dispatch** (lesson from Stage 16 ValueConverter):
```cpp
void MonitorPanel::refresh() {
    if (get_script_instance() != nullptr && has_method("_refresh")) {
        call("_refresh");
    }
    // C++ subclasses override _refresh() directly via GDVIRTUAL_CALL
    GDVIRTUAL_CALL(_refresh);
}
```

This avoids the GDScript "override native virtual" warning (Godot 4 forbids that).

### 3.3 `Monitor` singleton

```cpp
// monitor.h
class Monitor : public Object {
    GDCLASS(Monitor, Object);

    struct PanelDescriptor {
        String id;
        String title;
        String icon_path;
        Callable factory;  // returns MonitorPanel instance
    };
    HashMap<String, PanelDescriptor> _panels;

    static Monitor *_singleton;

public:
    static Monitor *get_singleton();

    // C++ registration
    void register_panel(const String &p_id, const String &p_title,
                        const String &p_icon_path, const Callable &p_factory);
    void unregister_panel(const String &p_id);
    bool has_panel(const String &p_id) const;
    PackedStringArray get_registered_panels() const;

    // Factory
    MonitorPanel *create_panel(const String &p_id, Application *p_app);

    // Default panels (registered in initialize_game_framework_module)
    void _register_default_panels();
};
```

**GDScript API** (all bound via ClassDB):
```gdscript
Monitor.register_panel("fsm", "FsmKit", "res://addons/fsm/icon.svg", create_fsm_panel)
Monitor.unregister_panel("fsm")
Monitor.has_panel("fsm")
Monitor.get_registered_panels()
```

GDScript factory example:
```gdscript
# monitor_examples/fsm_panel.gd
func create_fsm_panel() -> MonitorPanel:
    var p = preload("res://monitor_examples/fsm_panel_impl.gd").new()
    return p

# fsm_panel_impl.gd
extends MonitorPanel
func _get_panel_id() -> String: return "fsm"
func _refresh(): pass
```

---

## 4. Window Layout

### 4.1 Top-level structure

```
MonitorWindow : VBoxContainer
├── MonitorStatusbar (HBoxContainer, 32px height)
│   ├── StatusIndicator (HBox: dot + label)
│   ├── HSeparator
│   └── Toolbar (HBox: Refresh, Theme, Locale)
└── HSplitContainer (vertical = true, full expand)
    ├── HSplitContainer (sidebar | content | detail)
    │   ├── MonitorSidebar (VBox: search + ItemList)
    │   ├── ContentPanel (VBoxContainer, swappable)
    │   └── DetailPanel (VBoxContainer, swappable)
```

### 4.2 Sidebar behavior

- Top: search box (filter panels by title)
- Middle: `ItemList` showing panel titles, grouped:
  - "Framework" group: framework_panel
  - "Tools" group: activity_panel, service_panel, resource_panel, mvvm_panel
  - "Documentation" group: docs_panel
  - Custom group: dynamically registered plugin panels
- Bottom: status label ("Connected to <app_name>" / "No application")
- Selection signal: `panel_selected(id)` → ContentPanel swaps to `Monitor.create_panel(id, app)`

### 4.3 Statusbar behavior

- Connection indicator:
  - Green dot + "Connected: <app_name>" if Application exists
  - Gray dot + "No application" if null
- Refresh button: force immediate refresh of all visible panels
- Theme button: cycle dark / light (Editor theme)
- Locale button: cycle through `TranslationServer::get_loaded_locales()`

### 4.4 Content + Detail panel

- Content: shows the main view of selected panel (Activity stack tree, Service list, etc.)
- Detail: shows context-specific details (selected Activity intent/extras, selected service properties, etc.)
- For Stage 1: detail is empty placeholder
- For Stage 2+: detail wires up per-panel (ActivityPanel: intent extras; ServicePanel: service properties; etc.)

---

## 5. Data Source Snapshots

### 5.1 `ActivityManager::get_snapshot()`

```cpp
Dictionary ActivityManager::get_snapshot() const {
    Dictionary snap;
    Array stack;
    for (int i = 0; i < get_stack_size(); i++) {
        Activity *a = get_stack_activity(i);
        Dictionary entry;
        entry["id"] = a->get_instance_id();
        entry["action"] = a->get_intent().is_valid() ? a->get_intent()->get_action() : "";
        entry["extras"] = a->get_intent().is_valid() ? a->get_intent()->get_extras() : Dictionary();
        entry["flags"] = a->get_intent().is_valid() ? a->get_intent()->get_flags() : 0;
        entry["scene_path"] = a->get_scene_file_path();
        stack.push_back(entry);
    }
    snap["stack"] = stack;
    // ... dialogs, toasts similarly
    return snap;
}
```

### 5.2 `ServiceRegistry::get_snapshot()`

```cpp
Dictionary ServiceRegistry::get_snapshot() const {
    Dictionary snap;
    Array services;
    PackedStringArray names = get_service_names();
    for (const String &name : names) {
        Object *obj = get_service(name);
        Dictionary entry;
        entry["name"] = name;
        entry["type"] = obj ? obj->get_class() : "";
        entry["alive"] = obj ? ObjectDB::get_instance(obj->get_instance_id()) != nullptr : false;
        entry["id"] = obj ? obj->get_instance_id() : 0;
        services.push_back(entry);
    }
    snap["services"] = services;
    return snap;
}
```

### 5.3 `ResourceManager::get_snapshot()`

```cpp
Dictionary ResourceManager::get_snapshot() const {
    Dictionary snap;
    Array loading, loaded, failed;
    // Iterate _handles (HashMap<String, Ref<ResourceHandle>>)
    for (const auto &E : _handles) {
        const Ref<ResourceHandle> &h = E.value;
        Dictionary entry;
        entry["path"] = E.key;
        entry["id"] = h->get_instance_id();
        ResourceHandle::State state = h->get_state();
        switch (state) {
            case ResourceHandle::STATE_LOADING: loading.push_back(entry); break;
            case ResourceHandle::STATE_LOADED: loaded.push_back(entry); break;
            case ResourceHandle::STATE_FAILED: failed.push_back(entry); break;
        }
    }
    snap["loading"] = loading;
    snap["loaded"] = loaded;
    snap["failed"] = failed;
    return snap;
}
```

### 5.4 `BindingEngine::get_active_bindings()`

```cpp
Array BindingEngine::get_active_bindings() const {
    Array result;
    // Iterate _bindings (HashMap<ObjectID, BindingRecord>)
    for (const auto &E : _bindings) {
        const BindingRecord &rec = E.value;
        Dictionary entry;
        entry["view"] = rec.view_path;
        entry["view_prop"] = rec.view_prop;
        entry["vm"] = rec.vm_path;
        entry["vm_prop"] = rec.vm_prop;
        entry["mode"] = rec.mode;  // 0=one-way, 1=two-way
        result.push_back(entry);
    }
    return result;
}
```

### 5.5 `ObservableProperty::is_dirty()` / `get_change_count()`

```cpp
class ObservableProperty : public RefCounted {
    // ...
    bool is_dirty() const { return _dirty; }
    int get_change_count() const { return _change_count; }
    void clear_change_count() { _change_count = 0; }
    void clear_dirty() { _dirty = false; }
private:
    bool _dirty = false;
    int _change_count = 0;
};
```

`set_value()` increments `_change_count` and sets `_dirty = true`. `clear_dirty()` resets.

### 5.6 `Application::get_data_sources()`

```cpp
Array Application::get_data_sources() const {
    Array result;
    result.push_back(_activity_manager);   // ActivityManager IS-A MonitorDataSource
    result.push_back(_service_registry);
    result.push_back(_resource_manager);
    // BindingEngine added in Stage 4
    return result;
}
```

---

## 6. Panel Behaviors

### 6.1 `FrameworkPanel` (Stage 2)

- 4 sub-cards: Application / Activities / Services / Resources
- Each card = VBoxContainer with title + count label + collapsible list
- `refresh()` iterates `get_data_sources()` and pulls each manager's `get_snapshot()`

### 6.2 `ActivityPanel` (Stage 3)

- Top half: Activity stack (Tree control, ordered by stack index)
- Bottom half: Dialog list (ItemList)
- Bottom-right: Toast queue (ItemList)
- Right detail: selected Activity's intent.action / extras / flags / lifecycle state
- "Jump to Scene" button → `EditorInterface::open_scene_from_path(scene_path)`

### 6.3 `ServicePanel` (Stage 4)

- Top half: Service list (Tree: name / type / alive)
- Bottom half: selected service's properties (foldable Group)

### 6.4 `ResourcePanel` (Stage 4)

- 3-column layout: Loading / Loaded / Failed
- Each entry: path + resource type + size (if loaded)
- Click to inspect resource properties (uses Inspector dock via `EditorInterface::inspect_object`)

### 6.5 `MvvmPanel` (Stage 4)

- Top half: active bindings list (source → target)
- Bottom half: ObservableProperty heatmap (recent 1s change count, sorted)

### 6.6 `DocsPanel` (Stage 6)

- Left: search box + class list (ItemList, filterable)
- Right: EditorHelp rendering of selected class
- Tabs at top: "API" (doc_classes/*.xml) | "Guides" (res://docs/**/*.md)
- Search: class name / method name filter

---

## 7. Refresh Throttling

### 7.1 Default strategy

- 30fps throttled (33ms interval)
- Configurable per panel: `set_refresh_interval_ms(int)`
- Manual override via "Refresh" button in statusbar

### 7.2 Implementation

```cpp
void MonitorWindow::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_PROCESS: {
            uint64_t now = OS::get_singleton()->get_ticks_msec();
            if (now - _last_refresh_ms >= _refresh_interval_ms) {
                _refresh_all_panels();
                _last_refresh_ms = now;
            }
        } break;
        // ...
    }
}
```

Panels set `_needs_refresh = true` on activation; window polls and calls `refresh()` if needed.

---

## 8. Editor Plugin Integration

### 8.1 Registration

```cpp
// register_types.cpp
#ifdef TOOLS_ENABLED
void initialize_game_framework_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;

    GDREGISTER_CLASS(Monitor);
    GDREGISTER_CLASS(MonitorPanel);
    GDREGISTER_CLASS(MonitorWindow);
    GDREGISTER_CLASS(MonitorSidebar);
    GDREGISTER_CLASS(MonitorStatusbar);
    GDREGISTER_VIRTUAL_CLASS(MonitorDataSource);  // Not registered — pure C++ interface

    GDREGISTER_CLASS(FrameworkPanel);
    GDREGISTER_CLASS(ActivityPanel);
    GDREGISTER_CLASS(ServicePanel);
    GDREGISTER_CLASS(ResourcePanel);
    GDREGISTER_CLASS(MvvmPanel);
    GDREGISTER_CLASS(DocsPanel);

    // Default panel registration (Stage 1: just framework; Stage 2+: others)
    Monitor::get_singleton()->_register_default_panels();

    EditorNode::add_editor_plugin(Ref<MonitorPlugin>(memnew(MonitorPlugin)));
#endif
}
```

### 8.2 `MonitorPlugin` lifecycle

```cpp
void MonitorPlugin::_enter_tree() {
    _window = memnew(MonitorWindow);
    _window->set_min_size(Size2(800, 600));
    // Option A: dock to MainScreen
    // _window->set_parent_control(EditorNode::get_singleton()->get_main_screen());
    // Option B: standalone Window
    _window->show();
}

void MonitorPlugin::_exit_tree() {
    if (_window) {
        _window->queue_free();
        _window = nullptr;
    }
}
```

Decision: **standalone Window** for Stage 1 (less invasive), can dock later.

### 8.3 Menu entry

```cpp
void MonitorPlugin::_enter_tree() {
    // ...
    add_tool_menu_item("Framework Monitor", callable_mp(this, &MonitorPlugin::_show_window));
}
```

Menu path: **Project → Tools → Framework Monitor**.

---

## 9. Runtime Overlay (Stage 7, optional)

### 9.1 `MonitorOverlay : CanvasLayer`

```cpp
class MonitorOverlay : public CanvasLayer {
    GDCLASS(MonitorOverlay, CanvasLayer);

    Ref<MonitorWindow> _window;  // Shares panel logic with editor window

public:
    void _ready() override {
        _window = memnew(MonitorWindow);
        _window->set_application(_find_application());
        add_child(_window);
    }

    Application *_find_application() {
        // Walk up tree to find Application instance
    }

    void _input(InputEvent *e) override {
        if (e->is_action_pressed("ui_text_completion_accept")) {  // Ctrl+Shift+`
            _window->set_visible(!_window->is_visible());
        }
    }
};
```

### 9.2 Usage

```gdscript
# Any runtime scene
var overlay = MonitorOverlay.new()
add_child(overlay)
```

---

## 10. Documentation Panel (Stage 6)

### 10.1 EditorHelp integration

```cpp
class DocsPanel : public MonitorPanel {
    // ...
    EditorHelp *_help;

    void _on_create_help() {
        _help = memnew(EditorHelp);
        _help->set_v_size_flags(SIZE_EXPAND_FILL);
        add_child(_help);
    }

    void _on_class_selected(const String &p_class) {
        _help->go_to_class(p_class);  // EditorHelp API
    }
};
```

**Caution**: `EditorHelp` is in `editor/doc/` — only include in `#ifdef TOOLS_ENABLED`.

### 10.2 Markdown guides

- Scan `res://docs/**/*.md` at panel init
- Build simple markdown→BBCode renderer (or use RichTextLabel's `push_meta` / `add_text`)
- Defer to existing helpers if available; otherwise write minimal renderer

### 10.3 Search

- Single text box at panel top
- Filter class list by substring match on class name
- Filter method list by substring match on method name within selected class

---

## 11. Risks and Mitigations

| Risk | Mitigation |
|---|---|
| EditorHelp binary path in release build | Only `#include "editor/doc/editor_help.h"` under `#ifdef TOOLS_ENABLED`; DocsPanel itself is `#ifdef TOOLS_ENABLED` |
| GDScript can't override C++ virtuals (Stage 16 lesson) | Use `GDVIRTUAL` + `has_method` duck dispatch in MonitorPanel base class |
| 30fps refresh may be too aggressive on large projects | Default 30fps, configurable per panel, can disable |
| Multiple panels simultaneously created in single window | Each panel instance owned by ContentPanel, freed on swap |
| Custom plugins pollute registry | `unregister_panel` API + scoped registration (per-application) |
| Snapshot copies large data (e.g. Resource handles × 1000) | Snapshot is shallow Dictionary copy; lazy evaluation in panel; pagination if > 50 entries |

---

## 12. Out of Scope (deferred)

- **S8 IPC / multi-application**: requires Godot Debugger protocol bridge — Stage 8
- **In-game Markdown editor**: read-only viewer only
- **Hot reload of plugin panels**: requires `_reload_panel(id)` API — Stage 8+
- **Distributed tracing across network**: future work
- **Custom theme authoring**: relies on Editor theme; no custom theme file format yet

---

## 13. Validation

Each stage has its own verification (see `tasks.md` summary table). The cross-stage final acceptance:

- [ ] `python -m SCons platform=windows target=editor dev_build=yes -j%NUMBER_OF_PROCESSORS%` — 0 errors
- [ ] Editor launches + opens monitor window
- [ ] All 5 built-in panels render without errors
- [ ] Custom GDScript panel can be registered via `Monitor.register_panel(...)`
- [ ] gf_test 18-test regression all pass (no behavior change to existing API)
- [ ] plinko_game demo still runs (no regression in editor)
- [ ] DocsPanel renders at least one doc_classes/*.xml entry successfully
- [ ] PROGRESS.md updated with new stage entry