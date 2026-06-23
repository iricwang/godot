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

### 阶段 13 — MVVM 增强功能 ✅ (已完成，2026-06-08)

**目标**：解除硬编码限制，提升高级场景性能，支持值转换。

**核心变更**:

#### 1. BindingEngine 双向绑定信号动态注册

**问题**：之前硬编码 `_two_way_signals` 数组，无法扩展自定义信号。

**解决方案**：新增静态信号注册 API：
```gdscript
# 注册自定义双向绑定信号
BindingEngine.register_two_way_signal("range_changed")

# 取消注册
BindingEngine.unregister_two_way_signal("range_changed")

# 查询
BindingEngine.has_two_way_signal("range_changed")  # bool
BindingEngine.get_registered_two_way_signals()      # Array
```

**C++ API**：
```cpp
static void BindingEngine::register_two_way_signal(const StringName &p_signal);
static void BindingEngine::unregister_two_way_signal(const StringName &p_signal);
static bool BindingEngine::has_two_way_signal(const StringName &p_signal);
static Array BindingEngine::get_registered_two_way_signals();
```

#### 2. ObservableProperty 批量更新机制

**问题**：万级属性高频更新时，每次 `set_value` 触发 `value_changed` 会造成性能压力。

**解决方案**：新增 `begin_bulk_update()` / `end_bulk_update()`：
```gdscript
# 批量更新：100次赋值 → 1次通知
vm.begin_bulk_update()
for i in range(100):
    vm.counter = i
vm.end_bulk_update()  # 只触发一次 value_changed
```

**C++ API**：
```cpp
void ObservableProperty::begin_bulk_update();  // 抑制中间通知
void ObservableProperty::end_bulk_update();    // 触发最终通知
bool is_in_bulk_update() const;                 // 查询状态
```

**ViewModel 便捷方法**：
```gdscript
# base_view_model.gd 新增
func begin_bulk_update() -> void:
    for name in get_property_names():
        var prop = get_property(name)
        if prop:
            prop.begin_bulk_update()

func end_bulk_update() -> void:
    for name in get_property_names():
        var prop = get_property(name)
        if prop:
            prop.end_bulk_update()
```

#### 3. ValueConverter 值转换器

**问题**：无法在绑定时进行数据格式化/反格式化（如 float → "75%"）。

**解决方案**：新增 `ValueConverter` 基类和内置转换器：
```gdscript
# 内置转换器
IntToStringConverter      # int → "123"
FloatToPercentConverter   # 0.75 → "75%"
BoolToVisibilityConverter # bool → Control.PRESET_FULL_RECT / PRESET_EMPTY

# 自定义转换器
class MyConverter extends ValueConverter:
    func convert(value) -> String:
        return "Count: %d" % value
    func convert_back(value) -> int:
        return int(value.trim_prefix("Count: "))
```

**使用方式**：
```gdscript
# 在绑定时传入 converter
Context.bind_property($PercentLabel, "text", vm, "progress", 1,
    preload("res://converters/float_to_percent_converter.gd").new())

# @bind_property 注解也支持 converter 参数
@bind_property("text", "PercentLabel", 1, null, FloatToPercentConverter.new())
```

**新增文件**：
- `mvvm/value_converter.h`
- `mvvm/value_converter.cpp`
- `mvvm/observable_property.cpp` — 批量更新
- `mvvm/binding_engine.cpp` — converter 支持
- `gf_test/core/converters/bool_to_text_converter.gd` — 示例
- `gf_test/core/converters/float_to_percent_converter.gd` — 示例

