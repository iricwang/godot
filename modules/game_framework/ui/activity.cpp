/**************************************************************************/
/*  activity.cpp                                                          */
/**************************************************************************/

#include "activity.h"
#include "../context/context_base.inl"

#include "../context/application.h"
#include "../context/context.h"
#include "../context/standalone_application.h"
#include "activity_manager.h"
#include "proxy/activity_proxy.h"
#include "auto_activity_loader.h"
#include "proxy/dialog_proxy.h"
#include "proxy/toast_proxy.h"

#include "core/config/engine.h"
#include "core/io/resource.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

Activity::Activity() {
}

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
	if (_app) {
		_app->finish_activity(this);
	}
}

void Activity::set_no_history(bool p_no_history) {
	no_history = p_no_history;
}

bool Activity::get_no_history() const {
	return no_history;
}

void Activity::set_application(Application *p_app) {
	_app = p_app;
}

void Activity::set_context(Context *p_context) {
	_app = Object::cast_to<Application>(p_context);
}

Context *Activity::get_context() const {
	return _app;
}

bool Activity::is_standalone() const {
	return Object::cast_to<StandaloneApplication>(_app) != nullptr;
}

void Activity::_notification(int p_what) {
	if (p_what != NOTIFICATION_READY) {
		return;
	}
	// Defer: run_standalone_bootstrap reparents/builds nodes, which must not
	// happen during the READY pass ("parent is busy setting up children").
	// The bootstrap self-guards (no-op when an Application is already bound, e.g.
	// the managed flow where ActivityManager injects it before add_child).
	call_deferred(SNAME("run_standalone_bootstrap"), true);
}

// ============================================================
// Standalone bootstrap — single entry for self-hosted preview.
// ============================================================
//
// Gate is intentionally minimal (the user's model): if an Application is already
// bound, this is the normal ActivityManager-injected flow → no-op. Otherwise the
// node self-hosts via the shared StandaloneApplication::host(). Guarded against
// the editor so opening a scene in the inspector never spawns an Application.
//
// Public so tests can drive it; safe to call directly (idempotent).

void Activity::run_standalone_bootstrap(bool p_play_transitions) {
	// Already bound (managed flow or already bootstrapped) → not a preview.
	if (_app != nullptr) {
		return;
	}
	// Never self-host inside the editor.
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	// Build the shared standalone host environment (reparents `this` under it).
	StandaloneApplication *app = StandaloneApplication::host(this);
	if (!app) {
		return;
	}
	_app = app;

	// User hook — register mock services / preset Intent extras BEFORE _on_create.
	GDVIRTUAL_CALL(_on_setup_standalone, app);

	// Register ourselves into the stack as the root activity.
	ActivityManager *am = app->get_activity_manager();
	if (am) {
		Ref<Intent> bootstrap_intent = intent;
		if (bootstrap_intent.is_null()) {
			bootstrap_intent.instantiate();
			bootstrap_intent->set_action("__standalone__");
			intent = bootstrap_intent;
		}
		am->adopt_running_activity(this, bootstrap_intent);
	}

	// Dispatch GDScript lifecycle on the next idle tick — one frame for
	// anchors/layout to settle after the reparent.
	call_deferred(SNAME("_dispatch_standalone_lifecycle"), p_play_transitions);
}

// Internal — wired via call_deferred from run_standalone_bootstrap.
void Activity::_dispatch_standalone_lifecycle(bool p_play_transitions) {
	if (!is_standalone()) {
		return;
	}
	dispatch_create(Dictionary());
	dispatch_start();
	if (p_play_transitions && transition_in.is_valid()) {
		transition_in->play_enter(this);
	}
	dispatch_resume();
	// Flip the adopted proxy from LOADING to READY+RESUMED now that the
	// deferred lifecycle dispatch is done. Outside listeners (get_state /
	// `ready` signal subscribers) only see READY once the Activity actually
	// is created/resumed.
	if (_app) {
		ActivityManager *am = _app->get_activity_manager();
		if (am) {
			am->_mark_adopted_ready(this);
		}
	}
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

	ClassDB::bind_method(D_METHOD("set_application", "application"), &Activity::set_application);
	ClassDB::bind_method(D_METHOD("set_context", "context"), &Activity::set_context);
	ClassDB::bind_method(D_METHOD("get_context"), &Activity::get_context);
	ClassDB::bind_method(D_METHOD("get_application"), &Activity::get_application);

	ClassDB::bind_method(D_METHOD("is_standalone"), &Activity::is_standalone);
	ClassDB::bind_method(D_METHOD("run_standalone_bootstrap", "play_transitions"), &Activity::run_standalone_bootstrap, DEFVAL(false));

	// ---- Generic delegates (inherited from ContextBase<Activity>) — PMF static_cast ----
	using GetServiceT = Object *(Activity::*)(const StringName &) const;
	ClassDB::bind_method(D_METHOD("get_service", "name"), static_cast<GetServiceT>(&Activity::get_service));
	using HasServiceT = bool (Activity::*)(const StringName &) const;
	ClassDB::bind_method(D_METHOD("has_service", "name"), static_cast<HasServiceT>(&Activity::has_service));
	using GetHandleT = Ref<ResourceHandle> (Activity::*)(const String &);
	ClassDB::bind_method(D_METHOD("get_resource_handle", "path"), static_cast<GetHandleT>(&Activity::get_resource_handle));
	using LoadSyncT = Ref<Resource> (Activity::*)(const String &);
	ClassDB::bind_method(D_METHOD("load_resource_sync", "path"), static_cast<LoadSyncT>(&Activity::load_resource_sync));
	using LoadAsyncT = void (Activity::*)(const String &, const Callable &, int);
	ClassDB::bind_method(D_METHOD("load_resource_async", "path", "callback", "priority"), static_cast<LoadAsyncT>(&Activity::load_resource_async), DEFVAL(Callable()), DEFVAL(0));
	using StartActT = Ref<ActivityProxy> (Activity::*)(const Ref<Intent> &);
	ClassDB::bind_method(D_METHOD("start_activity", "intent"), static_cast<StartActT>(&Activity::start_activity));
	using StartActWithT = Ref<ActivityProxy> (Activity::*)(const String &, int, const Dictionary &);
	ClassDB::bind_method(D_METHOD("start_activity_with", "action", "flags", "extras"), static_cast<StartActWithT>(&Activity::start_activity_with), DEFVAL(0), DEFVAL(Dictionary()));
	using FinishTopT = void (Activity::*)();
	ClassDB::bind_method(D_METHOD("finish_top"), static_cast<FinishTopT>(&Activity::finish_top));
	using BackT = bool (Activity::*)();
	ClassDB::bind_method(D_METHOD("back"), static_cast<BackT>(&Activity::back));
	using ShowDlgT = Ref<DialogProxy> (Activity::*)(const Ref<Intent> &);
	ClassDB::bind_method(D_METHOD("show_dialog", "intent"), static_cast<ShowDlgT>(&Activity::show_dialog));
	using ShowToastT = Ref<ToastProxy> (Activity::*)(Toast *);
	ClassDB::bind_method(D_METHOD("show_toast", "toast"), static_cast<ShowToastT>(&Activity::show_toast));

	// Internal trampoline for the deferred lifecycle dispatch.
	ClassDB::bind_method(D_METHOD("_dispatch_standalone_lifecycle", "play_transitions"), &Activity::_dispatch_standalone_lifecycle);

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
	GDVIRTUAL_BIND(_on_setup_standalone, "app");
}
