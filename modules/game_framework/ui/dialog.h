/**************************************************************************/
/*  dialog.h                                                              */
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

// A modal overlay shown above the activity stack. Implemented as a Control overlay (not an OS Window) so it
// behaves consistently on mobile / web / console. The dialog scene draws its own backdrop + content; the manager
// handles instantiation, lifecycle, and transitions. Call dismiss() to close it.
//
// Dialog is a C++ "is-a IContext" via ContextBase<Dialog> — it carries an Application* and exposes the full
// Context-style API (show_toast / get_service / load_resource_* / ...) directly. The GDScript binding names
// are 100% backward compatible with the previous "Dialog holds Context*" model.
//
// ---- Run-As-Standalone ----
// On NOTIFICATION_READY Dialog defers run_standalone_bootstrap to the next idle tick. The bootstrap self-guards:
// no-op if an Application is already bound (the normal ActivityManager.show_dialog flow, since the manager
// sets `_app` BEFORE add_child) or if we're in the editor. Otherwise it spins up a minimal StandaloneApplication
// so a `*_dialog.tscn` can be F6'd directly for UI/binding preview. Closing a standalone-root Dialog quits the
// SceneTree.
class Dialog : public Control, public ContextBase<Dialog> {
	GDCLASS(Dialog, Control);

	Ref<Intent> intent;
	Ref<Transition> transition_in;
	Ref<Transition> transition_out;
	Application *_app = nullptr;
	Object *owner = nullptr; // lifecycle owner — Dialog is auto-dismissed when the owning Activity is destroyed
	bool _paused = false; // true while suspended under a FLAG_SCENE activity

protected:
	static void _bind_methods();
	void _notification(int p_what);

	GDVIRTUAL1(_on_create, Dictionary)
	GDVIRTUAL0(_on_pause)
	GDVIRTUAL0(_on_resume)
	GDVIRTUAL0(_on_dismiss)
	GDVIRTUAL1(_on_setup_standalone, Application *)

public:
	void set_intent(const Ref<Intent> &p_intent);
	Ref<Intent> get_intent() const;
	void set_transition_in(const Ref<Transition> &p_transition);
	Ref<Transition> get_transition_in() const;
	void set_transition_out(const Ref<Transition> &p_transition);
	Ref<Transition> get_transition_out() const;

	void dispatch_create(const Dictionary &p_saved_state);
	void dispatch_dismiss();

	// Scene-curtain pause/resume. Called by ActivityManager when a
	// FLAG_SCENE activity is pushed on top of (or popped off of) the
	// stack: the dialog is hidden + its _on_pause virtual is dispatched
	// so GDScript subclasses can drop their enhanced_input IMC, freeze
	// timers, etc. Idempotent -- a double-pause is a no-op.
	void dispatch_pause();
	void dispatch_resume();
	bool is_paused() const { return _paused; }

	// Internal trampoline used by run_standalone_bootstrap via call_deferred.
	void _dispatch_standalone_lifecycle(bool p_play_transitions);

	void dismiss();

	// ---- Owner (lifecycle binding) ----
	void set_lifecycle_owner(Object *p_owner);
	Object *get_lifecycle_owner() const;

	// ---- Application linkage / Context access ----
	void set_application(Application *p_app);
	void set_context(Context *p_context); // backward-compat alias
	Context *get_context() const; // returns _app

	// ---- IContext implementation ----
	Application *get_application() const override { return _app; }
	Object *as_object() override { return this; }

	bool is_standalone() const;

	// Public bootstrap entry — invoked via call_deferred from NOTIFICATION_READY.
	// Builds Application + StandaloneRoot, reparents, adopts as a running dialog, dispatches _on_create.
	// Safe to call from GDScript for tests; idempotent (no-op if Application is already bound).
	void run_standalone_bootstrap(bool p_play_transitions = false);

	Dialog();
};
