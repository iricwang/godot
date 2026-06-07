/**************************************************************************/
/*  activity_loader.h                                                     */
/**************************************************************************/
#pragma once

#include "core/object/gdvirtual.gen.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

// Abstract activity loader. Maps an action name to a .tscn scene path.
//
// Subclass in C++ or GDScript and pass to ActivityManager::set_loader()
// to implement custom resolution logic (database, manifest file, etc.).
//
// Return an empty String when the action cannot be resolved.
class ActivityLoader : public RefCounted {
	GDCLASS(ActivityLoader, RefCounted);

protected:
	static void _bind_methods();

	// GDScript override: return the scene path for p_action, or "" if not found.
	GDVIRTUAL1RC(String, _resolve, String)

public:
	// Called by ActivityManager when the manual registry has no entry.
	// Base implementation delegates to the GDScript virtual.
	virtual String resolve(const String &p_action) const;
};

