/**************************************************************************/
/* ei_modifier_dead_zone.h */
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

// Module-relative path so this header resolves correctly when included
// from test_main.cpp (which is in tests/ and lacks the module's CPPATH).
#include "../core/ei_modifier.h"

namespace ei {

// DeadZone per spec §4.3:
//1D: abs(x) < min_deadzone ?0 : sign(x) * (abs(x) - min_deadzone) / (1 - min_deadzone)
//2D: radial — convert to polar, apply1D formula to radius
//3D: per-axis1D (matches UE behavior — no radial3D)
// Bool: pass-through
//
// `max_deadzone` is reserved for dual-stick2D radial outer clamp. In v1 it
// is plumbed through but the formula is identical to1D when max ==1.0
// (default). Future revisions may clamp the outer ring.

class EIModifierDeadZone : public EIModifier {
	GDCLASS(EIModifierDeadZone, EIModifier);

public:
	void set_min_deadzone(float p_v);
	float get_min_deadzone() const;

	void set_max_deadzone(float p_v);
	float get_max_deadzone() const;

	virtual EIValue modify_value(const EIValue &p_input) const override;
	virtual String get_modifier_name() const override;

protected:
	static void _bind_methods();

private:
	float _min_deadzone =0.2f;
	float _max_deadzone =1.0f;
};

} // namespace ei
