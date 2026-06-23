/**************************************************************************/
/*  context.cpp                                                            */
/**************************************************************************/

#include "context.h"
#include "context_base.inl"

#include "application.h"
#include "core/config/engine.h"
#include "mvvm/binding_engine.h"
#include "mvvm/value_converter.h"
#include "mvvm/view_model.h"
#include "resource/resource_handle.h"
#include "resource/resource_manager.h"
#include "service/service_registry.h"
#include "ui/activity.h"
#include "ui/activity_manager.h"
#include "ui/dialog.h"
#include "ui/intent.h"
#include "ui/scene_service.h"
#include "ui/toast.h"

#include "core/object/class_db.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"

void Context::set_application(Application *p_app) {
	app = p_app;
}

// ---- Note ----
// The generic delegates (get_service / has_service / start_activity / show_dialog /
// show_toast / get_resource_handle / load_resource_* / start_activity_with /
// finish_top / back) are inherited from ContextBase<Context>. The implementations
// below are Context-only methods that ContextBase does not cover.

// ---- State queries ----

Activity *Context::get_current_activity() const {
	ERR_FAIL_NULL_V(app, nullptr);
	return app->get_activity_manager()->get_current_activity();
}

int Context::get_stack_size() const {
	ERR_FAIL_NULL_V(app, 0);
	return app->get_activity_manager()->get_stack_size();
}

// ---- Owner / explicit overloads ----

void Context::finish_activity(Activity *p_activity) {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->finish_activity(p_activity);
}

void Context::show_dialog_with_owner(const Ref<Intent> &p_intent, Object *p_owner) {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->show_dialog_with_owner(p_intent, p_owner);
}

void Context::dismiss_dialog(Dialog *p_dialog) {
	ERR_FAIL_NULL(app);
	app->get_activity_manager()->dismiss_dialog(p_dialog);
}