### 阶段 14 — 可选增强 / 已知简化(目前的 TODO)
- `Activity.dispatch_create` 目前传**空 saved_state**;真正的状态保存/恢复未做(参数走 `intent.extras`)。
- `Dialog` 的 `dismiss_on_outside` / 遮罩输入拦截**未内置**(目前由 dialog 场景自己画遮罩/处理点击)。
- `ResourceManager.load_async` 的 `priority` 参数**当前忽略**(未接入优先级队列)。
- `Transition` 的 `CUSTOM`(自定义动画/AnimationPlayer)**未实现**(枚举里也没留,需要时加)。
- `BindingEngine` 无显式 `unbind(view)`;靠 `vm.dispose()` + Godot 对象销毁自动断连。如需提前精确反绑定,后续补。
- `ServiceRegistry` 存裸 `Object*`(生命周期由注册方负责);如需强引用/类型校验可增强。

### 阶段 15 — @bind_signal 注解 Bug 修复 ✅ (已完成，2026-06-08)

**问题**：annotation_bind demo 中 `@bind_signal("pressed")` 注解**没有生效**，按钮点击无响应。

**Root Cause 1 — `_bind_commands` metadata 未设置**：
`gdscript_compiler.cpp` 在循环中收集了 `@bind_signal` 注解到 `bind_commands` 数组，但循环结束后**只 `set_meta("_bind_configs", ...)`，遗漏了 `set_meta("_bind_commands", ...)`**。导致运行时 `apply_bindings` 拿到的 `_bind_commands` 始终为空。

**Root Cause 2 — `_find_node_with_signal` 找第一个匹配就停**：
`@bind_signal("pressed")` 会匹配 owner 子树中**第一个有 `pressed` 信号的节点**（DFS），而 CheckBox 继承自 BaseButton 也有 `pressed` 信号。如果 CheckBox 出现在按钮之前，会被错误地连上。

**修复**：

1. **`gdscript_compiler.cpp:3013` — 补充 `_bind_commands` set_meta**：
```cpp
if (!bind_configs.is_empty()) {
    p_script->set_meta("_bind_configs", bind_configs);
}
if (!bind_commands.is_empty()) {
    p_script->set_meta("_bind_commands", bind_commands);  // ← 修复
}
```

2. **`view_model.cpp:apply_bindings` — 改为收集所有匹配节点**：
```cpp
// Collect all descendants with the requested signal.
Vector<Node *> matches;
_collect_nodes_with_signal(p_owner, signal_name, matches);
for (Node *n : matches) {
    BindingEngine::bind_command(n, signal_name, this, method);
}
```

**新增 API**：`ViewModel::_collect_nodes_with_signal()` 替代旧的 `_find_node_with_signal`，递归收集所有有指定信号的节点。

**验证**：Test 9 增强后断言全部通过：
- ✅ @bind_property 初始同步 (TitleLabel.text、StatusLabel.text、ProgressBar.value、CheckBox.button_pressed)
- ✅ @bind_property VM→View 同步 (vm.set_property 触发 UI 更新)
- ✅ vm.on_increment/on_reset/on_check_toggled 方法通过 GDScript `_set/_get` 正确路由
- ✅ @bind_signal 自动连接到 _btn_inc.pressed (vm.on_increment, vm.on_reset)
- ✅ @bind_signal 自动连接到 CheckBox.toggled (vm.on_check_toggled)
- ✅ CheckBox.toggled 实际触发 vm.on_check_toggled 修改 VM

**Test 10 仍然通过**：高级 MVVM 功能（批量更新 + 值转换器 + 动态信号）不受影响。

### 阶段 17 — 复杂业务场景示例集 ✅ (已完成，2026-06-08)

**目标**：从"语法演示"升级到"真实业务场景"，覆盖常见 MVVM 实战模式。

**新增 5 个完整示例**（位置：`modules/complex_examples/*/`）：

