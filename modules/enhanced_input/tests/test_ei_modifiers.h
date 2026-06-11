/**************************************************************************/
/* test_ei_modifiers.h */
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

#pragma once

// Includes resolve relative to the module root when test_main.cpp compiles
// from tests/. Use module-relative paths so the same include works for
// both in-module sources and the generated tests bundle.
#include "../core/ei_value.h"
#include "../modifiers/ei_modifier_dead_zone.h"
#include "../modifiers/ei_modifier_negate.h"
#include "../modifiers/ei_modifier_normalize.h"
#include "../modifiers/ei_modifier_scale.h"
#include "../modifiers/ei_modifier_swizzle_axis.h"

#include "tests/test_macros.h"

namespace TestEIModifiers {

// Pull the ei namespace into scope so test bodies read naturally
// (`EIValue::make_axis1d` rather than `ei::EIValue::make_axis1d`).
using namespace ei;

// Helper: float equality within tolerance.
static bool approx_eq(float a, float b, float eps =1e-5f) {
	return Math::abs(a - b) < eps;
}

static bool vec2_approx_eq(const Vector2 &a, const Vector2 &b, float eps =1e-5f) {
	return approx_eq(a.x, b.x, eps) && approx_eq(a.y, b.y, eps);
}

static bool vec3_approx_eq(const Vector3 &a, const Vector3 &b, float eps =1e-5f) {
	return approx_eq(a.x, b.x, eps) && approx_eq(a.y, b.y, eps) && approx_eq(a.z, b.z, eps);
}

// ---------------------------------------------------------------------------
// DeadZone
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Modifier] DeadZone1D spec scenario") {
	ei::EIModifierDeadZone dz;
	dz.set_min_deadzone(0.2f);
	dz.set_max_deadzone(1.0f);

	// Per spec §4.3: Axis1D(0.15) ->0.0 (below deadzone)
	EIValue out15 = dz.modify_value(EIValue::make_axis1d(0.15f));
	CHECK(out15.get_type() == EIValue::TYPE_AXIS1D);
	CHECK(approx_eq(out15.get_axis1d(),0.0f));

	// Per spec §4.3: Axis1D(0.5) -> (0.5-0.2)/(1-0.2) =0.375
	EIValue out5 = dz.modify_value(EIValue::make_axis1d(0.5f));
	CHECK(approx_eq(out5.get_axis1d(),0.375f));

	// Negative side preserves sign.
	EIValue outn5 = dz.modify_value(EIValue::make_axis1d(-0.5f));
	CHECK(approx_eq(outn5.get_axis1d(), -0.375f));

	// Exactly at deadzone boundary ->0.
	EIValue out20 = dz.modify_value(EIValue::make_axis1d(0.2f));
	CHECK(approx_eq(out20.get_axis1d(),0.0f));

	// Above max: clamps to1.
	EIValue out15o = dz.modify_value(EIValue::make_axis1d(1.5f));
	CHECK(approx_eq(out15o.get_axis1d(),1.0f));
}

TEST_CASE("[EnhancedInput][Modifier] DeadZone2D radial") {
	ei::EIModifierDeadZone dz;
	dz.set_min_deadzone(0.2f);
	dz.set_max_deadzone(1.0f);

	// r=0.15 inside deadzone radius ->0 (|r| <= min_deadzone).
	EIValue out_small = dz.modify_value(EIValue::make_axis2d(Vector2(0.09f,0.12f))); // r=0.15
	CHECK(vec2_approx_eq(out_small.get_axis2d(), Vector2()));

	// r=5.0 above max -> remapped to length1.0 (clamped), direction preserved.
	EIValue out_unit = dz.modify_value(EIValue::make_axis2d(Vector2(3.0f,4.0f))); // r=5
	// (5-0.2)/(1.0-0.2) =4.8/0.8 =6.0, clamped to1.
	CHECK(vec2_approx_eq(out_unit.get_axis2d(), Vector2(0.6f,0.8f)));

	// r in mid range -> remapped length, same direction.
	EIValue out_mid = dz.modify_value(EIValue::make_axis2d(Vector2(0.3f,0.4f))); // r=0.5
	// (0.5-0.2)/(0.8) =0.375. direction = (0.6,0.8). result = (0.225,0.3).
	CHECK(vec2_approx_eq(out_mid.get_axis2d(), Vector2(0.225f,0.3f)));
}

