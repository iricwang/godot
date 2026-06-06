/**************************************************************************/
/*  activity.h                                                            */
/**************************************************************************/
#pragma once

#include "scene/gui/control.h"

#include "intent.h"
#include "toast.h"
#include "transition.h"

#include "../resource/resource_handle.h"

class Context;
class Resource;

// A full-screen UI screen with an Android-style lifecycle. GDScript subclasses override the _on_* virtual methods.
// Lifecycle is driven by ActivityManager (it calls the dispatch_* methods, which forward to the script virtuals).
//
// Activity holds a Context* reference (set by ActivityManager during creation) that gives it access to
// Application-wide services (navigation, resource loading, service lookup, etc.). The most-used Context
// methods are re-exported directly on Activity for an "Activity IS-A Context" GDScript experience.
class Activity : public Control {
	GDCLASS(Activity, Control);

	Ref<Intent> intent;
	Ref<Transition> transition_in;
	Ref<Transition> transition_out;
	Context *context = nullptr;
	bool no_history = false;

protected:
	static void _bind_methods();

	GDVIRTUAL1(_on_create, Dictionary)
	GDVIRTUAL0(_on_start)
	GDVIRTUAL0(_on_resume)
	GDVIRTUAL0(_on_pause)
	GDVIRTUAL0(_on_stop)
	GDVIRTUAL0(_on_destroy)
	GDVIRTUAL1(_on_new_intent, Ref<Intent>)
	GDVIRTUAL0R(bool, _on_back_pressed)

public:
	void set_intent(const Ref<Intent> &p_intent);
	Ref<Intent> get_intent() const;
	void set_transition_in(const Ref<Transition> &p_transition);
	Ref<Transition> get_transition_in() const;
	void set_transition_out(const Ref<Transition> &p_transition);
	Ref<Transition> get_transition_out() const;

	// Lifecycle dispatch (called by ActivityManager). Each forwards to the GDScript virtual if overridden.
	void dispatch_create(const Dictionary &p_saved_state);
	void dispatch_start();
	void dispatch_resume();
	void dispatch_pause();
	void dispatch_stop();
	void dispatch_destroy();
	void dispatch_new_intent(const Ref<Intent> &p_intent);
	bool dispatch_back_pressed();

	void finish();

	// ---- No-history mode ----
	void set_no_history(bool p_no_history);
	bool get_no_history() const;

	// ---- Context access ----
	void set_context(Context *p_context);
	Context *get_context() const;

	// ---- Convenience methods (delegate to context) ----
	void start_activity(const Ref<Intent> &p_intent);
	void finish_top();
	bool back();
	void show_dialog(const Ref<Intent> &p_intent);
	void show_toast(const Ref<Toast> &p_toast);
	Object *get_service(const StringName &p_name) const;
	bool has_service(const StringName &p_name) const;
	Ref<ResourceHandle> get_resource_handle(const String &p_path);
	Ref<Resource> load_resource_sync(const String &p_path);
	void load_resource_async(const String &p_path, const Callable &p_callback = Callable(), int p_priority = 0);
};
