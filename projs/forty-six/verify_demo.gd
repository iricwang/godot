extends SceneTree
## verify_demo.gd — Headless smoke test for the FortySix demo.
##
## Run: bin/godot.windows.editor.dev.x86_64.console.exe \
##       --headless --path projs/forty-six -s verify_demo.gd
##
## Walks through every game_framework subsystem and asserts the expected
## state transitions. Prints "=== ALL ASSERTIONS PASSED ===" on success.
##
## NOTE: Because we extend SceneTree directly (the -s flag), we can't easily
## wait between frames for transitions to complete. We use process_frame
## awaits as needed and rely on FLAG_LOAD_SYNC for synchronous attach.

const GameStateService := preload("res://services/game_state_service.gd")


func _initialize() -> void:
	print("[verify_demo] === boot ===")
	# We extend SceneTree directly via -s, so the project's main_scene is NOT
	# auto-loaded. Load entry.tscn manually and let it run a frame.
	var packed: PackedScene = load("res://entry.tscn")
	assert(packed != null, "entry.tscn failed to load")
	var entry: Control = packed.instantiate()
	root.add_child(entry)
	# Two frames: one for _ready, one for any deferred init (Application starts services etc).
	await process_frame
	await process_frame

	# 1. Locate the UIRoot and the Application child it created in entry.gd.
	var ui_root: Node = entry
	assert(ui_root != null, "entry instance is null")
	assert(ui_root.name == "UIRoot", "expected UIRoot, got %s" % ui_root.name)
	var app := ui_root.get_node_or_null("Application") as Application
	assert(app != null, "Application child not found under UIRoot")
	print("[verify_demo] ok: UIRoot + Application present")

	# 2. ServiceRegistry — both services registered, fetchable.
	var sr := app.get_service_registry()
	assert(sr.has_service(&"game_state"), "game_state service missing")
	assert(sr.has_service(&"audio"), "audio service missing")
	var gs = sr.get_service(&"game_state")
	assert(gs is GameStateService, "game_state is not a GameStateService instance")
	assert(gs.get_target_clicks() == 46, "default target should be 46 (Normal)")
	print("[verify_demo] ok: services registered (target=%d)" % gs.get_target_clicks())

	# 3. ActivityManager — MainMenu booted synchronously via FLAG_LOAD_SYNC.
	var am := app.get_activity_manager()
	assert(am.get_stack_size() == 1, "stack should hold MainMenu (size=1), got %d" % am.get_stack_size())
	var top_proxy: ActivityProxy = am.get_current_activity_proxy()
	assert(top_proxy != null, "current activity proxy is null")
	assert(top_proxy.is_ready(), "MainMenu proxy not READY (state=%d)" % top_proxy.get_state())
	assert(top_proxy.get_action() == "main_menu", "top action should be main_menu, got %s" % top_proxy.get_action())
	print("[verify_demo] ok: MainMenu booted via FLAG_LOAD_SYNC (proxy=%s)" % top_proxy)

	# 4. Navigate MainMenu → Game (async path, no FLAG_LOAD_SYNC).
	var game_proxy: ActivityProxy = app.start_activity(Intent.create("game"))
	assert(game_proxy != null)
	# Drain a few frames for the threaded loader.
	for i in 30:
		if game_proxy.is_ready() or game_proxy.is_terminal():
			break
		await process_frame
	assert(game_proxy.is_ready(), "Game activity failed to reach READY (state=%d)" % game_proxy.get_state())
	assert(am.get_stack_size() == 2)
	assert(am.get_current_activity_proxy() == game_proxy)
	print("[verify_demo] ok: Game activity attached async; stack_size=%d" % am.get_stack_size())

	# 5. Open Pause dialog from Game.
	var game_node: Activity = game_proxy.get_activity()
	var dlg_proxy: DialogProxy = game_node.show_dialog(Intent.create("pause_dialog"))
	for i in 30:
		if dlg_proxy.is_ready() or dlg_proxy.is_terminal():
			break
		await process_frame
	assert(dlg_proxy.is_ready(), "Pause dialog never reached READY (state=%d)" % dlg_proxy.get_state())
	assert(am.get_dialog_count() == 1)
	print("[verify_demo] ok: Pause dialog presented; dialog_count=%d" % am.get_dialog_count())

	# 6. Dismiss the dialog → state transitions to FINISHED.
	dlg_proxy.get_dialog().dismiss()
	await process_frame
	assert(am.get_dialog_count() == 0)
	assert(dlg_proxy.get_state() == ContextProxy.STATE_FINISHED, \
		"dismissed dialog should be FINISHED, got %d" % dlg_proxy.get_state())
	print("[verify_demo] ok: dialog dismissed -> STATE_FINISHED")

	# 7. Push GameOver with extras and FLAG_NO_HISTORY.
	var go_proxy: ActivityProxy = game_node.start_activity(Intent.create("game_over", \
			Intent.FLAG_NO_HISTORY, {"score": 3.14, "new_record": true, "target": 46}))
	for i in 30:
		if go_proxy.is_ready() or go_proxy.is_terminal():
			break
		await process_frame
	assert(go_proxy.is_ready(), "GameOver not READY (state=%d)" % go_proxy.get_state())
	# Game is still in the stack until GameOver finishes; we are at top now.
	assert(am.get_current_activity_proxy() == go_proxy)
	# The previous Game activity should have its no_history flag respected when
	# GameOver navigates away. Read intent.extras roundtrip on GameOver.
	var go_intent: Intent = go_proxy.get_intent()
	assert(abs(float(go_intent.get_extras().get("score", 0)) - 3.14) < 0.001)
	assert(bool(go_intent.get_extras().get("new_record", false)))
	print("[verify_demo] ok: GameOver received extras (score=%.2f, new_record=true)" \
			% float(go_intent.get_extras()["score"]))

	# 8. Toast SERIAL queue. Push 3 toasts owned by GameOver and verify the queue state.
	var go_node: Activity = go_proxy.get_activity()
	var tp1: ToastProxy = go_node.show_toast(Toast.make_text("milestone 1", 0.05))
	var tp2: ToastProxy = go_node.show_toast(Toast.make_text("milestone 2", 0.05))
	var tp3: ToastProxy = go_node.show_toast(Toast.make_text("milestone 3", 0.05))
	assert(tp1.is_ready(), "first toast should be ready (pumped immediately)")
	assert(tp2.is_pending() or tp2.is_ready(), "tp2 should be PENDING or already pumped")
	assert(tp3.is_pending() or tp3.is_ready(), "tp3 should be PENDING or already pumped")
	print("[verify_demo] ok: SERIAL toast queue: tp1=READY, tp2/tp3 PENDING")

	# 9. FLAG_NEW_CLEAR navigation -- everything drains, MainMenu pushed fresh.
	var menu_again: ActivityProxy = go_node.start_activity( \
			Intent.create("main_menu", Intent.FLAG_NEW_CLEAR | Intent.FLAG_LOAD_SYNC))
	assert(menu_again.is_ready(), "FLAG_LOAD_SYNC menu push should be READY synchronously")
	assert(am.get_stack_size() == 1, "stack should hold only the new MainMenu, got %d" % am.get_stack_size())
	assert(am.get_current_activity_proxy().get_action() == "main_menu")
	print("[verify_demo] ok: FLAG_NEW_CLEAR drained the stack down to MainMenu")

	# 10. Record a run via the service and confirm best_time is set.
	var was_new: bool = gs.record_run(2.5)
	assert(was_new == true)
	assert(abs(gs.best_time - 2.5) < 0.001)
	assert(gs.total_runs >= 1)
	print("[verify_demo] ok: GameStateService.record_run(2.5) → new best, total_runs=%d" \
			% gs.total_runs)

	# 11. enhanced_input — simulate a Space-press into the autoload's _input
	# forward and verify the GameActivity's IA_Click triggers a vm.count bump.
	if Engine.has_singleton(&"EISubsystem"):
		# Build a fresh game stack so we can poke at the activity instance.
		var game_p: ActivityProxy = app.start_activity(Intent.create("game", Intent.FLAG_LOAD_SYNC))
		assert(game_p.is_ready(), "Game activity should be READY synchronously with FLAG_LOAD_SYNC")
		var ga: Activity = game_p.get_activity()
		assert(ga != null)
		var initial_count := int(ga.vm.count)
		# Forge a Space-down event and inject it through the EI subsystem the
		# same way the autoload's _input would. Both keycode AND physical_keycode
		# are set because the dispatcher's sampler matches on both paths.
		var ev := InputEventKey.new()
		ev.physical_keycode = KEY_SPACE
		ev.keycode = KEY_SPACE
		ev.pressed = true
		Engine.get_singleton(&"EISubsystem").inject_input(ev)
		# Process_frame so any deferred call_deferred from the binding handler runs.
		await process_frame
		await process_frame
		var new_count := int(ga.vm.count)
		assert(new_count == initial_count + 1, \
				"Space key should bump count via EI; got %d (was %d)" % [new_count, initial_count])
		print("[verify_demo] ok: enhanced_input Space → IA_Click triggered count bump (%d→%d)" \
				% [initial_count, new_count])
	else:
		print("[verify_demo] note: EISubsystem absent — keyboard-shortcut step skipped")

	# 12. Shutdown — Application is a child of UIRoot; on quit() NOTIFICATION_EXIT_TREE
	# will call shutdown() and cleanup_all() drains anything still live. We do this
	# explicitly here so the test exits cleanly. After shutdown the manager pointer
	# is invalidated, so we don't probe it again — just trust the green run.
	app.shutdown()
	print("[verify_demo] ok: shutdown returned cleanly")

	print("=== ALL ASSERTIONS PASSED ===")
	quit(0)
