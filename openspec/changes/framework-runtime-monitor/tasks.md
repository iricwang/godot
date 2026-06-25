task: framework-runtime-monitor
status: planning
created: 2026-06-23
spec: framework-runtime-monitor
proposal: openspec/changes/framework-runtime-monitor/proposal.md
detailed_spec: openspec/changes/framework-runtime-monitor/specs/framework-runtime-monitor.md

# S1 — 骨架与导航（无 plugin 数据）

- [ ] 1.1 `monitor.h/cpp` — 静态注册中心（`Monitor : Object`）
  - 单例 + `register_panel(id, title, icon_path, factory)` 静态 API
  - `unregister_panel(id)` / `get_registered_panels()` / `has_panel(id)` / `create_panel(id, app)`
  - 内部存 `HashMap<String, PanelDescriptor>`，descriptor 含 title / icon / factory
  - GDREGISTER_CLASS + ClassDB bind_method
- [ ] 1.2 `monitor_data_source.h` — 数据源抽象（纯 C++ 接口，不暴露给 GDScript）
  - `class MonitorDataSource { virtual Dictionary get_snapshot() const = 0; virtual ~MonitorDataSource() {} }`
- [ ] 1.3 `monitor_panel.h/cpp` — Panel 抽象基类（`MonitorPanel : VBoxContainer`）
  - 5 个 GDVIRTUAL 钩子：`bind_data_source(app)` / `refresh()` / `on_activated()` / `on_deactivated()` / `get_panel_id()`
  - `set_active(bool)` / `is_active()` 状态机
  - 30fps 节流刷新（`_refresh_throttle` 计时器）
- [ ] 1.4 `monitor_sidebar.h/cpp` — 左侧导航（`MonitorSidebar : VBoxContainer`）
  - `ItemList` 显示已注册 panel 列表
  - 选中信号 `panel_selected(id)`
  - 顶部分隔线：框架（root）/ 工具（按注册顺序）
- [ ] 1.5 `monitor_statusbar.h/cpp` — 顶栏（`MonitorStatusbar : HBoxContainer`）
  - 左侧：连接状态指示（圆点 + "已连接" / "未连接"）
  - 右侧：刷新按钮 + 主题切换按钮 + 语言切换按钮
- [ ] 1.6 `monitor_window.h/cpp` — Editor 主窗口（`MonitorWindow : HSplitContainer`）
  - HSplitContainer x2：sidebar / content / detail（detail S1 阶段先空）
  - 顶栏放在窗口顶部（VBoxContainer root）
  - `set_application(app)` 绑定数据源 + 通知所有 panel
  - `select_panel(id)` 切换内容区
- [ ] 1.7 `monitor_plugin.h/cpp` — EditorPlugin（`MonitorPlugin : EditorPlugin`）
  - `_enter_tree`：创建 `MonitorWindow`，加到 MainScreen 还是独立 Window（先做独立 Window）
  - 加 Project 菜单项 "Project → Tools → Framework Monitor"
  - `_exit_tree`：清理
- [ ] 1.8 `panels/navigation_panel.h/cpp` — 占位 panel（仅显示 "Select a panel from sidebar"）
- [ ] 1.9 修改 `register_types.cpp`：
  - `#ifdef TOOLS_ENABLED` 注册 `MonitorPlugin` 并 `EditorNode::add_editor_plugin(...)`
  - 注册 `Monitor` / `MonitorPanel` / `MonitorWindow` / `MonitorSidebar` / `MonitorStatusbar` / `NavigationPanel`
