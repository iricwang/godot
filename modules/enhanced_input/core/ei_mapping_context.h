/**************************************************************************/
/*  ei_mapping_context.h                                                 */
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
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/**************************************************************************/

#pragma once

// Sibling include (same `core/` directory) — see ei_modifier.h / ei_action.h
// for the rationale. Lets this header resolve from both in-module sources
// and tests/.
#include "core/io/resource.h"
#include "core/templates/vector.h"
#include "core/variant/typed_array.h"
#include "ei_action.h"

class InputEvent;

namespace ei {

// A bag of InputEvent → EIAction mappings, applied to the dispatch loop as
// a unit. Multiple IMCs can be active simultaneously; the dispatcher
// iterates them in priority order (see spec §4.6).
//
// Per spec §4.5:
// * `Mapping` is an internal struct (the spec shows it as a public nested
//   class; we keep it private and expose the list as TypedArray<Dictionary>
//   to the inspector and to GDScript). Internal callers
//   (EISubsystem dispatcher) use the typed `get_mappings_internal()`
//   accessor for performance.
// * Modifiers and triggers are applied in the order: per-mapping first,
//   then action.default_modifiers / action.default_triggers. This matches
//   the spec scenario in §4.5/§4.6 and the dispatch flow in §5.
// * `consumes` controls whether the dispatcher calls
//   `Viewport::set_input_as_handled()` after firing a mapping — see spec
//   §4.6 dispatch rules and §6.2 for layered-UI usage.
//
// EIMappingContext is a Resource so it can be saved as .tres, duplicated
// across scenes, and edited in the inspector.
class EIMappingContext : public Resource {
	GDCLASS(EIMappingContext, Resource);

public:
	// A single binding inside an IMC. Kept private (rather than the
	// spec's public nested class) because the Godot binding system
	// can't easily expose a Vector<MappingClass> directly; we surface
	// the list as TypedArray<Dictionary> for the inspector instead.
	struct Mapping {
		Ref<InputEvent> event; // any InputEvent subtype
		Ref<EIAction> action;
		TypedArray<EIModifier> modifiers; // applied BEFORE action.default_modifiers
		TypedArray<EITrigger> triggers; // applied BEFORE action.default_triggers
		bool consumes = true; // when true, dispatcher calls set_input_as_handled()
	};

	// --- Programmatic API --------------------------------------------

	// Append a mapping. Multiple mappings for the same (event, action)
	// pair are allowed; the dispatcher uses the first match per
	// context (see spec §6.2).
	void add_mapping(const Ref<InputEvent> &p_event,
			const Ref<EIAction> &p_action,
			const TypedArray<EIModifier> &p_mods,
			const TypedArray<EITrigger> &p_trigs,
			bool p_consumes);

	// Remove the FIRST mapping matching both event and action. No-op if
	// no such mapping exists. Matches by event reference (==) and
	// action reference (==), not by value.
	void remove_mapping(const Ref<InputEvent> &p_event, const Ref<EIAction> &p_action);

	// Drop all mappings. Useful for "reset to default" inspector
	// actions and for tests.
	void clear_mappings();

	// --- Inspector / serialization surface --------------------------

	// Array of Dictionary<event, action, modifiers, triggers, consumes>.
	// Each Dictionary is a snapshot of one Mapping. The setter
	// REPLACES the internal list; the getter returns a fresh array on
	// every call (TypedArray<Dictionary> is a copy-on-write handle).
	void set_mappings(const TypedArray<Dictionary> &p_mappings);
	TypedArray<Dictionary> get_mappings() const;

	int get_mapping_count() const;

	// --- Internal access for the dispatcher (P5b) --------------------

	// Direct access to the internal list. Use sparingly — only the
	// dispatcher should call this. Iterating the result is safe but
	// the returned reference is invalidated by any subsequent
	// add_mapping / remove_mapping / clear_mappings / set_mappings
	// call.
	const Vector<Mapping> &get_mappings_internal() const;

	// --- Metadata -----------------------------------------------------

	void set_context_name(const String &p_name);
	String get_context_name() const;

protected:
	static void _bind_methods();

private:
	String _context_name;
	Vector<Mapping> _mappings;
};

} // namespace ei
