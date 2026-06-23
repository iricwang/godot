/**************************************************************************/
/*  toast.h                                                               */
/**************************************************************************/
#pragma once

#include "../context_base.h"

#include "scene/gui/control.h"

#include "activity_launcher.h"
#include "intent.h"
#include "transition.h"

#include "../resource/resource_handle.h"

#include "core/object/object_id.h"

class Context;
class Resource;
class Application;

// A transient message overlay shown near the bottom of the screen by
// ActivityManager (a fading panel, FIFO-queued). Toast.make_text(...) builds a
// default text toast; a custom layout is authored as a scene whose root extends
// Toast and overrides _on_create — exactly like Dialog.
//
// Toast is a C++ "is-a IContext" via ContextBase<Toast> — it carries an
// Application* and exposes the full Context-style API. Like Activity / Dialog it
// can self-host for preview (F6) through its `launcher`.
//
// An optional lifecycle_owner binds the Toast to a Context; when the owning
// Activity is destroyed, ActivityManager cancels the owner's toasts.
class Toast : public Control, public ContextBase<Toast> {
	GDCLASS(Toast, Control);

	String text;
	double duration = 2.0;
	Ref<Intent> intent;
	Ref<Transition> transition_in;
	Ref<Transition> transition_out;
	Ref<ActivityLauncher> launcher;
	Application *_app = nullptr;
	Object *lifecycle_owner = nullptr; // owning Context; 0 = global. Named to avoid Node::set_owner clash.
	ObjectID owner_id; // cached id of lifecycle_owner for fast queue filtering

	bool _default_built = false; // guard so the default panel+label is built once

protected:
	static void _bind_methods();
	void _notification(int p_what);

	GDVIRTUAL1(_on_create, Dictionary)
	GDVIRTUAL0(_on_dismiss)
	GDVIRTUAL1(_on_setup_standalone, Application *)

public:
	// Factory — builds a default text Toast node. Returns a freshly created node
	// (not yet in any tree); pass it to Context.show_toast(...).
	static Toast *make_text(const String &p_text, double p_duration = 2.0);

	void set_text(const String &p_text);
	String get_text() const;
	void set_duration(double p_duration);
	double get_duration() const;

	void set_intent(const Ref<Intent> &p_intent);
	Ref<Intent> get_intent() const;
	void set_transition_in(const Ref<Transition> &p_transition);
	Ref<Transition> get_transition_in() const;
	void set_transition_out(const Ref<Transition> &p_transition);
	Ref<Transition> get_transition_out() const;

	// Lifecycle dispatch (called by ActivityManager / standalone bootstrap).
	void dispatch_create(const Dictionary &p_saved_state);
	void dispatch_dismiss();
	void _dispatch_standalone_lifecycle(bool p_play_transitions);

	void dismiss();

	// ---- Owner (lifecycle binding) ----
	void set_lifecycle_owner(Object *p_owner); // nullptr clears
	Object *get_lifecycle_owner() const; // returns owner if still alive, else nullptr
	bool is_owned_by(ObjectID p_id) const; // fast check for ActivityManager filtering

	// ---- Application linkage / Context access ----
	void set_application(Application *p_app);
	void set_context(Context *p_context); // backward-compat alias
	Context *get_context() const; // returns _app

	// ---- IContext implementation ----
	Application *get_application() const override { return _app; }
	Object *as_object() override { return this; }

	// ---- Launcher (Run-As-Standalone) ----
	void set_launcher(const Ref<ActivityLauncher> &p_launcher);
	Ref<ActivityLauncher> get_launcher() const;
	bool is_standalone() const;
	void run_standalone_bootstrap(bool p_play_transitions = false);

	Toast();
};
