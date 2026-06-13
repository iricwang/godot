# Anchors-driven RectTool Inspector — Detailed Spec

> Spec id: `anchors-rect-tool-inspector`
> Source proposal: `openspec/changes/anchors-rect-tool-inspector/proposal.md`
> Tasks: `openspec/changes/anchors-rect-tool-inspector/tasks.md`
> Targets: Godot 4.7-beta (`feature/game_framework` branch)
> Status: **Phase 1+2a+2b shipped** (commits `657e421a73` → `b25586b17b`)

This document is the single source of truth for how the editor's Inspector
treats `Control` Layout fields under the various anchor configurations. It
references implementation locations by `file:line`. When the engine evolves,
the predicate and the table are the two things that must stay in sync.

---

## 1. Background

### 1.1 The runtime model (unchanged)

Godot's `Control` describes a rectangle in its parent's space using
**4 anchors** (`data.anchor[SIDE_LEFT|TOP|RIGHT|BOTTOM]`, each in `[0,1]`)
and **4 offsets** (`data.offset[SIDE_*]`, in pixels). The actual edge
position on each side is:

```
edge_pos[i] = data.offset[i] + data.anchor[i] * parent_size[i & 1]    (i in 0..3)
```

— from `Control::_size_changed` at `scene/gui/control.cpp:2168-2260`. The
cached `pos_cache = (edge_pos[L], edge_pos[T])` and `size_cache =
(edge_pos[R]-edge_pos[L], edge_pos[B]-edge_pos[T])` are what `position` and
`size` getters return.

Crucially: when the two anchors on an axis differ (e.g. `anchor[L]=0`,
`anchor[R]=1`), that axis is **driven** by the parent's size — neither
`position` nor `size` on that axis are an independent input; they are
*outputs* of the formula above. This is the core fact the rest of the
spec is about.

The engine itself uses this exact predicate to issue a runtime warning when
the user calls `set_size()` on a stretching axis — see
`Control::_set_size` at `scene/gui/control.cpp:1535`:

```cpp
if (data.size_warning &&
    (data.anchor[SIDE_LEFT] != data.anchor[SIDE_RIGHT] ||
     data.anchor[SIDE_TOP]  != data.anchor[SIDE_BOTTOM])) {
    WARN_PRINT("Nodes with non-equal opposite anchors will have their size overridden after _ready()");
}
```

The same predicate drives our inspector behavior, so the two are **never**
out of sync.

### 1.2 The pre-existing inspector behavior (what we changed)

Before this change set, `Control::_validate_property`
(`scene/gui/control.cpp:489-665`) did the following for editor display:

- **Hide** `anchor_*` / `offset_*` / `grow_*` whenever
  `_get_anchors_layout_preset() != -1`
  (i.e. the user picked a preset rather than custom anchors).
- **Lock** `position`/`size` only when the preset is exactly
  `PRESET_FULL_RECT` (added by precursor commit `657e421a73`).

This produced two UX cliffs:

1. *_WIDE / VCENTER_WIDE / HCENTER_WIDE: `position`/`size` looked editable
   but the engine reverted the change and warned. There was no visible way
   to author the actual edges (offset_left/right etc. were hidden).
2. To author edges you had to drop into custom anchors mode, losing the
   semantic meaning of the chosen preset.

Unity's RectTransform (RectTool) handles this gracefully: stretching axes
expose `Left/Right` or `Top/Bottom` fields *named exactly that*, while
non-stretching axes show `Pos / Size`. The whole panel relayouts when you
change the anchor preset. We mirror this.

---

## 2. Behavioral contract

> Authoritative version: see the implementation predicate in §3.1 and the
> matrix in §3.2. Anything below this line is descriptive prose.

For any selected `Control` with `LayoutMode == ANCHORS` (or `UNCONTROLLED`,
which is the same as ANCHORS in the engine's eyes — see
`control.cpp:631`), the Inspector's Layout group displays exactly these
fields:

