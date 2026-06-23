/**************************************************************************/
/*  standalone_activity_launcher.h                                        */
/**************************************************************************/
#pragma once

#include "activity_launcher.h"

class IContext;

// Default ActivityLauncher: if the Context-bearing node entering the tree (an
// Activity or Dialog) is the actual entry scene (e.g. F6 in the editor, or
// `godot --path . <scene>.tscn`) and no Application has been bound to it yet,
// build a minimal Application + StandaloneRoot Control under the SceneTree
// root, reparent the node under StandaloneRoot, then drive the GDScript
// lifecycle. The node's own `run_standalone_bootstrap(bool)` does the building.
//
// Gate (must ALL hold for try_launch to take effect):
//   * no Application is bound yet (get_application() == nullptr) — i.e. this is
//     NOT the normal ActivityManager-injected flow, but a standalone preview
//   * the node is not running inside the editor (Engine::is_editor_hint)
//   * a SceneTree is available
//   * if `force_standalone` is false (default), the node must be the
//     SceneTree's current_scene; if true, runs regardless of position
//
// Exporting `play_transitions` lets the user toggle whether transition_in
// plays in standalone mode (default off, so the UI is visible immediately).
class StandaloneActivityLauncher : public ActivityLauncher {
	GDCLASS(StandaloneActivityLauncher, ActivityLauncher);

	bool force_standalone = false;
	bool play_transitions = false;

protected:
	static void _bind_methods();

public:
	void set_force_standalone(bool p_force);
	bool get_force_standalone() const;
	void set_play_transitions(bool p_play);
	bool get_play_transitions() const;

	bool try_launch(IContext *p_context) override;
};