- [ ] 1.10 修改 `SCsub`：确认 monitor/ 子目录被 glob 到（默认通配 `*.cpp` 已覆盖）
- [ ] 1.11 `doc_classes/Monitor.xml` + `MonitorPanel.xml` + `MonitorWindow.xml`
- [ ] 1.12 编译验证：
  - `python -m SCons platform=windows target=editor dev_build=yes -j%NUMBER_OF_PROCESSORS%`
  - 启动 `bin\godot.windows.editor.dev.x86_64.exe --path D:/AI_Temp/gf_test`
  - Project → Tools → Framework Monitor 能打开空窗口
  - 顶栏显示 "未连接 Application"（gf_test 默认入口是 enhanced_input_activity 不会创建 Application，先在 gf_test 入口加 `var app = Application.new()` 即可，或者窗口默认显示 demo Application）

# S2 — 框架总览 Panel（最小可用）

- [ ] 2.1 `application.{h,cpp}` 新增 `Array<MonitorDataSource*> get_data_sources() const`
  - 返回 ActivityManager / ServiceRegistry / ResourceManager / BindingEngine 的 snapshot 接口
- [ ] 2.2 `activity_manager.cpp` 新增 `Dictionary get_snapshot() const`
  - `{stack: [...], dialogs: [...], toasts: [...]}`，复用现有 6 个只读 API
  - 实现 `MonitorDataSource::get_snapshot()`
- [ ] 2.3 `service_registry.cpp` 新增 `Dictionary get_snapshot() const`
  - `{services: [{name, type, alive}]}`，遍历 service_map
  - 实现 `MonitorDataSource::get_snapshot()`
- [ ] 2.4 `resource_manager.cpp` 新增 `Dictionary get_snapshot() const`
  - `{loading: [...], loaded: [...], failed: [...]}`，遍历 handle map
  - 实现 `MonitorDataSource::get_snapshot()`
- [ ] 2.5 `panels/framework_panel.{h,cpp}` — 框架总览
  - 4 个子卡：Application / Activities / Services / Resources
  - 每个子卡用 VBoxContainer + Label 列表显示 snapshot
  - `refresh()` 时遍历 data_sources 重新拉取
- [ ] 2.6 修改 `monitor_window.cpp`：
  - `set_application(app)` 时调用 `framework_panel->bind_data_source(app)`
- [ ] 2.7 `gf_test/scenes/entry.gd`（如已存在）确认 `var app = Application.new(); app.initialize(...)` 在 entry 阶段创建（使 monitor 能看到 Application）
- [ ] 2.8 验证：
  - editor 启动 + F5 跑 gf_test + 开 Monitor → FrameworkPanel 显示 "Stack: [MainActivity]" / Services 列表 / Resources 计数
  - `start_activity(detail)` → 栈变为 2
  - `register_service("test", svc)` → Services 列表立刻出现
  - `load_async("res://icon.svg")` → Resources 计数 +1

# S3 — Activity 可视化

- [ ] 3.1 `panels/activity_panel.{h,cpp}` — Activity 栈树 + Dialog/Toast 列表
  - 上半：Activity 栈（Tree 控件，按栈顺序展示）
  - 下半：Dialog 列表（ItemList）
  - 右下：Toast 队列（ItemList）
- [ ] 3.2 选中 Activity 节点 → 右侧 detail 区显示 intent.action / extras / flags / lifecycle state
- [ ] 3.3 "Jump to Scene" 按钮：调 `EditorInterface::open_scene_from_path(scene_path)`
  - 需要把 scene_path 注册到 Intent extras 或者从 ActivityLoader 推断
- [ ] 3.4 ActivityManager 加 `get_activity_scene_path(idx)`（通过 ActivityLoader.get_path(action) 反查）
- [ ] 3.5 验证：gf_test 主场景下，ActivityPanel 显示 Stack: [MainActivity → DetailActivity]，点 Jump 打开 .tscn

# S4 — Service / Resource / MVVM 三 Panel

- [ ] 4.1 `panels/service_panel.{h,cpp}`：
  - 上半：服务列表（Tree，列 name / type / alive）
  - 下半：选中服务的属性查看（折叠 Group）
- [ ] 4.2 `panels/resource_panel.{h,cpp}`：
  - 三列布局：Loading / Loaded / Failed
  - 每列显示 handle.path + 资源类型 + 大小（可选）