- **`position`, `size`** — always present.
  - **READ_ONLY** when at least one axis stretches
    (`data.anchor[L] != data.anchor[R]` ∨ `data.anchor[T] != data.anchor[B]`).
  - Otherwise editable.
  - Vector2 widgets cannot lock individual components, so the lock is
    on the whole vector. *(See §6 for a future-work note about
    Phase 4's mixed-axis editor.)*

- **`anchors_preset`** — always present (unless `LayoutMode == POSITION`,
  in which case the preset dropdown is hidden — pre-existing behavior).

- **`anchor_left/top/right/bottom`** — visible iff `use_custom_anchors`
  (i.e. user explicitly typed anchor values rather than choosing a preset).

- **`offset_left/right`** — visible iff `use_custom_anchors` **OR**
  `stretch_x` is true.
- **`offset_top/bottom`** — visible iff `use_custom_anchors` **OR**
  `stretch_y` is true.

  When the offset becomes visible due to the stretch path (not custom
  anchor), the inspector renders it as an `EditorPropertyFloat` with
  `suffix = "px"` and label rewritten to `"Left" / "Top" / "Right" /
  "Bottom"`. (Custom-anchor path keeps the default `"Offset Left"` etc.
  labels for backward compatibility.)

- **`grow_horizontal`, `grow_vertical`** — visible iff `use_custom_anchors`.
  Pre-existing rule, preserved.

---

## 3. Implementation reference

### 3.1 Predicate (`scene/gui/control.cpp:642-643`)

```cpp
bool stretch_x = data.anchor[SIDE_LEFT] != data.anchor[SIDE_RIGHT];
bool stretch_y = data.anchor[SIDE_TOP]  != data.anchor[SIDE_BOTTOM];
```

That's it. Every visibility / readonly decision below derives from these
two booleans. Floating-point equality is fine here because the values are
written by `set_anchors_preset` from a small set of literals
(`{0.0, 0.5, 1.0}`) or by the user's typed input (which `Inspector`
re-quantizes to the underlying double).

### 3.2 Field-by-field rules (`scene/gui/control.cpp:635-675`)

```cpp
bool use_custom_anchors = use_anchors && _get_anchors_layout_preset() == -1;

bool hide_anchor_block =
    !use_custom_anchors &&
    (p_property.name.begins_with("anchor_") ||
     is_anchor_offset_property_name ||
     p_property.name.begins_with("grow_"));

if (hide_anchor_block) {
    bool keep_visible = false;
    if (use_anchors) {
        if (stretch_x && (p_property.name == "offset_left"  ||
                          p_property.name == "offset_right")) {
            keep_visible = true;
        }
        if (stretch_y && (p_property.name == "offset_top"   ||
                          p_property.name == "offset_bottom")) {
            keep_visible = true;
        }
    }
    if (!keep_visible) {
        p_property.usage ^= PROPERTY_USAGE_EDITOR;        // hide
    }
}

if (use_anchors && (stretch_x || stretch_y) &&
        (p_property.name == "size" || p_property.name == "position")) {
    p_property.usage |= PROPERTY_USAGE_READ_ONLY;        // lock
}
```

### 3.3 Label rewrite (`editor/scene/gui/control_editor_plugin.cpp:888-919`)

In `EditorInspectorPluginControl::parse_property`:

```cpp
if (p_path == "offset_left"  || p_path == "offset_top" ||
    p_path == "offset_right" || p_path == "offset_bottom") {
    bool stretch_x = control->get_anchor(SIDE_LEFT) != control->get_anchor(SIDE_RIGHT);
    bool stretch_y = control->get_anchor(SIDE_TOP)  != control->get_anchor(SIDE_BOTTOM);
    bool is_x_edge = (p_path == "offset_left" || p_path == "offset_right");
    bool is_y_edge = (p_path == "offset_top"  || p_path == "offset_bottom");
    bool relabel = (is_x_edge && stretch_x) || (is_y_edge && stretch_y);
    if (relabel) {
        EditorPropertyFloat *prop_editor = memnew(EditorPropertyFloat);
        EditorPropertyRangeHint hint;
        hint.suffix = "px";
        prop_editor->setup(hint);
        String new_label;
        if      (p_path == "offset_left")   new_label = TTR("Left");
        else if (p_path == "offset_top")    new_label = TTR("Top");
        else if (p_path == "offset_right")  new_label = TTR("Right");
        else                                new_label = TTR("Bottom");
        add_property_editor(p_path, prop_editor, false, new_label);
        return true;
    }
}
```

The `EditorInspectorPluginControl` is registered in
`ControlEditorPlugin::ControlEditorPlugin()` at
`editor/scene/gui/control_editor_plugin.cpp:1741-1743` (pre-existing) —
no new registration is required.

### 3.3.1 Why we only relabel on the stretch path

The "default" `Offset Left/Top/Right/Bottom` labels are correct semantics
in custom-anchor mode (they are offsets *relative* to the anchor, not the
absolute edge). Relabeling them universally would mislead users in that
mode. Relabel-only-when-stretching is therefore the right contract.

---

## 4. Behavioral matrix (test oracle)

This table is the test oracle. If the implementation drifts from it, the
implementation is wrong. Each row should also be reachable in a future
GDScript integration test (Phase 3+).

> Notation: ✏️ editable, 🔒 READ_ONLY (visible but locked), — not shown
> in inspector, **C** = custom-anchor case (`use_custom_anchors == true`).

