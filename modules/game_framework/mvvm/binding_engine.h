/**************************************************************************/
/*  binding_engine.h                                                      */
/**************************************************************************/
#pragma once

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/string/string_name.h"
#include "core/templates/list.h"
#include "core/variant/array.h"

class Node;
class ViewModel;
class ValueConverter;

// Establishes data bindings (ViewModel property <-> target node property) and command bindings
// (node signal -> ViewModel method). Two ways to use it:
//   * Explicit (preferred, type-checkable): bind_property() / bind_command().
//   * Batch sugar: bind(view, vm) scans node meta "bindings" / "commands" and calls the explicit API.
// Property bindings address the target by ObjectID, so once the target node is freed the update becomes a no-op
// (no dangling access). Command connections are torn down automatically when the source node is freed.
//
// Two-way binding (mode == 1): the target property change is watched via registered signal names
// and written back to the ViewModel. Use register_two_way_signal() to add custom signals.
// Default signals: "toggled", "value_changed", "text_changed", "pressed", "item_selected"
//
// ValueConverter support: pass a ValueConverter to transform values in both directions.
class BindingEngine : public Object {
	GDCLASS(BindingEngine, Object);

	static void _apply_to_target(const Variant &p_value, ObjectID p_target, const StringName &p_property);
	static void _apply_to_view_model(const Variant &p_value, ObjectID p_vm_id, const StringName &p_source_property, const Variant &p_converter);
	static void _bind_recursive(Node *p_node, ViewModel *p_vm);

public:
	static List<StringName> _two_way_signals;
	// Lazy initializer for default two-way signals — public so the file-local helper can call it
	// (avoids needing a friend declaration, and `private` is enforced by C++ convention here).
	static void _ensure_static_init();

protected:
	static void _bind_methods();

public:
	// Two-way signal management
	static void register_two_way_signal(const StringName &p_signal);
	static void unregister_two_way_signal(const StringName &p_signal);
	static bool has_two_way_signal(const StringName &p_signal);
	static Array get_registered_two_way_signals();

	static void bind(Node *p_view, ViewModel *p_vm);
	static void bind_property(Object *p_target, const StringName &p_target_property, ViewModel *p_vm, const StringName &p_source_property, int p_mode = 0, const Ref<ValueConverter> &p_converter = Ref<ValueConverter>());
	static void bind_command(Object *p_source, const StringName &p_signal, ViewModel *p_vm, const StringName &p_method);
};
