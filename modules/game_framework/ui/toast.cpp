/**************************************************************************/
/*  toast.cpp                                                             */
/**************************************************************************/

#include "toast.h"
#include "../context/context_base.inl"

#include "../context/application.h"
#include "../context/context.h"
#include "../context/standalone_application.h"
#include "activity_manager.h"
#include "proxy/activity_proxy.h"
#include "auto_activity_loader.h"
#include "proxy/dialog_proxy.h"
#include "proxy/toast_proxy.h"

#include "core/io/resource.h"
#include "core/object/class_db.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/main/scene_tree.h"

Toast::Toast() {
}

Toast *Toast::make_text(const String &p_text, double p_duration) {
	Toast *toast = memnew(Toast);
	toast->text = p_text;
	toast->duration = p_duration;
	return toast;
}

void Toast::set_text(const String &p_text) {
	text = p_text;
}

String Toast::get_text() const {
	return text;
}

void Toast::set_duration(double p_duration) {
	duration = p_duration;
}

double Toast::get_duration() const {
	return duration;
}

void Toast::set_intent(const Ref<Intent> &p_intent) {
	intent = p_intent;
}

Ref<Intent> Toast::get_intent() const {
	return intent;
}

void Toast::set_transition_in(const Ref<Transition> &p_transition) {
	transition_in = p_transition;
}

Ref<Transition> Toast::get_transition_in() const {
	return transition_in;
}

void Toast::set_transition_out(const Ref<Transition> &p_transition) {
	transition_out = p_transition;
}

Ref<Transition> Toast::get_transition_out() const {
	return transition_out;
}

void Toast::dispatch_create(const Dictionary &p_saved_state) {
	// Let a Toast subclass (custom layout scene) build its own UI; otherwise
	// fall back to the built-in panel+label using `text`.
	if (!GDVIRTUAL_CALL(_on_create, p_saved_state)) {
		if (!_default_built) {
			_default_built = true;
			PanelContainer *panel = memnew(PanelContainer);
			panel->set_anchors_preset(Control::PRESET_CENTER);
			Label *label = memnew(Label);
			label->set_text(text);
			label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			panel->add_child(label);
			add_child(panel);
		}
	}
}

void Toast::dispatch_dismiss() {
	GDVIRTUAL_CALL(_on_dismiss);
}

void Toast::dismiss() {
	if (_app) {
		ActivityManager *am = _app->get_activity_manager();
		if (am) {
			am->dismiss_toast(this);
		}
	}
}

// ---- Owner (lifecycle binding) ----

void Toast::set_lifecycle_owner(Object *p_owner) {
	lifecycle_owner = p_owner;
	owner_id = p_owner ? p_owner->get_instance_id() : ObjectID();
}

Object *Toast::get_lifecycle_owner() const {
	if (owner_id.is_valid()) {
		return ObjectDB::get_instance(owner_id);
	}
	return nullptr;
}

bool Toast::is_owned_by(ObjectID p_id) const {
	return owner_id == p_id;
}

// ---- Application linkage / Context access ----

void Toast::set_application(Application *p_app) {
	_app = p_app;
}

void Toast::set_context(Context *p_context) {
	_app = Object::cast_to<Application>(p_context);
}

Context *Toast::get_context() const {
	return _app; // Application IS-A Context, implicit upcast.
}

// ---- Run-As-Standalone ----

bool Toast::is_standalone() const {
	return Object::cast_to<StandaloneApplication>(_app) != nullptr;
}

void Toast::_notification(int p_what) {
	if (p_what != NOTIFICATION_READY) {
		return;
	}
	// Defer: run_standalone_bootstrap reparents/builds nodes, which must not
	// happen during the READY pass. The bootstrap self-guards — it's a no-op
	// when an Application is already bound (the managed flow where
	// ActivityManager injects it before add_child).
	call_deferred(SNAME("run_standalone_bootstrap"), false);
}

void Toast::run_standalone_bootstrap(bool p_play_transitions) {
	if (_app != nullptr) {
		return;
	}
	StandaloneApplication *app = StandaloneApplication::host(this);
	if (!app) {
		return;
	}
	_app = app;

	// User hook — register mock services BEFORE _on_create.
	GDVIRTUAL_CALL(_on_setup_standalone, app);

	ActivityManager *am = app->get_activity_manager();
	if (am) {
		Ref<Intent> bootstrap_intent = intent;
		if (bootstrap_intent.is_null()) {
			bootstrap_intent.instantiate();
			bootstrap_intent->set_action("__standalone__");
			intent = bootstrap_intent;
		}
		am->adopt_running_toast(this, bootstrap_intent);
	}

	call_deferred(SNAME("_dispatch_standalone_lifecycle"), p_play_transitions);
}

