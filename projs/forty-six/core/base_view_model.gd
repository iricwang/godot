extends ViewModel
## BaseViewModel — GDScript wrapper around the C++ ViewModel that lets you
## write [code]self.foo = 42[/code] instead of [code]set_property(&"foo", 42)[/code].
##
## Every assignment funnels through ObservableProperty, so any
## [BindingEngine] subscriber (Label.text, Range.value, custom view) updates
## automatically the same frame.
##
## Usage:
##   class_name MyVM
##   extends "res://core/base_view_model.gd"
##
##   func _init() -> void:
##       self.title = "Hello"     # creates the ObservableProperty on first set
##       self.score = 0
##
## Tip — batch updates skip intermediate signal emits:
##   vm.begin_bulk_update()
##   for i in 100: vm.score = i
##   vm.end_bulk_update()


func _set(property: StringName, value) -> bool:
	set_property(property, value)
	return true


func _get(property: StringName):
	return get_value(property)
