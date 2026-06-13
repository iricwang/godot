# Anchors-driven RectTool Inspector for Control

> Spec id: `anchors-rect-tool-inspector`
> Targets: Godot 4.7-beta (engine fork — feature/game_framework branch)
> Status: **shipped** in commits `657e421a73` (precursor) + `b25586b17b` (generalization).

## Why

Godot 的 `Control` 节点用 4-anchor + 4-offset 的方式描述矩形：当某一轴的两个 anchor 不相等时（"stretching axis"），该轴的位置与尺寸不再来自 `position`/`size`，而是由 `offset_left/right` 或 `offset_top/bottom` + 父矩形 + 那条 anchor 共同决定。

但 inspector 默认行为**与这个事实不符**：

- 在 `PRESET_FULL_RECT`、6 个 `*_WIDE`、`VCENTER_WIDE`、`HCENTER_WIDE` 这类 stretching preset 下，inspector 仍然把 `position` / `size` 当作可编辑字段亮着；用户改完之后引擎在 `Control::_set_size` (`scene/gui/control.cpp:1535`) 抛出 `"size overridden after _ready()"` 警告，并在下一帧把值改回来。
- 同时这些 preset 下 `offset_left/top/right/bottom` 被 `Control::_validate_property` 一刀切隐藏（行 ~636），用户没有任何途径直接调整矩形的真实边缘，只能去拖 2D viewport 手柄或回退到自定义 anchor 模式。

这与 Unity RectTransform 的 RectTool 体验形成强烈反差：Unity 在 stretching 轴上用 `Left / Right` 或 `Top / Bottom` 字段直接编辑 anchor 之间的 padding，并把对应的 `Pos X / Width`、`Pos Y / Height` 隐藏；非 stretching 轴上仍是 `Pos / Size`。整个面板根据 anchor preset 自动联动，**不需要手切到"自定义 anchor"才能调边距**。

我们要的是同样的 Unity-like 体验：

> 选中任何 Control，inspector 的 Layout 组应当根据当前 anchor preset / 自定义 anchor 状态自动决定显示哪些字段：stretching 轴露出 `Left / Top / Right / Bottom`（标签直接如此命名），non-stretching 轴显示 `Position / Size`，永远不让用户编辑那些会被 `_size_changed` 立即覆盖的字段。

## Scope

### In scope

- `scene/gui/control.cpp::Control::_validate_property` 中 anchor / offset / position / size 这一段属性可见性 / 只读性的策略。
- `editor/scene/gui/control_editor_plugin.cpp::EditorInspectorPluginControl::parse_property` 中 `offset_*` 四个 path 的 widget 替换 + 标签改写。
- 行为契约的清晰描述：每个 LayoutPreset / 自定义 anchor 配置下，inspector 应展示哪些字段、哪些 readonly。

### Out of scope (deferred)

- **2D viewport 拖拽手柄遵守锁定状态**（`editor/scene/canvas_item_editor_plugin.cpp` 的 `_drag_*`）：当前实现下 inspector 锁定的字段，仍可通过 2D viewport 的鼠标拖拽改写 size / position。要让两端语义统一需要进一步改 canvas_item_editor_plugin。属于 plan 里的"阶段 3"。
- **对 `position` / `size` 这类 Vector2 字段做单分量隐藏**：默认 `EditorPropertyVector2` widget 不能只锁一个分量，所以单轴 stretch（如 TOP_WIDE）下我们只能整体 READ_ONLY 而不是部分隐藏。要做到 Unity 的"Pos X 和 Left/Right 同时显示"需要写一个统一的 `RectTransformEditor` 自定义 EditorProperty，工作量约 150 行。属于"阶段 2b 精修"。
- 所有对运行时（非 editor）`Control` 行为的修改 — 保持 zero。

## What Changes

### Modified Capabilities

- **`control-anchors-inspector`**：在编辑器的 Inspector 中，`Control` 的 Layout 字段集合根据 anchor 状态联动显示。这是新增能力，名字反映它属于"editor inspector"维度，不是运行时 Control 行为。

### Files modified (already shipped)

