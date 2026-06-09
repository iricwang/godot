/**************************************************************************/
/*  view_model.cpp                                                        */
/**************************************************************************/

#include "view_model.h"

#include "binding_engine.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/variant/dictionary.h"
#include "scene/main/node.h"

Ref<ObservableProperty> ViewModel::get_property(const StringName &p_name) {
	HashMap<StringName, Ref<ObservableProperty>>::Iterator it = props.find(p_name);
	if (it) {
		return it->value;
	}
	Ref<ObservableProperty> prop;
	prop.instantiate();
	props[p_name] = prop;
	return prop;
}

bool ViewModel::has_property(const StringName &p_name) const {
	return props.has(p_name);
}

void ViewModel::set_property(const StringName &p_name, const Variant &p_value) {
	get_property(p_name)->set_value(p_value);
}

Variant ViewModel::get_value(const StringName &p_name) const {
	HashMap<StringName, Ref<ObservableProperty>>::ConstIterator it = props.find(p_name);
	return it ? it->value->get_value() : Variant();
}

void ViewModel::subscribe_property(const StringName &p_name, const Callable &p_callable) {
	get_property(p_name)->subscribe(p_callable);
}

PackedStringArray ViewModel::get_property_names() const {
	PackedStringArray out;
	for (const KeyValue<StringName, Ref<ObservableProperty>> &E : props) {
		out.push_back(String(E.key));
	}
	return out;
}

void ViewModel::begin_bulk_update() {
	for (KeyValue<StringName, Ref<ObservableProperty>> &E : props) {
		if (E.value.is_valid()) {
			E.value->begin_bulk_update();
		}
	}
}

void ViewModel::end_bulk_update() {
	for (KeyValue<StringName, Ref<ObservableProperty>> &E : props) {
		if (E.value.is_valid()) {
			E.value->end_bulk_update();
		}
	}
}

void ViewModel::dispose() {
	props.clear();
}

void ViewModel::_find_node_with_signal(Node *p_node, const StringName &p_signal, Node **r_found) {
	if (*r_found != nullptr) {
		return; // Already found.
	}
	if (p_node->has_signal(p_signal)) {
		*r_found = p_node;
		return;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_find_node_with_signal(p_node->get_child(i), p_signal, r_found);
		if (*r_found != nullptr) {
			return;
		}
	}
}

void ViewModel::_collect_nodes_with_signal(Node *p_node, const StringName &p_signal, Vector<Node *> &r_out) {
	if (p_node->has_signal(p_signal)) {
		r_out.push_back(p_node);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_collect_nodes_with_signal(p_node->get_child(i), p_signal, r_out);
	}
}

void ViewModel::apply_bindings(Node *p_owner) {
	ERR_FAIL_NULL(p_owner);

	Ref<Script> scr = get_script();
	ERR_FAIL_NULL(scr);

	Array bind_configs = scr->get_meta("_bind_configs", Array());
	for (int i = 0; i < bind_configs.size(); i++) {
		Dictionary cfg = bind_configs[i];
		StringName source_prop = cfg.get("source_prop", StringName());
		NodePath target_path = cfg.get("target_node", NodePath());
		String target_prop = cfg.get("target_prop", "");
		int mode = cfg.get("mode", 0);

		if (source_prop == StringName() || target_path.is_empty() || target_prop.is_empty()) {
			continue;
		}

		Node *target_node = p_owner->get_node_or_null(target_path);
		if (!target_node) {
			WARN_PRINT(vformat("ViewModel.apply_bindings: node '%s' not found for property '%s', skipping.",
					String(target_path), String(source_prop)));
			continue;
		}

		BindingEngine::bind_property(target_node, target_prop, this, source_prop, mode);
	}

	// Also run the batch metadata scan for any node-level "bindings"/"commands" metadata.
	BindingEngine::bind(p_owner, this);

	// Auto-wire @BindSignal commands: connect the method to **every** descendant node
	// that has the requested signal. This lets one VM method react to multiple triggers
	// (e.g. any "pressed" button), and avoids the wrong-target bug where a CheckBox's
	// `pressed` signal shadows a real Button's `pressed` (DFS would otherwise stop early).
	Array bind_commands = scr->get_meta("_bind_commands", Array());
	for (int i = 0; i < bind_commands.size(); i++) {
		Dictionary cmd = bind_commands[i];
		StringName signal_name = cmd.get("signal", StringName());
		StringName method = cmd.get("method", StringName());
		if (signal_name == StringName() || method == StringName()) {
			continue;
		}

		// Collect all descendants with the requested signal.
		Vector<Node *> matches;
		_collect_nodes_with_signal(p_owner, signal_name, matches);
		if (matches.is_empty()) {
			WARN_PRINT(vformat("ViewModel.apply_bindings: no node with signal '%s' found for @BindSignal method '%s', skipping.",
					String(signal_name), String(method)));
			continue;
		}
		for (Node *n : matches) {
			BindingEngine::bind_command(n, signal_name, this, method);
		}
	}
}

// ---- Godot Object overrides: native GDScript property syntax ----

bool ViewModel::_set(const StringName &p_name, const Variant &p_value) {
	set_property(p_name, p_value);
	return true; // handled — don't fall back to default Object behavior
}

bool ViewModel::_get(const StringName &p_name, Variant &r_ret) const {
	HashMap<StringName, Ref<ObservableProperty>>::ConstIterator it = props.find(p_name);
	if (it) {
		r_ret = it->value->get_value();
		return true;
	}
	return false; // not found — fall back to default
}

void ViewModel::_get_property_list(List<PropertyInfo> *p_list) const {
	for (const KeyValue<StringName, Ref<ObservableProperty>> &E : props) {
		PropertyInfo pi;
		pi.name = E.key;
		pi.type = E.value->get_value().get_type();
		pi.usage = PROPERTY_USAGE_DEFAULT;
		p_list->push_back(pi);
	}
}

void ViewModel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_property", "name"), &ViewModel::get_property);
	ClassDB::bind_method(D_METHOD("has_property", "name"), &ViewModel::has_property);
	ClassDB::bind_method(D_METHOD("set_property", "name", "value"), &ViewModel::set_property);
	ClassDB::bind_method(D_METHOD("get_value", "name"), &ViewModel::get_value);
	ClassDB::bind_method(D_METHOD("subscribe_property", "name", "callable"), &ViewModel::subscribe_property);
	ClassDB::bind_method(D_METHOD("get_property_names"), &ViewModel::get_property_names);
	ClassDB::bind_method(D_METHOD("begin_bulk_update"), &ViewModel::begin_bulk_update);
	ClassDB::bind_method(D_METHOD("end_bulk_update"), &ViewModel::end_bulk_update);
	ClassDB::bind_method(D_METHOD("dispose"), &ViewModel::dispose);
	ClassDB::bind_method(D_METHOD("apply_bindings", "owner"), &ViewModel::apply_bindings);
}
