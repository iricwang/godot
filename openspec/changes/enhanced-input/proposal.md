## Why

Godot 4.7 的 input 系统位于 `core/input/` 之下, 核心是 `Input` 单例 (`input.h/.cpp`) 和 `InputMap` 单例 (`input_map.h/.cpp`). 它提供:

- 扁平的 `action_name -> List<InputEvent>` 映射 (`InputMap::input_map`)
- 每帧从 DisplayServer 接收原始事件, 计算 `pressed / strength / raw_strength` 并暴露 `is_action_pressed / is_action_just_pressed / is_action_just_released / get_action_strength / get_action_raw_strength / get_axis / get_vector`
- Per-device 状态追踪, programmatic `action_press` / `action_release`, accumulated input toggle

这套系统在"按键 → 是否按下"层面是稳固的, 但对照 Unreal Engine 5 的 Enhanced Input 框架, 缺失了五项关键能力:

| 缺口 | 表现 | 影响 |
|---|---|---|
| **无 Context 栈** | 只有一个全局 `InputMap`, 没有"探索模式 / 战斗模式 / UI 模式"的概念 | 模式切换必须手写 enable/disable, 极易出错 |
| **无 Trigger 机制** | 没有 Hold / Tap / DoubleTap / Chord / Pulse 的内建实现 | 蓄力、连击、组合键全要手写计时器, 写 Plinko 蓄力时尤其痛 |
| **无 Modifier 链** | 只有 action 级别一个 `deadzone`; `get_vector` 给一个单点死区 | Negate / Scale / Normalize / Swizzle 全要手写 |
| **无 TriggerEvent 生命周期** | 只有 pressed / just_pressed / just_released / strength | 没有 Started / Triggered / Ongoing / Completed / Canceled, 写"按住的连续扣血"或"松开瞬间算完成"很别扭 |
| **无 Value Type 抽象** | 全部塞成 `float strength`; `get_vector` 把 4 个 action 拼成 Vector2 是临时拼凑 | 3D 游戏的角色移动 / 摄像机控制没法用统一的"Axis3D"动作表达 |

Godot 4.7 的场景层派发 (`scene/main/viewport.cpp`: `push_input → _gui_input_event → Node._input → shortcut_input`, `push_unhandled_input` 已 deprecated) 是清晰的 hook 模型, 我们的补强层可以 **平行于 InputMap 挂在 `Node._input` 钩子上**, 不破坏既有项目.

## What Changes

- **新增 GDExtension C++ 模块** `enhanced_input`, 编译为独立动态库 (`.dll`/`.so`/`.dylib`), **不修改 Godot 引擎源码**, 不需要重新编译 `godot.exe`
- **Resource 类型** (4 类, 全部继承 `Resource`):
  - `EIAction` — 逻辑动作, 携带 ValueType + default modifiers + default triggers
  - `EIMappingContext` — 键位上下文, 含 key→action 映射列表 + 优先级 + 栈语义
  - `EIModifier` — 修饰器抽象基类, 内建 5 种: DeadZone / Negate / Scale / Normalize / SwizzleAxis
  - `EITrigger` — 触发器抽象基类, 内建 7 种: Pressed (默认) / Hold / Tap / DoubleTap / Pulse / Chord / Release
- **Node 类型** (2 类, 继承 `Node`):
  - `EISubsystem` — Autoload 单例, 管理 context 栈 + action 状态 + 每帧派发
  - `EIComponent` — 挂在角色 / Pawn 上的绑定组件, 调用 `bind_action(action, ETriggerEvent::Triggered, this, &Foo::on_jump)`
- **值类型 4 种首发**: `Bool` / `Axis1D` (float) / `Axis2D` (Vector2) / `Axis3D` (Vector3)
- **触发事件 5 种**: `Started` / `Triggered` / `Ongoing` / `Completed` / `Canceled`, 与 UE 对齐
- **桥接**: 提供 `EIBridge.import_action() / import_context()` 工具, 把 Godot 原 InputMap 中的 action 转换为 `EIAction` + 简单 `EIMappingContext`, 老项目可渐进迁移
- **编辑器**: 自定义 `EditorInspectorPlugin`, 让 `.tres` 资源可点选 modifier / trigger
- **集成 demo**: 在 `plinko_game/` 下加 3 个 IMC (菜单 / 游戏中 / 暂停), 演示模式切换与蓄力 trigger

## Capabilities

### New Capabilities

- `enhanced-input-action`: 逻辑动作抽象, ValueType 4 选 1, default modifiers/triggers 列表
- `enhanced-input-mapping-context`: 上下文资源, key→action 映射 + per-mapping modifiers/triggers, 优先级 + 栈 push/pop
- `enhanced-input-modifier`: 修饰器抽象, 5 种内建实现
- `enhanced-input-trigger`: 触发器抽象, 7 种内建实现, 5 种 TriggerEvent 生命周期
- `enhanced-input-value`: 一等值类型包装, 4 种 variant 适配
- `enhanced-input-subsystem`: Autoload 单例, context 栈管理 + 每帧派发
- `enhanced-input-component`: 绑定 Node, callback 分发
- `enhanced-input-inspector`: 编辑器侧自定义 Inspector
- `enhanced-input-bridge`: 桥接 Godot 原 InputMap

### Modified Capabilities

None. 现有 `Input` / `InputMap` / `Viewport` 流程完全不修改.

## Impact

- **新增仓库** (在 Godot 引擎 workspace 之外): `D:\AI_Temp\Godot\enhanced_input_gdextension\`
- **新增 C++ 源文件**: 约 30 个 (`.h` + `.cpp`), 总计 ~3500-4500 行
- **编译产物**:
  - `bin/libenhanced_input.windows.template_debug.x86_64.dll`
  - `bin/libenhanced_input.linux.template_debug.x86_64.so`
  - `bin/libenhanced_input.macos.template_debug.framework`
- **plinko_game 集成**: 3-5 个 `.tres` 资源 + 1 个 Autoload 配置 + 1-2 个 demo 场景脚本
- **性能**: 每帧 `_input` 处理 `O(N_event × M_IMC × K_mapping_per_IMC)`, 简单实现下 < 0.1ms; chord / pulse / hold 等 timer 触发走 `_process`, 增量更新
- **API 兼容**: 不影响 Godot 原 `Input` / `InputMap` API, 老项目继续可用
- **GDExtension 兼容性**: 目标 Godot 4.7 (GDExtension ABI 4.3+), 跟 godot-cpp 4.7 绑定
- **风险**: Godot 4.7 仍为 beta, GDExtension API 可能有微调, 需跟踪 4.7 final release notes
