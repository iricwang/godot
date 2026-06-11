/**************************************************************************/
/* ei_modifier_swizzle_axis.cpp */
/**************************************************************************/
/* This file is part of: */
/* GODOT ENGINE */
/* https://godotengine.org */
/**************************************************************************/
/* Copyright (c)2014-present Godot Engine contributors (see AUTHORS.md). */
/* */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the */
/* "Software"), to deal in the Software without restriction, including */
/* without limitation the rights to use, copy, modify, merge, publish, */
/* distribute, sublicense, and/or sell copies of the Software, and to */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions: */
/* */
/* The above copyright notice and this permission notice shall be */
/* included in all copies or substantial portions of the Software. */
/* */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
/**************************************************************************/

#include "ei_modifier_swizzle_axis.h"

#include "core/object/class_db.h"

namespace ei {

void EIModifierSwizzleAxis::set_order_x(SourceAxis p_v) {
	_order_x = p_v;
}

EIModifierSwizzleAxis::SourceAxis EIModifierSwizzleAxis::get_order_x() const {
	return _order_x;
}

void EIModifierSwizzleAxis::set_order_y(SourceAxis p_v) {
	_order_y = p_v;
}

EIModifierSwizzleAxis::SourceAxis EIModifierSwizzleAxis::get_order_y() const {
	return _order_y;
}

void EIModifierSwizzleAxis::set_order_z(SourceAxis p_v) {
	_order_z = p_v;
}

EIModifierSwizzleAxis::SourceAxis EIModifierSwizzleAxis::get_order_z() const {
	return _order_z;
}

static float _pick_axis_scalar(const Vector3 &p_v, EIModifierSwizzleAxis::SourceAxis p_axis) {
	switch (p_axis) {
		case EIModifierSwizzleAxis::SOURCE_AXIS_X:
			return p_v.x;
		case EIModifierSwizzleAxis::SOURCE_AXIS_Y:
			return p_v.y;
		case EIModifierSwizzleAxis::SOURCE_AXIS_Z:
			return p_v.z;
	}
	return 0.0f;
}

EIValue EIModifierSwizzleAxis::modify_value(const EIValue &p_input) const {
	switch (p_input.get_type()) {
		case EIValue::TYPE_BOOL: {
			return p_input;
		}
		case EIValue::TYPE_AXIS1D: {
			//1D has only one axis to choose from — swizzle is meaningless.
			// We respect order_x anyway so that wiring it up is consistent.
			const Vector3 v(p_input.get_axis1d(),0.0f,0.0f);
			return EIValue::make_axis1d(_pick_axis_scalar(v, _order_x));
		}
		case EIValue::TYPE_AXIS2D: {
			const Vector2 v = p_input.get_axis2d();
			// Build a3D proxy (z=0) so a single picker handles all axes.
			const Vector3 proxy(v.x, v.y,0.0f);
			return EIValue::make_axis2d(Vector2(
					_pick_axis_scalar(proxy, _order_x),
					_pick_axis_scalar(proxy, _order_y)));
		}
		case EIValue::TYPE_AXIS3D: {
			const Vector3 v = p_input.get_axis3d();
			return EIValue::make_axis3d(Vector3(
					_pick_axis_scalar(v, _order_x),
					_pick_axis_scalar(v, _order_y),
					_pick_axis_scalar(v, _order_z)));
		}
	}
	return p_input;
}

String EIModifierSwizzleAxis::get_modifier_name() const {
	return "SwizzleAxis";
}

void EIModifierSwizzleAxis::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_order_x", "axis"), &EIModifierSwizzleAxis::set_order_x);
	ClassDB::bind_method(D_METHOD("get_order_x"), &EIModifierSwizzleAxis::get_order_x);
	ClassDB::bind_method(D_METHOD("set_order_y", "axis"), &EIModifierSwizzleAxis::set_order_y);
	ClassDB::bind_method(D_METHOD("get_order_y"), &EIModifierSwizzleAxis::get_order_y);
	ClassDB::bind_method(D_METHOD("set_order_z", "axis"), &EIModifierSwizzleAxis::set_order_z);
	ClassDB::bind_method(D_METHOD("get_order_z"), &EIModifierSwizzleAxis::get_order_z);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "order_x", PROPERTY_HINT_ENUM, "X,Y,Z"), "set_order_x", "get_order_x");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "order_y", PROPERTY_HINT_ENUM, "X,Y,Z"), "set_order_y", "get_order_y");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "order_z", PROPERTY_HINT_ENUM, "X,Y,Z"), "set_order_z", "get_order_z");
}

} // namespace ei
