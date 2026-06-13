task: anchors-rect-tool-inspector
status: shipped
created: 2026-06-13
spec: anchors-rect-tool-inspector
proposal: openspec/changes/anchors-rect-tool-inspector/proposal.md
detailed_spec: openspec/changes/anchors-rect-tool-inspector/specs/anchors-rect-tool-inspector.md

# Phases

## Phase 1 — per-axis stretch lock (✅ shipped, commit b25586b17b)

- [x] 1.1 用 `stretch_x = data.anchor[SIDE_LEFT] != data.anchor[SIDE_RIGHT]` /
      `stretch_y = data.anchor[SIDE_TOP]  != data.anchor[SIDE_BOTTOM]` 替换
      `_get_anchors_layout_preset() == PRESET_FULL_RECT` 单一判据。
      位置：`scene/gui/control.cpp::_validate_property`
- [x] 1.2 任一轴 stretch ⇒ `position`/`size` 加 `PROPERTY_USAGE_READ_ONLY`。
- [x] 1.3 编译验证（SCons editor target，链接 `bin\godot.windows.editor.x86_64.exe`）。
- [x] 1.4 16 个 LayoutPreset 上的人工 trace 覆盖全表。

## Phase 2a — re-expose stretching-axis offsets (✅ shipped, commit b25586b17b)

- [x] 2a.1 在 `_validate_property` 把"non-custom-anchor 时隐藏 anchor_/offset_/grow_"
      这条规则的"刀切"放出 4 个例外：
      - `stretch_x` 时保留 `offset_left` / `offset_right` 可见
      - `stretch_y` 时保留 `offset_top` / `offset_bottom` 可见
- [x] 2a.2 自定义 anchor (`use_custom_anchors=true`) 模式下行为不变，
      所有 anchor_/offset_/grow_ 维持可见。

## Phase 2b — relabel offsets to L/T/R/B (✅ shipped, commit b25586b17b)

- [x] 2b.1 在 `editor/scene/gui/control_editor_plugin.cpp` 顶部 include
      `editor/inspector/editor_properties.h`。
- [x] 2b.2 在 `EditorInspectorPluginControl::parse_property` 加 `offset_*` 分支：
      检测对应轴 stretch 时，new `EditorPropertyFloat`，
      `setup(EditorPropertyRangeHint{ .suffix = "px" })`，
      `add_property_editor(p_path, ed, false, TTR("Left/Top/Right/Bottom"))`，
      返回 `true` 拦截默认 widget。
- [x] 2b.3 复用 `ControlEditorPlugin` 构造器里 line 1741 的 `add_inspector_plugin`，
      无需新增注册。
- [x] 2b.4 编译验证（仅 `editor/scene/gui/control_editor_plugin.cpp` 增量重编 + editor lib 重链）。

## Phase 3 — 2D viewport drag handles respect lock (⏳ deferred)

> 当前状态：Inspector 端已禁止编辑，但 2D viewport 仍可通过 drag handle 改 size/position。
> 需要在 `editor/scene/canvas_item_editor_plugin.cpp` 的 `_drag_*` 路径计算
> stretch_x/stretch_y 并屏蔽对应的拖拽方向。

- [ ] 3.1 调研 `_get_anchor_handle_drag_type` / `DRAG_LEFT` / `DRAG_TOP_LEFT` / `DRAG_MOVE`
      在 stretching 轴上的当前行为
- [ ] 3.2 在 `_drag_*` 入口拒绝 stretch 轴的 size 修改；DRAG_MOVE 在 FULL_RECT 下完全禁止
- [ ] 3.3 该文件本仓库已存在 unrelated 未提交改动（resolution guide），
      建议先把那个落盘再做 Phase 3 以避免冲突

## Phase 4 — Unity-grade mixed-axis editor (⏳ deferred, optional polish)

> 当前在单轴 stretch 下（如 TOP_WIDE）由于 `EditorPropertyVector2` 不能锁单分量，
> `position`/`size` 整体灰显，用户失去了 fixed 轴 (y) 的编辑能力 —— 与 Unity 的
> "Pos Y / Height + Left / Right" 混合呈现不一致。

- [ ] 4.1 写一个 `RectTransformEditor : EditorProperty`（约 150 行，
      参考 `ControlOffsetTransformEditor` 的模板，同文件内）
- [ ] 4.2 该 editor 同时持有 4 个 EditorSpinSlider，按 stretch 状态动态显示
      `Pos X / Width / Pos Y / Height / Left / Right / Top / Bottom` 的子集
- [ ] 4.3 拦截 `position` / `size` / `offset_*` 全部 6 条 path
- [ ] 4.4 监听 `notify_property_list_changed` 信号在 preset 切换时刷新

# Notes

- Phase 1 + 2a + 2b 已经是单个 commit `b25586b17b` 整体落地（接在 precursor
  `657e421a73` 之后）。tasks.md 把它拆成三段是为 spec 的逻辑可读性。
- 涉及验证：当前所有验证以**人工 trace + 编译通过**为主，因为 Godot 没有
  专为 inspector 行为写的 unit test。Phase 3 / 4 着手时建议补 GDScript
  集成测试：在 SceneTree 里造一个 Control + 各 preset，断言
  `get_property_list` 返回的 `usage` 位匹配预期。
