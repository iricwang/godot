/**************************************************************************/
/*  activity_manager.h                                                    */
/**************************************************************************/
#pragma once

#include "core/object/object.h"
#include "core/object/object_id.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

#include "activity_loader.h"
#include "proxy/activity_proxy.h"
#include "proxy/dialog_proxy.h"
#include "intent.h"
#include "toast.h"
#include "proxy/toast_proxy.h"

class Activity;
class Dialog;
class Control;
class Application;
class Node;
class PackedScene;

// Single-task-stack activity manager (the multi-task / affinity machinery from Android is intentionally omitted).
// Resolves an Intent's action to a registered scene, instantiates the Activity, drives its lifecycle + transition,
// and maintains a back stack. Supports FLAG_SINGLE_TOP and FLAG_CLEAR_TOP. Also hosts modal Dialogs and a queued
// Toast system on the same root container.
//
// The stack/dialogs/toast_queue hold [ContextProxy] subclass refs, not raw Node*. A proxy is created
// synchronously at push time so flag matching and stack queries stay consistent while the underlying
// scene loads on a background thread. Setting [code]Intent.FLAG_LOAD_SYNC[/code] (or running on a
// no-thread platform like single-threaded Web) forces the legacy synchronous code path.
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
	Vector<Ref<ActivityProxy>> stack;
	Vector<Ref<DialogProxy>> dialogs;

	Vector<Ref<ToastProxy>> toast_queue;
	bool toast_active = false;
	ToastDisplayMode toast_mode = SERIAL;
	Vector<Ref<ToastProxy>> active_toast_proxies; // parallel mode: active toast proxies

	// In-flight async scene loads. Tracks the proxy + scene_path; ActivityManager polls
	// ResourceLoader::load_threaded_get_status every frame.
	struct LoadingTask {
		Ref<ContextProxy> proxy;
		String scene_path;
	};
	Vector<LoadingTask> loading;
	bool polling_connected = false;
	void _ensure_polling();
	void _poll_loads();

	Application *app = nullptr; // set by Application::initialize()

	void _begin_exit(Activity *p_act); // run exit transition then queue_free
	void _show_next_toast();
	void _present_toast(const Ref<ToastProxy> &p_proxy); // add the Toast node to the tree, animate, schedule auto-dismiss
	void _on_toast_finished(Object *p_panel);

	// ---- Proxy helpers ----
	int _stack_index_of_action(const String &p_action) const;
	int _stack_index_of_activity(Activity *p_act) const;
	int _dialogs_index_of_dialog(Dialog *p_dlg) const;
	void _drop_stack_entry(int p_idx, bool p_dispatch_lifecycle); // dispatch_pause/stop/destroy if READY, cancel if LOADING; queue_free node
	// ---- Scene curtain (Intent::FLAG_SCENE) helpers ----
	// Stop+hide every Activity currently in the stack and pause+hide every
	// Dialog. The dialog IDs are recorded on `p_new_scene_proxy` so they
	// can be selectively restored when that proxy is later popped.
	// Idempotent against a stack already under another scene curtain
	// (everything is already STOPPED/_paused), but still records IDs so
	// the nested scene knows what to restore on its own pop.
	void _drape_scene_curtain(const Ref<ActivityProxy> &p_new_scene_proxy);
	// Mirror of the above: dispatch_resume + set_visible(true) on the
	// dialogs `p_popped` originally suspended, but ONLY when the current
	// new top isn't itself another scene curtain (otherwise we'd un-hide
	// dialogs that should still be under a curtain).
	void _lift_scene_curtain(const Ref<ActivityProxy> &p_popped);
	String _resolve_scene_path(const String &p_action) const;

	// ---- Async scene load orchestration ----
	void _begin_activity_load(const Ref<ActivityProxy> &p_proxy, Activity *p_pause_top);
	void _begin_dialog_load(const Ref<DialogProxy> &p_proxy);
	void _attach_activity(const Ref<ActivityProxy> &p_proxy, const Ref<PackedScene> &p_packed);
	void _attach_dialog(const Ref<DialogProxy> &p_proxy, const Ref<PackedScene> &p_packed);

	// ---- Lifecycle transition helpers (operate on ActivityProxy::lifecycle_stage) ----
	// Guarded so we never call dispatch_pause on a never-resumed Activity, etc.
	void _transition_to_paused(const Ref<ActivityProxy> &p_proxy);
	void _transition_to_stopped_hidden(const Ref<ActivityProxy> &p_proxy);
	void _transition_to_resumed(const Ref<ActivityProxy> &p_proxy);
	void _transition_to_destroyed(const Ref<ActivityProxy> &p_proxy);

	// Resume the Activity referenced by p_proxy->pause_owner_id, but only if it
	// is now the top of the stack and still in PAUSED/STOPPED. Used on rollback.
	void _resume_pause_owner_if_top(const Ref<ActivityProxy> &p_cancelled_proxy);

	// Handler for ContextProxy::cancelled signal — drains any CANCELLED proxy
	// still resident in stack / dialogs / toast_queue (idempotent).
	void _on_proxy_cancelled();

