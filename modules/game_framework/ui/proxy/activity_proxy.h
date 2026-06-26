/**************************************************************************/
/*  activity_proxy.h                                                      */
/**************************************************************************/
#pragma once

#include "context_proxy.h"

#include "core/object/object_id.h"
#include "core/templates/vector.h"

class Activity;

// Concrete ContextProxy held by ActivityManager::stack. Adds Activity-specific bookkeeping:
//   * no_history flag mirrored from Intent at push time, so a CANCELLED/LOADING entry can
//     still be inspected by the manager without dereferencing a not-yet-existing Activity.
//   * pending_new_intent captures a SINGLE_TOP/CLEAR_TOP retarget that arrived while the
//     proxy was still LOADING, so dispatch_new_intent runs as soon as the node is READY.
//   * lifecycle_stage tracks where the underlying Activity is in its create/start/resume
//     /pause/stop/destroy chain so ActivityManager can guard transitions (e.g. don't
//     dispatch_stop on a never-resumed activity, don't dispatch_resume on a destroyed one).
class ActivityProxy : public ContextProxy {
	GDCLASS(ActivityProxy, ContextProxy);

public:
	enum LifecycleStage {
		LIFECYCLE_NONE,      // before create
		LIFECYCLE_STARTED,   // create+start dispatched, not (yet) resumed
		LIFECYCLE_RESUMED,   // resumed — visible top
		LIFECYCLE_PAUSED,    // paused (resume happened earlier, then pause)
		LIFECYCLE_STOPPED,   // stopped (resume happened, then pause, then stop) — invisible
		LIFECYCLE_DESTROYED, // destroyed — the Activity Node has been queue_free'd
	};

private:
	bool no_history = false;
	Ref<Intent> pending_new_intent;
	LifecycleStage lifecycle_stage = LIFECYCLE_NONE;
	// True when this Activity was pushed with Intent::FLAG_SCENE -- it
	// owns a "curtain" over everything that was alive before it. The
	// ActivityManager uses this flag to decide whether to stay-suspended
	// when a nested scene above gets popped, and to gate the dialog
	// restore on this proxy's pop.
	bool scene_curtain = false;
	// ObjectIDs of the Dialog nodes that this scene curtain hid+paused
	// at push time. Restored (set_visible(true) + dispatch_resume) when
	// the proxy is popped, unless another scene is now on top.
	Vector<ObjectID> suspended_dialog_ids;

protected:
	static void _bind_methods();

public:
	void set_no_history(bool p_v) { no_history = p_v; }
	bool get_no_history() const { return no_history; }

	void set_pending_new_intent(const Ref<Intent> &p_intent) { pending_new_intent = p_intent; }
	Ref<Intent> get_pending_new_intent() const { return pending_new_intent; }
	bool has_pending_new_intent() const { return pending_new_intent.is_valid(); }
	void clear_pending_new_intent() { pending_new_intent = Ref<Intent>(); }

	LifecycleStage get_lifecycle_stage() const { return lifecycle_stage; }
	void _set_lifecycle_stage(LifecycleStage p_stage) { lifecycle_stage = p_stage; }

	// ---- Scene-curtain bookkeeping (Intent::FLAG_SCENE) ----
	bool is_scene_curtain() const { return scene_curtain; }
	void _set_scene_curtain(bool p_curtain) { scene_curtain = p_curtain; }
	void _add_suspended_dialog(ObjectID p_id) { suspended_dialog_ids.push_back(p_id); }
	const Vector<ObjectID> &get_suspended_dialog_ids() const { return suspended_dialog_ids; }
	void _clear_suspended_dialogs() { suspended_dialog_ids.clear(); }

	// Returns the live Activity*, or nullptr if it has been freed / not yet attached.
	Activity *get_activity() const;
};

VARIANT_ENUM_CAST(ActivityProxy::LifecycleStage);
