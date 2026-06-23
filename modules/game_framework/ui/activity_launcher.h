/**************************************************************************/
/*  activity_launcher.h                                                   */
/**************************************************************************/
#pragma once

#include "core/io/resource.h"

class IContext;

// Strategy that decides whether — and how — a Context-bearing UI node (Activity
// or Dialog) should self-bootstrap when it enters the SceneTree without an
// ActivityManager hosting it.
//
// The node holds one ActivityLauncher Ref. On NOTIFICATION_READY the node calls
// `launcher->try_launch(this)` (Activity/Dialog upcast to IContext*); the
// launcher decides if its conditions match — chiefly "no Application is bound
// yet, so this is NOT the normal ActivityManager-injected flow" — and, if so,
// builds the minimal hosting environment (Application + StandaloneRoot) and
// drives the GDScript lifecycle.
//
// Operating on IContext* (not Activity*) lets the SAME launcher class/instance
// serve both Activity and Dialog: both are `Control + ContextBase<Self>` and
// expose `run_standalone_bootstrap(bool)`. The name keeps "Activity" for
// backward compatibility (registered class + Activity property type) but the
// mechanism is generic.
//
// Replacing the launcher swaps the entire startup model — no code in the node
// needs to change to support new modes (nested preview, editor "Play This",
// multi-window, snapshot-replay, ...). The default launcher is
// `StandaloneActivityLauncher`, which implements the "F6 single-screen debug"
// behaviour.
//
// Returns true to claim ownership of the launch attempt; false means "not my
// job, leave the node alone".
//
// Why not a GDVIRTUAL hook: ActivityLauncher is a C++ extension point; the
// concrete launchers we ship today are all C++ (StandaloneActivityLauncher).
// If a future use case wants GDScript-authored launchers, we can add a
// GDVIRTUAL with an Object* parameter and dynamic_cast inside.
class ActivityLauncher : public Resource {
	GDCLASS(ActivityLauncher, Resource);

protected:
	static void _bind_methods();

public:
	// Called by Activity/Dialog::_notification(NOTIFICATION_READY) exactly once.
	// Default implementation is a no-op; concrete subclasses do the work.
	virtual bool try_launch(IContext *p_context) { return false; }
};
