## responsive.gd — Tiny adaptive-layout helper.
##
## Use this from any [Activity] / [Dialog] _build_ui() to pick sizes that
## work on both a 1920×1080 desktop and a 393×852 phone preview.
##
## Detection model:
##   * is_mobile()   — the platform/feature-tag branch (set by Game workspace
##                      Platform = Mobile, or true on Android/iOS at runtime)
##   * is_portrait() — current viewport is taller than wide
##   * is_compact()  — viewport SHORT side ≤ 480 design pixels
##                      (phone-ish regardless of orientation)
##
## All helpers fall back to DisplayServer.window_get_size() when not yet in
## tree, so they work in _on_create before the first layout pass.
##
## NOTE on live switching: design coordinates are baked when the child
## process boots (GODOT_EDITOR_VIEWPORT_OVERRIDE / content_scale_size).
## Toggling Resolution/Orientation in the editor while the game is running
## resizes the OS window but does NOT re-trigger _on_create — so layouts
## decided at startup stay at startup. Restart the play session to re-pick.
extends Object
class_name Responsive


# --- Capability queries ----------------------------------------------------

## True when the active branch is mobile. Mirrors PlatformBranch.is_mobile().
static func is_mobile() -> bool:
	return OS.has_feature("mobile")


## True when the viewport is taller than wide.
static func is_portrait(node: Node = null) -> bool:
	var v: Vector2i = _viewport_size(node)
	return v.y > v.x


## True when the shorter side of the viewport is "phone-sized". Independent
## of orientation: a landscape iPhone (852×393) is still compact because the
## 393 short-side breaches the threshold.
static func is_compact(node: Node = null) -> bool:
	var v: Vector2i = _viewport_size(node)
	return mini(v.x, v.y) <= 480


# --- Sizing helpers --------------------------------------------------------

## Scale a "design" font size down on compact viewports. Inputs are assumed
## to be authored for ~720..1080 short-side desktops; on a 393-wide phone we
## drop by ~30%, clamped at 10 so labels never disappear.
static func font(design: int, node: Node = null) -> int:
	if is_compact(node):
		return maxi(int(round(design * 0.7)), 10)
	return design


## Touch-friendly minimum size for buttons. Apple HIG suggests 44pt and
## Material 48dp; we land at 52 on mobile/compact and respect the caller's
## design height otherwise. Width passes through (callers usually want
## SIZE_EXPAND_FILL anyway).
static func button_min(design: Vector2 = Vector2(240, 44), node: Node = null) -> Vector2:
	var min_h: int = 44
	if is_compact(node) or is_mobile():
		min_h = 52
	return Vector2(design.x, max(design.y, float(min_h)))


## Lateral gutter for content. Plenty of breathing room on desktop, tight
## but not cramped on phones.
static func gutter(node: Node = null) -> int:
	return 12 if is_compact(node) else 40


## Vertical separation between siblings inside a VBox/HBox. Slightly tighter
## on compact viewports so dense menus still fit above the fold.
static func gap(design: int = 14, node: Node = null) -> int:
	if is_compact(node):
		return maxi(int(round(design * 0.7)), 6)
	return design


## True when "two-cell" content (e.g. count + target) should stack vertically
## instead of side-by-side. Triggered on compact portrait viewports.
static func stack_vertically(node: Node = null) -> bool:
	return is_compact(node) and is_portrait(node)


## Returns the smaller of (viewport_width - 2*gutter) and `cap`, useful for
## panel min widths that should shrink on phones but stay readable on
## desktops. A `cap` of -1 means no upper bound.
static func panel_width(cap: int, node: Node = null) -> int:
	var v: Vector2i = _viewport_size(node)
	var available: int = v.x - 2 * gutter(node)
	if cap <= 0:
		return maxi(available, 200)
	return mini(cap, maxi(available, 200))


# --- Internals -------------------------------------------------------------

static func _viewport_size(node: Node) -> Vector2i:
	if node != null and node.is_inside_tree():
		var vp: Viewport = node.get_viewport()
		if vp != null:
			return Vector2i(vp.get_visible_rect().size)
	return DisplayServer.window_get_size()
