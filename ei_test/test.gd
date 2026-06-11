extends SceneTree
# P2 test — verify that EIAction resource can be created in memory, mutated,
# saved as .tres, reloaded, and that its value_type / default_modifiers /
# description properties round-trip cleanly through the inspector binding.
#
# Run: godot --headless -s test.gd

const SAVE_PATH := "user://test_ia_jump.tres"

func _init() -> void:
	print("[ei_test/p2] === EIAction .tres round-trip test ===")

	#1. ClassDB sanity.
	assert(ClassDB.class_exists("EIAction"), "EIAction must be registered with ClassDB")
	assert(ClassDB.class_exists("EIModifier"), "EIModifier must be registered with ClassDB")
	var parent := ClassDB.get_parent_class("EIAction")
	print("[ei_test/p2] EIAction parent class = ", parent)
	assert(parent == "Resource", "EIAction inherits Resource")

	var mod_parent := ClassDB.get_parent_class("EIModifier")
	print("[ei_test/p2] EIModifier parent class = ", mod_parent)
	assert(mod_parent == "Resource", "EIModifier inherits Resource")

	#2. Instantiate from GDScript and mutate.
	var ia = ClassDB.instantiate("EIAction")
	assert(ia != null, "instantiate('EIAction') must succeed")
	print("[ei_test/p2] instantiated: ", ia)

	# Initial state.
	assert(ia.value_type ==0, "default value_type is BOOL (0)")
	assert(ia.default_modifiers is Array, "default_modifiers is an Array")
	assert(ia.default_modifiers.size() ==0, "default_modifiers starts empty")
	assert(ia.description == "", "default description is empty")

	# Mutate.
	ia.value_type =2 # AXIS2D
	ia.description = "Two-axis movement (WASD)"
	var mods: Array = ia.default_modifiers
	# P3 will let us construct real modifiers; in P2 we just verify the
	# TypedArray<EIModifier> slot accepts an empty array and round-trips.
	print("[ei_test/p2] mods.size() after touch = ", mods.size())

	#3. Save as .tres.
	var save_err := ResourceSaver.save(ia, SAVE_PATH)
	print("[ei_test/p2] ResourceSaver.save err = ", save_err)
	assert(save_err == OK, "save must succeed")

	#4. Reload from .tres.
	var loaded := load(SAVE_PATH)
	assert(loaded != null, "load must return a non-null resource")
	assert(loaded.get_class() == "EIAction", "loaded class must be EIAction (round-trip)")

	#5. Verify fields round-tripped.
	assert(loaded.value_type ==2, "value_type round-trips (AXIS2D=2)")
	assert(loaded.description == "Two-axis movement (WASD)", "description round-trips")
	assert(loaded.default_modifiers.size() ==0, "default_modifiers size round-trips (empty)")

	#6. Inspector-style setters via bound methods (defensive: simulate a
	# GDScript caller doing ia.set_value_type(...) etc).
	loaded.set_value_type(1) # AXIS1D
	assert(loaded.get_value_type() ==1, "set_value_type/get_value_type bound methods work")
	loaded.set_description("updated via method")
	assert(loaded.get_description() == "updated via method", "set/get_description bound methods work")

	print("[ei_test/p2] === ALL ASSERTIONS PASSED ===")
	quit()
