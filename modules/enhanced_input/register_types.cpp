/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                             https://godotengine.org                   */
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

#include "register_types.h"

#include "core/ei_action.h"
#include "core/ei_bridge.h"
#include "core/ei_component.h"
#include "core/ei_input_event_sampler.h"
#include "core/ei_mapping_context.h"
#include "core/ei_modifier.h"
#include "core/ei_subsystem.h"
#include "core/ei_trigger.h"
#include "core/object/class_db.h"
#include "modifiers/ei_modifier_dead_zone.h"
#include "modifiers/ei_modifier_negate.h"
#include "modifiers/ei_modifier_normalize.h"
#include "modifiers/ei_modifier_scale.h"
#include "modifiers/ei_modifier_swizzle_axis.h"
#include "triggers/ei_trigger_chord.h"
#include "triggers/ei_trigger_double_tap.h"
#include "triggers/ei_trigger_hold.h"
#include "triggers/ei_trigger_pressed.h"
#include "triggers/ei_trigger_pulse.h"
#include "triggers/ei_trigger_release.h"
#include "triggers/ei_trigger_tap.h"

#ifdef TOOLS_ENABLED
// P7: editor-side custom inspector for EIMappingContext. Only pulled
// in for editor builds; the .cpp behind this header is itself gated
// by `env.editor_build` in SCsub, so a non-editor binary linking
// against this module never sees EditorInspectorPlugin / EditorPlugin.
#include "editor/ei_editor_plugin.h"
#include "editor/plugins/editor_plugins.h"
#endif

// P1: EISubsystem lifecycle.
//
// EISubsystem is a Node subclass. We register its GDCLASS with ClassDB
// here so GDScript (and the test runner) can `ClassDB.instantiate(...)`
// it, but the singleton instance is NOT created at module init.
//
// Why no `memnew` here: in test mode (godot --test) there is no
// SceneTree, so a free-floating EISubsystem Node shows up in
// `Node.print_orphan_nodes()` and breaks GDScript runtime tests that
// assert on the orphan-node output. We need a parent in the tree to
// silence that.
//
// The GDScript autoload (ei_autoload.gd) is the natural owner: it runs
// in the SceneTree, instantiates the C++ EISubsystem via
// `ClassDB.instantiate("EISubsystem")`, adds it as a child of itself
// (so it lives in the tree and is freed when the autoload exits), and
// registers it with `Engine.register_singleton("EISubsystem", ...)` so
// `Engine.get_singleton("EISubsystem")` returns it.
//
// In test mode the autoload never runs, so no EISubsystem instance is
// ever created — no leak, no orphan print, no test regression.

void initialize_enhanced_input_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// EIValue is a plain C++ struct (no GDCLASS), so it is not registered
		// with ClassDB. It is exposed to GDScript indirectly via the EISubsystem
		// API which returns Variant.

		// P2: register base classes.
		// * EIModifier has pure virtual methods (modify_value, get_modifier_name) —
		// use register_abstract_class so the compiler doesn't try to instantiate
		// it (C2259). Concrete subclasses in P3 (DeadZone, Negate, ...) will be
		// GDREGISTER_CLASS'd normally and instantiable from .tres / inspector.
		// * EIModifier must be registered BEFORE EIAction because
		// EIAction.default_modifiers is TypedArray<EIModifier> and the class
		// registry needs the element type to be known at add-property time.
		ClassDB::register_abstract_class<ei::EIModifier>();
		GDREGISTER_CLASS(ei::EIAction);

		// P3: concrete modifier subclasses. Order doesn't matter as long as the
		// abstract base is registered first.
		GDREGISTER_CLASS(ei::EIModifierDeadZone);
		GDREGISTER_CLASS(ei::EIModifierNegate);
		GDREGISTER_CLASS(ei::EIModifierScale);
		GDREGISTER_CLASS(ei::EIModifierNormalize);
		GDREGISTER_CLASS(ei::EIModifierSwizzleAxis);

		// P4: EITrigger is the abstract base for trigger state machines. Like
		// EIModifier it has a pure virtual (update_state) and needs
		// register_abstract_class. EIAction.default_triggers is
		// TypedArray<EITrigger> in P4, so the base MUST be registered
		// before EIAction is bound (which already happened above).
		ClassDB::register_abstract_class<ei::EITrigger>();

		// P4: concrete trigger subclasses — all 7 from spec §4.4.
		GDREGISTER_CLASS(ei::EITriggerPressed);
		GDREGISTER_CLASS(ei::EITriggerHold);
		GDREGISTER_CLASS(ei::EITriggerTap);
		GDREGISTER_CLASS(ei::EITriggerDoubleTap);
		GDREGISTER_CLASS(ei::EITriggerPulse);
		GDREGISTER_CLASS(ei::EITriggerChord);
		GDREGISTER_CLASS(ei::EITriggerRelease);

		// P1: minimal subsystem class registration (full impl lands in P5).
		// No instance is created here — see file header for rationale.
		GDREGISTER_CLASS(ei::EISubsystem);

		// P5a: EIMappingContext is a Resource holding InputEvent→EIAction
		// mappings. Registered here so the inspector and GDScript can
		// instantiate it for .tres authoring. EIInputEventSampler is a
		// static helper (no GDCLASS) and does not need ClassDB.
		GDREGISTER_CLASS(ei::EIMappingContext);

		// P5 final: EIComponent is the GDScript-friendly wrapper that
		// owns a list of trigger-event subscriptions and forwards them
		// to the EISubsystem singleton. P5b subscribes via direct
		// bind_action on the subsystem; EIComponent is the per-instance
		// registry that auto-unbinds on _exit_tree.
		GDREGISTER_CLASS(ei::EIComponent);

		// P6: EIBridge is the read-only importer from Godot's InputMap
		// to EI's resource model. Useful for projects bootstrapping EI
		// without rewriting every existing binding. Pure RefCounted;
		// not a Node, not a Resource.
		GDREGISTER_CLASS(ei::EIBridge);
	}
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		// P7 (spec §4.9): register the EditorPlugin. The plugin's
		// constructor instantiates an EIInspectorPlugin (for EIMappingContext)
		// and adds it via add_inspector_plugin(). EditorPlugins::add_by_type<>
		// instantiates the plugin and the engine takes ownership.
		GDREGISTER_CLASS(ei::EIEditorPlugin);
		EditorPlugins::add_by_type<ei::EIEditorPlugin>();
	}
#endif
}

void uninitialize_enhanced_input_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// Nothing to unregister: ClassDB manages class lifetime, and the
	// singleton instance (if any) is owned by the GDScript autoload's
	// scene tree and freed when the tree is torn down.
}
