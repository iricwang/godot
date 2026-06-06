/**************************************************************************/
/*  resource_manager.h                                                    */
/**************************************************************************/
#pragma once

#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/callable.h"

#include "resource_handle.h"

// Resource manager owned by Application. Provides synchronous and asynchronous loading on top of Godot's
// ResourceLoader. Async loading uses load_threaded_request + per-frame polling (driven by SceneTree's
// process_frame signal). On platforms without threads (e.g. single-threaded Web export) it falls back to a
// synchronous load so it never hangs.
class ResourceManager : public Object {
	GDCLASS(ResourceManager, Object);

	HashMap<StringName, Ref<ResourceHandle>> handles;

	struct AsyncTask {
		Ref<ResourceHandle> handle;
		Callable callback;
	};
	Vector<AsyncTask> loading;
	bool polling_connected = false;

	void _ensure_polling();

protected:
	static void _bind_methods();

public:
	Ref<ResourceHandle> get_handle(const String &p_path);
	Ref<Resource> load_sync(const String &p_path);
	void load_async(const String &p_path, const Callable &p_callback = Callable(), int p_priority = 0);

	// Per-frame poll of in-flight async loads. Connected to SceneTree::process_frame.
	void _poll();

	ResourceManager();
	~ResourceManager();
};
