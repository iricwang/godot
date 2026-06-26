# verify_proxy.gd — Headless smoke test for the ContextProxy async pipeline.
#
# Run with:
#   bin\godot.windows.editor.dev.x86_64.console.exe --headless --path plinko_game -s verify_proxy.gd
#
# Asserts:
#   * ActivityProxy / DialogProxy / ToastProxy are registered and reachable from GDScript.
#   * ContextProxy.State enum constants match the documented numeric values.
#   * Intent.FLAG_LOAD_SYNC bit is exposed and the value matches the doc.
#   * Application + ActivityManager round-trips and exposes proxy accessors.
#   * Starting an Activity for an unregistered + unresolvable action returns a FAILED proxy
#     (sync path), without leaving anything in the stack.

extends SceneTree

const _EXIT_OK = 0
const _EXIT_FAIL = 1


func _init() -> void:
	print("[verify_proxy] === boot ===")

	# Class registration.
	assert(ClassDB.class_exists("ContextProxy"), "ContextProxy is not registered")
	assert(ClassDB.class_exists("ActivityProxy"), "ActivityProxy is not registered")
	assert(ClassDB.class_exists("DialogProxy"), "DialogProxy is not registered")
	assert(ClassDB.class_exists("ToastProxy"), "ToastProxy is not registered")
	print("[verify_proxy] ok: proxy classes registered")

	# Enum values lock-in.
	assert(ContextProxy.STATE_PENDING == 0, "STATE_PENDING != 0")
	assert(ContextProxy.STATE_LOADING == 1, "STATE_LOADING != 1")
	assert(ContextProxy.STATE_READY == 2, "STATE_READY != 2")
	assert(ContextProxy.STATE_FAILED == 3, "STATE_FAILED != 3")
	assert(ContextProxy.STATE_CANCELLED == 4, "STATE_CANCELLED != 4")
	assert(ContextProxy.STATE_FINISHED == 5, "STATE_FINISHED != 5")
	print("[verify_proxy] ok: ContextProxy.State enum values stable")

	# Intent FLAG_LOAD_SYNC.
	assert(Intent.FLAG_LOAD_SYNC == 32, "FLAG_LOAD_SYNC != 32")
	var it: Intent = Intent.create("__no_such__", Intent.FLAG_LOAD_SYNC)
	assert(it.has_flag(Intent.FLAG_LOAD_SYNC), "FLAG_LOAD_SYNC not set")
	print("[verify_proxy] ok: Intent.FLAG_LOAD_SYNC == 32")

	# End-to-end Application/ActivityManager FAILED path.
	var root: Control = Control.new()
	root.name = "VerifyRoot"
	get_root().add_child(root)

	var app: Application = Application.new()
	app.name = "VerifyApp"
	get_root().add_child(app)
	app.initialize(root)

	# Replace the default loader with one that always returns "" so we get a clean failure.
	# We can't easily install a custom GDScript loader from here, so instead we register a
	# missing scene path and rely on the synchronous load failing.
	app.register_activity("__verify_missing__", "res://__definitely_does_not_exist__.tscn")

	var fail_intent: Intent = Intent.create("__verify_missing__", Intent.FLAG_LOAD_SYNC)
	var proxy: ActivityProxy = app.get_activity_manager().start_activity(fail_intent)
	assert(proxy != null, "start_activity returned null")
	assert(proxy.get_state() == ContextProxy.STATE_FAILED, "expected STATE_FAILED, got %d" % proxy.get_state())
	assert(app.get_activity_manager().get_stack_size() == 0, "stack should be empty after failure")
	print("[verify_proxy] ok: FLAG_LOAD_SYNC + missing scene -> STATE_FAILED + rollback")

	# get_current_activity_proxy returns null on an empty stack.
	assert(app.get_activity_manager().get_current_activity_proxy() == null, "empty stack should give null proxy")
	print("[verify_proxy] ok: get_current_activity_proxy null on empty stack")

	app.shutdown()
	app.queue_free()
	root.queue_free()

	print("=== ALL ASSERTIONS PASSED ===")
	quit(_EXIT_OK)