| # | 场景 | VM 关键模式 | Activity 关键模式 |
|---|------|------------|------------------|
| 1 | **UserList** 列表渲染 | 集合属性（Array）作为 ObservableProperty；CRUD 命令；搜索过滤 | subscribe_property("filtered_users") 触发 `_rebuild_list` 重建 VBoxContainer 子节点 |
| 2 | **ShoppingCart** 购物车 | 计算属性（subtotal/discount/tax/total/item_count）— 通过 `_recompute_totals()` + `begin/end_bulk_update` | CurrencyConverter、PercentageConverter 行内格式化 |
| 3 | **RegistrationForm** 注册表单 | 实时校验（regex）+ 异步服务端校验（`await Engine.get_main_loop().process_frame` 模拟网络延迟）+ can_submit 派生属性 | 错误高亮 + submit 按钮联动 |
| 4 | **OrderState** 订单状态机 | 有限状态机（enum + STATE_TRANSITIONS map）；按状态派生可用操作（can_pay/can_ship/...）；状态历史 audit log | 状态徽章颜色 + 操作按钮自动启用/禁用 + 历史时间线 |
| 5 | **PlayerCard** 玩家卡片 | 嵌套数据（EquipmentItemVM 列表）；槽位装备替换；战力派生（`atk*1.5 + def*1.2 + hp/10`）；稀有度颜色 | 4 槽位槽网格 + 装备模板菜单（PopupMenu） |

**新增 utility**：
- `core/converters/currency_converter.gd` — 千分位货币格式化
- `core/converters/percentage_converter.gd` — 百分比格式化

**新增入口**：
- `complex_examples/complex_examples_menu.gd` — 5 个示例的列表菜单

**Headless 测试覆盖**（Test 11-15）：
- ✅ Test 11 UserList：CRUD、过滤、清空
- ✅ Test 12 ShoppingCart：计算属性、折扣码、数量改 0=删除
- ✅ Test 13 RegistrationForm：同步校验 + 异步服务端校验 + 协议联动
- ✅ Test 14 OrderState：状态机转换、可用操作、状态历史
- ✅ Test 15 PlayerCard：嵌套装备、槽位替换、升级、重置

**踩坑记录**（已修复）：
1. `//` 注释在 GDScript 不支持 — 改用 `#`
2. `checked ? a : b` 三元语法 GDScript 4 不支持 — 改用 `a if cond else b`
3. `class_name` 是 Godot 4 关键字 — 用作变量名冲突，重命名为 `player_class`
4. `func foo(p: str)` 错误类型 — 改用 `String`
5. VM 继承 `RefCounted` 不是 `Node`，不能用 `get_tree()` — 用 `Engine.get_main_loop().process_frame`

**验证**：Test 1-15 全部通过（除 Test 8 Activity 栈已清空问题是 demo 原本就有的）。

### 阶段 16 — GDScript ValueConverter 重名警告修复 ✅ (已完成，2026-06-08)

**问题**：GDScript 子类（`bool_to_text_converter.gd`、`float_to_percent_converter.gd`）定义了 `convert` / `convert_back` 方法触发 Godot 4 解析器警告：

```
ERROR: The method "convert()" overrides a method from native class "ValueConverter". 
This won't be called by the engine and may not work as expected. (Warning treated as error.)
```

**Root Cause**：
- Godot 4 的 GDScript **不能 override C++ 虚函数**（GDScript 端的 "override" 不会调用 C++ vtable）
- 之前的设计是 public virtual `convert` / `convert_back` — GDScript 子类同名方法会被 Godot 误判成"override"

**修复**：改为**模板方法 + 鸭子类型分派**模式

1. **C++ 父类**:
   - `convert` / `convert_back` 改为 public **非虚**模板方法
   - 新增 protected virtual `_convert` / `_convert_back`（C++ 子类 override）
   - `convert` 实现: 先用 `has_method("_convert")` 检查 GDScript 是否定义了钩子；若有则 `call("_convert", value)` 走 GDScript；否则调用 C++ 虚函数 `_convert`

2. **C++ 内置转换器**（`IntToStringConverter` / `FloatToPercentConverter` / `BoolToTextConverter`）:
   - 改为 override protected virtual `_convert` / `_convert_back`

