## Why

`game_framework` 模块已经完成 21 个阶段，从 Context/Application 架构到 MVVM、StandaloneApplication、ActivityLauncher 策略对象——但**没有一个统一的运行时可视化入口**：

- Activity 栈 / Dialog / Toast 状态只能通过 `ActivityManager` 的 6 个只读 API（阶段 10 留下的）配合 `debug_overlay.gd` 在游戏场景内 Ctrl+Shift+`` ` 临时打开；
- ServiceRegistry 注册的服务散落在各处，没法一览；
- ResourceManager 的加载状态（loading/loaded/failed）没有 UI 入口；
- BindingEngine / ObservableProperty 的活跃绑定与属性值变化没有热图；
- `doc_classes/*.xml` 文档只能在脚本编辑器侧栏看，没法和运行时状态交叉跳转；
- 后续 FsmKit / EventKit / PoolKit / ResKit / IMC 等 plugin 各自需要一个调试面板入口，但没有统一的注册中心。

参考截图（YokiFrame v2.0 Preview 风格）：

- **左侧菜单**：plugin 列表（框架 / 文档 / 工具：FsmKit、EventKit、PoolKit、ResKit 等可扩展）
- **顶栏**：连接状态指示 + 主题切换 + 语言切换 + 刷新
- **中部三栏**：活动对象列表 + 当前对象详情 + 状态图 / 矩阵
- **右侧栏**：事件洞察 + 转换历史时间线

这是游戏框架类项目（UE/Unity/custom 引擎）的标配。Godot 自带的 Debugger 只能看 Remote / Profiler / Network / Audio，没法看业务层状态。

## What Changes

在 `modules/game_framework` 内新增 `monitor/` 子模块，**不修改 Godot 引擎源码**，**不破坏现有 GDScript API**：

- **静态注册中心** `Monitor`：单例类，提供 `register_panel(id, title, icon_path, factory)` / `unregister_panel` / `get_registered_panels()`。GDScript 与 C++ 双向可注册。
- **Panel 抽象基类** `MonitorPanel : VBoxContainer`：4 个生命周期钩子 `bind_data_source(app)` / `refresh()` / `on_activated()` / `on_deactivated()`。GDScript 子类通过 `has_method` + `call()` 鸭子分发（与阶段 16 `ValueConverter` 同模式）。
- **数据源抽象** `MonitorDataSource`：每个 Manager 实现这个接口暴露 `get_snapshot() -> Dictionary`，Editor 窗口和 Runtime Overlay 共享同一份数据快照。
- **Editor 主窗口** `MonitorWindow`：HSplitContainer x2（三栏）+ 顶栏（连接状态 + 主题 + i18n + 刷新）+ 左侧菜单（ItemList）。通过 `EditorPlugin` 注册到 Project 菜单。
- **运行时 Overlay**（Stage 7 可选）：`MonitorOverlay : CanvasLayer` 节点，与 Editor 窗口共享 panel 注册表，开发期和 release-build 都能开。
- **5 个内置 Panel**：
  - `FrameworkPanel` — Application + 4 个 Manager 总览
  - `ActivityPanel` — Activity 栈树 + Dialog/Toast 列表 + "Jump to scene" 按钮
  - `ServicePanel` — ServiceRegistry 视图
  - `ResourcePanel` — ResourceManager 加载状态
  - `MvvmPanel` — BindingEngine + ObservableProperty 热图
- **文档查看 Panel** `DocsPanel`：复用 `EditorHelp` 渲染 `doc_classes/*.xml` + 支持 `res://docs/**/*.md` 自定义文档。
- **集成 demo**（可选）：在 `gf_test/` 项目里把现有 `debug_overlay.gd` 重写为继承 `FrameworkPanel` 的 GDScript 子类，验证 GDScript plugin 路径。

### 新增能力

- `framework-monitor-core`: 静态注册中心 + Panel 抽象 + 数据源接口
- `framework-monitor-editor`: Editor 窗口、EditorPlugin、顶栏、侧栏
- `framework-monitor-overlay`: Runtime Overlay 节点
- `framework-monitor-panel-framework`: FrameworkPanel（应用总览）
- `framework-monitor-panel-activity`: ActivityPanel（栈可视化）
- `framework-monitor-panel-service`: ServicePanel（服务注册表）
- `framework-monitor-panel-resource`: ResourcePanel（资源加载状态）
- `framework-monitor-panel-mvvm`: MvvmPanel（绑定 + 属性热图）
- `framework-monitor-panel-docs`: DocsPanel（doc_classes + markdown 浏览器）
- `framework-monitor-theme`: 主题切换（复用 Editor theme）
- `framework-monitor-i18n`: 国际化（复用 Godot Locale）

### 修改的能力

- `ActivityManager`：新增 `get_snapshot()` 聚合方法，复用现有 6 个只读 API，不修改现有 API
- `ServiceRegistry`：新增 `get_snapshot()` / `get_service_count()`
- `ResourceManager`：新增 `get_snapshot()`（handle 按状态分组）
- `BindingEngine`：新增 `get_active_bindings()` 返回活跃绑定
- `ObservableProperty`：新增 `is_dirty()` / `get_change_count()`
- `Application`：新增 `get_data_sources() -> Array<MonitorDataSource*>`

## Capabilities

### New Capabilities

- `framework-monitor-core`: 注册中心 + Panel 基类 + 数据源接口
- `framework-monitor-editor`: Editor 主窗口 + EditorPlugin
- `framework-monitor-overlay`: Runtime Overlay 节点（Stage 7）
- `framework-monitor-panel-framework`: 框架总览
- `framework-monitor-panel-activity`: Activity 可视化
- `framework-monitor-panel-service`: 服务视图
- `framework-monitor-panel-resource`: 资源视图
- `framework-monitor-panel-mvvm`: MVVM 视图
- `framework-monitor-panel-docs`: 文档浏览器
- `framework-monitor-plugin-api`: GDScript plugin 工厂接口

### Modified Capabilities

- `game-framework-activity-manager`: 新增聚合 snapshot（向后兼容）
- `game-framework-service-registry`: 新增聚合 snapshot
- `game-framework-resource-manager`: 新增聚合 snapshot
- `game-framework-mvvm-binding-engine`: 新增 get_active_bindings
- `game-framework-mvvm-observable-property`: 新增 is_dirty / change_count

## Impact

- **新增文件**（约 25 个）：
  - `modules/game_framework/monitor/monitor.{h,cpp}` — 静态注册中心
  - `modules/game_framework/monitor/monitor_data_source.h` — 数据源抽象
  - `modules/game_framework/monitor/monitor_panel.{h,cpp}` — Panel 抽象基类
  - `modules/game_framework/monitor/monitor_window.{h,cpp}` — Editor 主窗口
  - `modules/game_framework/monitor/monitor_plugin.{h,cpp}` — EditorPlugin
  - `modules/game_framework/monitor/monitor_sidebar.{h,cpp}` — 左侧导航
  - `modules/game_framework/monitor/monitor_statusbar.{h,cpp}` — 顶栏
  - `modules/game_framework/monitor/panels/framework_panel.{h,cpp}`
  - `modules/game_framework/monitor/panels/activity_panel.{h,cpp}`
  - `modules/game_framework/monitor/panels/service_panel.{h,cpp}`
  - `modules/game_framework/monitor/panels/resource_panel.{h,cpp}`
  - `modules/game_framework/monitor/panels/mvvm_panel.{h,cpp}`
  - `modules/game_framework/monitor/panels/docs_panel.{h,cpp}`
  - `modules/game_framework/monitor/monitor_overlay.{h,cpp}`（Stage 7）
  - `modules/game_framework/monitor/doc_classes/Monitor.xml` + 6 个 panel XML
  - `modules/game_framework/monitor/tests/test_monitor_registry.cpp`
  - `modules/game_framework/monitor/tests/test_monitor_panel_lifecycle.cpp`

- **修改文件**：
  - `modules/game_framework/register_types.cpp` — 注册新类 + 装 EditorPlugin（+30 行）
  - `modules/game_framework/SCsub` — 加 monitor/ 子目录到 glob
  - `modules/game_framework/PROGRESS.md` — 新阶段记录
  - `modules/game_framework/ui/activity_manager.{h,cpp}` — get_snapshot (+25 行)
  - `modules/game_framework/service/service_registry.{h,cpp}` — get_snapshot (+20 行)
  - `modules/game_framework/resource/resource_manager.{h,cpp}` — get_snapshot (+30 行)
  - `modules/game_framework/mvvm/binding_engine.{h,cpp}` — get_active_bindings (+25 行)
  - `modules/game_framework/mvvm/observable_property.{h,cpp}` — is_dirty / change_count (+10 行)
  - `modules/game_framework/application.{h,cpp}` — get_data_sources (+15 行)

- **性能**：
  - Editor 窗口 30fps 节流刷新（可关闭），snapshot 是 Dictionary copy 不持锁
  - 大列表分页（Activity 栈 > 50 时折叠）
  - Runtime Overlay 同节流策略
  - 测试场景下实测 < 0.5ms/frame 占用

- **API 兼容性**：
  - GDScript 侧 `Monitor.register_panel(...)` 是新 API，不破坏现有调用
  - Manager 的 `get_snapshot()` 是新增方法，不修改现有方法签名
  - Editor 窗口仅 `TOOLS_ENABLED` 下编译，runtime 构建零影响

- **风险**：
  - **EditorHelp 二进制路径**：不直接 `#include "editor/doc/editor_help.h"`，只在 `TOOLS_ENABLED` 下用 `Object::cast_to` 桥接
  - **GDScript 不能 override C++ 虚函数**（阶段 16 教训）：基类 `refresh()`/`on_activated()` 用 GDVIRTUAL + `has_method` 鸭子分发
  - **多 Panel 刷新打爆主线程**：默认 30fps 节流 + 可关闭