/**************************************************************************/
/*  transition.cpp                                                        */
/**************************************************************************/

#include "transition.h"

#include "core/object/class_db.h"
#include "scene/animation/tween.h"
#include "scene/gui/control.h"

void Transition::set_enter_type(Type p_type) {
	enter_type = p_type;
}

Transition::Type Transition::get_enter_type() const {
	return enter_type;
}

void Transition::set_exit_type(Type p_type) {
	exit_type = p_type;
}

Transition::Type Transition::get_exit_type() const {
	return exit_type;
}

void Transition::set_duration(double p_duration) {
	duration = p_duration;
}

double Transition::get_duration() const {
	return duration;
}

Ref<Tween> Transition::play_enter(Control *p_node) {
	ERR_FAIL_NULL_V(p_node, Ref<Tween>());
	if (enter_type == NONE) {
		return Ref<Tween>();
	}

	const Vector2 size = p_node->get_size();
	const Vector2 base = p_node->get_position();
	Ref<Tween> tween = p_node->create_tween();
	ERR_FAIL_COND_V(tween.is_null(), Ref<Tween>());

	switch (enter_type) {
		case FADE: {
			p_node->set_modulate(Color(1, 1, 1, 0));
			tween->tween_property(p_node, NodePath("modulate:a"), 1.0, duration);
		} break;
		case SLIDE_LEFT: {
			p_node->set_position(base + Vector2(size.x, 0));
			tween->tween_property(p_node, NodePath("position"), base, duration);
		} break;
		case SLIDE_RIGHT: {
			p_node->set_position(base - Vector2(size.x, 0));
			tween->tween_property(p_node, NodePath("position"), base, duration);
		} break;
		case SLIDE_UP: {
			p_node->set_position(base + Vector2(0, size.y));
			tween->tween_property(p_node, NodePath("position"), base, duration);
		} break;
		case SLIDE_DOWN: {
			p_node->set_position(base - Vector2(0, size.y));
			tween->tween_property(p_node, NodePath("position"), base, duration);
		} break;
		case SCALE: {
			p_node->set_pivot_offset(size * 0.5);
			p_node->set_scale(Vector2(0.85, 0.85));
			p_node->set_modulate(Color(1, 1, 1, 0));
			tween->set_parallel(true);
			tween->tween_property(p_node, NodePath("scale"), Vector2(1, 1), duration);
			tween->tween_property(p_node, NodePath("modulate:a"), 1.0, duration);
		} break;
		default:
			break;
	}
	return tween;
}

Ref<Tween> Transition::play_exit(Control *p_node) {
	ERR_FAIL_NULL_V(p_node, Ref<Tween>());
	if (exit_type == NONE) {
		return Ref<Tween>();
	}

	const Vector2 size = p_node->get_size();
	const Vector2 base = p_node->get_position();
	Ref<Tween> tween = p_node->create_tween();
	ERR_FAIL_COND_V(tween.is_null(), Ref<Tween>());

	switch (exit_type) {
		case FADE: {
			tween->tween_property(p_node, NodePath("modulate:a"), 0.0, duration);
		} break;
		case SLIDE_LEFT: {
			tween->tween_property(p_node, NodePath("position"), base - Vector2(size.x, 0), duration);
		} break;
		case SLIDE_RIGHT: {
			tween->tween_property(p_node, NodePath("position"), base + Vector2(size.x, 0), duration);
		} break;
		case SLIDE_UP: {
			tween->tween_property(p_node, NodePath("position"), base - Vector2(0, size.y), duration);
		} break;
		case SLIDE_DOWN: {
			tween->tween_property(p_node, NodePath("position"), base + Vector2(0, size.y), duration);
		} break;
		case SCALE: {
			p_node->set_pivot_offset(size * 0.5);
			tween->set_parallel(true);
			tween->tween_property(p_node, NodePath("scale"), Vector2(0.85, 0.85), duration);
			tween->tween_property(p_node, NodePath("modulate:a"), 0.0, duration);
		} break;
		default:
			break;
	}
	return tween;
}

void Transition::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enter_type", "type"), &Transition::set_enter_type);
	ClassDB::bind_method(D_METHOD("get_enter_type"), &Transition::get_enter_type);
	ClassDB::bind_method(D_METHOD("set_exit_type", "type"), &Transition::set_exit_type);
	ClassDB::bind_method(D_METHOD("get_exit_type"), &Transition::get_exit_type);
	ClassDB::bind_method(D_METHOD("set_duration", "duration"), &Transition::set_duration);
	ClassDB::bind_method(D_METHOD("get_duration"), &Transition::get_duration);
	ClassDB::bind_method(D_METHOD("play_enter", "node"), &Transition::play_enter);
	ClassDB::bind_method(D_METHOD("play_exit", "node"), &Transition::play_exit);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "enter_type", PROPERTY_HINT_ENUM, "None,Fade,SlideLeft,SlideRight,SlideUp,SlideDown,Scale"), "set_enter_type", "get_enter_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "exit_type", PROPERTY_HINT_ENUM, "None,Fade,SlideLeft,SlideRight,SlideUp,SlideDown,Scale"), "set_exit_type", "get_exit_type");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration", PROPERTY_HINT_RANGE, "0.0,3.0,0.01"), "set_duration", "get_duration");

	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(FADE);
	BIND_ENUM_CONSTANT(SLIDE_LEFT);
	BIND_ENUM_CONSTANT(SLIDE_RIGHT);
	BIND_ENUM_CONSTANT(SLIDE_UP);
	BIND_ENUM_CONSTANT(SLIDE_DOWN);
	BIND_ENUM_CONSTANT(SCALE);
}