3. **GDScript 子类**:
   - 方法名改为 `_convert` / `_convert_back`（带下划线前缀）
   - 不与 C++ 父类 public 方法同名 → 无警告
   - 通过 C++ 父类的 `has_method` 检测 + `call()` 反射调用

**关键代码**:
```cpp
Variant ValueConverter::convert(const Variant &p_value) {
    // GDScript 4 cannot override C++ virtuals, so we duck-type:
    if (get_script_instance() != nullptr && has_method("_convert")) {
        return call("_convert", p_value);
    }
    return _convert(p_value);
}
```

**GDScript 用法示例**:
```gdscript
extends ValueConverter
class BoolToTextConverter:
    func _convert(value) -> String:
        return "✓" if value else "✗"
    func _convert_back(value) -> bool:
        return value == "✓"
```

**验证**：Test 10 新增 GDScript 转换器断言：
- ✅ `bool_text.convert(true) == "✓ 已启用"`
- ✅ `float_pct_gd.convert(0.5) == "50%"`
- ✅ `float_pct_gd.convert_back("40%") ≈ 0.4`

Test 1-10 全部通过 ✅，无 GDScript parse 警告。

### 阶段 18 — Activity Run-As-Standalone（F6 单 Activity 调试）✅（已完成，2026-06-22）

**目标**：允许在编辑器里对 `xxx_activity.tscn` 按 F6 直接运行单个 Activity 验证 UI/绑定，**业务代码零修改**，**零项目配置**。

**核心变更**：
- `Activity.h` 新增 `BootMode { BOOT_AUTO, BOOT_STANDALONE, BOOT_MANAGED }` 枚举 + `standalone` / `standalone_play_transitions` 成员 + `_standalone_app` / `_standalone_root` 内部指针 + GDVIRTUAL1(`_on_setup_standalone`, Application *)。
- `Activity::_notification(NOTIFICATION_READY)` 4-门 gate：boot_mode != MANAGED && context == nullptr && !is_editor_hint() && current_scene == this → `call_deferred("_bootstrap_standalone")`。
- `_bootstrap_standalone()` 工作流：
  1. 在 `SceneTree.root` 下 `memnew(Application)` + `StandaloneRoot:Control`（anchors=FULL_RECT）；
  2. `set_current_scene(_standalone_app)` 保证 SceneTree 状态一致；
  3. `reparent(_standalone_root)` 把 Activity 移到 StandaloneRoot 下（layering / 输入路径与 managed 路径一致）；
  4. `app->initialize(StandaloneRoot)` 装管理器，`ActivityManager.set_loader(AutoActivityLoader(base="res://"))` 让跨 Activity 跳转开箱即用；
  5. `GDVIRTUAL_CALL(_on_setup_standalone, app)` 给业务注册 mock service 的机会；
  6. `ActivityManager.adopt_running_activity(this, intent)` 把自己入栈；
  7. `call_deferred("_standalone_dispatch_lifecycle")` → 下一帧统一派发 `_on_create → _on_start → _on_resume`。
- `ActivityManager::adopt_running_activity(Activity*, Ref<Intent>)` 新接口：把已经在 SceneTree 里的 Activity 加入 stack，不 add_child / 不派发生命周期。
- `ActivityManager::finish_activity` 检测 standalone-root（stack.size==1 且 is_standalone()）时改走 `SceneTree::quit()`，跳过 `_begin_exit`（不播 exit transition，不 queue_free 自己）。

**默认行为**：
- 默认 `BOOT_AUTO`，无须改任何 Activity 子类。
- 默认 standalone 不预注册任何 service —— 业务在 `_on_setup_standalone(app)` 自助注册。
- 默认 `standalone_play_transitions=false`（避免 F6 一开屏看不见）。

