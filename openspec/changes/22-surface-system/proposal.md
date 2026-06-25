# 22 — Surface 框架：6 层分层 + 形态策略

> openspec 草案。**这是规划阶段**，未动工。
> 关联模块：`game_framework/ui` (阶段 0-21 已有 Activity/Dialog/Toast/Transition + IContext)
> 关联 demo：`D:/AI_Temp/gf_test`

## Why

`game_framework/ui` 当前有 Activity（页面）、Dialog（对话框）、Toast（提示）、Transition（转场）四块，已经能撑起 80% 的常规游戏 UI 流程（阶段 0-21 已验证 18+ 项 headless 测试）。但"半透的、全屏的"这类需求**只能套在现有类型上变通实现**，导致：

1. **没有"层 (Layer)" 概念**。HUD 挂在游戏世界上方、Modal 居中遮罩、Debug 压所有界面——这三类"应该画在哪一层、属于哪个 z-order 范围"在现有架构里**靠 reparent 手写**。
2. **没有"形态 (Kind)"概念**。半透 HUD、不抢焦点的浮空提示、抢焦点的全屏菜单、阻塞下层的模态——这些**只是行为不同**，但 Activity/Dialog/Toast 强行三分，"行为相似的界面被分到不同类，行为不同的界面被塞进同类"。
3. **输入路由是隐式的**。Dialog 现在是"手动画遮罩 + 自己处理点击"（PROGRESS §14 已知简化），没有"modal 推开时自动让下层 Control 失效"的统一机制。
4. **转场策略是写死的**。Transition 现在只对 Activity 派发，Dialog/Toast 各自实现自己的入场（PROGRESS §10 转场示例里 Dialog SCALE→FADE、Toast 是 fade tween），没有"按 Kind 选默认转场"的可扩展点。
5. **缺调试层 (System)**。PROGRESS §10 的 debug_overlay 用全局覆盖层，跟业务 UI 抢同一个 z 平面，业务 Modal 弹出时调试面板会被压。

对照开源实现：

| 方案 | 关键抽象 | 我们的取舍 |
|---|---|---|
| **GameFramework (Unity)** UIGroup + UIForm | 同一 Group 内 Form 栈式管理，Group 之间有 Depth | ✅ 跟这个最像，复用 Activity 栈模型 |
| **UMG (Unreal) / Slate** | WidgetTree + Slot，每个 widget 一个 slot | ❌ 太重，Godot Control 树已经够用 |
| **RmlUi** | Context + Document + Element (CSS-like) | ❌ 不适合 C++ engine module 集成 |
| **Dear ImGui / ImGuiX** | Immediate-mode + Window | ❌ 不适合做主 UI；可以给 Debug 层用 |
| **Cocoa Touch** | UIWindow + ViewController + Presenter | ✅ 借鉴"Layer 决定 root 上下文"思路 |

**结论**：现有 Activity 栈 + Dialog + Toast 的方向是对的，**缺的是显式分层 + 形态策略**。Surface 框架不是另起炉灶，而是把现有的"ActivityManager + Dialog + Toast 三个管理器"**统一到"6 个 Layer 的 SurfaceManager 集合"下**。

## What Changes

### 1. 概念引入

```
Layer（渲染层）  ←── 固定 6 层，按 z-order 排好，不可改顺序
   │
   ▼
SurfaceKind（界面形态）←── 决定输入/焦点/转场/动画语义
   │
   ▼
具体类（Activity / Dialog / Toast / Hud / Tooltip / DebugPanel）
```

### 2. 6 个固定 Layer（自下而上）

| idx | 名字      | 用途                                        | 典型形态                       | 渲染             |
| --- | --------- | ------------------------------------------- | ------------------------------ | ---------------- |
| 0   | `WORLD`   | 3D 场景挂的 UI（血条浮空、伤害数字）        | Hud (Passthrough)              | CanvasLayer      |
| 1   | `GAME`    | 游戏内常驻 HUD（小地图、角色状态、操作按钮）| Hud (Passthrough 局部)         | CanvasLayer      |
| 2   | `SCENE`   | 场景/关卡内覆盖层（暂停面板、结算面板）     | Activity / Hud                 | CanvasLayer      |
| 3   | `UI`      | 完整的页面/菜单（背包、商店、主菜单）       | Activity (独占)                | CanvasLayer      |
| 4   | `MODAL`   | 模态对话框（确认、错误、教程弹窗）          | Dialog (遮罩+阻塞)             | CanvasLayer      |
| 5   | `SYSTEM`  | 最高优先级（断线重连、错误遮罩、调试）      | 全屏 Dialog / DebugPanel       | CanvasLayer      |

