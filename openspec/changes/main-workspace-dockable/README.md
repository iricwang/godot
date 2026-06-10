# 主工作区可停靠化（Dockable Main Workspaces）

> 让 2D / 3D / Script / Game / AssetLib 五个主工作区像 Unity 一样可拖拽吸附 / 拼接 / 拉出为独立窗口。

## 1. 现状速览（基于 4.7-beta 源码梳理）

| 系统 | 是否已支持「拖拽吸附 / 独立窗口 / layout 保存」 | 关键位置 |
| --- | --- | --- |
| 左/右/底 dock（FileSystem、Inspector、SceneTree、Import…） | ✅ 完整支持 | `editor/docks/editor_dock_manager.{h,cpp}` + 11 个 `DockSlot` |
| **2D / 3D / Script / Game / AssetLib 五个主工作区** | ❌ 互斥切换。Script & Game 有「拉出窗口」按钮，但无法停靠、无法拼接、无法保存停靠位置 | `editor/editor_main_screen.{h,cpp}`，`add_main_plugin` 只产出一个顶部 toggle button |
| 独立窗口包装器（`WindowWrapper`） | ✅ 已经成熟，dock 与 Script/Game 工作区均在用 | `editor/gui/window_wrapper.{h,cpp}` |

dock 系统已经有：11 个固定 slot + `DockTabContainer`（每槽位一个 TabContainer）+ 拖拽时的 `EditorDockDragHint` 落点提示 + `DockSlotGrid` 8×8 选择栅格 + `WindowWrapper` 浮窗 + `[docks]` 段落的 layout 持久化。**这套设施完全够用，无需自研另一套**。

`EditorMainScreen` 与 `EditorDockManager` 是两套独立机制。本次改造的本质就是把前者「降级」为后者的客户。

---

## 2. 改造目标（与你确认过的方案 B）

- ✅ **复用** `EditorDockManager` 把 5 个主工作区变成 dock（拖拽/吸附/拉出独立窗口/layout 保存 全部免费拿到）。
- ✅ **保留**顶部 5 个 toggle button 作为快捷键入口，行为重新定义为「`focus_dock(对应 dock)`」 = 把对应 dock 的 tab 置前、不隐藏其它工作区。
- ✅ **保留** `EditorMainScreen::select(EDITOR_*)` 这一公开 API 的兼容签名，不破坏 ~22 处外部调用（包括 8 处硬编码 `select(EDITOR_SCRIPT)`），但语义改为「置前/聚焦」而非「互斥可见」。
- ✅ **保留** `EditorMainScreen::EDITOR_2D/3D/SCRIPT/GAME/ASSETLIB` 枚举值不变。
- ✅ 兼容旧 `editor_layout.cfg`：发现旧版 `[EditorNode]/selected_main_editor_idx` 时降级为「focus 该 dock」并以默认布局填充新 dock 信息。
- ✅ 自动切屏（Node3D 选中 → 3D、运行游戏 → Game、F1 → Script）统一改为 `focus_dock`，不再隐藏其它工作区。

---

## 3. 关键设计决策

### 3.1 用「主工作区 dock」桥接 EditorPlugin ↔ EditorDock

引入一个新的内部桥接类 `MainScreenDock : public EditorDock`：

```cpp
// editor/docks/main_screen_dock.h（新增）
class MainScreenDock : public EditorDock {
    EditorPlugin *plugin = nullptr;          // 被它代理的 main-screen plugin
    Control *plugin_root = nullptr;          // plugin 加到 get_control() 的那个子节点
    void _update_layout(int p_layout) override; // 当被拖入浮窗时 / 嵌回时
public:
    void bind_plugin(EditorPlugin *p, Control *root);
    EditorPlugin *get_plugin() const { return plugin; }
};
```

- 每个 main-screen plugin 注册时，`EditorMainScreen` 帮它造一个 `MainScreenDock`，把 plugin 之前 add 到 `get_control()` 的那一个 Control reparent 到该 dock 内。
- 该 dock 设置 `default_slot = DOCK_SLOT_NONE` + 一个新的「中央区」slot（见 3.2）。
- `available_layouts` 开启 V/H/Floating。
- `layout_key` 用 `"main_screen_2d" / "main_screen_3d" / ...`，让 layout 持久化稳定。