| Case | anchor (L,T,R,B) | stretch_x | stretch_y | position | size | anchor_* | offset_left | offset_right | offset_top | offset_bottom | grow_* |
|---|---|---|---|---|---|---|---|---|---|---|---|
| TOP_LEFT       | (0,0,0,0)       | F | F | ✏️ | ✏️ | — | — | — | — | — | — |
| TOP_RIGHT      | (1,0,1,0)       | F | F | ✏️ | ✏️ | — | — | — | — | — | — |
| BOTTOM_LEFT    | (0,1,0,1)       | F | F | ✏️ | ✏️ | — | — | — | — | — | — |
| BOTTOM_RIGHT   | (1,1,1,1)       | F | F | ✏️ | ✏️ | — | — | — | — | — | — |
| CENTER_LEFT    | (0,0.5,0,0.5)   | F | F | ✏️ | ✏️ | — | — | — | — | — | — |
| CENTER_TOP     | (0.5,0,0.5,0)   | F | F | ✏️ | ✏️ | — | — | — | — | — | — |
| CENTER_RIGHT   | (1,0.5,1,0.5)   | F | F | ✏️ | ✏️ | — | — | — | — | — | — |
| CENTER_BOTTOM  | (0.5,1,0.5,1)   | F | F | ✏️ | ✏️ | — | — | — | — | — | — |
| CENTER         | (0.5,0.5,0.5,0.5)| F | F | ✏️ | ✏️ | — | — | — | — | — | — |
| LEFT_WIDE      | (0,0,0,1)       | F | T | 🔒 | 🔒 | — | — | — | ✏️*Top* | ✏️*Bottom* | — |
| RIGHT_WIDE     | (1,0,1,1)       | F | T | 🔒 | 🔒 | — | — | — | ✏️*Top* | ✏️*Bottom* | — |
| VCENTER_WIDE   | (0.5,0,0.5,1)   | F | T | 🔒 | 🔒 | — | — | — | ✏️*Top* | ✏️*Bottom* | — |
| TOP_WIDE       | (0,0,1,0)       | T | F | 🔒 | 🔒 | — | ✏️*Left* | ✏️*Right* | — | — | — |
| BOTTOM_WIDE    | (0,1,1,1)       | T | F | 🔒 | 🔒 | — | ✏️*Left* | ✏️*Right* | — | — | — |
| HCENTER_WIDE   | (0,0.5,1,0.5)   | T | F | 🔒 | 🔒 | — | ✏️*Left* | ✏️*Right* | — | — | — |
| FULL_RECT      | (0,0,1,1)       | T | T | 🔒 | 🔒 | — | ✏️*Left* | ✏️*Right* | ✏️*Top* | ✏️*Bottom* | — |
| **C** any      | any, no stretch | F | F | ✏️ | ✏️ | ✏️ | ✏️*Offset Left* | ✏️*Offset Right* | ✏️*Offset Top* | ✏️*Offset Bottom* | ✏️ |
| **C** stretch  | any, stretching | T or F | T or F | 🔒 | 🔒 | ✏️ | ✏️*Offset Left* | ✏️*Offset Right* | ✏️*Offset Top* | ✏️*Offset Bottom* | ✏️ |

*Italic name in cell* = label shown in the inspector.

---

## 5. Test plan (manual + future automation)

### 5.1 Manual smoke (current)

After rebuilding the editor binary:

1. Open any scene with a `Control` (e.g. the Plinko `game.tscn` already
   in `D:/Proj-Godot/Simple`).
2. Select a Control node.
3. In the Layout group, change `anchors_preset` and verify the matrix:
   - `Top Left` → Position+Size editable, no edge fields.
   - `Left Wide` → Position+Size grayed; `Top` and `Bottom` SpinBoxes
     appear with `px` suffix.
   - `Top Wide` → Position+Size grayed; `Left`/`Right` SpinBoxes appear.
   - `Full Rect` → Position+Size grayed; all four `Left/Top/Right/Bottom`
     SpinBoxes appear.
4. Drag any `anchor_*` (in custom anchor mode) so anchors mismatch on one
   axis — verify position/size lock on without preset change.

### 5.2 Future automated coverage (Phase 3+)

