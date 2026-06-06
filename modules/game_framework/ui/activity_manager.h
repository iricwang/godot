/**************************************************************************/
/*  activity_manager.h                                                    */
/**************************************************************************/
#pragma once

#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

#include "intent.h"
#include "toast.h"

class Activity;
class Dialog;
class Control;
class Application;

// Single-task-stack activity manager (the multi-task / affinity machinery from Android is intentionally omitted).
// Resolves an Intent's action to a registered scene, instantiates the Activity, drives its lifecycle + transition,
// and maintains a back stack. Supports FLAG_SINGLE_TOP and FLAG_CLEAR_TOP. Also hosts modal Dialogs and a queued
// Toast system on the same root container.
class ActivityManager : public Object {
	GDCLASS(ActivityManager, Object);

	Control *root = nullptr; // container under which activities/dialogs/toasts are added (set by the game at startup)
	HashMap<String, String> registry; // action -> scene path
	Vector<Activity *> stack;
	Vector<Dialog *> dialogs;

	Vector<Ref<Toast>> toast_queue;
	bool toast_active = false;

	Application *app = nullptr; // set by Application::initialize()

	void _begin_exit(Activity *p_act); // run exit transition then queue_free
	void _show_next_toast();
	void _on_toast_finished(Object *p_panel);

protected:
	static void _bind_methods();

public:
	void set_application(Application *p_app);
	Application *get_application() const;

	void set_root(Control *p_root);
	Control *get_root() const;

	void register_activity(const String &p_action, const String &p_scene_path);
	void start_activity(const Ref<Intent> &p_intent);
	void finish_activity(Activity *p_activity);
	void finish_top();
	bool back();

	void show_dialog(const Ref<Intent> &p_intent);
	void show_dialog_with_owner(const Ref<Intent> &p_intent, Object *p_owner);
	void dismiss_dialog(Dialog *p_dialog);
	void show_toast(const Ref<Toast> &p_toast);
	void show_toast_with_owner(const Ref<Toast> &p_toast, Object *p_owner);

	Activity *get_current_activity() const;
	int get_stack_size() const;

	// ---- Debug inspection ----
	Activity *get_stack_activity(int p_idx) const;
	int get_dialog_count() const;
	Dialog *get_dialog(int p_idx) const;
	int get_toast_queue_count() const;
	Ref<Toast> get_toast_queue_item(int p_idx) const;
	bool is_toast_active() const;

	// Drain all activities, dialogs, and toasts. Called by Application::shutdown().
	void cleanup_all();

	ActivityManager();
	~ActivityManager();

private:
	// Auto-dismiss dialogs whose owner matches the given Object (Activity being destroyed).
	void _dismiss_owned_dialogs(Object *p_owner);
	// Remove toasts owned by the given Object from the pending queue.
	void _cancel_owned_toasts(Object *p_owner);
	// Resolve a null owner to the current top Activity (or Application if stack is empty).
	Object *_resolve_default_owner();
};
