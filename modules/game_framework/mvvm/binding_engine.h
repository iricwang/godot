/**************************************************************************/
/*  binding_engine.h                                                      */
/**************************************************************************/
#pragma once

#include "core/object/object.h"
#include "core/string/string_name.h"

class Node;
class ViewModel;

// Establishes one-way data bindings (ViewModel property -> target node property) and command bindings
// (node signal -> ViewModel method). Two ways to use it:
//   * Explicit (preferred, type-checkable): bind_property() / bind_command().
//   * Batch sugar: bind(view, vm) scans node meta "bindings" / "commands" and calls the explicit API.
// Property bindings address the target by ObjectID, so once the target node is freed the update becomes a no-op
// (no dangling access). Command connections are torn down automatically when the source node is freed.
class BindingEngine : public Object {
	GDCLASS(BindingEngine, Object);

	static void _apply_to_target(const Variant &p_value, ObjectID p_target, const StringName &p_property);
	static void _bind_recursive(Node *p_node, ViewModel *p_vm);

protected:
	static void _bind_methods();

public:
	static void bind(Node *p_view, ViewModel *p_vm);
	static void bind_property(Object *p_target, const StringName &p_target_property, ViewModel *p_vm, const StringName &p_source_property);
	static void bind_command(Object *p_source, const StringName &p_signal, ViewModel *p_vm, const StringName &p_method);
};
