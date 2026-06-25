/**************************************************************************/
/*  activity_proxy.h                                                      */
/**************************************************************************/
#pragma once

#include "context_proxy.h"

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

	// Returns the live Activity*, or nullptr if it has been freed / not yet attached.
	Activity *get_activity() const;
};

VARIANT_ENUM_CAST(ActivityProxy::LifecycleStage);