### 3.2 中央区作为第 12 个 dock slot（关键架构点）

目前主视口区域不是 dock 系统的一部分（`editor_main_screen` 直接 add 到 `srt` VBox）。要让用户能把 2D/3D 与左/右/底 dock 自由拼接，**必须**让中央区也变成一个 `DockTabContainer`。

具体做法：
1. 把 `EditorMainScreen` 的 `main_screen_vbox` 替换为新的 `CenterDockTabContainer`（继承自 `DockTabContainer`，类似 `EditorBottomPanel`）。
2. 新枚举值 `DOCK_SLOT_CENTER`（追加到 `EditorDock::DockSlot` 末尾，**追加不修改既有索引**，确保旧 layout 的 `dock_1..dock_11` 解析不变）。
3. `EditorDockManager::register_dock_slot` 注册 center slot；`DockSlotGrid` 加一格代表中央区，让用户能把任何 dock 拖到中央。
4. 5 个主工作区默认填到 center slot，且生成的 tab 顺序与原 5 按钮一致。

⚠️ **风险**：让中央成为 dock 意味着用户可以把 2D 视图缩到 1 像素或拖走全部主工作区。需要给 center slot 加 `min_size` 兜底；center slot 为空时显示提示「Drag a workspace here」。

### 3.3 顶部按钮的新语义

```
old: 按钮按下 → select(i) → 隐藏其它 plugin、显示第 i 个
new: 按钮按下 → editor_dock_manager->focus_dock(main_screen_docks[i])
              → 若 dock 关闭 → 先 open_dock 到 center slot
              → 若 dock 在 floating window → 把窗口 raise + grab focus
              → 若 dock 在 center slot → set_current_tab 置前
```

按钮的「按下/未按下」视觉态：哪个 dock 当前持有焦点（来自 `DockTabContainer::dock_focused` 钩子）就按下哪个；多个工作区同时可见时只突出焦点那一个。

### 3.4 `make_visible(bool)` 的兼容

旧契约：`select(new)` 时对 old plugin 调 `make_visible(false)`、对 new plugin 调 `make_visible(true)`。

新契约（最小破坏）：
- 主工作区 dock 被 open / focus / 浮窗显示 → `make_visible(true)`。
- 主工作区 dock 被 close / 浮窗隐藏 → `make_visible(false)`。
- 同时被多个工作区可见时，所有可见的都收到 `make_visible(true)`，只有焦点那个收到 `notify_main_screen_changed`。

→ 对 2D、3D 这种主要靠 `make_visible` 决定 `set_process()` / `set_physics_process()` 的插件影响：之前隐藏的工作区会停止处理，现在它们如果同时可见就会都跑——**这就是 Unity 的行为**，符合预期，但 CPU 占用会上升。**需要在文档里写明**。

### 3.5 兼容已有 WindowWrapper（Script & Game）

Script 和 Game 已经各自有 `WindowWrapper` + `set/get_window_layout`。新方案下：
- Dock 系统会另外提供一套浮窗机制（`make_dock_floating`），它和 plugin 自带的 `WindowWrapper` 会冲突。
- **方案**：拆掉 ScriptEditorPlugin / GameViewPlugin 自带的 `WindowWrapper`，统一让 `MainScreenDock` 负责浮窗。把它们 `set/get_window_layout` 中保存的 `window_rect` 迁移到 dock 系统的 `dock_floating` 字典。
- 保留一次性兼容代码：旧 `[ScriptEditor]/window_rect` 与 `[GameView]/window_rect` 存在时，迁移为新 dock floating rect，迁移后从配置文件删除旧 key。

### 3.6 自动切屏改为 focus

三个触发点（`editor_node.cpp:3208`、`:4653`、`game_view_plugin.cpp:559/600/622/1773`）全部把 `select(i)` 替换为 `focus_dock(main_screen_docks[i])`。`can_auto_switch_screens()` 的语义保留（只在当前焦点是 2D/3D 时才自动切），避免「正在写 script 时不小心点了个 Node3D 被强制切到 3D」。