void Toast::_dispatch_standalone_lifecycle(bool p_play_transitions) {
	if (!is_standalone()) {
		return;
	}
	// Standalone preview: center it and keep it visible (no auto-dismiss timer).
	set_anchors_preset(Control::PRESET_FULL_RECT);
	dispatch_create(intent.is_valid() ? intent->get_extras() : Dictionary());
	if (p_play_transitions && transition_in.is_valid()) {
		transition_in->play_enter(this);
	}
}

void Toast::_bind_methods() {
	ClassDB::bind_static_method("Toast", D_METHOD("make_text", "text", "duration"), &Toast::make_text, DEFVAL(2.0));

	ClassDB::bind_method(D_METHOD("set_text", "text"), &Toast::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &Toast::get_text);
	ClassDB::bind_method(D_METHOD("set_duration", "duration"), &Toast::set_duration);
	ClassDB::bind_method(D_METHOD("get_duration"), &Toast::get_duration);

	ClassDB::bind_method(D_METHOD("set_intent", "intent"), &Toast::set_intent);
	ClassDB::bind_method(D_METHOD("get_intent"), &Toast::get_intent);
	ClassDB::bind_method(D_METHOD("set_transition_in", "transition"), &Toast::set_transition_in);
	ClassDB::bind_method(D_METHOD("get_transition_in"), &Toast::get_transition_in);
	ClassDB::bind_method(D_METHOD("set_transition_out", "transition"), &Toast::set_transition_out);
	ClassDB::bind_method(D_METHOD("get_transition_out"), &Toast::get_transition_out);

	ClassDB::bind_method(D_METHOD("dismiss"), &Toast::dismiss);

	ClassDB::bind_method(D_METHOD("set_lifecycle_owner", "owner"), &Toast::set_lifecycle_owner);
	ClassDB::bind_method(D_METHOD("get_lifecycle_owner"), &Toast::get_lifecycle_owner);

	ClassDB::bind_method(D_METHOD("set_application", "application"), &Toast::set_application);
	ClassDB::bind_method(D_METHOD("set_context", "context"), &Toast::set_context);
	ClassDB::bind_method(D_METHOD("get_context"), &Toast::get_context);
	ClassDB::bind_method(D_METHOD("get_application"), &Toast::get_application);

	ClassDB::bind_method(D_METHOD("is_standalone"), &Toast::is_standalone);
	ClassDB::bind_method(D_METHOD("run_standalone_bootstrap", "play_transitions"), &Toast::run_standalone_bootstrap, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("_dispatch_standalone_lifecycle", "play_transitions"), &Toast::_dispatch_standalone_lifecycle);

	// ---- Generic delegates (inherited from ContextBase<Toast>) — PMF static_cast ----
	using GetServiceT = Object *(Toast::*)(const StringName &) const;
	ClassDB::bind_method(D_METHOD("get_service", "name"), static_cast<GetServiceT>(&Toast::get_service));
	using HasServiceT = bool (Toast::*)(const StringName &) const;
	ClassDB::bind_method(D_METHOD("has_service", "name"), static_cast<HasServiceT>(&Toast::has_service));
	using LoadAsyncT = void (Toast::*)(const String &, const Callable &, int);
	ClassDB::bind_method(D_METHOD("load_resource_async", "path", "callback", "priority"), static_cast<LoadAsyncT>(&Toast::load_resource_async), DEFVAL(Callable()), DEFVAL(0));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text"), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration"), "set_duration", "get_duration");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "transition_in", PROPERTY_HINT_RESOURCE_TYPE, "Transition"), "set_transition_in", "get_transition_in");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "transition_out", PROPERTY_HINT_RESOURCE_TYPE, "Transition"), "set_transition_out", "get_transition_out");

	GDVIRTUAL_BIND(_on_create, "saved_state");
	GDVIRTUAL_BIND(_on_dismiss);
	GDVIRTUAL_BIND(_on_setup_standalone, "app");
}
