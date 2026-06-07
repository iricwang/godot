/**************************************************************************/
/*  context.cpp                                                            */
/**************************************************************************/

#include "context.h"

#include "application.h"
#include "mvvm/binding_engine.h"
#include "mvvm/view_model.h"
#include "resource/resource_handle.h"
#include "resource/resource_manager.h"
#include "service/service_registry.h"
#include "ui/activity.h"
#include "ui/activity_manager.h"
#include "ui/scene_service.h"
#include "ui/dialog.h"
#include "ui/intent.h"
#include "ui/toast.h"

#include "core/object/class_db.h"
#include "scene/main/node.h"

void Context::set_application(Application *p_app) {
	app = p_app;
}

Application *Context::get_application() const {
	return app;
}

// ---- Service lookup ----

Object *Context::get_service(const StringName &p_name) const {
	ERR_FAIL_NULL_V(app, nullptr);
	return app->get_service_registry()->get_service(p_name);
}

bool Context::has_service(const StringName &p_name) const {
	ERR_FAIL_NULL_V(app, false);
	return app->get_service_registry()->has_service(p_name);
}

// ---- Resource loading ----

Ref<ResourceHandle> Context::get_resource_handle(const String &p_path) {
	ERR_FAIL_NULL_V(app, Ref<ResourceHandle>());
	return app->get_resource_manager()->get_handle(p_path);
}

Ref<Resource> Context::load_resource_sync(const String &p_path) {
	ERR_FAIL_NULL_V(app, Ref<Resource>());
	return app->get_resource_manager()->load_sync(p_path);
}

void Context::load_resource_async(const String &p_path, const Callable &p_callback, int p_priority) {
	ERR_FAIL_NULL(app);
	app->get_resource_manager()->load_async(p_path, p_callback, p_priority);
}

// ---- Navigation ----

void Context::start_activity(const Ref<Intent> &p_intent) {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->start_activity(p_intent);
}

void Context::start_activity_with(const String &p_action, int p_flags, const Dictionary &p_extras) {
	start_activity(Intent::create(p_action, p_flags, p_extras));
}

void Context::finish_activity(Activity *p_activity) {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->finish_activity(p_activity);
}

void Context::finish_top() {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->finish_top();
}

bool Context::back() {
	ERR_FAIL_NULL_V(app, false);
	return app->get_activity_manager()->back();
}

Activity *Context::get_current_activity() const {
	ERR_FAIL_NULL_V(app, nullptr);
	return app->get_activity_manager()->get_current_activity();
}

int Context::get_stack_size() const {
	ERR_FAIL_NULL_V(app, 0);
	return app->get_activity_manager()->get_stack_size();
}

// ---- Overlays ----

void Context::show_dialog(const Ref<Intent> &p_intent) {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->show_dialog(p_intent);
}

void Context::show_dialog_with_owner(const Ref<Intent> &p_intent, Object *p_owner) {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->show_dialog_with_owner(p_intent, p_owner);
}

void Context::dismiss_dialog(Dialog *p_dialog) {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->dismiss_dialog(p_dialog);
}

void Context::show_toast(const Ref<Toast> &p_toast) {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->show_toast(p_toast);
}

void Context::show_toast_with_owner(const Ref<Toast> &p_toast, Object *p_owner) {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->show_toast_with_owner(p_toast, p_owner);
}

void Context::clear_all_toasts() {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->clear_all_toasts();
}

void Context::clear_toasts_by_owner(Object *p_owner) {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->clear_toasts_by_owner(p_owner);
}

// ---- Activity registration ----

void Context::register_activity(const String &p_action, const String &p_scene_path) {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->register_activity(p_action, p_scene_path);
}

// ---- Scene switching ----

void Context::change_scene(const String &p_path) {
	ERR_FAIL_NULL(app);
	app->get_scene_service()->change_scene(p_path);
}

void Context::change_scene_sync(const String &p_path) {
	ERR_FAIL_NULL(app);
	app->get_scene_service()->change_scene_sync(p_path);
}

