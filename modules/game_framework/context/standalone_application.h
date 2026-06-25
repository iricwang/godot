/**************************************************************************/
/*  standalone_application.h                                              */
/**************************************************************************/
#pragma once

#include "application.h"

class Control;

// StandaloneApplication — an Application that hosts a single UI node (Activity /
// Dialog / Toast) in "preview" mode, with no game project wiring.
//
// This encapsulates the host-graph construction that Activity / Dialog / Toast
// previously each hand-rolled in their own run_standalone_bootstrap. The static
// host() builds the tree:
//
//   SceneTree.root
//     └─ StandaloneApplication
//        └─ StandaloneRoot (Control, FULL_RECT)
//           └─ <the UI node>   ← reparented here
//
// "Is this node running standalone?" is answered by whether its bound
// Application is a StandaloneApplication (Object::cast_to). No per-node bool
// flag is needed — standalone-ness is a property of the hosting Application.
class StandaloneApplication : public Application {
	GDCLASS(StandaloneApplication, Application);

	Control *standalone_root = nullptr;

protected:
	static void _bind_methods();

public:
	Control *get_standalone_root() const;

	// Build the standalone host environment around p_ui (which must already be
	// in a SceneTree), reparent p_ui under StandaloneRoot, wire managers, and
	// install a default AutoActivityLoader rooted at res://. Returns the new
	// StandaloneApplication (already added to the tree), or nullptr on failure.
	// The caller is responsible for adopting p_ui into the manager and
	// dispatching its lifecycle (the per-type bit the common host can't do).
	static StandaloneApplication *host(Control *p_ui);

	StandaloneApplication();
};
