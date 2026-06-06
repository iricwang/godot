/**************************************************************************/
/*  transition.h                                                          */
/**************************************************************************/
#pragma once

#include "core/io/resource.h"

class Control;
class Tween;

// Reusable enter/exit animation for an Activity (or Dialog). Implemented with Tween so it works on any Control.
// FADE and SCALE only touch modulate/scale (anchor-safe); SLIDE_* animate position and assume the node is laid
// out at an explicit position (ActivityManager places activities at (0,0) with an explicit size).
class Transition : public Resource {
	GDCLASS(Transition, Resource);

public:
	enum Type {
		NONE,
		FADE,
		SLIDE_LEFT, // enter from the right edge / exit to the left
		SLIDE_RIGHT, // enter from the left edge / exit to the right
		SLIDE_UP, // enter from the bottom / exit to the top
		SLIDE_DOWN, // enter from the top / exit to the bottom
		SCALE,
	};

private:
	Type enter_type = FADE;
	Type exit_type = FADE;
	double duration = 0.3;

protected:
	static void _bind_methods();

public:
	void set_enter_type(Type p_type);
	Type get_enter_type() const;
	void set_exit_type(Type p_type);
	Type get_exit_type() const;
	void set_duration(double p_duration);
	double get_duration() const;

	// Returns the running Tween (or an invalid Ref if nothing to animate), so callers can await/connect "finished".
	Ref<Tween> play_enter(Control *p_node);
	Ref<Tween> play_exit(Control *p_node);
};

VARIANT_ENUM_CAST(Transition::Type);
