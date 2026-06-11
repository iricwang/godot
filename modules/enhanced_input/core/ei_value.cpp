/**************************************************************************/
/*  ei_value.cpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "ei_value.h"

#include "core/error/error_macros.h"

namespace ei {

// ---------------------------------------------------------------------------
// Static factories
// ---------------------------------------------------------------------------

EIValue EIValue::make_bool(bool p_b) {
	EIValue v;
	v._type = TYPE_BOOL;
	v._bool_v = p_b;
	return v;
}

EIValue EIValue::make_axis1d(float p_v) {
	EIValue v;
	v._type = TYPE_AXIS1D;
	v._axis1d_v = p_v;
	return v;
}

EIValue EIValue::make_axis2d(const Vector2 &p_v) {
	EIValue v;
	v._type = TYPE_AXIS2D;
	v._axis2d_v = p_v;
	return v;
}

EIValue EIValue::make_axis3d(const Vector3 &p_v) {
	EIValue v;
	v._type = TYPE_AXIS3D;
	v._axis3d_v = p_v;
	return v;
}

// ---------------------------------------------------------------------------
// Typed accessors
// ---------------------------------------------------------------------------

EIValue::Type EIValue::get_type() const {
	return _type;
}

bool EIValue::get_bool() const {
#ifdef DEBUG_ENABLED
	if (_type != TYPE_BOOL) {
		WARN_PRINT("EIValue::get_bool: value type is not BOOL");
	}
#endif
	return _type == TYPE_BOOL ? _bool_v : false;
}

float EIValue::get_axis1d() const {
#ifdef DEBUG_ENABLED
	if (_type != TYPE_AXIS1D) {
		WARN_PRINT("EIValue::get_axis1d: value type is not AXIS1D");
	}
#endif
	return _type == TYPE_AXIS1D ? _axis1d_v : 0.0f;
}

Vector2 EIValue::get_axis2d() const {
#ifdef DEBUG_ENABLED
	if (_type != TYPE_AXIS2D) {
		WARN_PRINT("EIValue::get_axis2d: value type is not AXIS2D");
	}
#endif
	return _type == TYPE_AXIS2D ? _axis2d_v : Vector2();
}

Vector3 EIValue::get_axis3d() const {
#ifdef DEBUG_ENABLED
	if (_type != TYPE_AXIS3D) {
		WARN_PRINT("EIValue::get_axis3d: value type is not AXIS3D");
	}
#endif
	return _type == TYPE_AXIS3D ? _axis3d_v : Vector3();
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool EIValue::is_zero() const {
	switch (_type) {
		case TYPE_BOOL:
			return !_bool_v;
		case TYPE_AXIS1D:
			return _axis1d_v == 0.0f;
		case TYPE_AXIS2D:
			return _axis2d_v == Vector2();
		case TYPE_AXIS3D:
			return _axis3d_v == Vector3();
	}
	return false;
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

bool EIValue::operator==(const EIValue &p_other) const {
	if (_type != p_other._type) {
		return false;
	}
	switch (_type) {
		case TYPE_BOOL:
			return _bool_v == p_other._bool_v;
		case TYPE_AXIS1D:
			return _axis1d_v == p_other._axis1d_v;
		case TYPE_AXIS2D:
			return _axis2d_v.is_equal_approx(p_other._axis2d_v);
		case TYPE_AXIS3D:
			return _axis3d_v.is_equal_approx(p_other._axis3d_v);
	}
	return false;
}

bool EIValue::operator!=(const EIValue &p_other) const {
	return !(*this == p_other);
}

// ---------------------------------------------------------------------------
// Type promotion / demotion
// ---------------------------------------------------------------------------

EIValue EIValue::with_type_promoted(Type p_target) const {
	if (_type == p_target) {
		return *this;
	}
	// Bool -> 1D
	if (_type == TYPE_BOOL && p_target == TYPE_AXIS1D) {
		return make_axis1d(_bool_v ? 1.0f : 0.0f);
	}
	// Bool -> 2D
	if (_type == TYPE_BOOL && p_target == TYPE_AXIS2D) {
		return make_axis2d(Vector2(_bool_v ? 1.0f : 0.0f, 0.0f));
	}
	// Bool -> 3D
	if (_type == TYPE_BOOL && p_target == TYPE_AXIS3D) {
		return make_axis3d(Vector3(_bool_v ? 1.0f : 0.0f, 0.0f, 0.0f));
	}
	// 1D -> 2D (broadcast)
	if (_type == TYPE_AXIS1D && p_target == TYPE_AXIS2D) {
		return make_axis2d(Vector2(_axis1d_v, 0.0f));
	}
	// 1D -> 3D (broadcast)
	if (_type == TYPE_AXIS1D && p_target == TYPE_AXIS3D) {
		return make_axis3d(Vector3(_axis1d_v, 0.0f, 0.0f));
	}
	// 2D -> 3D (z=0)
	if (_type == TYPE_AXIS2D && p_target == TYPE_AXIS3D) {
		return make_axis3d(Vector3(_axis2d_v.x, _axis2d_v.y, 0.0f));
	}
	// 3D -> 2D (drop z)
	if (_type == TYPE_AXIS3D && p_target == TYPE_AXIS2D) {
		return make_axis2d(Vector2(_axis3d_v.x, _axis3d_v.y));
	}
	// Demotions to a narrower scalar: take the X component. The
	// sampler uses this for key/button events bound to an Axis1D
	// action (raw Axis1D(1.0) -> ... is unaffected) and the
	// dispatcher uses it when an Axis1D modifier runs on an
	// Axis2D input.
	if (_type == TYPE_AXIS2D && p_target == TYPE_AXIS1D) {
		return make_axis1d(_axis2d_v.x);
	}
	if (_type == TYPE_AXIS3D && p_target == TYPE_AXIS1D) {
		return make_axis1d(_axis3d_v.x);
	}
	// Demotions to Bool: "any non-zero" semantics. Matches the
	// EIValue::is_zero() convention and the action's `is_held` flag.
	// Used by the sampler for key/button events on a Bool action
	// (raw Axis1D(1.0) -> Bool(true)) and by the dispatcher's value
	// checks downstream.
	if (_type == TYPE_AXIS1D && p_target == TYPE_BOOL) {
		return make_bool(_axis1d_v != 0.0f);
	}
	if (_type == TYPE_AXIS2D && p_target == TYPE_BOOL) {
		return make_bool(_axis2d_v != Vector2());
	}
	if (_type == TYPE_AXIS3D && p_target == TYPE_BOOL) {
		return make_bool(_axis3d_v != Vector3());
	}
	// Unsupported cross-type combinations: zero-value of target.
	switch (p_target) {
		case TYPE_BOOL:
			return make_bool(false);
		case TYPE_AXIS1D:
			return make_axis1d(0.0f);
		case TYPE_AXIS2D:
			return make_axis2d(Vector2());
		case TYPE_AXIS3D:
			return make_axis3d(Vector3());
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Variant round-trip (P1 inline; refactor to ei_value_serializer.h later)
// ---------------------------------------------------------------------------

Variant EIValue::to_variant() const {
	switch (_type) {
		case TYPE_BOOL:
			return _bool_v;
		case TYPE_AXIS1D:
			return _axis1d_v;
		case TYPE_AXIS2D:
			return _axis2d_v;
		case TYPE_AXIS3D:
			return _axis3d_v;
	}
	return Variant();
}

EIValue EIValue::from_variant(const Variant &p_v) {
	switch (p_v.get_type()) {
		case Variant::BOOL:
			return make_bool((bool)p_v);
		case Variant::FLOAT:
		case Variant::INT:
			return make_axis1d((float)p_v);
		case Variant::VECTOR2:
			return make_axis2d((Vector2)p_v);
		case Variant::VECTOR3:
			return make_axis3d((Vector3)p_v);
		default:
			return make_bool(false);
	}
}

} // namespace ei
