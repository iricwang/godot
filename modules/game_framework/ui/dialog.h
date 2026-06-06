/**************************************************************************/
/*  dialog.h                                                              */
/**************************************************************************/
#pragma once

#include "scene/gui/control.h"

#include "intent.h"
#include "toast.h"
#include "transition.h"

#include "../resource/resource_handle.h"

class Context;
class Resource;

// A modal overlay shown above the activity stack. Implemented as a Control overlay (not an OS Window) so it
// behaves consistently on mobile / web / console. The dialog scene draws its own backdrop + content; the manager
// handles instantiation, lifecycle, and transitions. Call dismiss() to close it.
//
// Dialog holds a Context* reference (set by ActivityManager during creation) that gives it access to
// Application-wide services. The most-used Context methods are re-exported directly on Dialog.
class Dialog : public Control {
	GDCLASS(Dialog, Control);

	Ref<Intent> intent;
	Ref<Transition> transition_in;
	Ref<Transition> transition_out;
	Context *context = nullptr;
	Object *owner = nullptr; // lifecycle owner — Dialog is auto-dismissed when the owning Activity is destroyed

protected:
	static void _bind_methods();

	GDVIRTUAL1(_on_create, Dictionary)
	GDVIRTUAL0(_on_dismiss)

public:
	void set_intent(const Ref<Intent> &p_intent);
	Ref<Intent> get_intent() const;
	void set_transition_in(const Ref<Transition> &p_transition);
	Ref<Transition> get_transition_in() const;
	void set_transition_out(const Ref<Transition> &p_transition);
	Ref<Transition> get_transition_out() const;

	void dispatch_create(const Dictionary &p_saved_state);
	void dispatch_dismiss();

	void dismiss();

	// ---- Owner (lifecycle binding) ----
	void set_lifecycle_owner(Object *p_owner);
	Object *get_lifecycle_owner() const;

	// ---- Context access ----
	void set_context(Context *p_context);
	Context *get_context() const;

	// ---- Convenience methods (delegate to context) ----
	void show_toast(const Ref<Toast> &p_toast);
	Object *get_service(const StringName &p_name) const;
	bool has_service(const StringName &p_name) const;
	Ref<ResourceHandle> get_resource_handle(const String &p_path);
	Ref<Resource> load_resource_sync(const String &p_path);
	void load_resource_async(const String &p_path, const Callable &p_callback = Callable(), int p_priority = 0);
};
