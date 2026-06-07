/**************************************************************************/
/*  auto_activity_loader.h                                                */
/**************************************************************************/
#pragma once

#include "activity_loader.h"

// Default activity loader. Resolves an action name to a .tscn file by
// scanning a configurable base directory using a fixed naming convention.
//
// Search order for action "foo" with base_path "res://activities":
//   1. res://activities/foo/foo_activity.tscn   (sub-dir + _activity suffix)
//   2. res://activities/foo/foo.tscn            (sub-dir, bare name)
//   3. res://activities/foo_activity.tscn       (flat + _activity suffix)
//   4. res://activities/foo.tscn                (flat, bare name)
//
// Slash-separated actions like "game/battle" are supported:
//   dir-part  = "game/"
//   name-part = "battle"
//   → res://activities/game/battle/battle_activity.tscn  (etc.)
//
// Set on ActivityManager via:
//   var loader = AutoActivityLoader.new()
//   loader.base_path = "res://screens"
//   get_activity_manager().set_loader(loader)
class AutoActivityLoader : public ActivityLoader {
	GDCLASS(AutoActivityLoader, ActivityLoader);

	String base_path = "res://activities";

protected:
	static void _bind_methods();

public:
	void set_base_path(const String &p_path);
	String get_base_path() const;

	virtual String resolve(const String &p_action) const override;
};