```gdscript
# tests/scene/gui/test_control_layout_inspector.gd
func test_anchors_preset_drives_property_visibility():
    var ctrl := Control.new()
    add_child(ctrl)
    ctrl.anchors_preset = Control.PRESET_LEFT_WIDE
    var props := ctrl.get_property_list()
    var by_name := {}
    for p in props:
        by_name[p.name] = p
    # size and position must be present and READ_ONLY
    assert(by_name["size"].usage     & PROPERTY_USAGE_READ_ONLY != 0)
    assert(by_name["position"].usage & PROPERTY_USAGE_READ_ONLY != 0)
    # offset_top / offset_bottom must be present (visible)
    assert(by_name["offset_top"]    .usage & PROPERTY_USAGE_EDITOR != 0)
    assert(by_name["offset_bottom"] .usage & PROPERTY_USAGE_EDITOR != 0)
    # offset_left / offset_right must be hidden (no EDITOR bit)
    assert(by_name["offset_left"] .usage & PROPERTY_USAGE_EDITOR == 0)
    assert(by_name["offset_right"].usage & PROPERTY_USAGE_EDITOR == 0)
```

Equivalent rows for every `LayoutPreset` should be added; the matrix in §4
is the table-driven form of this test.

---

## 6. Non-goals & limitations

### 6.1 Single-component lock for Vector2 fields

Inspector's default `EditorPropertyVector2` widget does **not** expose a
way to mark only one component (X or Y) read-only. Therefore in single-axis
stretch cases (e.g. `TOP_WIDE`: stretch_x=T, stretch_y=F) we lock the
**entire** `position`/`size` Vector2 — the user loses the ability to edit
the fixed-axis component (`y`/`height`) from the inspector. They can still:

- See the runtime value (READ_ONLY shows it).
- Adjust it via 2D viewport drag handles (which currently bypass our
  inspector lock; see Phase 3 in tasks.md).
- Achieve the same effect by typing a delta into `offset_top` /
  `offset_bottom` — both will scroll the rect since on a fixed axis they
  represent absolute pixel offsets.

To fully match Unity's "Pos Y + Height + Left + Right" mixed display,
we'd need a custom `RectTransformEditor` (Phase 4 in tasks.md, ~150 lines,
modeled after the existing `ControlOffsetTransformEditor`).

### 6.2 2D viewport drag handles

The 2D editor's `_drag_*` paths in
`editor/scene/canvas_item_editor_plugin.cpp` do **not** consult
`PROPERTY_USAGE_READ_ONLY` and will happily drag a Control whose
inspector says "locked". This is Phase 3 work and is intentionally
deferred — it requires per-handle stretch-axis logic that is best
prototyped after the Phase 1+2 contract has stabilized. Not a regression
(behavior is identical to pre-`657e421a73`); just an alignment debt.

### 6.3 Container children

Children of a `Container` (HBoxContainer, VBoxContainer, etc.) already
have the `properties_managed_by_container` block in `_validate_property`
(`control.cpp:651-664`) that locks `anchor_/offset_/grow_/position/size/
rotation/scale/pivot_offset`. That block runs **after** our block. A
Container child's effective state is therefore the **stricter** of:
"managed by container" → READ_ONLY-everything, vs. "stretching axis →
position/size READ_ONLY". The user sees the container lock as
authoritative, which is correct.

### 6.4 Runtime behavior (`Engine::is_editor_hint() == false`)

`_validate_property` returns early at `control.cpp:677-679` when not
running in the editor. Our changes are therefore **completely inert** at
runtime. No GDScript or scene logic can observe a difference. (The
`PROPERTY_USAGE_*` flags are only consulted by the editor inspector and
some `ResourceFormatSaver` paths; the latter aren't relevant for `Control`
node properties.)

---

## 7. Glossary

- **Stretching axis** — the axis (x or y) on which the two anchors differ.
  The rectangle's edges on that axis follow the parent on resize.
- **Single-anchor preset** — any of the 9 presets where all four anchors
  collapse to the same point (`TOP_LEFT`, `CENTER`, `CENTER_RIGHT`, …).
  No axis is stretching.
- **Wide preset** — `*_WIDE`, `VCENTER_WIDE`, `HCENTER_WIDE`. Exactly one
  axis stretches.
- **Full preset** — `PRESET_FULL_RECT`. Both axes stretch.
- **Custom anchor mode** — `_get_anchors_layout_preset() == -1`,
  i.e. the user has typed anchor values that don't match any preset.
  Triggered by `data.stored_use_custom_anchors == true` (set when the user
  edits `anchor_*` directly).

---

## 8. Change log

| Commit | Date | Summary |
|---|---|---|
| `657e421a73` | 2026-06-13 | Precursor: lock `position`/`size` in `PRESET_FULL_RECT` only. (`scene/gui/control.cpp` +8 lines) |
| `b25586b17b` | 2026-06-13 | Generalize lock to all stretching presets via `data.anchor[SIDE_*]` predicate; re-expose stretching-axis offsets in non-custom mode; add Inspector plugin path that relabels them as `Left/Top/Right/Bottom`. (`scene/gui/control.cpp` +37/-6, `editor/scene/gui/control_editor_plugin.cpp` +34/0) |