protected:
	static void _bind_methods();

public:
	// Called by Activity / Dialog / Toast standalone bootstrap when its
	// _dispatch_standalone_lifecycle trampoline has finished dispatching
	// create/start/resume on the deferred frame. Flips the adopted proxy
	// from LOADING to READY+RESUMED and emits the `ready` signal so external
	// listeners see the same state the proxy will be in for the rest of the run.
	void _mark_adopted_ready(Object *p_instance);

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

	Ref<ActivityProxy> start_activity(const Ref<Intent> &p_intent);

	// Insert an Activity that is already in the SceneTree (e.g. the entry
	// scene that bootstrapped itself in Run-As-Standalone mode) into the
	// stack as the bottom-most entry. The Activity must already have its
	// context and intent set by the caller; no lifecycle is dispatched here.
	Ref<ActivityProxy> adopt_running_activity(Activity *p_activity, const Ref<Intent> &p_intent);

	void finish_activity(Activity *p_activity);
	void finish_top();
	bool back();

	Ref<DialogProxy> show_dialog(const Ref<Intent> &p_intent);
	Ref<DialogProxy> show_dialog_with_owner(const Ref<Intent> &p_intent, Object *p_owner);
	void dismiss_dialog(Dialog *p_dialog);

	// Register an already-instantiated, already-parented Dialog as a running
	// dialog (standalone preview path). Bookkeeping only: sets intent +
	// application + owner and tracks it in `dialogs`; does NOT add_child or
	// dispatch any lifecycle (the Dialog's own bootstrap does that).
	Ref<DialogProxy> adopt_running_dialog(Dialog *p_dialog, const Ref<Intent> &p_intent);
	Ref<ToastProxy> show_toast(Toast *p_toast);
	Ref<ToastProxy> show_toast_with_owner(Toast *p_toast, Object *p_owner);
	void clear_all_toasts();
	void clear_toasts_by_owner(Object *p_owner);

	// Register an already-instantiated, already-parented Toast as a running toast
	// (standalone preview path). Bookkeeping only: sets intent + application +
	// owner and tracks it; does NOT add_child, animate, or auto-dismiss.
	Ref<ToastProxy> adopt_running_toast(Toast *p_toast, const Ref<Intent> &p_intent);
	// Dismiss a shown Toast (standalone-root → quits the SceneTree, like Dialog).
	void dismiss_toast(Toast *p_toast);

	void set_toast_display_mode(ToastDisplayMode p_mode);
	ToastDisplayMode get_toast_display_mode() const;

	Activity *get_current_activity() const;
	Ref<ActivityProxy> get_current_activity_proxy() const;
	int get_stack_size() const;

	// ---- Debug inspection ----
	Activity *get_stack_activity(int p_idx) const;
	Ref<ActivityProxy> get_stack_proxy(int p_idx) const;
	int get_dialog_count() const;
	Dialog *get_dialog(int p_idx) const;
	Ref<DialogProxy> get_dialog_proxy(int p_idx) const;
	int get_toast_queue_count() const;
	Toast *get_toast_queue_item(int p_idx) const;
	Ref<ToastProxy> get_toast_queue_proxy(int p_idx) const;
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