**入口 / 退出语义**：
- F6 / `godot --path . xxx_activity.tscn` → Activity 自启动；
- `back()` / `finish()` 在 standalone root 时调 `SceneTree::quit()`；
- 子 Activity 跳转走原 stack（`start_activity(intent)`），最后 pop 到 standalone root 后 finish 再 quit。

**验证（headless，exit 0）**：
- `current_scene` 被替换为 `StandaloneApplication`；
- Activity 已 reparent 到 `StandaloneRoot/`；
- `Activity.is_standalone() == true`；
- `Activity.get_context() == standalone Application`；
- `show_toast` 不崩；
- `ActivityManager.stack_size == 1` 且 top 为本 Activity；
- `finish()` → `SceneTree.quit()` 干净退出。

**文件**：
- `ui/activity.h/cpp` — 4-门 gate + `_bootstrap_standalone` + 新 properties；
- `ui/activity_manager.h/cpp` — `adopt_running_activity` + standalone-aware finish；
- `doc_classes/Activity.xml` — 新增 Run-As-Standalone 章节 + `_on_setup_standalone` / `is_standalone` / 3 个 enum 常量 / 2 个 export property 文档。

### 阶段 19 — IContext 接口 + CRTP 重构（Android Context 模型，C++ 层 "is-a"）✅（已完成，2026-06-22）

**目标**：消除 Activity / Dialog 与 Context 之间的"组合 + 手写 14×重复 delegate"模式，按 Android `Activity extends ContextWrapper extends Context` 的思路引入 C++ 层 "is-a IContext" 关系，让 Activity / Dialog / Context 三方在 C++ 端可统一接受 `IContext *`。

**关键约束（用户明确）**：
- GDScript ABI 100% 不变（方法名 / 签名 / 默认值全部保留）。
- 现有 GDScript 业务代码（gf_test 18 项自动化测试 + standalone Activity demo）零修改回归通过。
- 不动 Toast owner 链（future work，3 处 `_resolve_default_owner()` 仍返 `Object *`）。

**架构终态**：
```
IContext                              ← pure C++ interface, 2 纯虚 (get_application + as_object)
   ▲ implements via CRTP
   │
   ContextBase<Self>                  ← template mixin, 12 个 inline 默认实现，routing through
      ▲                                  static_cast<Self*>(this)->get_application()
      │
      ├─ Context : Node, ContextBase<Context>        ← GDCLASS 保留 (向后兼容 + 静态工厂)
      │      └─ Application : Context                ← Manager 持有方
      │
      ├─ Activity : Control, ContextBase<Activity>   ← C++ is-a IContext
      └─ Dialog   : Control, ContextBase<Dialog>     ← C++ is-a IContext
```

**核心变更**：
- 新增 3 个文件：
  - `context_interface.h` — `class IContext { virtual Application *get_application() const = 0; virtual Object *as_object() = 0; }`。
  - `context_base.h` — `template <typename Self> class ContextBase : public IContext` 提供 12 个 delegate 方法**声明**（仅前向声明，避免 application.h ↔ context.h 循环依赖）。
  - `context_base.inl` — 12 个 template 方法**定义**，由 implementer 的 .cpp 顶部 include。
- 改造 `Context`：`public Node, public ContextBase<Context>`，删 12 个手写 delegate，保留 7 个 Context-only 方法（finish_activity / get_current_activity / get_stack_size / owner 重载 / register_activity / change_scene\* / static MVVM）。
- 改造 `Activity`：`public Control, public ContextBase<Activity>`，原 `Context *context` 字段 → `Application *_app`。`set_context(Context*)` 保留为安全 `Object::cast_to<Application>` alias，`get_context()` 返回 `_app`（Application IS-A Context，隐式 upcast）。新增 `set_application(Application*)` / `get_application()` 显式接口。删 12 个 delegate 实现。
- 改造 `Dialog`：同 Activity，5 个 delegate 删干净。
- 改造 `ActivityManager`：3 处 `set_context(app)` → `set_application(app)`，语义更精准。

