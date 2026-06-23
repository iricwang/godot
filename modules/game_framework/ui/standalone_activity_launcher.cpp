/**************************************************************************/
/*  standalone_activity_launcher.cpp                                      */
/**************************************************************************/

#include "standalone_activity_launcher.h"

#include "../application.h"
#include "../context_interface.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"

bool StandaloneActivityLauncher::try_launch(IContext *p_context) {
	ERR_FAIL_NULL_V(p_context, false);

	// Core gate: an Application is already bound → this is the normal
	// ActivityManager-injected flow (or an already-completed bootstrap).
	// Either way, not ours to launch. Only an unbound node is a standalone
	// preview context.
	if (p_context->get_application() != nullptr) {
		return false;
	}
	// Editor "Open Scene" — don't run anything; let it sit in the inspector.
	if (Engine::get_singleton()->is_editor_hint()) {
		return false;
	}

	// The node must be in a SceneTree to host anything.
	Node *node = Object::cast_to<Node>(p_context->as_object());
	if (node == nullptr) {
		return false;
	}
	SceneTree *st = node->get_tree();
	if (st == nullptr) {
		return false;
	}
	// Position check: only the entry-scene node gets bootstrapped, unless
	// `force_standalone` is on (e.g. an EditorPlugin "Play This" workflow).
	if (!force_standalone && st->get_current_scene() != node) {
		return false;
	}

	// All gates open — tell the node to run its bootstrap on the next tick.
	// Both Activity and Dialog expose `run_standalone_bootstrap(play_transitions)`
	// as a stable API; we route through call_deferred to avoid mutating the
	// SceneTree inside the NOTIFICATION_READY pass.
	node->call_deferred(SNAME("run_standalone_bootstrap"), play_transitions);
	return true;
}

void StandaloneActivityLauncher::set_force_standalone(bool p_force) {
	force_standalone = p_force;
}

bool StandaloneActivityLauncher::get_force_standalone() const {
	return force_standalone;
}

void StandaloneActivityLauncher::set_play_transitions(bool p_play) {
	play_transitions = p_play;
}

bool StandaloneActivityLauncher::get_play_transitions() const {
	return play_transitions;
}

void StandaloneActivityLauncher::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_force_standalone", "force"), &StandaloneActivityLauncher::set_force_standalone);
	ClassDB::bind_method(D_METHOD("get_force_standalone"), &StandaloneActivityLauncher::get_force_standalone);
	ClassDB::bind_method(D_METHOD("set_play_transitions", "play"), &StandaloneActivityLauncher::set_play_transitions);
	ClassDB::bind_method(D_METHOD("get_play_transitions"), &StandaloneActivityLauncher::get_play_transitions);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "force_standalone"), "set_force_standalone", "get_force_standalone");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "play_transitions"), "set_play_transitions", "get_play_transitions");
}