> **关键约束**：
> - 6 个 CanvasLayer 在 `Application::initialize(root)` 时一次性建好，`layer = idx`；
> - 任何 Surface 入栈时 `reparent` 到对应 CanvasLayer，不在 Application root 直接 add_child；
> - 渲染顺序由 CanvasLayer 自带 z-order 保证，业务代码不再写 z_index。

### 3. SurfaceKind 枚举（6 种）

```gdscript
class_name Surface
extends Control

enum Kind { ACTIVITY, DIALOG, TOAST, HUD, TOOLTIP, DEBUG_PANEL }
@export var kind: Kind = Kind.ACTIVITY

# 每种 Kind 有一组默认行为（策略对象决定）：
#   ACTIVITY:     独占同层、focused、转场用 Activity 自己的 Transition
#   DIALOG:       独占同层、focused、转场用 SCALE→FADE、自动画 dim 遮罩
#   TOAST:        短时、不 focused、不阻塞下层、淡入淡出、自动消失
#   HUD:          常驻、跟节点或屏幕坐标、不抢焦点、passthrough 输入
#   TOOLTIP:      跟随鼠标/目标节点、passthrough、淡入淡出
#   DEBUG_PANEL:  全屏、focused、独立快捷键、不被业务 hide 误关
```

### 4. 形态策略对象（参考阶段 20 ActivityLauncher 思路）

每种 Kind 对应一个 `SurfacePolicy` 资源，控制 5 件事：

1. **入栈/出栈行为**（reparent 到哪个 CanvasLayer，是否触发 dim）
2. **输入路由**（是否阻塞下层、是否抢焦点、是否 passthrough）
3. **转场选择**（入场/出场用哪个 Transition 资源）
4. **生命周期适配**（要不要走 Activity 的 6 段生命周期，还是用简化的 _on_show/_on_hide）
5. **资源回收**（对象池 / 节点释放策略）

```cpp
class SurfacePolicy : public Resource {
    GDCLASS(SurfacePolicy, Resource);

public:
    virtual Surface::Layer default_layer() const = 0;
    virtual bool blocks_input() const { return false; }
    virtual bool dim_background() const { return false; }
    virtual Ref<Transition> enter_transition() const;
    virtual Ref<Transition> exit_transition() const;
    virtual void on_attach(Control *p_surface, CanvasLayer *p_layer_root);
    virtual void on_detach(Control *p_surface);
};
```

**6 个内建策略**：`ActivityPolicy / DialogPolicy / ToastPolicy / HudPolicy / TooltipPolicy / DebugPanelPolicy` —— 跟 Activity/Dialog/Toast 已有行为一一对应，新增的 3 个（HUD/TOOLTIP/DEBUG_PANEL）是补缺。

### 5. SurfaceManager 集合

**不删现有 ActivityManager**。新增 `SurfaceManagerSet`：

```cpp
class SurfaceManagerSet : public Object {
    GDCLASS(SurfaceManagerSet, Object);

    SurfaceManager *layers[6];  // 一一对应 WORLD/GAME/SCENE/UI/MODAL/SYSTEM
    HashMap<ObjectID, Ref<SurfacePolicy>> _surface_to_policy;
    // 委托给 ActivityManager/Dialog/Toast 的现有方法, 加 layer 维度
public:
    void push_surface(Control *p_surface, Surface::Kind p_kind);
    void pop_surface(Control *p_surface);
    // 内部：根据 kind → policy → default_layer() → reparent + transition
};
```

**现有 ActivityManager 改造**（最小侵入）：
- `push_activity()` 内部改为 `push_surface(activity, ACTIVITY)`，policy 默认 `ActivityPolicy`（layer=UI）
- `show_dialog()` 内部改为 `push_surface(dialog, DIALOG)`，policy 默认 `DialogPolicy`（layer=MODAL）
- `show_toast()` 内部改为 `push_surface(toast, TOAST)`，policy 默认 `ToastPolicy`（layer=SYSTEM 顶层）
- 老的 `_stack` / `_dialogs` / `_toast_queue` 字段**保留**作为 debug / 兼容性查询，事实数据源迁移到 SurfaceManagerSet

