/**************************************************************************/
/*  binding_engine.cpp                                                    */
/**************************************************************************/

#include "binding_engine.h"

#include "observable_property.h"
#include "view_model.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "scene/main/node.h"

void BindingEngine::_apply_to_target(const Variant &p_value, ObjectID p_target, const StringName &p_property) {
	Object *target = ObjectDB::get_instance(p_target);
	if (target) {
		target->set(p_property, p_value);
	}
}

void BindingEngine::bind_property(Object *p_target, const StringName &p_target_property, ViewModel *p_vm, const StringName &p_source_property) {
	ERR_FAIL_NULL(p_target);
	ERR_FAIL_NULL(p_vm);

	bool valid = false;
	p_target->get(p_target_property, &valid);
	if (!valid) {
		WARN_PRINT(vformat("BindingEngine: target '%s' has no property '%s'; skipping binding.", p_target->get_class(), String(p_target_property)));
		return;
	}

	Ref<ObservableProperty> prop = p_vm->get_property(p_source_property);

	// Initial sync, then keep the target updated on every change.
	p_target->set(p_target_property, prop->get_value());
	prop->connect("value_changed", callable_mp_static(&BindingEngine::_apply_to_target).bind(p_target->get_instance_id(), p_target_property));
}

void BindingEngine::bind_command(Object *p_source, const StringName &p_signal, ViewModel *p_vm, const StringName &p_method) {
	ERR_FAIL_NULL(p_source);
	ERR_FAIL_NULL(p_vm);

	if (!p_source->has_signal(p_signal)) {
		WARN_PRINT(vformat("BindingEngine: source '%s' has no signal '%s'; skipping command.", p_source->get_class(), String(p_signal)));
		return;
	}
	if (!p_vm->has_method(p_method)) {
		WARN_PRINT(vformat("BindingEngine: view model has no method '%s'; skipping command.", String(p_method)));
		return;
	}

	const Callable callable(p_vm, p_method);
	if (!p_source->is_connected(p_signal, callable)) {
		p_source->connect(p_signal, callable);
	}
}

void BindingEngine::_bind_recursive(Node *p_node, ViewModel *p_vm) {
	if (p_node->has_meta("bindings")) {
		const Array bindings = p_node->get_meta("bindings");
		for (int i = 0; i < bindings.size(); ++i) {
			const Dictionary d = bindings[i];
			const StringName target_prop = d.get("target_property", StringName());
			const StringName source_prop = d.get("source_property", StringName());
			if (target_prop != StringName() && source_prop != StringName()) {
				bind_property(p_node, target_prop, p_vm, source_prop);
			}
		}
	}

	if (p_node->has_meta("commands")) {
		const Array commands = p_node->get_meta("commands");
		for (int i = 0; i < commands.size(); ++i) {
			const Dictionary d = commands[i];
			const StringName signal_name = d.get("signal", StringName());
			const StringName method = d.get("method", StringName());
			if (signal_name != StringName() && method != StringName()) {
				bind_command(p_node, signal_name, p_vm, method);
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); ++i) {
		_bind_recursive(p_node->get_child(i), p_vm);
	}
}

void BindingEngine::bind(Node *p_view, ViewModel *p_vm) {
	ERR_FAIL_NULL(p_view);
	ERR_FAIL_NULL(p_vm);
	_bind_recursive(p_view, p_vm);
}

void BindingEngine::_bind_methods() {
	ClassDB::bind_static_method("BindingEngine", D_METHOD("bind", "view", "view_model"), &BindingEngine::bind);
	ClassDB::bind_static_method("BindingEngine", D_METHOD("bind_property", "target", "target_property", "view_model", "source_property"), &BindingEngine::bind_property);
	ClassDB::bind_static_method("BindingEngine", D_METHOD("bind_command", "source", "signal", "view_model", "method"), &BindingEngine::bind_command);
}
