/**************************************************************************/
/*  dialog.h                                                              */
/**************************************************************************/
#pragma once

#include "../context_base.h"

#include "scene/gui/control.h"

#include "activity_launcher.h"
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
// ---- Run-As-Standalone (Strategy) ----
// Like Activity, Dialog carries one `launcher: Ref<ActivityLauncher>` (default = StandaloneActivityLauncher).
// On NOTIFICATION_READY it calls `launcher->try_launch(this)` once. If no Application is bound (i.e. the Dialog
// is the entry scene, not opened via ActivityManager.show_dialog), the launcher self-hosts it: builds an
// Application + StandaloneRoot, reparents the Dialog under it, and dispatches _on_create — so a `*_dialog.tscn`
// can be F6'd directly for UI/binding preview. Clear the launcher (set_launcher(null)) to disable.
class Dialog : public Control, public ContextBase<Dialog> {
	GDCLASS(Dialog, Control);

	Ref<Intent> intent;
	Ref<Transition> transition_in;
	Ref<Transition> transition_out;
	Ref<ActivityLauncher> launcher;
	Application *_app = nullptr;
	Object *owner = nullptr; // lifecycle owner — Dialog is auto-dismissed when the owning Activity is destroyed

protected:
	static void _bind_methods();
	void _notification(int p_what);

	GDVIRTUAL1(_on_create, Dictionary)
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

	// ---- Launcher (Run-As-Standalone) ----
	void set_launcher(const Ref<ActivityLauncher> &p_launcher);
	Ref<ActivityLauncher> get_launcher() const;
	bool is_standalone() const;

	// Public bootstrap entry — called by StandaloneActivityLauncher via call_deferred.
	// Builds Application + StandaloneRoot, reparents, adopts as a running dialog, dispatches _on_create.
	// Safe to call from GDScript for custom launchers / tests.
	void run_standalone_bootstrap(bool p_play_transitions = false);

	Dialog();
};
