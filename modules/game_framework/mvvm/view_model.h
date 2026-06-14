/**************************************************************************/
/*  view_model.h                                                          */
/**************************************************************************/
#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"

#include "observable_property.h"

class Node;
struct PropertyInfo;

// Base class for view models. Holds named ObservableProperty values. GDScript subclasses add business logic and
// command methods. Properties are created lazily on first access.
//
// Native property syntax via _set / _get: GDScript subclasses can use ordinary assignment/read without
// calling set_property()/get_value() explicitly.  Example:
//   extends ViewModel
//   func _init(): counter = 0; title = "Hello"      # routed to _set → ObservableProperty
//   func on_inc(): counter += 1                      # routed to _get → _set → triggers value_changed
//
// The string-based API (set_property / get_value / get_property) is still available and is the internal
// path used by BindingEngine when it observes changes.  Properties can be written through either channel
// and read through either channel — they share the same ObservableProperty backing store.
class ViewModel : public RefCounted {
	GDCLASS(ViewModel, RefCounted);

	HashMap<StringName, Ref<ObservableProperty>> props;

protected:
	static void _bind_methods();

	// Godot Object overrides — route native GDScript property access to ObservableProperty map.
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	Ref<ObservableProperty> get_property(const StringName &p_name); // creates it if missing
	bool has_property(const StringName &p_name) const;
	void set_property(const StringName &p_name, const Variant &p_value);
	Variant get_value(const StringName &p_name) const;
	void subscribe_property(const StringName &p_name, const Callable &p_callable);
	PackedStringArray get_property_names() const;

	// Batch update: apply begin/end_bulk_update to every existing property.
	// C++ side iterates the internal HashMap (no GDScript bridge cost).
	void begin_bulk_update();
	void end_bulk_update();

	void dispose(); // releases all properties (disconnecting their subscribers)

	// Auto-wire @BindProperty / @BindSignal annotations by scanning properties
	// and calling BindingEngine. p_owner is the Node that owns this ViewModel
	// (typically the node the script is attached to).
	void apply_bindings(Node *p_owner);

private:
	static void _find_node_with_signal(Node *p_node, const StringName &p_signal, Node **r_found);
	static void _collect_nodes_with_signal(Node *p_node, const StringName &p_signal, Vector<Node *> &r_out);
};
