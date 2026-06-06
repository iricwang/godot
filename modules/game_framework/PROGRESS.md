# game_framework — 进度与后续计划(交接文档)

> 开工先读本文件。这是 Godot 自定义游戏框架 module 的进度 + 路线图。
> 位置:`D:/AI_Temp/Godot/godot-4.7-beta/modules/game_framework`
> 源自 `deepseek_html_20260605_050006.html` 设计方案 + 优化评审。

## 1. 形态与总体决策

- **引擎 module**(非 GDExtension/godot-cpp),和同引擎的 `lua`/`psd_ui` 并列。无 ABI 坑、API 全自由。
- **C++ 底座 + GDScript 业务**:核心类 C++ 实现并暴露给 GDScript;业务 ViewModel/Activity 用 GDScript 继承。
- **目标全平台**(移动/PC/Web/主机)——这条约束否决了原方案的 native 动态插件。
- 构建:仓库根 `build.bat`(已去掉 lua 开关)。`python -m SCons platform=windows target=editor tests=yes -j31`。

## 2. 已完成(阶段 0–7,全部编译通过)

| 目录 | 类 | 基类/形态 | GDScript API 摘要 |
|---|---|---|---|
| `context/` | `Context` | Object | `get_application`/`get_service`/`start_activity`/`show_dialog`/`show_toast`/`load_resource_async`/`register_activity` + 静态 `bind`/`bind_property`/`bind_command`/`make_toast` |
| `context/` | `Application` | Context | `initialize(root)`/`shutdown`/`get_service_registry`/`get_resource_manager`/`get_activity_manager` |
| `service/` | `ServiceRegistry` | Object(非单例) | `register_service(name,obj)` / `get_service(name)` / `has_service` / `unregister_service` / `get_service_names` |
| `resource/` | `ResourceHandle` | RefCounted | `get_path`/`get_state`/`is_loaded`/`get_resource`/`load_sync` + 信号 `loaded(res)`/`failed(path)` |
| `resource/` | `ResourceManager` | Object(非单例) | `load_sync(path)` / `load_async(path,callable,priority)` / `get_handle(path)` |
| `ui/` | `Intent` | RefCounted | `action`/`data`/`extras`/`flags` + `set_flag/has_flag`;Flag:`FLAG_SINGLE_TOP`/`FLAG_CLEAR_TOP` |
| `ui/` | `Activity` | Control + Context 成员 | 生命周期 override + `get_context`/`start_activity`/`show_toast`/`get_service`/`load_resource_async` 等便捷方法 |
| `ui/` | `Transition` | Resource | `enter_type`/`exit_type`(NONE/FADE/SLIDE_*/SCALE)/`duration`;`play_enter(node)`/`play_exit(node)`→Tween |
| `ui/` | `ActivityManager` | Object(非单例) | 新增检查API: `get_stack_activity(idx)`/`get_dialog_count`/`get_dialog(idx)`/`get_toast_queue_count`/`get_toast_queue_item(idx)`/`is_toast_active` |
| `ui/` | `Dialog` | Control + Context 成员 | `_on_create`/`_on_dismiss`;`get_context`/`show_toast`/`get_service` 等便捷方法;`dismiss()`;`set_lifecycle_owner`/`get_lifecycle_owner` |
| `ui/` | `Toast` | RefCounted | `Toast.make_text(text,duration)`;`text`/`duration`;`set_owner/get_owner/is_owned_by`;`set_custom_scene/get_custom_scene` |
| `mvvm/` | `ObservableProperty` | RefCounted | `set_value`/`get_value`/`subscribe(cb)`/`unsubscribe(cb)` + 信号 `value_changed(value)` |
| `mvvm/` | [`BaseViewModel`] | GDScript 包装 (`base_view_model.gd`) | 继承 C++ `ViewModel`，GDScript 层 `_set`/`_get` 使 `self.xxx = yyy` 原生属性语法可用 |
| `mvvm/` | `ViewModel` | RefCounted | C++ `_set`/`_get`/`_get_property_list` 重写 → 路由到 `ObservableProperty` map；保留 `set_property`/`get_value` 字符串 API |
| `mvvm/` | `BindingEngine` | 静态工具 | `bind(view,vm)`(扫 meta)/`bind_property(target,prop,vm,src)`/`bind_command(source,signal,vm,method)` |

