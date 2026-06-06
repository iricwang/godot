/**************************************************************************/
/*  resource_handle.h                                                     */
/**************************************************************************/
#pragma once

#include "core/io/resource.h"
#include "core/object/ref_counted.h"

// A handle to a (possibly not-yet-loaded) resource. It is a thin state machine + signals around Godot's own
// resource loading. It deliberately does NOT keep its own reference count: lifetime is handled by Godot's
// Ref<Resource> + ResourceLoader cache, so there is no manual retain/release to get wrong.
class ResourceHandle : public RefCounted {
	GDCLASS(ResourceHandle, RefCounted);

public:
	enum State {
		STATE_UNLOADED,
		STATE_LOADING,
		STATE_LOADED,
		STATE_FAILED,
	};

private:
	String path;
	State state = STATE_UNLOADED;
	Ref<Resource> resource;

protected:
	static void _bind_methods();

public:
	void set_path(const String &p_path);
	String get_path() const;
	State get_state() const;
	bool is_loaded() const;
	Ref<Resource> get_resource() const;

	// Synchronous load. Async loading is driven by ResourceManager, which updates this handle's state.
	Ref<Resource> load_sync();

	// Internal hooks used by ResourceManager (not exposed to script).
	void _set_state(State p_state);
	void _set_resource(const Ref<Resource> &p_resource);
	void _emit_loaded(const Ref<Resource> &p_resource);
	void _emit_failed();
};

VARIANT_ENUM_CAST(ResourceHandle::State);