### 6. 输入路由（推荐方案 C + SurfaceManager 辅助）

```gdscript
class_name SurfaceInputRouter
extends RefCounted

# Modal 栈：被 modal 盖住的 layer, 全部 Control 设 MOUSE_FILTER_IGNORE
# Passthrough 节点：MOUSE_FILTER_PASS
# Focused 节点：MOUSE_FILTER_STOP

var _active_modal_layers: Array[int] = []  # 当前阻塞的 layer 索引

func push_modal(layer_idx: int) -> void:
    _active_modal_layers.append(layer_idx)
    _apply_mouse_filter()  # 自动让低于 modal 的所有 layer 的 Control set MOUSE_FILTER_IGNORE

func pop_modal() -> void:
    _active_modal_layers.pop_back()
    _apply_mouse_filter()
```

**键盘焦点**走 Godot 原生 `Control.grab_focus()`，但 SurfaceManager 在 push 时自动 focus top，pop 时 focus 下一层 top（不是 back-buffer 而是 layer-aware 的）。

### 7. HUD 跟 3D/2D 节点绑定

`HudPolicy` 配套一个新工具 `Surface.attach_to_world(target_node, offset, screen_space)`：
- `screen_space = true` → 固定屏幕坐标
- `screen_space = false` → 节点销毁时自动 unbind（ObjectID 失效检测）

## Capabilities

### New Capabilities

- `surface-layer-6-fixed`: 6 个固定 CanvasLayer 一次性建好,layer 顺序和 z-order 不可改
- `surface-kind-enum`: 6 种 SurfaceKind 枚举
- `surface-policy-abstract`: SurfacePolicy 资源抽象基类 + 5 个 hook
- `surface-policy-builtin`: 6 个内建策略 (Activity/Dialog/Toast/Hud/Tooltip/DebugPanel)
- `surface-manager-set`: SurfaceManagerSet 集合,管理 6 个 layer 各自的栈
- `surface-input-router`: 模态/透传/焦点三态输入路由
- `surface-transition-per-kind`: 每种 Kind 绑定自己的转场
- `surface-hud-world-attach`: HUD 跟 3D 节点绑定 (ObjectID-aware)
- `surface-debug-panel-shortcut`: System 层独立快捷键,不被业务 Modal 关掉

### Modified Capabilities

- `activity-manager-push-pop`: 内部委托到 SurfaceManagerSet,API 签名不变
- `context-show-dialog`: 内部委托到 SurfaceManagerSet,API 签名不变
- `context-show-toast`: 内部委托到 SurfaceManagerSet,API 签名不变
- `transition-resource`: 现在可被 SurfacePolicy 引用 (本来就是 Resource,改动小)

## Impact

### 兼容性

- **GDScript API 100% 向后兼容**。`Activity.show_dialog() / show_toast() / start_activity()` 全保留，内部实现改为走 SurfaceManagerSet。
- **现有 18+ 项 headless 测试**零修改（PROGRESS §1-21 累计）回归通过。
- `ActivityManager._stack / _dialogs / _toast_queue` 保留为兼容字段，**事实数据源**改到 SurfaceManagerSet，但**只读访问仍然可用**。

### 新增 C++ 文件

- `ui/surface.h / .cpp` — 抽象基类 / enum
- `ui/surface_policy.h / .cpp` — 策略资源
- `ui/surface_manager.h / .cpp` — 单层 SurfaceManager
- `ui/surface_manager_set.h / .cpp` — 6 层集合
- `ui/surface_input_router.h / .cpp` — 输入路由
- `ui/surface_policies/` 目录 — 6 个内建策略

预计 +800-1200 行 C++。

### 新增 GDScript 文件

- `scripts/surface_input_router.gd` — 鼠标过滤状态机
- `scripts/hud_attach_helper.gd` — HUD 跟节点绑定
- `tests/surface_layer_test.gd` — 6 层压栈/出栈
- `tests/surface_modal_block_test.gd` — 模态阻塞下层
- `tests/surface_passthrough_test.gd` — HUD 透传输入
- `tests/surface_hud_attach_test.gd` — HUD 跟 3D 节点

