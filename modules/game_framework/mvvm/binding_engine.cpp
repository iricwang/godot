/**************************************************************************/
/*  binding_engine.cpp                                                    */
/**************************************************************************/

#include "binding_engine.h"

#include "observable_property.h"
#include "view_model.h"
#include "value_converter.h"

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

void BindingEngine::_apply_to_view_model(const Variant &p_value, ObjectID p_vm_id, const StringName &p_source_property, const Variant &p_converter) {
	Object *obj = ObjectDB::get_instance(p_vm_id);
	ViewModel *vm = Object::cast_to<ViewModel>(obj);
	if (vm) {
		Variant converted = p_value;
		if (p_converter.get_type() == Variant::OBJECT) {
			Ref<ValueConverter> conv = p_converter;
			if (conv.is_valid()) {
				converted = conv->convert_back(p_value);
			}
		}
		vm->set_property(p_source_property, converted);
	}
}

// Default signal names for two-way binding on common Control types.
List<StringName> BindingEngine::_two_way_signals;

void BindingEngine::_bind_methods() {
	ClassDB::bind_static_method("BindingEngine", D_METHOD("register_two_way_signal", "signal"), &BindingEngine::register_two_way_signal);
	ClassDB::bind_static_method("BindingEngine", D_METHOD("unregister_two_way_signal", "signal"), &BindingEngine::unregister_two_way_signal);
	ClassDB::bind_static_method("BindingEngine", D_METHOD("has_two_way_signal", "signal"), &BindingEngine::has_two_way_signal);
	ClassDB::bind_static_method("BindingEngine", D_METHOD("get_registered_two_way_signals"), &BindingEngine::get_registered_two_way_signals);

	ClassDB::bind_static_method("BindingEngine", D_METHOD("bind", "view", "view_model"), &BindingEngine::bind);
	// DEFVAL(0) covers mode; converter has no C++ default in DEFVAL since it's a typed Ref arg,
	// so we provide a nil-Ref default at the call site via the static overload below.
	ClassDB::bind_static_method("BindingEngine", D_METHOD("bind_property", "target", "target_property", "view_model", "source_property", "mode", "converter"), &BindingEngine::bind_property, DEFVAL(0), DEFVAL(Variant()));
	ClassDB::bind_static_method("BindingEngine", D_METHOD("bind_command", "source", "signal", "view_model", "method"), &BindingEngine::bind_command);
}

// Lazy initializer: register default two-way signals on first call to _ensure_static_init().
// Avoids static initialization order fiasco (SIOF) — static class members are
// zero-initialized by C++ rules, but a global struct with a constructor that touches them
// may run before the binding engine class registration completes. Lazy init sidesteps that.
void BindingEngine::_ensure_static_init() {
	static bool initialized = false;
	if (initialized) {
		return;
	}
	initialized = true;
	BindingEngine::_two_way_signals.push_back("toggled");
	BindingEngine::_two_way_signals.push_back("value_changed");
	BindingEngine::_two_way_signals.push_back("text_changed");
	BindingEngine::_two_way_signals.push_back("pressed");
	BindingEngine::_two_way_signals.push_back("item_selected");
}

void BindingEngine::register_two_way_signal(const StringName &p_signal) {
	_ensure_static_init();
	for (const StringName &sig : _two_way_signals) {
		if (sig == p_signal) {
			return; // Already registered.
		}
	}
	_two_way_signals.push_back(p_signal);
}

void BindingEngine::unregister_two_way_signal(const StringName &p_signal) {
	_ensure_static_init();
	_two_way_signals.erase(p_signal);
}

bool BindingEngine::has_two_way_signal(const StringName &p_signal) {
	_ensure_static_init();
	for (const StringName &sig : _two_way_signals) {
		if (sig == p_signal) {
			return true;
		}
	}
	return false;
}

Array BindingEngine::get_registered_two_way_signals() {
	_ensure_static_init();
	Array result;
	for (const StringName &sig : _two_way_signals) {
		result.push_back(String(sig));
	}
	return result;
}

void BindingEngine::bind_property(Object *p_target, const StringName &p_target_property, ViewModel *p_vm, const StringName &p_source_property, int p_mode, const Ref<ValueConverter> &p_converter) {
	ERR_FAIL_NULL(p_target);
	ERR_FAIL_NULL(p_vm);

	bool valid = false;
	p_target->get(p_target_property, &valid);
	if (!valid) {
		WARN_PRINT(vformat("BindingEngine: target '%s' has no property '%s'; skipping binding.", p_target->get_class(), String(p_target_property)));
		return;
	}

	Ref<ObservableProperty> prop = p_vm->get_property(p_source_property);

	// Apply converter for initial sync and VM→View updates.
	Variant display_value = prop->get_value();
	if (p_converter.is_valid()) {
		display_value = p_converter->convert(display_value);
	}

	// Initial sync, then keep the target updated on every change.
	p_target->set(p_target_property, display_value);
	prop->connect("value_changed", callable_mp_static(&BindingEngine::_apply_to_target).bind(p_target->get_instance_id(), p_target_property));

	// Two-way binding: watch target changes and write back to ViewModel.
	if (p_mode == 1) {
		_ensure_static_init();
		ObjectID vm_id = p_vm->get_instance_id();
		// Bind the Ref<ValueConverter> as a Variant argument to the Callable. Godot's Callable
		// keeps a strong reference to bound Variants, so the converter stays alive for as
		// long as the connection exists. When the connection is torn down (target freed or
		// vm.dispose), the bound Variant (and its Ref) is released automatically.
		Variant converter_var;
		if (p_converter.is_valid()) {
			converter_var = Variant(p_converter);
		}
		for (const StringName &sig : _two_way_signals) {
			if (p_target->has_signal(sig)) {
				Callable cb = callable_mp_static(&BindingEngine::_apply_to_view_model).bind(vm_id, p_source_property, converter_var);
				if (!p_target->is_connected(sig, cb)) {
					p_target->connect(sig, cb);
				}
				break; // First matching signal wins.
			}
		}
	}
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
			const int mode = d.get("mode", 0);
			if (target_prop != StringName() && source_prop != StringName()) {
				bind_property(p_node, target_prop, p_vm, source_prop, mode);
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
