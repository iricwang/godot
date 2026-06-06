/**************************************************************************/
/*  dialog.cpp                                                            */
/**************************************************************************/

#include "dialog.h"

#include "../context.h"

#include "core/io/resource.h"
#include "core/object/class_db.h"

void Dialog::set_intent(const Ref<Intent> &p_intent) {
	intent = p_intent;
}

Ref<Intent> Dialog::get_intent() const {
	return intent;
}

void Dialog::set_transition_in(const Ref<Transition> &p_transition) {
	transition_in = p_transition;
}

Ref<Transition> Dialog::get_transition_in() const {
	return transition_in;
}

void Dialog::set_transition_out(const Ref<Transition> &p_transition) {
	transition_out = p_transition;
}

Ref<Transition> Dialog::get_transition_out() const {
	return transition_out;
}

void Dialog::dispatch_create(const Dictionary &p_saved_state) {
	GDVIRTUAL_CALL(_on_create, p_saved_state);
}

void Dialog::dispatch_dismiss() {
	GDVIRTUAL_CALL(_on_dismiss);
}

void Dialog::dismiss() {
	if (context) {
		context->dismiss_dialog(this);
	}
}

// ---- Owner (lifecycle binding) ----

void Dialog::set_lifecycle_owner(Object *p_owner) {
	owner = p_owner;
}

Object *Dialog::get_lifecycle_owner() const {
	return owner;
}

// ---- Context access ----

void Dialog::set_context(Context *p_context) {
	context = p_context;
}

Context *Dialog::get_context() const {
	return context;
}

// ---- Convenience methods (delegate to context) ----

void Dialog::show_toast(const Ref<Toast> &p_toast) {
	ERR_FAIL_NULL(context);
	context->show_toast_with_owner(p_toast, this);
}

Object *Dialog::get_service(const StringName &p_name) const {
	ERR_FAIL_NULL_V(context, nullptr);
	return context->get_service(p_name);
}

bool Dialog::has_service(const StringName &p_name) const {
	ERR_FAIL_NULL_V(context, false);
	return context->has_service(p_name);
}

Ref<ResourceHandle> Dialog::get_resource_handle(const String &p_path) {
	ERR_FAIL_NULL_V(context, Ref<ResourceHandle>());
	return context->get_resource_handle(p_path);
}

Ref<Resource> Dialog::load_resource_sync(const String &p_path) {
	ERR_FAIL_NULL_V(context, Ref<Resource>());
	return context->load_resource_sync(p_path);
}

void Dialog::load_resource_async(const String &p_path, const Callable &p_callback, int p_priority) {
	ERR_FAIL_NULL(context);
	context->load_resource_async(p_path, p_callback, p_priority);
}

void Dialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_intent", "intent"), &Dialog::set_intent);
	ClassDB::bind_method(D_METHOD("get_intent"), &Dialog::get_intent);
	ClassDB::bind_method(D_METHOD("set_transition_in", "transition"), &Dialog::set_transition_in);
	ClassDB::bind_method(D_METHOD("get_transition_in"), &Dialog::get_transition_in);
	ClassDB::bind_method(D_METHOD("set_transition_out", "transition"), &Dialog::set_transition_out);
	ClassDB::bind_method(D_METHOD("get_transition_out"), &Dialog::get_transition_out);
	ClassDB::bind_method(D_METHOD("dismiss"), &Dialog::dismiss);

	ClassDB::bind_method(D_METHOD("set_lifecycle_owner", "owner"), &Dialog::set_lifecycle_owner);
	ClassDB::bind_method(D_METHOD("get_lifecycle_owner"), &Dialog::get_lifecycle_owner);

	ClassDB::bind_method(D_METHOD("set_context", "context"), &Dialog::set_context);
	ClassDB::bind_method(D_METHOD("get_context"), &Dialog::get_context);

	ClassDB::bind_method(D_METHOD("show_toast", "toast"), &Dialog::show_toast);
	ClassDB::bind_method(D_METHOD("get_service", "name"), &Dialog::get_service);
	ClassDB::bind_method(D_METHOD("has_service", "name"), &Dialog::has_service);
	ClassDB::bind_method(D_METHOD("get_resource_handle", "path"), &Dialog::get_resource_handle);
	ClassDB::bind_method(D_METHOD("load_resource_sync", "path"), &Dialog::load_resource_sync);
	ClassDB::bind_method(D_METHOD("load_resource_async", "path", "callback", "priority"), &Dialog::load_resource_async, DEFVAL(Callable()), DEFVAL(0));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "transition_in", PROPERTY_HINT_RESOURCE_TYPE, "Transition"), "set_transition_in", "get_transition_in");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "transition_out", PROPERTY_HINT_RESOURCE_TYPE, "Transition"), "set_transition_out", "get_transition_out");

	GDVIRTUAL_BIND(_on_create, "saved_state");
	GDVIRTUAL_BIND(_on_dismiss);
}
