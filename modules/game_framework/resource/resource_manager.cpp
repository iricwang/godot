/**************************************************************************/
/*  resource_manager.cpp                                                  */
/**************************************************************************/

#include "resource_manager.h"

#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/main/scene_tree.h"

Ref<ResourceHandle> ResourceManager::get_handle(const String &p_path) {
	const StringName key = p_path;
	HashMap<StringName, Ref<ResourceHandle>>::Iterator it = handles.find(key);
	if (it) {
		return it->value;
	}
	Ref<ResourceHandle> handle;
	handle.instantiate();
	handle->set_path(p_path);
	handles[key] = handle;
	return handle;
}

Ref<Resource> ResourceManager::load_sync(const String &p_path) {
	return get_handle(p_path)->load_sync();
}

void ResourceManager::load_async(const String &p_path, const Callable &p_callback, int p_priority) {
	Ref<ResourceHandle> handle = get_handle(p_path);

	if (handle->is_loaded()) {
		if (p_callback.is_valid()) {
			p_callback.call_deferred(handle->get_resource());
		}
		return;
	}

	// Single-threaded platforms (e.g. Web export without thread support): load synchronously to avoid hanging.
	if (!OS::get_singleton()->has_feature("threads")) {
		Ref<Resource> res = handle->load_sync();
		if (p_callback.is_valid()) {
			p_callback.call_deferred(res);
		}
		return;
	}

	const Error err = ResourceLoader::load_threaded_request(p_path);
	if (err != OK) {
		handle->_set_state(ResourceHandle::STATE_FAILED);
		handle->_emit_failed();
		if (p_callback.is_valid()) {
			p_callback.call_deferred(Ref<Resource>());
		}
		return;
	}

	handle->_set_state(ResourceHandle::STATE_LOADING);
	AsyncTask task;
	task.handle = handle;
	task.callback = p_callback;
	loading.push_back(task);
	_ensure_polling();
}

void ResourceManager::_poll() {
	for (int i = loading.size() - 1; i >= 0; --i) {
		const String path = loading[i].handle->get_path();
		const ResourceLoader::ThreadLoadStatus status = ResourceLoader::load_threaded_get_status(path);

		if (status == ResourceLoader::THREAD_LOAD_LOADED) {
			Error err = OK;
			Ref<Resource> res = ResourceLoader::load_threaded_get(path, &err);
			Ref<ResourceHandle> handle = loading[i].handle;
			const Callable cb = loading[i].callback;
			handle->_set_resource(res);
			handle->_set_state(ResourceHandle::STATE_LOADED);
			handle->_emit_loaded(res);
			loading.remove_at(i);
			if (cb.is_valid()) {
				cb.call_deferred(res);
			}
		} else if (status == ResourceLoader::THREAD_LOAD_FAILED || status == ResourceLoader::THREAD_LOAD_INVALID_RESOURCE) {
			Ref<ResourceHandle> handle = loading[i].handle;
			const Callable cb = loading[i].callback;
			handle->_set_state(ResourceHandle::STATE_FAILED);
			handle->_emit_failed();
			loading.remove_at(i);
			if (cb.is_valid()) {
				cb.call_deferred(Ref<Resource>());
			}
		}
		// THREAD_LOAD_IN_PROGRESS: keep polling next frame.
	}
}

void ResourceManager::_ensure_polling() {
	if (polling_connected) {
		return;
	}
	SceneTree *st = SceneTree::get_singleton();
	if (!st) {
		return;
	}
	st->connect("process_frame", callable_mp(this, &ResourceManager::_poll));
	polling_connected = true;
}

void ResourceManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_handle", "path"), &ResourceManager::get_handle);
	ClassDB::bind_method(D_METHOD("load_sync", "path"), &ResourceManager::load_sync);
	ClassDB::bind_method(D_METHOD("load_async", "path", "callback", "priority"), &ResourceManager::load_async, DEFVAL(Callable()), DEFVAL(0));
}

ResourceManager::ResourceManager() {
}

ResourceManager::~ResourceManager() {
}