### 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| 现有测试因 layer 切换回归 | 中 | 保留 ActivityManager._stack 兼容字段,断言不破 |
| Godot 4.7-beta CanvasLayer.z_index 行为微调 | 低 | 6 层用 CanvasLayer.layer (整数),跟 z_index 分离 |
| HUD 跟 3D 节点绑定的 transform 同步 | 中 | 仅在 _process 同步一次,不用 _physics_process |
| Modal 阻塞 + UI 滚动条冲突 | 中 | 单独写测试覆盖;SurfaceManager 在 modal push 时记录"原 focused Control",pop 时 restore |
| DebugPanel 跟业务 Modal 互斥 | 中 | SYSTEM 层永远最高,只接受 SYSTEM 层之上的 push |

## 分阶段路线

### 阶段 22.0 — Surface 基类 + 6 层 CanvasLayer

- C++ `Surface` 抽象 + `Layer` 枚举
- Application 启动时建 6 个 CanvasLayer
- SurfaceManagerSet 集合空壳
- 测试：layer 顺序断言

### 阶段 22.1 — SurfacePolicy + 6 个内建策略

- SurfacePolicy 资源基类
- ActivityPolicy / DialogPolicy / ToastPolicy（照搬现有行为,不改语义）
- HudPolicy / TooltipPolicy / DebugPanelPolicy（新增,先空实现,等阶段 22.4 补）
- 测试：policy 选定 layer / dim 行为

### 阶段 22.2 — ActivityManager 委托改造

- `push_activity` / `show_dialog` / `show_toast` 内部改为 `push_surface`
- 现有 _stack / _dialogs / _toast_queue 保留为只读镜像
- 18+ 项老测试零修改通过

### 阶段 22.3 — SurfaceInputRouter

- Modal 链 + mouse_filter 状态机
- 焦点 push/pop 路由
- 测试：modal 阻塞下层 / passthrough 不抢焦点

### 阶段 22.4 — HUD / Tooltip / DebugPanel 实装

- HudPolicy.attach_to_world 实现
- TooltipPolicy 跟鼠标/目标节点
- DebugPanelPolicy + 独立快捷键注册
- 测试：3D 节点销毁 → HUD 自动 unbind

### 阶段 22.5 — 转场按 Kind 选择

- Activity/Dialog/Toast/Hud/Tooltip/DebugPanel 各绑定默认 Transition
- 测试：kind 切换 → transition 切换

### 阶段 22.6 — 文档 + demo 更新

- PROGRESS 增补
- 6 个新测试纳入 headless 自动化
- `gf_test` 加 HUD/Modal/Debug demo

## 待用户拍板的决策点

1. **方向 A vs B**
   - A：在现有 ActivityManager 上扩，按 kind 路由到 6 个 layer（**改造小，向前兼容好**）
   - B：新建独立 SurfaceManager，Activity/Dialog/Toast 完全委托给它（**架构干净，迁移期长**）
   - **我推荐 A，先 P0+P1 跑通再 B**

2. **HUD 是不是一等公民**
   - 是 → World/Game 两层要实装透输入 + 跟 3D 节点绑定（**给 3D 射击/2D 横版都留好**）
   - 否 → World/Game 两层暂时只放普通 Activity，等真用上再升级
   - **我推荐是**

3. **System 调试层**独立 vs 复用 Modal
   - 独立：独立快捷键、不被业务 Modal 误关（**给 debug_panel.gd 留好**）
   - 复用：少一个 layer，省心智
   - **我推荐独立**

4. **输入拦截**选哪种（**已推荐 C**）
   - C：`Control.mouse_filter` 链 + SurfaceManager 辅助
   - 其他详见 §Why 表

## 验收标准

- 18+ 项老测试零修改通过
- 6 个新测试（layer / modal / passthrough / hud_attach / policy / transition-per-kind）全过
- `gf_test` 加 1 个 demo 同时展示：HUD（不抢焦点）+ Modal（遮罩+阻塞）+ DebugPanel（独立快捷键）
- PROGRESS 增补"阶段 22"
- GDScript API 100% 向后兼容,业务侧零迁移成本