**ClassDB 绑定技巧**：CRTP 基类方法用 PMF static_cast 桥接 —— `using ShowToastT = void (Activity::*)(const Ref<Toast> &); ClassDB::bind_method(D_METHOD("show_toast", "toast"), static_cast<ShowToastT>(&Activity::show_toast));` —— Spike 已证明可行。

**编译期循环依赖解法**：context_base.h 只放方法**声明**，全部 manager 类前向声明；模板方法**定义**放 `.inl`，由具体 .cpp 在 include application.h 后 include。

**LoC 减少（粗估）**：
- `activity.h/cpp` 减少 ~140 行手写 delegate；
- `dialog.h/cpp` 减少 ~60 行手写 delegate；
- `context.h/cpp` 减少 ~50 行手写实现；
- 共减少约 250 行重复代码；新增 ~150 行 CRTP 基础设施（3 个新头文件） — 净减少约 100 行，但**每加一个 Context 方法**只动 ContextBase 一处（之前要改 3 处）。

**验证（8 项 headless 端到端，exit 0）**：
1. standalone bootstrap（enhanced_input_activity.tscn F6）
2. Context 静态方法仍可用（bind_property / make_toast）
3. `Activity.get_context()` 返回 Application（验证 IS-A Context 关系）
4. `Activity.get_application()` 等效返回
5. `Activity.show_toast` 走 ContextBase 不崩
6. `Activity.get_service` 不崩（无服务返 null）
7. `Activity.start_activity(detail)` → stack=2，detail 也拿到同一 Application
8. `back()` 回 standalone，`finish()` → quit 干净

**未完成的 future work（用户决策推迟）**：
- Toast::set_owner(IContext\*) 强类型化 + ActivityManager `_resolve_default_owner()` 返 `IContext *`。C++ 调用方目前可通过 `IContext::as_object()` 桥接，足够用。
- GDScript 端 `activity is Context` 仍返 false（Godot GDCLASS 单继承约束）。文档已注明。

**文件**：
- `context_interface.h` / `context_base.h` / `context_base.inl`（新）
- `context.h/cpp` / `application.h/cpp`（继承链调整 — Application 自动通过 Context 继承 ContextBase<Context>）
- `ui/activity.h/cpp` / `ui/dialog.h/cpp`（多继承 + 删 delegate）
- `ui/activity_manager.cpp`（set_context → set_application）
- `doc_classes/Context.xml` / `doc_classes/Activity.xml` / `doc_classes/Dialog.xml`（新增 IContext 实现说明 + set_application/get_application 文档）

### 阶段 20 — ActivityLauncher 策略对象（移除 Activity 内 4-门 gate）✅（已完成，2026-06-22）

**目标**：消除 `Activity::_notification(NOTIFICATION_READY)` 内"4 门 gate + BootMode 枚举"的硬编码启动检测；把启动逻辑外置成可替换的策略对象，让 Activity 不再知道"自己是不是入口"。

**核心问题（阶段 18 留下的）**：
```cpp
void Activity::_notification(int p_what) {  // 18 行 if-梯子
    if (standalone || boot_mode == BOOT_MANAGED || _app != nullptr) return;
    if (Engine::is_editor_hint()) return;
    if (get_tree() == nullptr) return;
    if (boot_mode == BOOT_AUTO && get_tree()->get_current_scene() != this) return;
    call_deferred("_bootstrap_standalone");
}
```
- 违反 SRP：Activity 同时是"演员"和"启动器"。
- 不可单测：检测逻辑与 SceneTree / Engine 强耦合。
- 难扩展：加新模式（编辑器预览 / 嵌套 Activity / 多窗口）必改 Activity。