String Context::get_current_scene() const {
	ERR_FAIL_NULL_V(app, String());
	return app->get_scene_service()->get_current_scene();
}

// ---- MVVM static shortcuts ----

void Context::bind(Node *p_view, ViewModel *p_vm) {
	BindingEngine::bind(p_view, p_vm);
}

void Context::bind_property(Object *p_target, const StringName &p_target_property, ViewModel *p_vm, const StringName &p_source_property) {
	BindingEngine::bind_property(p_target, p_target_property, p_vm, p_source_property);
}

void Context::bind_command(Object *p_source, const StringName &p_signal, ViewModel *p_vm, const StringName &p_method) {
	BindingEngine::bind_command(p_source, p_signal, p_vm, p_method);
}

// ---- Toast factory ----

Ref<Toast> Context::make_toast(const String &p_text, double p_duration) {
	return Toast::make_text(p_text, p_duration);
}

void Context::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_application", "application"), &Context::set_application);
	ClassDB::bind_method(D_METHOD("get_application"), &Context::get_application);

	ClassDB::bind_method(D_METHOD("get_service", "name"), &Context::get_service);
	ClassDB::bind_method(D_METHOD("has_service", "name"), &Context::has_service);

	ClassDB::bind_method(D_METHOD("get_resource_handle", "path"), &Context::get_resource_handle);
	ClassDB::bind_method(D_METHOD("load_resource_sync", "path"), &Context::load_resource_sync);
	ClassDB::bind_method(D_METHOD("load_resource_async", "path", "callback", "priority"), &Context::load_resource_async, DEFVAL(Callable()), DEFVAL(0));

	ClassDB::bind_method(D_METHOD("start_activity", "intent"), &Context::start_activity);
	ClassDB::bind_method(D_METHOD("start_activity_with", "action", "flags", "extras"), &Context::start_activity_with, DEFVAL(0), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("finish_activity", "activity"), &Context::finish_activity);
	ClassDB::bind_method(D_METHOD("finish_top"), &Context::finish_top);
	ClassDB::bind_method(D_METHOD("back"), &Context::back);
	ClassDB::bind_method(D_METHOD("get_current_activity"), &Context::get_current_activity);
	ClassDB::bind_method(D_METHOD("get_stack_size"), &Context::get_stack_size);

	ClassDB::bind_method(D_METHOD("show_dialog", "intent"), &Context::show_dialog);
	ClassDB::bind_method(D_METHOD("show_dialog_with_owner", "intent", "owner"), &Context::show_dialog_with_owner);
	ClassDB::bind_method(D_METHOD("dismiss_dialog", "dialog"), &Context::dismiss_dialog);
	ClassDB::bind_method(D_METHOD("show_toast", "toast"), &Context::show_toast);
	ClassDB::bind_method(D_METHOD("show_toast_with_owner", "toast", "owner"), &Context::show_toast_with_owner);
		ClassDB::bind_method(D_METHOD("clear_all_toasts"), &Context::clear_all_toasts);
		ClassDB::bind_method(D_METHOD("clear_toasts_by_owner", "owner"), &Context::clear_toasts_by_owner);

	ClassDB::bind_method(D_METHOD("register_activity", "action", "scene_path"), &Context::register_activity);

	ClassDB::bind_method(D_METHOD("change_scene", "path"), &Context::change_scene);
	ClassDB::bind_method(D_METHOD("change_scene_sync", "path"), &Context::change_scene_sync);
	ClassDB::bind_method(D_METHOD("get_current_scene"), &Context::get_current_scene);

	ClassDB::bind_static_method("Context", D_METHOD("bind", "view", "vm"), &Context::bind);
	ClassDB::bind_static_method("Context", D_METHOD("bind_property", "target", "target_property", "vm", "source_property"), &Context::bind_property);
	ClassDB::bind_static_method("Context", D_METHOD("bind_command", "source", "signal", "vm", "method"), &Context::bind_command);
	ClassDB::bind_static_method("Context", D_METHOD("make_toast", "text", "duration"), &Context::make_toast, DEFVAL(2.0));
}
