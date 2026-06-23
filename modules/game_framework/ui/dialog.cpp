/**************************************************************************/
/*  dialog.cpp                                                            */
/**************************************************************************/

#include "dialog.h"
#include "../context_base.inl"

#include "../application.h"
#include "../context.h"
#include "../standalone_application.h"
#include "activity_manager.h"
#include "auto_activity_loader.h"
#include "standalone_activity_launcher.h"

#include "core/config/engine.h"
#include "core/io/resource.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

Dialog::Dialog() {
	// Default launcher: enables the "F6 single-Dialog preview" path out of the box.
	// Users can disable by calling set_launcher(null) or swap to a different strategy.
	launcher = Ref<ActivityLauncher>(memnew(StandaloneActivityLauncher));
}

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
	if (_app) {
		_app->dismiss_dialog(this);
	}
}

// ---- Owner (lifecycle binding) ----

void Dialog::set_lifecycle_owner(Object *p_owner) {
	owner = p_owner;
}

Object *Dialog::get_lifecycle_owner() const {
	return owner;
}

// ---- Application linkage / Context access ----

void Dialog::set_application(Application *p_app) {
	_app = p_app;
}

void Dialog::set_context(Context *p_context) {
	// Backward-compat alias. ActivityManager always injects the Application,
	// which IS-A Context, so down-casting back is safe.
	_app = Object::cast_to<Application>(p_context);
}

Context *Dialog::get_context() const {
	return _app; // Application IS-A Context, implicit upcast.
}

// ---- Launcher (Run-As-Standalone) ----

void Dialog::set_launcher(const Ref<ActivityLauncher> &p_launcher) {
	launcher = p_launcher;
}

Ref<ActivityLauncher> Dialog::get_launcher() const {
	return launcher;
}

bool Dialog::is_standalone() const {
	return Object::cast_to<StandaloneApplication>(_app) != nullptr;
}

void Dialog::_notification(int p_what) {
	if (p_what != NOTIFICATION_READY) {
		return;
	}
	if (launcher.is_valid()) {
		launcher->try_launch(this);
	}
}

// ============================================================
// Standalone bootstrap — single entry for self-hosted preview.
// ============================================================
//
// Uses the shared StandaloneApplication::host() to build the host environment,
// then adopts itself as a running dialog and dispatches _on_create. Guard is the
// user's model: only self-host when no Application is bound (not the managed
// flow) and not in the editor. Safe to call directly or via a launcher.

void Dialog::run_standalone_bootstrap(bool p_play_transitions) {
	if (_app != nullptr) {
		return;
	}
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	StandaloneApplication *app = StandaloneApplication::host(this);
	if (!app) {
		return;
	}
	_app = app;

	// User hook — register mock services / preset Intent extras BEFORE _on_create.
	GDVIRTUAL_CALL(_on_setup_standalone, app);

	// Register ourselves as a running dialog (bookkeeping only; already parented).
	ActivityManager *am = app->get_activity_manager();
	if (am) {
		Ref<Intent> bootstrap_intent = intent;
		if (bootstrap_intent.is_null()) {
			bootstrap_intent.instantiate();
			bootstrap_intent->set_action("__standalone__");
			intent = bootstrap_intent;
		}
		am->adopt_running_dialog(this, bootstrap_intent);
	}

	call_deferred(SNAME("_dispatch_standalone_lifecycle"), p_play_transitions);
}

// Internal — wired via call_deferred from run_standalone_bootstrap.
void Dialog::_dispatch_standalone_lifecycle(bool p_play_transitions) {
	if (!is_standalone()) {
		return;
	}
	dispatch_create(intent.is_valid() ? intent->get_extras() : Dictionary());
	if (p_play_transitions && transition_in.is_valid()) {
		transition_in->play_enter(this);
	}
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

	ClassDB::bind_method(D_METHOD("set_application", "application"), &Dialog::set_application);
	ClassDB::bind_method(D_METHOD("set_context", "context"), &Dialog::set_context);
	ClassDB::bind_method(D_METHOD("get_context"), &Dialog::get_context);
	ClassDB::bind_method(D_METHOD("get_application"), &Dialog::get_application);

	ClassDB::bind_method(D_METHOD("set_launcher", "launcher"), &Dialog::set_launcher);
	ClassDB::bind_method(D_METHOD("get_launcher"), &Dialog::get_launcher);
	ClassDB::bind_method(D_METHOD("is_standalone"), &Dialog::is_standalone);
	ClassDB::bind_method(D_METHOD("run_standalone_bootstrap", "play_transitions"), &Dialog::run_standalone_bootstrap, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("_dispatch_standalone_lifecycle", "play_transitions"), &Dialog::_dispatch_standalone_lifecycle);

	// ---- Generic delegates (inherited from ContextBase<Dialog>) ----
	using ShowToastT = void (Dialog::*)(Toast *);
	ShowToastT show_toast_pmf = static_cast<ShowToastT>(&Dialog::show_toast);
	ClassDB::bind_method(D_METHOD("show_toast", "toast"), show_toast_pmf);

	using GetServiceT = Object *(Dialog::*)(const StringName &) const;
	GetServiceT get_service_pmf = static_cast<GetServiceT>(&Dialog::get_service);
	ClassDB::bind_method(D_METHOD("get_service", "name"), get_service_pmf);

	using HasServiceT = bool (Dialog::*)(const StringName &) const;
	HasServiceT has_service_pmf = static_cast<HasServiceT>(&Dialog::has_service);
	ClassDB::bind_method(D_METHOD("has_service", "name"), has_service_pmf);

	using GetHandleT = Ref<ResourceHandle> (Dialog::*)(const String &);
	GetHandleT get_handle_pmf = static_cast<GetHandleT>(&Dialog::get_resource_handle);
	ClassDB::bind_method(D_METHOD("get_resource_handle", "path"), get_handle_pmf);

	using LoadSyncT = Ref<Resource> (Dialog::*)(const String &);
	LoadSyncT load_sync_pmf = static_cast<LoadSyncT>(&Dialog::load_resource_sync);
	ClassDB::bind_method(D_METHOD("load_resource_sync", "path"), load_sync_pmf);

	using LoadAsyncT = void (Dialog::*)(const String &, const Callable &, int);
	LoadAsyncT load_async_pmf = static_cast<LoadAsyncT>(&Dialog::load_resource_async);
	ClassDB::bind_method(D_METHOD("load_resource_async", "path", "callback", "priority"), load_async_pmf, DEFVAL(Callable()), DEFVAL(0));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "transition_in", PROPERTY_HINT_RESOURCE_TYPE, "Transition"), "set_transition_in", "get_transition_in");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "transition_out", PROPERTY_HINT_RESOURCE_TYPE, "Transition"), "set_transition_out", "get_transition_out");

	ADD_GROUP("Standalone", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "launcher", PROPERTY_HINT_RESOURCE_TYPE, "ActivityLauncher"), "set_launcher", "get_launcher");

	GDVIRTUAL_BIND(_on_create, "saved_state");
	GDVIRTUAL_BIND(_on_dismiss);
	GDVIRTUAL_BIND(_on_setup_standalone, "app");
}
