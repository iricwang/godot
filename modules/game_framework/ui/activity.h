/**************************************************************************/
/*  activity.h                                                            */
/**************************************************************************/
#pragma once

#include "../context/context_base.h"

#include "scene/gui/control.h"

#include "intent.h"
#include "toast.h"
#include "transition.h"

#include "../resource/resource_handle.h"

class Context;
class Resource;
class Application;

// A full-screen UI screen with an Android-style lifecycle. GDScript subclasses override the _on_* virtual methods.
// Lifecycle is driven by ActivityManager (it calls the dispatch_* methods, which forward to the script virtuals).
//
// Activity is a C++ "is-a IContext" via ContextBase<Activity> — it carries an Application* and exposes the
// full Context-style API (start_activity / show_toast / get_service / load_resource_* ...) directly. The
// GDScript binding names are 100% backward compatible with the previous "Activity holds Context*" model.
//
// ---- Run-As-Standalone ----
// On NOTIFICATION_READY the Activity defers run_standalone_bootstrap to the next idle tick. The bootstrap
// self-guards: it's a no-op if an Application is already bound (the normal ActivityManager-injected flow,
// since the manager sets `_app` BEFORE add_child) or if we're running inside the editor. Otherwise it spins
// up a minimal StandaloneApplication so a `*_activity.tscn` can be F6'd directly for UI/binding preview.
class Activity : public Control, public ContextBase<Activity> {
	GDCLASS(Activity, Control);

private:
	Ref<Intent> intent;
	Ref<Transition> transition_in;
	Ref<Transition> transition_out;
	Application *_app = nullptr;
	bool no_history = false;

protected:
	static void _bind_methods();
	void _notification(int p_what);

	GDVIRTUAL1(_on_create, Dictionary)
	GDVIRTUAL0(_on_start)
	GDVIRTUAL0(_on_resume)
	GDVIRTUAL0(_on_pause)
	GDVIRTUAL0(_on_stop)
	GDVIRTUAL0(_on_destroy)
	GDVIRTUAL1(_on_new_intent, Ref<Intent>)
	GDVIRTUAL0R(bool, _on_back_pressed)
	GDVIRTUAL1(_on_setup_standalone, Application *)

public:
	void set_intent(const Ref<Intent> &p_intent);
	Ref<Intent> get_intent() const;
	void set_transition_in(const Ref<Transition> &p_transition);
	Ref<Transition> get_transition_in() const;
	void set_transition_out(const Ref<Transition> &p_transition);
	Ref<Transition> get_transition_out() const;

	// Lifecycle dispatch (called by ActivityManager).
	void dispatch_create(const Dictionary &p_saved_state);
	void dispatch_start();
	void dispatch_resume();
	void dispatch_pause();
	void dispatch_stop();
	void dispatch_destroy();
	void dispatch_new_intent(const Ref<Intent> &p_intent);
	bool dispatch_back_pressed();

	// Internal trampoline used by run_standalone_bootstrap via call_deferred.
	void _dispatch_standalone_lifecycle(bool p_play_transitions);

	void finish();

	// ---- No-history mode ----
	void set_no_history(bool p_no_history);
	bool get_no_history() const;

	// ---- Application linkage / Context access ----
	void set_application(Application *p_app);
	void set_context(Context *p_context); // backward-compat alias
	Context *get_context() const; // returns _app (Application IS-A Context)

	// ---- IContext implementation ----
	Application *get_application() const override { return _app; }
	Object *as_object() override { return this; }

	// True when hosted by a StandaloneApplication (self-hosted preview), false
	// when driven by a normal ActivityManager. Derived from the bound Application.
	bool is_standalone() const;

	// Public bootstrap entry — invoked via call_deferred from NOTIFICATION_READY.
	// Builds Application + StandaloneRoot, reparents, adopts into stack, dispatches lifecycle.
	// Safe to call from GDScript for tests; idempotent (no-op if Application is already bound).
	void run_standalone_bootstrap(bool p_play_transitions = false);

	Activity();
};