TEST_CASE("[EnhancedInput][Modifier] DeadZone3D per-axis") {
	ei::EIModifierDeadZone dz;
	dz.set_min_deadzone(0.2f);

	EIValue out = dz.modify_value(EIValue::make_axis3d(Vector3(0.5f,0.15f, -0.5f)));
	Vector3 v = out.get_axis3d();
	CHECK(approx_eq(v.x,0.375f));
	CHECK(approx_eq(v.y,0.0f));
	CHECK(approx_eq(v.z, -0.375f));
}

TEST_CASE("[EnhancedInput][Modifier] DeadZone pass-through Bool") {
	ei::EIModifierDeadZone dz;
	EIValue out = dz.modify_value(EIValue::make_bool(true));
	CHECK(out.get_type() == EIValue::TYPE_BOOL);
	CHECK(out.get_bool());
}

// ---------------------------------------------------------------------------
// Negate
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Modifier] Negate spec scenario") {
	ei::EIModifierNegate neg;
	neg.set_negate_x(false);
	neg.set_negate_y(true);

	// Per spec §4.3: Negate{x=false,y=true} on Axis2D(1,1) -> Axis2D(1,-1)
	EIValue out = neg.modify_value(EIValue::make_axis2d(Vector2(1.0f,1.0f)));
	CHECK(vec2_approx_eq(out.get_axis2d(), Vector2(1.0f, -1.0f)));
}

TEST_CASE("[EnhancedInput][Modifier] Negate1D") {
	ei::EIModifierNegate neg;
	neg.set_negate_x(true);

	EIValue out_pos = neg.modify_value(EIValue::make_axis1d(0.7f));
	CHECK(approx_eq(out_pos.get_axis1d(), -0.7f));

	// off -> pass through.
	neg.set_negate_x(false);
	EIValue out_off = neg.modify_value(EIValue::make_axis1d(0.7f));
	CHECK(approx_eq(out_off.get_axis1d(),0.7f));
}

TEST_CASE("[EnhancedInput][Modifier] Negate3D per-axis") {
	ei::EIModifierNegate neg;
	neg.set_negate_z(true);

	EIValue out = neg.modify_value(EIValue::make_axis3d(Vector3(1.0f,2.0f,3.0f)));
	CHECK(vec3_approx_eq(out.get_axis3d(), Vector3(1.0f,2.0f, -3.0f)));
}

TEST_CASE("[EnhancedInput][Modifier] Negate pass-through Bool") {
	ei::EIModifierNegate neg;
	neg.set_negate_x(true);
	neg.set_negate_y(true);
	neg.set_negate_z(true);

	EIValue out = neg.modify_value(EIValue::make_bool(true));
	CHECK(out.get_bool());
}

// ---------------------------------------------------------------------------
// Scale
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Modifier] Scale1D") {
	ei::EIModifierScale s;
	s.set_scale_x(2.0f);

	EIValue out = s.modify_value(EIValue::make_axis1d(0.3f));
	CHECK(approx_eq(out.get_axis1d(),0.6f));
}

TEST_CASE("[EnhancedInput][Modifier] Scale2D per-axis") {
	ei::EIModifierScale s;
	s.set_scale_x(2.0f);
	s.set_scale_y(0.5f);

	EIValue out = s.modify_value(EIValue::make_axis2d(Vector2(1.0f,1.0f)));
	CHECK(vec2_approx_eq(out.get_axis2d(), Vector2(2.0f,0.5f)));
}

TEST_CASE("[EnhancedInput][Modifier] Scale3D") {
	ei::EIModifierScale s;
	s.set_scale_x(2.0f);
	s.set_scale_y(3.0f);
	s.set_scale_z(4.0f);

	EIValue out = s.modify_value(EIValue::make_axis3d(Vector3(1.0f,1.0f,1.0f)));
	CHECK(vec3_approx_eq(out.get_axis3d(), Vector3(2.0f,3.0f,4.0f)));
}

TEST_CASE("[EnhancedInput][Modifier] Scale pass-through Bool") {
	ei::EIModifierScale s;
	s.set_scale_x(100.0f);
	EIValue out = s.modify_value(EIValue::make_bool(false));
	CHECK(out.get_bool() == false);
}

// ---------------------------------------------------------------------------
// Normalize
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Modifier] Normalize2D spec scenario") {
	ei::EIModifierNormalize n;

	// Per spec §4.3: Axis2D(3,4) -> Axis2D(0.6,0.8)
	EIValue out = n.modify_value(EIValue::make_axis2d(Vector2(3.0f,4.0f)));
	CHECK(vec2_approx_eq(out.get_axis2d(), Vector2(0.6f,0.8f),1e-4f));
}

TEST_CASE("[EnhancedInput][Modifier] Normalize2D zero vector") {
	ei::EIModifierNormalize n;
	EIValue out = n.modify_value(EIValue::make_axis2d(Vector2()));
	CHECK(vec2_approx_eq(out.get_axis2d(), Vector2()));
}

