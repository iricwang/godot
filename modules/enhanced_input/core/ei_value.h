/**************************************************************************/
/*  ei_value.h                                                            */
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

#pragma once

#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/variant/variant.h"

// First-class value type for action data. Implemented as a tagged union —
// no `Variant` allocation in the runtime hot path. Serialization helpers
// (to_variant / from_variant) are inline here for P1 simplicity; they may
// be moved to a separate header in a later phase.

namespace ei {

class EIValue {
public:
	enum Type {
		TYPE_BOOL = 0,
		TYPE_AXIS1D = 1,
		TYPE_AXIS2D = 2,
		TYPE_AXIS3D = 3,
	};

	// --- Constructors (static factory style, no implicit conversions) ------
	static EIValue make_bool(bool p_b);
	static EIValue make_axis1d(float p_v);
	static EIValue make_axis2d(const Vector2 &p_v);
	static EIValue make_axis3d(const Vector3 &p_v);

	// Default-construct to a typed zero (BOOL=false).
	EIValue() :
			_type(TYPE_BOOL), _bool_v(false) {}

	// --- Typed accessors ---------------------------------------------------
	Type get_type() const;
	bool get_bool() const;
	float get_axis1d() const;
	Vector2 get_axis2d() const;
	Vector3 get_axis3d() const;

	// --- Queries -----------------------------------------------------------
	bool is_zero() const;

	// --- Comparison --------------------------------------------------------
	bool operator==(const EIValue &p_other) const;
	bool operator!=(const EIValue &p_other) const;

	// --- Conversion (explicit only) ---------------------------------------
	// Promote / demote across types. Bool->1D lifts false=0/true=1.
	// 1D->2D/3D broadcasts the scalar.
	// 2D->3D sets z=0. 3D->2D drops z. Anything else returns zero-value.
	EIValue with_type_promoted(Type p_target) const;

	// Variant round-trip for .tres serialization. NOT used in hot path.
	Variant to_variant() const;
	static EIValue from_variant(const Variant &p_v);

private:
	Type _type;
	union {
		bool _bool_v;
		float _axis1d_v;
		Vector2 _axis2d_v;
		Vector3 _axis3d_v;
	};
};

} // namespace ei