| 文件 | 改动 |
|---|---|
| `scene/gui/control.cpp` | `_validate_property` 用 `data.anchor[SIDE_*]` 直接判 `stretch_x` / `stretch_y`。任一轴 stretch ⇒ `position`/`size` 加 `PROPERTY_USAGE_READ_ONLY`；stretch 轴上的对应 `offset_*` 重新可见（即便不在 custom-anchor 模式）。判据与 `_set_size` 抛 size-overridden warning 的判据同源，杜绝两边不一致。 |
| `editor/scene/gui/control_editor_plugin.cpp` | `EditorInspectorPluginControl::parse_property` 拦截 `offset_left/top/right/bottom`：对应轴 stretch 时用 `EditorPropertyFloat (suffix=px)` 替代默认 widget，通过 `add_property_editor(path, ed, false, p_label)` 把 label 改成 `Left / Top / Right / Bottom`。Inspector plugin 已经在 `ControlEditorPlugin` 构造器里 add_inspector_plugin（行 1741）注册过，无需新增注册。 |

### Behavioral matrix

| LayoutPreset | anchor (L,T,R,B) | stretch_x | stretch_y | Position / Size | Left | Top | Right | Bottom |
|---|---|---|---|---|---|---|---|---|
| TOP_LEFT | (0,0,0,0) | F | F | ✏️可编辑 | — | — | — | — |
| TOP_RIGHT | (1,0,1,0) | F | F | ✏️ | — | — | — | — |
| BOTTOM_LEFT | (0,1,0,1) | F | F | ✏️ | — | — | — | — |
| BOTTOM_RIGHT | (1,1,1,1) | F | F | ✏️ | — | — | — | — |
| CENTER_LEFT | (0,0.5,0,0.5) | F | F | ✏️ | — | — | — | — |
| CENTER_TOP | (0.5,0,0.5,0) | F | F | ✏️ | — | — | — | — |
| CENTER_RIGHT | (1,0.5,1,0.5) | F | F | ✏️ | — | — | — | — |
| CENTER_BOTTOM | (0.5,1,0.5,1) | F | F | ✏️ | — | — | — | — |
| CENTER | (0.5,0.5,0.5,0.5) | F | F | ✏️ | — | — | — | — |
| LEFT_WIDE | (0,0,0,1) | F | T | 🔒只读 | — | ✏️ | — | ✏️ |
| RIGHT_WIDE | (1,0,1,1) | F | T | 🔒 | — | ✏️ | — | ✏️ |
| VCENTER_WIDE | (0.5,0,0.5,1) | F | T | 🔒 | — | ✏️ | — | ✏️ |
| TOP_WIDE | (0,0,1,0) | T | F | 🔒 | ✏️ | — | ✏️ | — |
| BOTTOM_WIDE | (0,1,1,1) | T | F | 🔒 | ✏️ | — | ✏️ | — |
| HCENTER_WIDE | (0,0.5,1,0.5) | T | F | 🔒 | ✏️ | — | ✏️ | — |
| FULL_RECT | (0,0,1,1) | T | T | 🔒 | ✏️ | ✏️ | ✏️ | ✏️ |
| 自定义 anchor (任一轴 stretch) | 任意 | (任一为 T) | 🔒 | 全部 anchor_/offset_/grow_ 字段保留可见（既有行为不变） |
| 自定义 anchor (两轴均不 stretch) | 任意 | F | F | ✏️ | 全部 anchor_/offset_/grow_ 保留 |

✏️ = 可编辑；🔒 = READ_ONLY 灰显但运行时值仍可见；— = inspector 不显示

## Impact

- **二进制重编译**：是。`scene/gui/control.cpp` 改动会拉动 scene library 重链；`editor/scene/gui/control_editor_plugin.cpp` 拉 editor library 重链。无 ABI 影响（全在 `_validate_property` / `parse_property` 等 virtual 内部）。
- **GDScript / 老项目**：行为零变化。`_validate_property` 只影响 inspector 显示与否、是否只读，不影响 `set/get_property` 的运行时语义；`get_property_list` 在游戏运行时本就不被引擎查询。
- **现有 commit `657e421a73`** ("锁定整矩形模式下size/position") 是这套方案的雏形，仅锁了 `PRESET_FULL_RECT`。`b25586b17b` 把同一思路一般化到所有 stretching 场景并补上标签改造。两者一起构成 v1。
- **风险**：低。判据基于 `data.anchor[SIDE_*]` 这个引擎自己长期使用的状态字段；Inspector 插件改的是 v1 路径，无 GDExtension ABI 风险。
- **后续**："阶段 3"（2D viewport 拖拽对齐）和"阶段 2b 精修"（自定义 RectTransformEditor 让单轴 stretch 仍能保留 fixed 轴的 Pos/Size 可编辑）可以独立提案。