**架构终态**：
```
ActivityLauncher : Resource         ← 抽象启动策略
   ▲
   └─ StandaloneActivityLauncher    ← 默认实例：F6 单 Activity 调试
      (force_standalone, play_transitions 两个开关)

Activity : Control
   - 字段 launcher: Ref<ActivityLauncher>  ← 默认 = StandaloneActivityLauncher
   - _notification(READY): launcher->try_launch(this)  ← 1 行
   - run_standalone_bootstrap(bool): 由 launcher 通过 call_deferred 调
```

**关键变更**：
- 新增 `ui/activity_launcher.h/cpp` — `ActivityLauncher : Resource` 抽象基类，`virtual bool try_launch(Activity*) { return false; }`。
- 新增 `ui/standalone_activity_launcher.h/cpp` — 默认 launcher 实现，5 个 gate 检查通过后 `p_activity->call_deferred("run_standalone_bootstrap", play_transitions)`。
- 改 `Activity.h/cpp`：
  - 删 `enum BootMode { BOOT_AUTO/STANDALONE/MANAGED }` + `boot_mode` / `standalone_play_transitions` / `_standalone_app` / `_standalone_root` 字段；
  - 加 `Ref<ActivityLauncher> launcher` 字段 + 默认构造 `StandaloneActivityLauncher`；
  - `_notification(READY)` 从 18 行收缩为 3 行（含 if 头尾）；
  - 原 `_bootstrap_standalone()` 改名为 public `run_standalone_bootstrap(bool play_transitions)`，作为 launcher 和测试的稳定入口；
  - 内部 `_dispatch_standalone_lifecycle(bool)` 是 deferred lifecycle 派发的 trampoline。
- `register_types.cpp` 注册 2 个新类。

**默认行为不变**：未配置过的 Activity 仍然 F6 即跑，因为构造函数装上默认 `StandaloneActivityLauncher`。

**GDScript ABI 变化**：
- 删除：`Activity.boot_mode` / `set_boot_mode` / `get_boot_mode` / 3 个 `BOOT_*` 常量 / `standalone_play_transitions` / `set/get_standalone_play_transitions`。
- 新增：`Activity.launcher` (Ref<ActivityLauncher>) / `set_launcher` / `get_launcher` / `run_standalone_bootstrap(play_transitions)`。
- 保留：`is_standalone()` / `_on_setup_standalone(app)` 不变。
- 业务侧已知用法（gf_test）：之前没用过 BootMode 任何 const，所以零迁移成本。

**LoC 净变化**：
- `activity.h/cpp` 净减 ~30 行（删 standalone 内部细节，加策略字段）；
- 新增 ~80 行 launcher 基础设施；
- 总体 +50 行，但每个类 SRP 清晰。

**验证（headless 8 项 + 编译）**：
1. T1+T2 默认 launcher bootstrap，is_standalone=true；
2. T3 默认 launcher 实例确实是 StandaloneActivityLauncher；
3. T4 show_toast / get_service 走 ContextBase 仍正常；
4. T5 跨 Activity 跳转 stack +1；
5. T6 back 回 standalone；
6. T7 finish→quit 干净；
7. T8 `act.set_launcher(null)` 后 `_notification` 完全不动作（is_standalone=false, Application=null）；
8. scons 重编 + 链接二进制 0 error。

**未来扩展点（不改 Activity）**：
- `EditorPreviewLauncher` — 编辑器侧 Play Scene 按钮触发；
- `NestedActivityLauncher` — 嵌套预览，Activity 作为子节点；
- `SnapshotReplayLauncher` — 回放测试；
- 加 `GDVIRTUAL1R(bool, _try_launch, Object*)` 让 GDScript 也能写 launcher。

**文件**：
- `ui/activity_launcher.h/cpp`（新）
- `ui/standalone_activity_launcher.h/cpp`（新）
- `ui/activity.h/cpp` — 简化 _notification + 用 launcher
- `register_types.cpp` — 注册 ActivityLauncher / StandaloneActivityLauncher
- `doc_classes/Activity.xml` — 删 BootMode 文档 + 加 launcher member
- `doc_classes/ActivityLauncher.xml` / `StandaloneActivityLauncher.xml`（新）

