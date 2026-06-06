/**************************************************************************/
/*  observable_property.h                                                 */
/**************************************************************************/
#pragma once

#include "core/object/ref_counted.h"

// A single observable value. Change notification is delivered through the "value_changed" signal, so subscribers
// are managed by Godot's own connection system: when a subscriber Object is freed, its connection is removed
// automatically (no dangling callbacks, no manual cleanup needed).
class ObservableProperty : public RefCounted {
	GDCLASS(ObservableProperty, RefCounted);

	Variant value;

protected:
	static void _bind_methods();

public:
	void set_value(const Variant &p_value);
	Variant get_value() const;

	void subscribe(const Callable &p_callable); // connects to "value_changed"
	void unsubscribe(const Callable &p_callable);
};
