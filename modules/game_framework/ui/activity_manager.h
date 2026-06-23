/**************************************************************************/
/*  activity_manager.h                                                    */
/**************************************************************************/
#pragma once

#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

#include "activity_loader.h"
#include "intent.h"
#include "toast.h"

class Activity;
class Dialog;
class Control;
class Application;
class Node;

// Single-task-stack activity manager (the multi-task / affinity machinery from Android is intentionally omitted).
// Resolves an Intent's action to a registered scene, instantiates the Activity, drives its lifecycle + transition,
// and maintains a back stack. Supports FLAG_SINGLE_TOP and FLAG_CLEAR_TOP. Also hosts modal Dialogs and a queued
// Toast system on the same root container.
class ActivityManager : public Object {
	GDCLASS(ActivityManager, Object);

public:
	enum ToastDisplayMode {
		SERIAL,   // FIFO queue — one toast at a time (default)
		PARALLEL, // all toasts render simultaneously
	};

private:
	Control *root = nullptr; // container under which activities/dialogs/toasts are added (set by the game at startup)
	HashMap<String, String> registry; // action -> scene path
	Ref<ActivityLoader> loader;       // fallback resolver when action is not in registry
	Vector<Activity *> stack;
	Vector<Dialog *> dialogs;

	Vector<Toast *> toast_queue;
	bool toast_active = false;
	ToastDisplayMode toast_mode = SERIAL;
	Vector<Node *> active_toast_panels; // parallel mode: active toast nodes

	Application *app = nullptr; // set by Application::initialize()

	void _begin_exit(Activity *p_act); // run exit transition then queue_free
	void _show_next_toast();
	void _present_toast(Toast *p_toast); // add the Toast node to the tree, animate, schedule auto-dismiss
	void _on_toast_finished(Object *p_panel);

protected:
	static void _bind_methods();

public:
	void set_application(Application *p_app);
	Application *get_application() const;

	void set_root(Control *p_root);
	Control *get_root() const;

	void register_activity(const String &p_action, const String &p_scene_path);

	// ---- Loader ----
	// The loader is consulted whenever an action is NOT found in the manual registry.
	// ActivityManager ships with AutoActivityLoader pre-installed as the default.
	void set_loader(const Ref<ActivityLoader> &p_loader);
	Ref<ActivityLoader> get_loader() const;

	void start_activity(const Ref<Intent> &p_intent);

	// Insert an Activity that is already in the SceneTree (e.g. the entry
	// scene that bootstrapped itself in Run-As-Standalone mode) into the
	// stack as the bottom-most entry. The Activity must already have its
	// context and intent set by the caller; no lifecycle is dispatched here.
	void adopt_running_activity(Activity *p_activity, const Ref<Intent> &p_intent);

	void finish_activity(Activity *p_activity);
	void finish_top();
	bool back();

	void show_dialog(const Ref<Intent> &p_intent);
	void show_dialog_with_owner(const Ref<Intent> &p_intent, Object *p_owner);
	void dismiss_dialog(Dialog *p_dialog);

	// Register an already-instantiated, already-parented Dialog as a running
	// dialog (standalone preview path). Bookkeeping only: sets intent +
	// application + owner and tracks it in `dialogs`; does NOT add_child or
	// dispatch any lifecycle (the Dialog's own bootstrap does that).
	void adopt_running_dialog(Dialog *p_dialog, const Ref<Intent> &p_intent);
	void show_toast(Toast *p_toast);
	void show_toast_with_owner(Toast *p_toast, Object *p_owner);
	void clear_all_toasts();
	void clear_toasts_by_owner(Object *p_owner);

	// Register an already-instantiated, already-parented Toast as a running toast
	// (standalone preview path). Bookkeeping only: sets intent + application +
	// owner and tracks it; does NOT add_child, animate, or auto-dismiss.
	void adopt_running_toast(Toast *p_toast, const Ref<Intent> &p_intent);
	// Dismiss a shown Toast (standalone-root → quits the SceneTree, like Dialog).
	void dismiss_toast(Toast *p_toast);

	void set_toast_display_mode(ToastDisplayMode p_mode);
	ToastDisplayMode get_toast_display_mode() const;

	Activity *get_current_activity() const;
	int get_stack_size() const;

	// ---- Debug inspection ----
	Activity *get_stack_activity(int p_idx) const;
	int get_dialog_count() const;
	Dialog *get_dialog(int p_idx) const;
	int get_toast_queue_count() const;
	Toast *get_toast_queue_item(int p_idx) const;
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

VARIANT_ENUM_CAST(ActivityManager::ToastDisplayMode);