### 阶段 21 — StandaloneApplication 封装 + Toast 升级为 Context 节点（同 Dialog）✅（已完成，2026-06-22）

**目标**：消除 Activity/Dialog 各自重复的 standalone host 构建；把 Toast 从 `RefCounted` 描述符升级为 `Control + ContextBase<Toast>` 节点，三者统一可单独 F6 预览。

**核心变更**：
- **新增 `StandaloneApplication : Application`**（`standalone_application.h/.cpp`，模块根）。静态 `host(Control*)` 封装公共宿主图：建 app + `StandaloneRoot`(FULL_RECT) → `root_window->add_child` → 若入参是 current_scene 则 `set_current_scene(app)` → reparent 入参 → `initialize` → 装 `AutoActivityLoader("res://")`。`register_types` 注册。
- **三节点 `run_standalone_bootstrap` 收敛**为：`host(this)` → `_on_setup_standalone` → `adopt_running_<activity|dialog|toast>` → 派发生命周期。自带门禁：`_app != null`（managed 流程）或 `is_editor_hint()` → no-op。
- **`is_standalone()` 派生**：删三者的 `bool standalone` 成员，改 `Object::cast_to<StandaloneApplication>(_app) != nullptr`。standalone-ness 是 Application 类型的属性。
- **Toast 重写**（`ui/toast.{h,cpp}`）：`RefCounted`→`Control + ContextBase<Toast>`；`make_text` 返回 `Toast*`；默认 `dispatch_create` 内建 panel+label（脚本未覆写 `_on_create` 时）；`owner`→`lifecycle_owner`（避让 `Node::set_owner`）；**删 `custom_scene`**（自定义 toast = 根 `extends Toast` 的场景）；加 `launcher`/`_on_create`/`_on_dismiss`/`_on_setup_standalone`/`dismiss`/`run_standalone_bootstrap`。
- **ActivityManager toast 管线重写**：`toast_queue` `Vector<Ref<Toast>>`→`Vector<Toast*>`；`_spawn_toast_node`→`_present_toast(Toast*)`（节点自身 add_child + dispatch_create + fade tween）；新增 `adopt_running_toast`/`dismiss_toast`；队列清理对未入树孤儿节点用 `memdelete`。
- **签名变更**：`Context`/`ContextBase` 的 `show_toast`/`show_toast_with_owner`/`make_toast` 由 `Ref<Toast>`→`Toast*`；5 处 ClassDB PMF 别名同步。

**关键修复**：Activity `_notification(READY)` 直调 `run_standalone_bootstrap` 会在 READY 期间改树（"parent is busy setting up children"）→ 改 `call_deferred`。Dialog/Toast 经 launcher 已是 deferred。

**gf_test 迁移**：`core/toast_custom.gd` `extends MarginContainer`→`extends Toast`（UI 移 `_on_create`）；`core/scenes/toast_custom.tscn` 根 type→`Toast`；`main_activity.gd:_on_toast_custom` 改 `instantiate()+show_toast`；新增 `tests/{dialog,toast}_standalone_test.tscn`。

**验证现状**：C++ 编译 0 error；`dialog_standalone_test` / `toast_standalone_test` headless 功能断言 **PASS**（`_on_setup_standalone`/`_on_create`/`_on_dismiss` 链正确）。**遗留待查**：(1) 单 Activity/Dialog/Toast 退出时偶发 segfault(139)（PASS 之后、teardown 阶段）；(2) `scenes/entry.tscn` 18 项回归在 banner 后无输出（疑似 `change_scene(game.tscn)` 或 deferred bootstrap 与 managed 流程交互），需进一步定位。`run/main_scene` 当前指向 `enhanced_input_activity.tscn`（单 Activity 入口），跑 18 项回归须显式 `res://scenes/entry.tscn`。

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