TEST_CASE("[EnhancedInput][Modifier] Normalize2D min length suppression") {
	ei::EIModifierNormalize n;
	n.set_min_length(0.5f);

	// r=0.3 < min_length => zero.
	EIValue out = n.modify_value(EIValue::make_axis2d(Vector2(0.18f,0.24f)));
	CHECK(vec2_approx_eq(out.get_axis2d(), Vector2()));
}

TEST_CASE("[EnhancedInput][Modifier] Normalize3D") {
	ei::EIModifierNormalize n;
	// (1,2,2) has length3. Normalized: (1/3,2/3,2/3).
	EIValue out = n.modify_value(EIValue::make_axis3d(Vector3(1.0f,2.0f,2.0f)));
	CHECK(vec3_approx_eq(out.get_axis3d(), Vector3(1.0f /3.0f,2.0f /3.0f,2.0f /3.0f),1e-4f));
}

TEST_CASE("[EnhancedInput][Modifier] Normalize pass-through Bool and1D") {
	ei::EIModifierNormalize n;

	EIValue out_bool = n.modify_value(EIValue::make_bool(true));
	CHECK(out_bool.get_bool());

	EIValue out_1d = n.modify_value(EIValue::make_axis1d(0.42f));
	CHECK(approx_eq(out_1d.get_axis1d(),0.42f));
}

// ---------------------------------------------------------------------------
// SwizzleAxis
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Modifier] SwizzleAxis3D XY->YZ (order_x=Y, order_y=Z, order_z=X)") {
	ei::EIModifierSwizzleAxis s;
	s.set_order_x(ei::EIModifierSwizzleAxis::SOURCE_AXIS_Y);
	s.set_order_y(ei::EIModifierSwizzleAxis::SOURCE_AXIS_Z);
	s.set_order_z(ei::EIModifierSwizzleAxis::SOURCE_AXIS_X);

	EIValue out = s.modify_value(EIValue::make_axis3d(Vector3(1.0f,2.0f,3.0f)));
	CHECK(vec3_approx_eq(out.get_axis3d(), Vector3(2.0f,3.0f,1.0f)));
}

TEST_CASE("[EnhancedInput][Modifier] SwizzleAxis2D swap X and Y") {
	ei::EIModifierSwizzleAxis s;
	s.set_order_x(ei::EIModifierSwizzleAxis::SOURCE_AXIS_Y);
	s.set_order_y(ei::EIModifierSwizzleAxis::SOURCE_AXIS_X);

	EIValue out = s.modify_value(EIValue::make_axis2d(Vector2(7.0f,9.0f)));
	CHECK(vec2_approx_eq(out.get_axis2d(), Vector2(9.0f,7.0f)));
}

TEST_CASE("[EnhancedInput][Modifier] SwizzleAxis identity (defaults)") {
	ei::EIModifierSwizzleAxis s;
	// Defaults are X/X/X -> identity.

	EIValue out2 = s.modify_value(EIValue::make_axis2d(Vector2(1.0f,2.0f)));
	CHECK(vec2_approx_eq(out2.get_axis2d(), Vector2(1.0f,2.0f)));

	EIValue out3 = s.modify_value(EIValue::make_axis3d(Vector3(4.0f,5.0f,6.0f)));
	CHECK(vec3_approx_eq(out3.get_axis3d(), Vector3(4.0f,5.0f,6.0f)));
}

TEST_CASE("[EnhancedInput][Modifier] SwizzleAxis pass-through Bool") {
	ei::EIModifierSwizzleAxis s;
	s.set_order_x(ei::EIModifierSwizzleAxis::SOURCE_AXIS_Z); // nonsense but should not affect bool.
	EIValue out = s.modify_value(EIValue::make_bool(true));
	CHECK(out.get_bool());
}

// ---------------------------------------------------------------------------
// get_modifier_name for all subclasses
// ---------------------------------------------------------------------------

TEST_CASE("[EnhancedInput][Modifier] subclass names") {
	ei::EIModifierDeadZone dz;
	ei::EIModifierNegate neg;
	ei::EIModifierScale sc;
	ei::EIModifierNormalize n;
	ei::EIModifierSwizzleAxis s;
	CHECK(dz.get_modifier_name() == "DeadZone");
	CHECK(neg.get_modifier_name() == "Negate");
	CHECK(sc.get_modifier_name() == "Scale");
	CHECK(n.get_modifier_name() == "Normalize");
	CHECK(s.get_modifier_name() == "SwizzleAxis");
}

} // namespace TestEIModifiers
