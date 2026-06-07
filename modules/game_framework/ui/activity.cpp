/**************************************************************************/
/*  activity.cpp                                                          */
/**************************************************************************/

#include "activity.h"

#include "../application.h"
#include "../context.h"

#include "core/io/resource.h"
#include "core/object/class_db.h"

void Activity::set_intent(const Ref<Intent> &p_intent) {
	intent = p_intent;
}

Ref<Intent> Activity::get_intent() const {
	return intent;
}

void Activity::set_transition_in(const Ref<Transition> &p_transition) {
	transition_in = p_transition;
}

Ref<Transition> Activity::get_transition_in() const {
	return transition_in;
}

void Activity::set_transition_out(const Ref<Transition> &p_transition) {
	transition_out = p_transition;
}

Ref<Transition> Activity::get_transition_out() const {
	return transition_out;
}

void Activity::dispatch_create(const Dictionary &p_saved_state) {
	GDVIRTUAL_CALL(_on_create, p_saved_state);
}

void Activity::dispatch_start() {
	GDVIRTUAL_CALL(_on_start);
}

void Activity::dispatch_resume() {
	GDVIRTUAL_CALL(_on_resume);
}

void Activity::dispatch_pause() {
	GDVIRTUAL_CALL(_on_pause);
}

void Activity::dispatch_stop() {
	GDVIRTUAL_CALL(_on_stop);
}

void Activity::dispatch_destroy() {
	GDVIRTUAL_CALL(_on_destroy);
}

void Activity::dispatch_new_intent(const Ref<Intent> &p_intent) {
	intent = p_intent;
	GDVIRTUAL_CALL(_on_new_intent, p_intent);
}

bool Activity::dispatch_back_pressed() {
	bool ret = false;
	GDVIRTUAL_CALL(_on_back_pressed, ret);
	return ret;
}

void Activity::finish() {
	if (context) {
		context->finish_activity(this);
	}
}

// ---- No-history mode ----

void Activity::set_no_history(bool p_no_history) {
	no_history = p_no_history;
}

bool Activity::get_no_history() const {
	return no_history;
}

// ---- Context access ----

void Activity::set_context(Context *p_context) {
	context = p_context;
}

Context *Activity::get_context() const {
	return context;
}

// ---- Convenience methods (delegate to context) ----

void Activity::start_activity(const Ref<Intent> &p_intent) {
	ERR_FAIL_NULL(context);
	context->start_activity(p_intent);
}

void Activity::start_activity_with(const String &p_action, int p_flags, const Dictionary &p_extras) {
	ERR_FAIL_NULL(context);
	context->start_activity_with(p_action, p_flags, p_extras);
}

void Activity::finish_top() {
	ERR_FAIL_NULL(context);
	context->finish_top();
}

bool Activity::back() {
	ERR_FAIL_NULL_V(context, false);
	return context->back();
}

void Activity::show_dialog(const Ref<Intent> &p_intent) {
	ERR_FAIL_NULL(context);
	context->show_dialog_with_owner(p_intent, this);
}

void Activity::show_toast(const Ref<Toast> &p_toast) {
	ERR_FAIL_NULL(context);
	context->show_toast_with_owner(p_toast, this);
}

Object *Activity::get_service(const StringName &p_name) const {
	ERR_FAIL_NULL_V(context, nullptr);
	return context->get_service(p_name);
}

bool Activity::has_service(const StringName &p_name) const {
	ERR_FAIL_NULL_V(context, false);
	return context->has_service(p_name);
}

Ref<ResourceHandle> Activity::get_resource_handle(const String &p_path) {
	ERR_FAIL_NULL_V(context, Ref<ResourceHandle>());
	return context->get_resource_handle(p_path);
}

Ref<Resource> Activity::load_resource_sync(const String &p_path) {
	ERR_FAIL_NULL_V(context, Ref<Resource>());
	return context->load_resource_sync(p_path);
}

void Activity::load_resource_async(const String &p_path, const Callable &p_callback, int p_priority) {
	ERR_FAIL_NULL(context);
	context->load_resource_async(p_path, p_callback, p_priority);
}

void Activity::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_intent", "intent"), &Activity::set_intent);
	ClassDB::bind_method(D_METHOD("get_intent"), &Activity::get_intent);
	ClassDB::bind_method(D_METHOD("set_transition_in", "transition"), &Activity::set_transition_in);
	ClassDB::bind_method(D_METHOD("get_transition_in"), &Activity::get_transition_in);
	ClassDB::bind_method(D_METHOD("set_transition_out", "transition"), &Activity::set_transition_out);
	ClassDB::bind_method(D_METHOD("get_transition_out"), &Activity::get_transition_out);
	ClassDB::bind_method(D_METHOD("finish"), &Activity::finish);

	ClassDB::bind_method(D_METHOD("set_no_history", "no_history"), &Activity::set_no_history);
	ClassDB::bind_method(D_METHOD("get_no_history"), &Activity::get_no_history);

	ClassDB::bind_method(D_METHOD("set_context", "context"), &Activity::set_context);
	ClassDB::bind_method(D_METHOD("get_context"), &Activity::get_context);

	ClassDB::bind_method(D_METHOD("start_activity", "intent"), &Activity::start_activity);
	ClassDB::bind_method(D_METHOD("start_activity_with", "action", "flags", "extras"), &Activity::start_activity_with, DEFVAL(0), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("finish_top"), &Activity::finish_top);
	ClassDB::bind_method(D_METHOD("back"), &Activity::back);
	ClassDB::bind_method(D_METHOD("show_dialog", "intent"), &Activity::show_dialog);
	ClassDB::bind_method(D_METHOD("show_toast", "toast"), &Activity::show_toast);
	ClassDB::bind_method(D_METHOD("get_service", "name"), &Activity::get_service);
	ClassDB::bind_method(D_METHOD("has_service", "name"), &Activity::has_service);
	ClassDB::bind_method(D_METHOD("get_resource_handle", "path"), &Activity::get_resource_handle);
	ClassDB::bind_method(D_METHOD("load_resource_sync", "path"), &Activity::load_resource_sync);
	ClassDB::bind_method(D_METHOD("load_resource_async", "path", "callback", "priority"), &Activity::load_resource_async, DEFVAL(Callable()), DEFVAL(0));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "transition_in", PROPERTY_HINT_RESOURCE_TYPE, "Transition"), "set_transition_in", "get_transition_in");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "transition_out", PROPERTY_HINT_RESOURCE_TYPE, "Transition"), "set_transition_out", "get_transition_out");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "no_history"), "set_no_history", "get_no_history");

	GDVIRTUAL_BIND(_on_create, "saved_state");
	GDVIRTUAL_BIND(_on_start);
	GDVIRTUAL_BIND(_on_resume);
	GDVIRTUAL_BIND(_on_pause);
	GDVIRTUAL_BIND(_on_stop);
	GDVIRTUAL_BIND(_on_destroy);
	GDVIRTUAL_BIND(_on_new_intent, "intent");
	GDVIRTUAL_BIND(_on_back_pressed);
}