- [ ] 4.3 `binding_engine.cpp` 新增 `Array get_active_bindings() const`
- [ ] 4.4 `observable_property.{h,cpp}` 新增 `is_dirty()` / `get_change_count()` / `clear_change_count()`
- [ ] 4.5 `panels/mvvm_panel.{h,cpp}`：
  - 上半：活跃绑定列表（source → target）
  - 下半：ObservableProperty 热图（最近 1s 内变化次数排序）

# S5 — Plugin 扩展接口

- [ ] 5.1 `Monitor` 类公开 GDScript 注册 API（`ClassDB::bind_method`）
- [ ] 5.2 GDScript 工厂示例：`monitor_examples/fsm_panel.gd`（extends MonitorPanel）
  - 伪数据展示状态机切换
  - 演示完整 plugin 路径
- [ ] 5.3 文档：`docs/monitor_plugin_authoring.md`（如何写自定义 panel）
- [ ] 5.4 启动扫描 `res://monitor_plugins/**/*.gd`（可选，自动加载）
- [ ] 5.5 验证：写一个 GDScript FsmKit panel，启动 editor 后左侧菜单出现 FsmKit

# S6 — 文档查看功能

- [ ] 6.1 `panels/docs_panel.{h,cpp}`：
  - 左侧：类 / 方法搜索框 + 类列表（ItemList）
  - 右侧：复用 EditorHelp 渲染 doc_classes/*.xml
  - 支持 `res://docs/**/*.md` 自定义文档
- [ ] 6.2 顶栏搜索框（panel 内部），按类名 / 方法名过滤
- [ ] 6.3 与 Stage 5 联动：plugin 可注册自定义文档子节
- [ ] 6.4 验证：搜 `Activity.boot_mode`（历史 API），跳到对应段落；新建 `docs/test.md` 能被识别

# S7（可选）— Runtime Overlay

- [ ] 7.1 `monitor_overlay.{h,cpp}` — `MonitorOverlay : CanvasLayer`
  - 运行时场景 `add_child(MonitorOverlay)` 即可开启
  - 共享 panel 注册表（与 Editor 窗口同一份）
  - 快捷键 `Ctrl+Shift+`` ` 默认开关
- [ ] 7.2 `release build` 验证：导出模板 + MonitorOverlay 节点能跑

# S8（可选）— IPC / 多 Application

- [ ] 8.1 监听 Godot Debugger 协议桥接
- [ ] 8.2 顶栏连接指示器扩展：显示远程 IP:port + app name
- [ ] 8.3 多 Application 切换器（侧栏顶部下拉框）

# 验证里程碑

| Stage | 验证 |
|---|---|
| S1 | 编译通过 + editor 能开空窗口 + 顶栏显示 "未连接 Application" |
| S2 | gf_test F5 + Monitor 显示 Stack/Services/Resources |
| S3 | ActivityPanel Jump 按钮打开 .tscn |
| S4 | 注册 service 后 ServicePanel 立刻看到 + load_async 失败计数 +1 |
| S5 | GDScript FsmKit panel 出现在左侧菜单 |
| S6 | DocsPanel 搜 `Activity.boot_mode` 跳到对应段落 |
| S7 | release build + MonitorOverlay 能跑 |
| S8 | 远程连接 + 多 Application 切换 |

# 跨 Stage 收尾

- [ ] X.1 `modules/game_framework/PROGRESS.md` 加新阶段记录（参照阶段 21 风格）
- [ ] X.2 `modules/game_framework/doc_classes/` 补齐所有 Monitor*.xml
- [ ] X.3 跑 gf_test 18 项回归，确认无回归
- [ ] X.4 plinko_game demo 跑通，确认无回归
- [ ] X.5 写 `docs/monitor_user_guide.md`（用户视角：如何打开 monitor + 各 panel 看什么）
- [ ] X.6 写 `docs/monitor_plugin_authoring.md`（开发者视角：如何写自定义 panel）