无全局单例。Application 由 GDScript 创建并初始化，所有管理器为 Application 成员变量。

## 3. 相对原 HTML 方案已落实的优化(勿回退)

1. GDExtension → **引擎 module**。
2. 资源 Handle **去手动三重引用计数** → 只做异步状态机+信号,卸载交 `Ref`+`ResourceLoader`。
3. 仿 Android **砍多 task/affinity**,只单任务栈 + `SINGLE_TOP`/`CLEAR_TOP`。
4. native dlopen 插件 → **`ServiceRegistry` 编译期服务发现**(全平台)。
5. Dialog/Toast 用 **Control 覆盖层**(非 OS Window)。
6. MVVM 用 **Godot 信号**(自动断连,无悬挂);BindingEngine **显式 API + meta 糖 + 校验**;属性绑定按 **ObjectID** 寻址(目标销毁自动 no-op)。
7. 异步加载 **Web/无线程平台自动降级为同步**。

## 4. GDScript 用法速查

```gdscript
# === 初始化（主场景 _ready） ===
var app: Application

func _ready():
    app = Application.new()
    app.initialize($UIRoot)
    app.register_activity("main", "res://ui/main_activity.tscn")
    app.register_activity("detail", "res://ui/detail_activity.tscn")
    app.start_activity(Intent.create("main"))

# === main_activity.tscn 根节点: extends Activity ===
var vm = preload("res://vm/main_vm.gd").new()   # extends base_view_model
func _on_create(saved_state):
    # ViewModel 原生属性语法: self.xxx = yyy → 自动 ObservableProperty 通知 + BindingEngine 同步 UI
    vm.title = "Hello"; vm.hp = 100
    # 绑定方式 1: Context 静态方法
    Context.bind_property($TitleLabel, "text", vm, "title")
    Context.bind_property($HpBar, "value", vm, "hp")
    Context.bind_command($GoBtn, "pressed", vm, "on_go")
    # 绑定方式 2: 直接调 BindingEngine（等价）
    # BindingEngine.bind_property($TitleLabel, "text", vm, "title")

func _on_destroy(): vm.dispose()

# === 跳转 + Toast + Dialog（Activity 内，用便捷方法） ===
func _go():
    var it = Intent.new(); it.action = "detail"; it.extras = {"id": 7}
    it.set_flag(Intent.FLAG_SINGLE_TOP)
    start_activity(it)                                    # Activity 便捷方法
    show_toast(Context.make_toast("已打开详情", 2.0))      # 等价于 Toast.make_text(...)

# === 也可以通过 get_context() 访问更多能力 ===
func _another():
    get_context().register_activity("settings", "res://ui/settings.tscn")
    get_context().load_resource_async("res://big.png", func(res): $Tex.texture = res)

# === 资源加载 ===
func _load():
    load_resource_async("res://big.png", func(res): $Tex.texture = res)
    # 或: get_context().load_resource_async(...)
```

## 5. 后续路线图(待做)

### 阶段 5 — 集成 + GDScript demo ✅ (已完成，2026-06-06)

**Demo 项目**: `D:/AI_Temp/gf_test`，主场景 `res://scenes/main.tscn`。

**文件结构**:
```
gf_test/
  project.godot                          # run/main_scene="res://scenes/main.tscn"
  scenes/
    main.tscn                            # UIRoot (Control) + entry.gd
    main_activity.tscn                   # 根节点 type="Activity"
    detail_activity.tscn                 # 根节点 type="Activity"
    dialog_confirm.tscn                  # 根节点 type="Dialog"
  scripts/
    entry.gd                             # 入口: Application 创建 + 自动测试
    main_activity.gd                     # extends Activity — 全量 API 测试
    detail_activity.gd                   # extends Activity — Intent extras 传递 + 返回栈
    dialog_confirm.gd                    # extends Dialog — Context 便捷方法
    debug_overlay.gd                     # Debug 堆栈查看器 — Ctrl+Shift+` 切换
    base_view_model.gd                   # MVVM 包装基类 (extends ViewModel, GDScript _set/_get → self.xxx 语法)
    main_vm.gd                           # extends base_view_model (counter/title/progress/status)
    detail_vm.gd                         # extends base_view_model (id/from_page/description)
    dialog_vm.gd                         # extends base_view_model (title/message/result)
    toast_custom.gd                      # 自定义 Toast 示例 — 圆角+图标+半透明
  scenes/
    toast_custom.tscn                    # 自定义 Toast 场景
