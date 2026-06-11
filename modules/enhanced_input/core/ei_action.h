/**************************************************************************/
/* ei_action.h */
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

// Sibling includes (same `core/` directory) so this header resolves the
// same way when included from test_main.cpp (which lacks our module's
// CPPATH) and from the in-module .cpp files. The module-root form
// ("core/ei_modifier.h") only works when the module's CPPATH is set;
// the sibling form works in both contexts.
#include "core/io/resource.h"
#include "core/variant/typed_array.h"
#include "ei_modifier.h"
#include "ei_trigger.h"

namespace ei {

// Logical input unit, decoupled from any key/button.
//
// Per spec §4.2 (enhanced-input.md v0.2):
// * EIAction is a Resource so it can be saved as .tres and reused across
// multiple EIMappingContext entries (deduped at runtime by the subsystem).
// * default_modifiers are applied AFTER per-mapping modifiers in the IMC.
// * default_triggers are applied AFTER per-mapping triggers in the IMC.
// * P2-P3 used TypedArray<Resource> as a placeholder for default_triggers.
// P4 narrows this to TypedArray<EITrigger>. Existing .tres files load with
// an empty default array (Godot's standard forward-compat behavior).

class EIAction : public Resource {
	GDCLASS(EIAction, Resource);

public:
	enum ValueType {
		VALUE_TYPE_BOOL =0,
		VALUE_TYPE_AXIS1D =1,
		VALUE_TYPE_AXIS2D =2,
		VALUE_TYPE_AXIS3D =3,
	};

	void set_value_type(ValueType p_t);
	ValueType get_value_type() const;

	void set_default_modifiers(const TypedArray<EIModifier> &p_mods);
	TypedArray<EIModifier> get_default_modifiers() const;

	void set_default_triggers(const TypedArray<EITrigger> &p_trigs);
	TypedArray<EITrigger> get_default_triggers() const;

	void set_description(const String &p_desc);
	String get_description() const;

protected:
	static void _bind_methods();

private:
	ValueType _value_type = VALUE_TYPE_BOOL;
	TypedArray<EIModifier> _default_modifiers;
	TypedArray<EITrigger> _default_triggers;
	String _description;
};

} // namespace ei

VARIANT_ENUM_CAST(ei::EIAction::ValueType);