### 3.7 EditorInterface 公开 API 兼容

`EditorInterface::get_editor_main_screen()` 返回 `VBoxContainer*` —— 此 API 在 GDScript 插件生态中有使用。新方案下：
- 保留该方法。
- 返回值改为：center slot 内当前焦点 dock 的内部 `VBoxContainer`（旧插件可继续 add_child 到这里，但它们的子节点会被「绑定」到当前焦点工作区——这一行为差异需要在 changelog 中标注，并增加新 API `EditorInterface::get_workspace_dock(name)` 让插件准确选择目标）。

---

## 4. 改动文件清单（按风险递增）

| 风险 | 文件 | 改动 |
| --- | --- | --- |
| 低 | `editor/docks/editor_dock.h` | 在 `DockSlot` 枚举末尾追加 `DOCK_SLOT_CENTER`（**末尾追加，不改既有值**）|
| 低 | `editor/docks/main_screen_dock.{h,cpp}` | 新增桥接类 |
| 低 | `editor/docks/SCsub` | 加入新文件 |
| 中 | `editor/docks/editor_dock_manager.{h,cpp}` | `dock_slots[]` 容量 +1；`DockSlotGrid` 加中央格；浮窗布局迁移逻辑 |
| 中 | `editor/docks/dock_tab_container.{h,cpp}` | 新增 `CenterDockTabContainer` 子类（处理空 slot 提示、min_size 兜底）|
| 中 | `editor/editor_main_screen.{h,cpp}` | `add_main_plugin` 创建 `MainScreenDock` 并 register 到 dock manager；`select(i)` 重写为 `focus_dock`；`make_visible` 多重可见语义；layout 持久化部分仅保留按钮焦点态，dock 状态委托给 dock manager |
| 中 | `editor/editor_node.cpp` | center vbox 替换为 center dock slot；`_load_editor_layout` 加旧版迁移；自动切屏调用点改为 `focus_dock` |
| 中 | `editor/run/game_view_plugin.{h,cpp}` | 移除自带 `WindowWrapper`；`set/get_window_layout` 改为读写迁移占位；运行/停止时 `select` → `focus_dock` |
| 中 | `editor/script/script_editor_plugin.{h,cpp}` | 同上 |
| 低 | 8 处硬编码 `select(EDITOR_SCRIPT)` 调用点 | 不动签名，靠 `select` 语义切换自动获得「focus 而非隐藏」行为 |
| 文档 | `doc/classes/EditorInterface.xml`、`doc/classes/EditorPlugin.xml` | 标注 `make_visible` 新语义、新增 `get_workspace_dock` |

预估代码量：新增 ~500 行（主要是 MainScreenDock 与 CenterDockTabContainer）+ 修改 ~300 行。

---

## 5. 分阶段交付（每段都能编译运行、可单独 review）

### Phase 1 — 基础设施（无行为变化）
- 1.1 追加 `DOCK_SLOT_CENTER` 枚举值。
- 1.2 新增 `MainScreenDock`、`CenterDockTabContainer` 空骨架，编译通过但暂不替换 main_screen 实现。
- 1.3 EditorDockManager 扩容 + DockSlotGrid 加中央格，但中央格暂时不允许放置（feature flag）。
- ✅ 验收：引擎能编译运行，行为完全等同 4.7-beta，所有现有测试通过。

### Phase 2 — 把 5 个 main plugin 接入 dock 系统
- 2.1 改 `EditorMainScreen::add_main_plugin`：除原有按钮外，额外创建 `MainScreenDock` 并 register。
- 2.2 改 `_make_visible` / `select` / 焦点态联动。
- 2.3 顶部 5 个按钮改为「focus 而非互斥切换」。
- 2.4 开启 center slot 的拖拽接收。
- ✅ 验收：能从顶部按钮切换、能把 2D 拖到右侧 dock 区与 Inspector 并列、能把 Script 拖出独立窗口、关闭独立窗口后回到 center slot。

