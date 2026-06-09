/**************************************************************************/
/*  observable_property.h                                                 */
/**************************************************************************/
#pragma once

#include "core/object/ref_counted.h"

// A single observable value. Change notification is delivered through the "value_changed" signal, so subscribers
// are managed by Godot's own connection system: when a subscriber Object is freed, its connection is removed
// automatically (no dangling callbacks, no manual cleanup needed).
//
// Batch update support: use begin_bulk_update() / end_bulk_update() to suppress intermediate notifications.
// Only the final value change will trigger "value_changed" after end_bulk_update().
class ObservableProperty : public RefCounted {
	GDCLASS(ObservableProperty, RefCounted);

	Variant value;
	bool _bulk_update = false;
	// Snapshot of value at the moment begin_bulk_update() was called. We use this in
	// end_bulk_update() to decide whether the final value differs from what the
	// subscribers last saw, and only fire value_changed in that case.
	Variant _bulk_start_value;

protected:
	static void _bind_methods();

public:
	void set_value(const Variant &p_value);
	Variant get_value() const;

	// Batch update: suppress intermediate notifications
	void begin_bulk_update();
	void end_bulk_update();
	bool is_in_bulk_update() const { return _bulk_update; }

	void subscribe(const Callable &p_callable); // connects to "value_changed"
	void unsubscribe(const Callable &p_callable);
};