void Context::show_toast_with_owner(Toast *p_toast, Object *p_owner) {
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

void Context::bind_property(Object *p_target, const StringName &p_target_property, ViewModel *p_vm, const StringName &p_source_property, int p_mode, const Ref<ValueConverter> &p_converter) {
	BindingEngine::bind_property(p_target, p_target_property, p_vm, p_source_property, p_mode, p_converter);
}

void Context::bind_command(Object *p_source, const StringName &p_signal, ViewModel *p_vm, const StringName &p_method) {
	BindingEngine::bind_command(p_source, p_signal, p_vm, p_method);
}

// ---- Toast factory ----

Toast *Context::make_toast(const String &p_text, double p_duration) {
	return Toast::make_text(p_text, p_duration);
}

void Context::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_application", "application"), &Context::set_application);
	ClassDB::bind_method(D_METHOD("get_application"), &Context::get_application);

	// ---- Generic delegates (inherited from ContextBase<Context>) ----
	// Use PMF static_cast so the compiler picks up the overload defined in the CRTP base.
	using GetServiceT = Object *(Context::*)(const StringName &) const;
	GetServiceT get_service_pmf = static_cast<GetServiceT>(&Context::get_service);
	ClassDB::bind_method(D_METHOD("get_service", "name"), get_service_pmf);

	using HasServiceT = bool (Context::*)(const StringName &) const;
	HasServiceT has_service_pmf = static_cast<HasServiceT>(&Context::has_service);
	ClassDB::bind_method(D_METHOD("has_service", "name"), has_service_pmf);

	using GetHandleT = Ref<ResourceHandle> (Context::*)(const String &);
	GetHandleT get_handle_pmf = static_cast<GetHandleT>(&Context::get_resource_handle);
	ClassDB::bind_method(D_METHOD("get_resource_handle", "path"), get_handle_pmf);

	using LoadSyncT = Ref<Resource> (Context::*)(const String &);
	LoadSyncT load_sync_pmf = static_cast<LoadSyncT>(&Context::load_resource_sync);
	ClassDB::bind_method(D_METHOD("load_resource_sync", "path"), load_sync_pmf);

	using LoadAsyncT = void (Context::*)(const String &, const Callable &, int);
	LoadAsyncT load_async_pmf = static_cast<LoadAsyncT>(&Context::load_resource_async);
	ClassDB::bind_method(D_METHOD("load_resource_async", "path", "callback", "priority"), load_async_pmf, DEFVAL(Callable()), DEFVAL(0));

	using StartActT = void (Context::*)(const Ref<Intent> &);
	StartActT start_act_pmf = static_cast<StartActT>(&Context::start_activity);
	ClassDB::bind_method(D_METHOD("start_activity", "intent"), start_act_pmf);

	using StartActWithT = void (Context::*)(const String &, int, const Dictionary &);
	StartActWithT start_act_with_pmf = static_cast<StartActWithT>(&Context::start_activity_with);
	ClassDB::bind_method(D_METHOD("start_activity_with", "action", "flags", "extras"), start_act_with_pmf, DEFVAL(0), DEFVAL(Dictionary()));

	using FinishTopT = void (Context::*)();
	FinishTopT finish_top_pmf = static_cast<FinishTopT>(&Context::finish_top);
	ClassDB::bind_method(D_METHOD("finish_top"), finish_top_pmf);

	using BackT = bool (Context::*)();
	BackT back_pmf = static_cast<BackT>(&Context::back);
	ClassDB::bind_method(D_METHOD("back"), back_pmf);

	using ShowDlgT = void (Context::*)(const Ref<Intent> &);
	ShowDlgT show_dlg_pmf = static_cast<ShowDlgT>(&Context::show_dialog);
	ClassDB::bind_method(D_METHOD("show_dialog", "intent"), show_dlg_pmf);

	using ShowToastT = void (Context::*)(Toast *);
	ShowToastT show_toast_pmf = static_cast<ShowToastT>(&Context::show_toast);
	ClassDB::bind_method(D_METHOD("show_toast", "toast"), show_toast_pmf);

	// ---- Context-only methods ----
	ClassDB::bind_method(D_METHOD("finish_activity", "activity"), &Context::finish_activity);
	ClassDB::bind_method(D_METHOD("get_current_activity"), &Context::get_current_activity);
	ClassDB::bind_method(D_METHOD("get_stack_size"), &Context::get_stack_size);

	ClassDB::bind_method(D_METHOD("show_dialog_with_owner", "intent", "owner"), &Context::show_dialog_with_owner);
	ClassDB::bind_method(D_METHOD("dismiss_dialog", "dialog"), &Context::dismiss_dialog);
	ClassDB::bind_method(D_METHOD("show_toast_with_owner", "toast", "owner"), &Context::show_toast_with_owner);
	ClassDB::bind_method(D_METHOD("clear_all_toasts"), &Context::clear_all_toasts);
	ClassDB::bind_method(D_METHOD("clear_toasts_by_owner", "owner"), &Context::clear_toasts_by_owner);

	ClassDB::bind_method(D_METHOD("register_activity", "action", "scene_path"), &Context::register_activity);

	ClassDB::bind_method(D_METHOD("change_scene", "path"), &Context::change_scene);
	ClassDB::bind_method(D_METHOD("change_scene_sync", "path"), &Context::change_scene_sync);
	ClassDB::bind_method(D_METHOD("get_current_scene"), &Context::get_current_scene);

	// ---- Static MVVM / Toast helpers ----
	ClassDB::bind_static_method("Context", D_METHOD("bind", "view", "vm"), &Context::bind);
	ClassDB::bind_static_method("Context", D_METHOD("bind_property", "target", "target_property", "vm", "source_property", "mode", "converter"), &Context::bind_property, DEFVAL(0), DEFVAL(Variant()));
	ClassDB::bind_static_method("Context", D_METHOD("bind_command", "source", "signal", "vm", "method"), &Context::bind_command);
	ClassDB::bind_static_method("Context", D_METHOD("make_toast", "text", "duration"), &Context::make_toast, DEFVAL(2.0));
}

void Context::try_launch() {
	if (get_application() != nullptr) {
		return;
	}
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	// The node must be in a SceneTree to host anything.
	Node *node = Object::cast_to<Node>(as_object());
	if (node == nullptr) {
		return;
	}
	SceneTree *st = node->get_tree();
	if (st == nullptr) {
		return;
	}
	if (st->get_current_scene() != node) {
		return;
	}

}