### Phase 3 — 拆掉 Script / Game 自带的 WindowWrapper
- 3.1 移除两个 plugin 内部的 `WindowWrapper`，删除它们对应的「Make Floating」shortcut（与 dock 系统的浮窗 shortcut 合并）。
- 3.2 实现旧 `[ScriptEditor]/window_rect` 与 `[GameView]/window_rect` → 新 dock_floating 字典的一次性迁移。
- ✅ 验收：从旧版本 `editor_layout.cfg` 启动，浮窗位置正确恢复；冷启动行为与新建项目一致。

### Phase 4 — 自动切屏改 focus + EditorInterface 兼容 API
- 4.1 `editor_node.cpp:3208 / :4653` 与 `game_view_plugin.cpp:559/600/622/1773` 切换调用改为 `focus_dock`。
- 4.2 新增 `EditorInterface::get_workspace_dock(StringName)` 暴露给 GDScript。
- 4.3 文档更新。
- ✅ 验收：选中 Node3D 不再「藏起 Script」，只是把 3D 工作区 tab 置前；运行游戏时同理。

### Phase 5 — 回归 + 文档
- 5.1 `editor_main_screen` 单元测试若有 → 更新。
- 5.2 手动回归：所有 `select(EDITOR_*)` 22 处调用点行为验证。
- 5.3 changelog + 迁移指南。

---

## 6. 回归风险

| 风险 | 缓解 |
| --- | --- |
| 旧 layout 文件被破坏（用户升级后丢失布局） | Phase 3 的一次性迁移 + 保留 `[EditorNode]/selected_main_editor_idx` 作为「初始焦点」的解释 |
| GDScript 插件依赖 `get_editor_main_screen()` 返回的 VBox 来 add_child | 保留 API，文档新增说明，并提供 `get_workspace_dock` 作为新的精准 API |
| 2D + 3D 同时跑导致性能下降 | 文档警告 + 编辑器设置项 `editor/workspaces/pause_invisible_workspaces` 默认 false |
| 主工作区都被关闭后用户无法找回 | Center slot 为空时显示中央提示 + 顶部按钮可重新打开 dock |
| 第三方 EditorPlugin 实现 `has_main_screen()` 但未预期被同时可见 | 加 plugin 级别 opt-out：`EditorPlugin::_supports_concurrent_visibility()`，默认 true，false 时回退到旧的互斥语义 |
| Game 运行时录制/embed 逻辑（`game_view_plugin.cpp:1350` 使用 `get_control()` 计算 embed 坐标） | 改为使用 `MainScreenDock` 自身的全局 rect |

---

## 7. 未决细项（待你或后续 review 时拍板）

1. **顶部 5 按钮是否保留？**
   - 现方案：保留，作为快捷 focus 入口（兼容 muscle memory）。
   - 备选：彻底移除按钮（更接近 Unity）。
2. **center slot 是否允许放置非主工作区 dock（例如 FileSystem）？**
   - 现方案：允许。完全自由。
   - 备选：center 只允许 main-screen dock，其它 dock 拒绝。
3. **`EditorPlugin::_supports_concurrent_visibility()` 是否纳入本次？**
   - 现方案：纳入（保护第三方插件）。
   - 备选：本次先不加，等用户反馈。

---

## 8. 验收清单（PR 合并前必须勾选）

- [ ] 顶部 5 按钮按下能把对应工作区 tab 置前。
- [ ] 任一工作区可拖到左/右/底 dock 区与其它 dock 并列。
- [ ] 任一工作区可拖出为独立窗口，关闭窗口后回到 center。
- [ ] 5 个工作区可同时可见（例如 2D + 3D 左右分屏 + Script 浮窗）。
- [ ] 重启编辑器后 layout 恢复（包含 floating 窗口的位置 & 屏幕）。
- [ ] 用旧版 `editor_layout.cfg` 启动不崩、行为合理。
- [ ] 选中 Node3D 把 3D 置前；F1 把 Script 置前；运行游戏把 Game 置前。
- [ ] 所有现有 22 处 `select(EDITOR_*)` 调用点行为符合新语义（不隐藏其它）。
- [ ] `get_editor_main_screen()` GDScript API 不破坏现有插件。
- [ ] 文档与 changelog 完成。
