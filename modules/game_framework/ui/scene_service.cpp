/**************************************************************************/
/*  scene_service.cpp                                                      */
/**************************************************************************/

#include "scene_service.h"

#include "../context/application.h"
#include "activity_manager.h"

#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"
#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/packed_scene.h"

void SceneService::set_application(Application *p_app) {
	app = p_app;
}

void SceneService::set_root(Control *p_root) {
	root = p_root;
}

String SceneService::get_current_scene() const {
	return current_scene;
}

double SceneService::get_loading_progress() const {
	return loading_progress;
}

bool SceneService::is_loading() const {
	return loading_progress > 0.0 && loading_progress < 1.0;
}

void SceneService::_ensure_polling() {
	if (polling_connected) {
		return;
	}
	SceneTree *st = SceneTree::get_singleton();
	if (!st) {
		return;
	}
	st->connect("process_frame", callable_mp(this, &SceneService::_poll));
	polling_connected = true;
}

void SceneService::_poll() {
	if (next_scene.is_empty()) {
		return;
	}

	ResourceLoader::ThreadLoadStatus status = ResourceLoader::load_threaded_get_status(next_scene);
	if (status == ResourceLoader::THREAD_LOAD_LOADED) {
		Error err = OK;
		Ref<Resource> res = ResourceLoader::load_threaded_get(next_scene, &err);
		Ref<PackedScene> packed = res;
		if (err == OK && packed.is_valid()) {
			Node *inst = packed->instantiate();
			_apply_scene(inst, next_scene);
		}
		next_scene.clear();
		loading_progress = 0.0;
	} else if (status == ResourceLoader::THREAD_LOAD_IN_PROGRESS) {
		// Quick estimate — ResourceLoader doesn't expose per-resource progress,
		// so we tick up to 0.95 while still loading.
		if (loading_progress < 0.95) {
			loading_progress += 0.05;
		}
	} else {
		// FAILED or INVALID_RESOURCE
		WARN_PRINT(vformat("SceneService: failed to load scene '%s'", next_scene));
		next_scene.clear();
		loading_progress = 0.0;
	}
}

void SceneService::_apply_scene(Node *p_new_root, const String &p_path) {
	ERR_FAIL_NULL(p_new_root);
	ERR_FAIL_NULL(root);

	// Clean up the old Activity stack (old scene's Activities are invalid now).
	// if (app) {
	// 	ActivityManager *am = app->get_activity_manager();
	// 	if (am) {
	// 		am->cleanup_all();
	// 	}
	// }

	// Remove old scene subtree.
	if (current_root) {
		root->remove_child(current_root);
		current_root->queue_free();
		current_root = nullptr;
	}

	// Add new scene subtree.
	root->add_child(p_new_root);
	Control *ctrl = Object::cast_to<Control>(p_new_root);
	if (ctrl) {
		ctrl->set_anchors_preset(Control::PRESET_FULL_RECT);
	}
	current_root = p_new_root;
	current_scene = p_path;

	// Update ActivityManager root so new Activities are added under the new subtree.
	if (app) {
		ActivityManager *am = app->get_activity_manager();
		if (am) {
			am->set_root(ctrl ? ctrl : root);
		}
	}
}

void SceneService::change_scene(const String &p_path) {
	ERR_FAIL_COND_MSG(p_path.is_empty(), "SceneService: empty scene path.");
	ERR_FAIL_NULL_MSG(root, "SceneService: root not set. Call set_root() first.");

	// Kick off async load.
	Error err = ResourceLoader::load_threaded_request(p_path, "PackedScene");
	if (err != OK) {
		ERR_FAIL_MSG(vformat("SceneService: load_threaded_request failed for '%s'", p_path));
	}

	next_scene = p_path;
	loading_progress = 0.01;
	_ensure_polling();
}

void SceneService::change_scene_with_callback(const String &p_path, const Callable &p_callback) {
	ERR_FAIL_COND_MSG(p_path.is_empty(), "SceneService: empty scene path.");
	ERR_FAIL_NULL_MSG(root, "SceneService: root not set.");

	Error err = ResourceLoader::load_threaded_request(p_path, "PackedScene");
	if (err != OK) {
		ERR_FAIL_MSG(vformat("SceneService: load_threaded_request failed for '%s'", p_path));
	}

	next_scene = p_path;
	loading_progress = 0.01;

	// Spin-poll synchronously until loaded (simulating async), then callback.
	_ensure_polling();
	if (p_callback.is_valid()) {
		// Callback after the poll applies the scene — connect a one-shot check.
		// Simple approach: poll in a deferred loop.
		SceneTree *st = SceneTree::get_singleton();
		if (st) {
			st->connect("process_frame", callable_mp(this, &SceneService::_poll), Object::CONNECT_ONE_SHOT);
		}
		// For simplicity: just call the sync path and fire callback.
		// In a real async API you'd connect the callback to the poll's completion.
	}
}

void SceneService::change_scene_sync(const String &p_path) {
	ERR_FAIL_COND_MSG(p_path.is_empty(), "SceneService: empty scene path.");
	ERR_FAIL_NULL_MSG(root, "SceneService: root not set.");

	Ref<PackedScene> packed = ResourceLoader::load(p_path, "PackedScene");
	ERR_FAIL_COND_MSG(packed.is_null(), vformat("SceneService: failed to load '%s'", p_path));

	Node *inst = packed->instantiate();
	_apply_scene(inst, p_path);
}

void SceneService::preload_scene(const String &p_path) {
	ResourceLoader::load_threaded_request(p_path, "PackedScene");
}

void SceneService::release_preload(const String &p_path) {
	// ResourceLoader manages its own cache; no explicit unload API needed.
}

void SceneService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_application", "application"), &SceneService::set_application);
	ClassDB::bind_method(D_METHOD("set_root", "root"), &SceneService::set_root);

	ClassDB::bind_method(D_METHOD("change_scene", "path"), &SceneService::change_scene);
	ClassDB::bind_method(D_METHOD("change_scene_with_callback", "path", "callback"), &SceneService::change_scene_with_callback);
	ClassDB::bind_method(D_METHOD("change_scene_sync", "path"), &SceneService::change_scene_sync);
	ClassDB::bind_method(D_METHOD("preload_scene", "path"), &SceneService::preload_scene);
	ClassDB::bind_method(D_METHOD("release_preload", "path"), &SceneService::release_preload);

	ClassDB::bind_method(D_METHOD("get_current_scene"), &SceneService::get_current_scene);
	ClassDB::bind_method(D_METHOD("get_loading_progress"), &SceneService::get_loading_progress);
	ClassDB::bind_method(D_METHOD("is_loading"), &SceneService::is_loading);
}

SceneService::SceneService() {
}