```

**运行时命令**:
```bash
# 交互运行（GUI，手动点按钮）
bin\godot.windows.editor.x86_64.exe --path D:/AI_Temp/gf_test

# 自动化测试（headless，7 项 assert 全通过）
bin\godot.windows.editor.x86_64.console.exe --headless --path D:/AI_Temp/gf_test
```

**自动化验证结果（2026-06-06，exit 0 零断言失败）**:

| 测试 | 验证点 | 结果 |
|---|---|---|
| Test 1 — 服务发现 | `get_current_activity()` → Activity 类型; `has_service("test")` → true; `get_context().get_application() == app`; `Context.get_service("test").ping(...)` → "pong: automated test"; `ServiceRegistry.get_service(...)` 直接路径 | ✅ |
| Test 2 — 导航 | 初始 `stack_size == 1`; `start_activity(detail)` → `stack_size == 2`; 生命周期: `_on_pause → _on_create → _on_start → _on_resume → _on_stop` 顺序正确; `get_current_activity().get_intent().action == "detail"`; Intent extras `{id:99, from:"autotest"}` 传递正确 | ✅ |
| Test 3 — Dialog | `show_dialog(confirm_dialog)` 触发 `ConfirmDialog._on_create`; `Context.bind_property` 绑定 title/message 正确 | ✅ |
| Test 4 — Toast | `show_toast(Context.make_toast(...))` 入队 + FIFO 显示 | ✅ |
| Test 5 — 返回栈 | `back()` → DetailActivity `_on_back_pressed` → `_on_destroy` → MainActivity `_on_resume`; `stack_size 2→1` | ✅ |
| Test 6 — Dialog owner | `show_dialog_with_owner(app)` → 跳转 detail → Dialog 未被清理（owner=App） | ✅ |
| Test 7 — FLAG 增强 | NO_HISTORY / NEW_CLEAR / REORDER_TO_FRONT 全通过 | ✅ |
| 收尾 | `Application.shutdown()` 触发 `cleanup_all()` → Dialog dismiss + Activity destroy | ✅ |

**Activity 便捷方法验证清单**（MainActivity 通过按钮回调覆盖）:
`start_activity()` / `show_dialog()` / `show_toast()` / `get_service()` / `has_service()` / `get_resource_handle()` / `load_resource_async()` / `back()` / `get_context()` — 全部通过。

**开发辅助**:
- `Ctrl+Shift+`` ` 打开/关闭 Debug 堆栈查看器，实时显示 Activity 栈 / Dialog / Toast / Application 状态。
- `ActivityManager` 提供 `get_stack_activity(idx)` 等 6 个检查 API。
- 自定义 Toast：`toast.set_custom_scene("res://scenes/toast_custom.tscn")`。
- 转场：三个 demo 组件均已配置 Transition（Main: SLIDE_RIGHT→FADE, Detail: SLIDE_LEFT→SLIDE_RIGHT, Dialog: SCALE→FADE）。

**已知问题**:
- `load_resource_async("res://icon.svg")` 因 demo 项目无此文件返回 null，框架层 `ResourceManager::_poll()` 正确处理 `THREAD_LOAD_FAILED`。
- `ServiceRegistry` 存裸 `Object*`，`entry.gd` 需用成员变量 `_test_svc` 持有引用防止 RefCounted GC 回收（PROGRESS.md §9 记录的已知约束）。

### 阶段 6 — 文档
- 可选:`doc_classes/*.xml`(Godot module 内置文档)或一份 markdown 使用手册。

### 阶段 7 — 压测(用真实数据替换原 HTML 的预估)
- MVVM:建 1e4 个 `ObservableProperty` + 绑定,批量 `set_value`,测单帧耗时(原估 <3ms,需实测)。
- 资源:批量 `load_async` 千张纹理,测完成时间 + 确认主线程不阻塞(原估 <0.8s)。
- Activity 切换:测转场 60fps + 调度耗时(原估 <0.2ms)。

### 阶段 8 — Context/Application 架构重构 ✅ (已完成，2026-06-06)

**目标**：仿 Android 系统，将单例模式改为 `Context → Application` 层级体系。

**核心变更**:
- **新增 `Context` 类**(`context.h/.cpp`)：继承 Object，持有 `Application*`，提供统一服务访问入口（导航/资源/覆盖层/MVVM/服务发现）。
- **新增 `Application` 类**(`application.h/.cpp`)：继承 Context，拥有 `ServiceRegistry`/`ResourceManager`/`ActivityManager` 实例（成员变量，非单例）。
- **Activity/Dialog 新增 Context 能力**：各持有 `Context*` 成员（由 ActivityManager 创建时注入），并重导出高频方法（`start_activity`/`show_toast`/`get_service`/`load_resource_async` 等），实现 "Activity IS-A Context" 的 GDScript 体感。
- **彻底移除 Engine 级单例**：`ServiceRegistry`/`ResourceManager`/`ActivityManager` 不再注册为 Godot 单例，由 Application 独有。
- **ActivityManager 新增 `cleanup_all()`**：drain 全部 Activity/Dialog/Toast，供 `Application::shutdown()` 调用。

**GDScript API 变化**:
```gdscript
# 旧（全局单例）
ActivityManager.set_root($UIRoot); ActivityManager.register_activity(...)

# 新（Application 驱动）
var app = Application.new(); app.initialize($UIRoot)
app.register_activity("main", "res://..."); app.start_activity(Intent.create("main"))

# Activity 内（通过便捷方法）
start_activity(it); show_toast(Context.make_toast("hello", 2.0))
# 或通过 get_context()
get_context().start_activity(it)
```

### 阶段 9 — Dialog/Toast 生命周期绑定 + Activity Flag 增强 ✅ (已完成，2026-06-06)

**第一部分：Dialog/Toast 生命周期绑定**
- **Toast 新增 `owner_id` 字段**（ObjectID）：创建时通过 `set_owner(Object*)` 绑定归属 Context
- **Dialog 新增 `lifecycle_owner` 字段**（Object*，命名避让 Node::set_owner）：Activity 销毁时自动 dismiss 归属 Dialog
- **ActivityManager 新增辅助方法**：`_dismiss_owned_dialogs(p_owner)` / `_cancel_owned_toasts(p_owner)` / `_resolve_default_owner()`
- **Context 新增 `show_dialog_with_owner(intent, owner)` / `show_toast_with_owner(toast, owner)`**
- **Activity/Dialog 便捷方法自动传入 `this` 作为 owner**：`activity.show_dialog(it)` → owner=该Activity，销毁时自动清理

**owner 默认策略**：
| 调用方式 | owner 默认值 |
|---|---|
| `activity.show_dialog(intent)` | 该 Activity 自身 |
| `activity.show_toast(toast)` | 该 Activity 自身 |
| `app.show_dialog(intent)` | 当前栈顶 Activity |
| `show_dialog_with_owner(intent, null)` | 当前栈顶 Activity |

**第二部分：Activity Flag 增强**
- **新增 `FLAG_NO_HISTORY`** (1<<2)：Activity 离开栈顶时自动 destroy，不在返回栈中留痕。Activity 新增 `no_history` 字段。
- **新增 `FLAG_REORDER_TO_FRONT`** (1<<3)：目标 Activity 在栈中 → 移到栈顶，不销毁任何 Activity。已在栈顶则降级为 SINGLE_TOP。
- **新增 `FLAG_NEW_CLEAR`** (1<<4)：启动前清空整个栈+所有 Dialog，目标成为新栈底。

**start_activity Flag 处理优先级**：NEW_CLEAR → SINGLE_TOP → CLEAR_TOP → REORDER_TO_FRONT → 正常新建

**自动化验证（2026-06-06，headless 7 项全通过）**：
| 测试 | 结果 |
|---|---|
| Test 3 — Dialog owner=Application | 启动 detail → Dialog 未被清理 ✅ |
| Test 4 — Toast owner 生命周期 | Main.finish() → Toast 随 Main destroy 清除 ✅ |
| Test 5 — FLAG_NO_HISTORY | NO_HISTORY detail 被新 main 覆盖时自动 remove ✅ |
| Test 6 — FLAG_NEW_CLEAR | 清栈+清Dialog → stack_size==1 ✅ |
| Test 7 — FLAG_REORDER_TO_FRONT | main 从栈底移到栈顶（_on_new_intent） ✅ |

### 阶段 10 — Debug 堆栈查看器 + Toast 自定义场景 + 转场示例 ✅ (已完成，2026-06-06)

**第一部分：ActivityManager 检查 API**
- 新增 6 个只读检查方法，供调试面板/外部工具使用：`get_stack_activity(idx)` / `get_dialog_count()` / `get_dialog(idx)` / `get_toast_queue_count()` / `get_toast_queue_item(idx)` / `is_toast_active()` — 全部绑定到 GDScript。

**第二部分：Toast 自定义场景**
- **Toast 新增 `custom_scene` 属性**（String）：指向 `.tscn` 路径，非空时 `_show_next_toast()` 实例化该场景替代默认 PanelContainer+Label。
- **约定**：自定义场景根节点若有 `_on_toast_bind(toast: Toast)` 方法，实例化后自动调用传入 toast 数据。
- **fallback**：custom_scene 为空或加载失败 → 回退到默认 PanelContainer+Label Toast。
- **Demo 示例** `toast_custom.tscn`：圆角 StyleBoxFlat + ★ 图标 + 半透明背景。

**第三部分：Debug 堆栈查看器**
- **文件** `D:/AI_Temp/gf_test/scripts/debug_overlay.gd`：全屏覆盖层，实时显示 Activity 栈 / Dialog 列表 / Toast 队列 / Application 状态。
- **快捷键** `Ctrl+Shift+`` ` 打开/关闭（`KEY_QUOTELEFT` 反引号键）。
- **自动刷新**：每秒从 ActivityManager 检查 API 重新读取并更新 RichTextLabel（bbcode 表格）。
- Esc 键关闭。

**第四部分：转场示例**
- MainActivity：入场 `SLIDE_RIGHT` 0.3s / 出场 `FADE` 0.25s
- DetailActivity：入场 `SLIDE_LEFT` 0.3s / 出场 `SLIDE_RIGHT` 0.25s
- ConfirmDialog：入场 `SCALE` 0.25s / 出场 `FADE` 0.2s

**GDScript API 示例**：
```gdscript
# 自定义 Toast
var toast = Context.make_toast("★ 自定义 Toast", 3.0)
toast.set_custom_scene("res://scenes/toast_custom.tscn")
show_toast(toast)

# 调试面板 — Ctrl+Shift+` 打开，查看栈/Dialog/Toast/服务
var am = app.get_activity_manager()
for i in am.get_stack_size():
    print(am.get_stack_activity(i).get_intent().action)
```

**验证结果（headless 7 项全通过）**：
| 测试 | 结果 |
|---|---|
| Test 1-7 原有测试 | 全部 OK ✅ |
| `_show_next_toast` custom_scene 分支编译 | 零错误 ✅ |
| 转场属性赋值 | headless 无 Tween 冲突 ✅ |

### 阶段 11 — MVVM 原生属性语法 ✅ (已完成，2026-06-06)

**目标**：摆脱 `vm.set_property("counter", 5)` / `vm.get_value("counter")` 的字符串 API，支持原生 `self.counter = 5` / `self.counter` 语法。

**实现方案**：两层 _set/_get 路由

1. **C++ `ViewModel`**：重写 Godot Object 的 `_set` / `_get` / `_get_property_list`，路由到内部 `ObservableProperty` map。`set_property("counter", 5)` → `get_property("counter")->set_value(5)` → `emit value_changed`。

2. **GDScript `base_view_model.gd`**（8 行包装层）：继承 C++ `ViewModel`，在 GDScript 层声明 `_set`/`_get`，转发到 C++ `set_property`/`get_value`。用户 extends 这个而不是直接 extends ViewModel。

**原理**：GDScript 解析器检查脚本继承链中是否有 GDScript 层 `_set`/`_get` 声明 → 有则放行 `self.xxx` 语法。运行时 `self.counter = 5` → `base_view_model._set("counter", 5)` → C++ `set_property → ObservableProperty.set_value` → `value_changed` 信号 → `BindingEngine` 自动同步绑定的 UI 节点。

**GDScript 使用对比**：
```gdscript
# 旧（字符串 API，仍可用但不推荐）
extends ViewModel
func _init(): set_property("counter", 0)
func on_inc(): set_property("counter", get_value("counter") + 1)

# 新（原生属性，推荐）
extends "res://scripts/base_view_model.gd"
func _init(): self.counter = 0; self.title = "Hello"
func on_inc(): self.counter += 1   # → _get → _set → ObservableProperty → 自动更新 UI
```

**BindingEngine / ObservableProperty / Context 完全不动**。旧 API 仍然可用（向后兼容）。

**验证**：headless 7 项全部通过，`vm.counter += 1` 无解析错误，UI 计数正确更新。

### 阶段 12 — 可选增强 / 已知简化(目前的 TODO)
- `Activity.dispatch_create` 目前传**空 saved_state**;真正的状态保存/恢复未做(参数走 `intent.extras`)。
- `Dialog` 的 `dismiss_on_outside` / 遮罩输入拦截**未内置**(目前由 dialog 场景自己画遮罩/处理点击)。
- `ResourceManager.load_async` 的 `priority` 参数**当前忽略**(未接入优先级队列)。
- `ObservableProperty` **无节流/批量更新**(变化即同步通知);万级高频更新若有压力可加批处理。
- `Transition` 的 `CUSTOM`(自定义动画/AnimationPlayer)**未实现**(枚举里也没留,需要时加)。
- `BindingEngine` 无显式 `unbind(view)`;靠 `vm.dispose()` + Godot 对象销毁自动断连。如需提前精确反绑定,后续补。
- `ServiceRegistry` 存裸 `Object*`(生命周期由注册方负责);如需强引用/类型校验可增强。

## 6. 构建与验证

- 编译:`build.bat`(增量;新增/改文件后重跑)。`ui/`、`mvvm/`、`resource/`、`service/` 已在 `SCsub` 通配,新增 `.cpp` 自动纳入。
- headless 冒烟:`bin\godot.windows.editor.x86_64.console.exe --headless --editor --quit --path <项目>`。
- **已知非问题**:4.7 master(dev 版)在**引擎二进制重编后首次** headless editor quit 会偶发退出崩溃(`EditorNode ... singleton is null` + 0xC0000005),**预热后(第 2 次起)稳定 exit 0**,与框架无关,GUI 使用不受影响。验证时跑 2 次、看第 2 次。

## 7. 关键路径

- 注册入口:`register_types.cpp`(`MODULE_INITIALIZATION_LEVEL_SCENE` 注册全部类,无 Engine 级单例)。
- Application 由用户 GDScript 创建:`var app = Application.new(); app.initialize($UIRoot)`。
- Context 层级:`Application → Context → ActivityManager/ResourceManager/ServiceRegistry`;Activity/Dialog 通过组合持有 `Context*` 实现对公共服务的访问。
- 构建脚本:`SCsub`(通配 `*.cpp` + 各子目录,根目录 `*.cpp` 自动纳入 context/application)、`config.py`。
- 异步轮询:`ResourceManager::_ensure_polling` 连接 `SceneTree::process_frame`。
- 转场:`Transition::play_enter/exit` 用 `Control::create_tween`;FADE/SCALE 改 modulate/scale(anchor-safe),SLIDE 改 position(ActivityManager 把 Activity 放在 (0,0)+显式 size)。
