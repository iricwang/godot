/**************************************************************************/
/* ei_modifier_scale.cpp */
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

#include "ei_modifier_scale.h"

#include "core/object/class_db.h"

namespace ei {

void EIModifierScale::set_scale_x(float p_v) {
	_scale_x = p_v;
}

float EIModifierScale::get_scale_x() const {
	return _scale_x;
}

void EIModifierScale::set_scale_y(float p_v) {
	_scale_y = p_v;
}

float EIModifierScale::get_scale_y() const {
	return _scale_y;
}

void EIModifierScale::set_scale_z(float p_v) {
	_scale_z = p_v;
}

float EIModifierScale::get_scale_z() const {
	return _scale_z;
}

EIValue EIModifierScale::modify_value(const EIValue &p_input) const {
	switch (p_input.get_type()) {
		case EIValue::TYPE_BOOL: {
			return p_input;
		}
		case EIValue::TYPE_AXIS1D: {
			return EIValue::make_axis1d(p_input.get_axis1d() * _scale_x);
		}
		case EIValue::TYPE_AXIS2D: {
			const Vector2 v = p_input.get_axis2d();
			return EIValue::make_axis2d(Vector2(v.x * _scale_x, v.y * _scale_y));
		}
		case EIValue::TYPE_AXIS3D: {
			const Vector3 v = p_input.get_axis3d();
			return EIValue::make_axis3d(Vector3(v.x * _scale_x, v.y * _scale_y, v.z * _scale_z));
		}
	}
	return p_input;
}

String EIModifierScale::get_modifier_name() const {
	return "Scale";
}

void EIModifierScale::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_scale_x", "scale"), &EIModifierScale::set_scale_x);
	ClassDB::bind_method(D_METHOD("get_scale_x"), &EIModifierScale::get_scale_x);
	ClassDB::bind_method(D_METHOD("set_scale_y", "scale"), &EIModifierScale::set_scale_y);
	ClassDB::bind_method(D_METHOD("get_scale_y"), &EIModifierScale::get_scale_y);
	ClassDB::bind_method(D_METHOD("set_scale_z", "scale"), &EIModifierScale::set_scale_z);
	ClassDB::bind_method(D_METHOD("get_scale_z"), &EIModifierScale::get_scale_z);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_x"), "set_scale_x", "get_scale_x");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_y"), "set_scale_y", "get_scale_y");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_z"), "set_scale_z", "get_scale_z");
}

} // namespace ei
