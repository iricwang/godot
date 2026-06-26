extends SceneTree
## verify_scene_activity.gd — Headless smoke test for Intent.FLAG_SCENE.
##
## Run: bin/godot.windows.editor.dev.x86_64.console.exe \
##       --headless --path projs/forty-six -s verify_scene_activity.gd
##
## Builds a stack like:
##   [MainMenu]                                  ← bottom (PAUSED+visible? no, PAUSED+hidden)
##   [Game] + Dialog{PauseDialog}                ← top normally
##
## Then pushes a SceneActivity (the "game_over" Activity used as a scene):
##   * Asserts the stack grew to 3 entries.
##   * Asserts MainMenu is STOPPED, Game is STOPPED, PauseDialog is hidden+paused.
##   * Asserts the SceneActivity is RESUMED+visible.
##   * Pops the SceneActivity via finish() → asserts the previous top resumed
##     and the dialog is visible+resumed again.

const SCENE_TARGET := "game_over"


func _initialize() -> void:
	print("[verify_scene] === boot ===")
	var packed: PackedScene = load("res://entry.tscn")
	assert(packed != null, "entry.tscn failed to load")
	var entry: Control = packed.instantiate()
	root.add_child(entry)
	await process_frame
	await process_frame

	var app := entry.get_node("Application") as Application
	assert(app != null)
	var am := app.get_activity_manager()

	# Push Game on top of MainMenu (already auto-pushed by entry boot).
	var game_p: ActivityProxy = app.start_activity(Intent.create("game", Intent.FLAG_LOAD_SYNC))
	assert(game_p.is_ready(), "Game must be READY synchronously with FLAG_LOAD_SYNC")
	assert(am.get_stack_size() == 2, "stack should be [MainMenu, Game]")
	var game: Activity = game_p.get_activity()
	assert(game.visible == true)
	print("[verify_scene] ok: Game pushed (stack=2, Game visible)")

	# Open a dialog under Game (the normal owner-bound flow).
	var dlg_p: DialogProxy = game.show_dialog(Intent.create("pause_dialog"))
	for i in 30:
		if dlg_p.is_ready() or dlg_p.is_terminal():
			break
		await process_frame
	assert(dlg_p.is_ready(), "PauseDialog must be READY")
	var dlg: Dialog = dlg_p.get_dialog()
	assert(dlg.visible == true)
	assert(dlg.is_paused() == false, "fresh dialog is not paused")
	assert(am.get_dialog_count() == 1)
	print("[verify_scene] ok: PauseDialog presented (visible=true, paused=false)")

	# Now the punchline: push a SCENE activity on top.
	var scene_p: ActivityProxy = app.start_activity(Intent.create(SCENE_TARGET,
			Intent.FLAG_SCENE | Intent.FLAG_LOAD_SYNC, {"score": 1.0, "target": 46, "new_record": false}))
	assert(scene_p.is_ready(), "Scene activity must be READY (LOAD_SYNC)")
	assert(am.get_stack_size() == 3, "stack should be [MainMenu, Game, Scene]")
	var scene: Activity = scene_p.get_activity()
	assert(scene.visible == true, "Scene activity must be visible")
	assert(scene_p.get_lifecycle_stage() == ActivityProxy.LIFECYCLE_RESUMED,
			"Scene activity must be RESUMED")
	# Curtain effects on everything below:
	assert(game.visible == false, "Game must be hidden under scene curtain")
	assert(game_p.get_lifecycle_stage() == ActivityProxy.LIFECYCLE_STOPPED,
			"Game must be STOPPED, got %d" % game_p.get_lifecycle_stage())
	var menu_p: ActivityProxy = am.get_stack_proxy(0)
	var menu: Activity = menu_p.get_activity()
	assert(menu.visible == false, "MainMenu must be hidden under scene curtain")
	assert(menu_p.get_lifecycle_stage() == ActivityProxy.LIFECYCLE_STOPPED,
			"MainMenu must be STOPPED")
	assert(dlg.visible == false, "PauseDialog must be hidden under scene curtain")
	assert(dlg.is_paused() == true, "PauseDialog must be paused under scene curtain")
	print("[verify_scene] ok: scene-curtain draped — Game/MainMenu STOPPED+hidden, Dialog paused+hidden")

	# Pop the scene. Expect: previous top (Game) resumes + visible; dialog unhides + resumes.
	scene.finish()
	await process_frame
	await process_frame
	assert(am.get_stack_size() == 2, "stack should be back to [MainMenu, Game]")
	assert(game.visible == true, "Game must be visible again after scene pop")
	assert(game_p.get_lifecycle_stage() == ActivityProxy.LIFECYCLE_RESUMED,
			"Game must be RESUMED again, got %d" % game_p.get_lifecycle_stage())
	assert(dlg.visible == true, "PauseDialog must be visible again")
	assert(dlg.is_paused() == false, "PauseDialog must be unpaused again")
	# MainMenu is below Game so it stays hidden+stopped (no change).
	assert(menu.visible == false)
	# Timer-state preservation: Game was NOT running (we never clicked Click)
	# AND the PauseDialog was already up before the scene was pushed, so the
	# user's "paused" state must survive the curtain lift. _on_resume must
	# NOT have unconditionally set _running = true.
	assert(game._running == false,
			"Game._running must stay false (was paused under dialog); got %s" % game._running)
	print("[verify_scene] ok: scene-curtain lifted — Game RESUMED, Dialog visible+unpaused, _running preserved=false, MainMenu still hidden")

	# Inverse case: dismiss the dialog and start the timer, push+pop a scene,
	# and confirm _running is restored to true.
	dlg.dismiss()
	await process_frame
	await process_frame
	# Drive the timer on by faking a click (game._on_click without going
	# through EI — we just want the side effect of setting _running).
	game._on_click()
	assert(game._running == true, "Click should start the timer")
	var pre_score: int = int(game.vm.count)

	var scene3_p: ActivityProxy = app.start_activity(Intent.create(SCENE_TARGET,
			Intent.FLAG_SCENE | Intent.FLAG_LOAD_SYNC, {"score": 0.5, "target": 46, "new_record": false}))
	assert(scene3_p.is_ready())
	assert(game._running == false, "Game._running must be false while under the scene curtain")
	scene3_p.get_activity().finish()
	await process_frame
	await process_frame
	assert(game._running == true,
			"Game._running must restore to true after the curtain lifts; got %s" % game._running)
	assert(int(game.vm.count) == pre_score, "Click count should not have changed under the curtain")
	print("[verify_scene] ok: _running save/restore preserves both paused and running states")

	# Sanity: try pushing two scenes nested. We re-open a fresh dialog first
	# because the inverse case above dismissed `dlg`, and the nested scenes
	# assertion needs a live dialog to observe.
	var dlg2_p: DialogProxy = game.show_dialog(Intent.create("pause_dialog"))
	for i in 30:
		if dlg2_p.is_ready() or dlg2_p.is_terminal():
			break
		await process_frame
	assert(dlg2_p.is_ready(), "fresh PauseDialog for nested test must be READY")
	var dlg2: Dialog = dlg2_p.get_dialog()
	assert(dlg2.visible == true and dlg2.is_paused() == false)

	var scene1_p: ActivityProxy = app.start_activity(Intent.create(SCENE_TARGET,
			Intent.FLAG_SCENE | Intent.FLAG_LOAD_SYNC, {"score": 1.0, "target": 46, "new_record": false}))
	assert(scene1_p.is_ready())
	assert(dlg2.visible == false and dlg2.is_paused() == true,
			"dialog must be hidden+paused under outer scene")
	var scene2_p: ActivityProxy = app.start_activity(Intent.create(SCENE_TARGET,
			Intent.FLAG_SCENE | Intent.FLAG_LOAD_SYNC, {"score": 2.0, "target": 46, "new_record": false}))
	assert(scene2_p.is_ready())
	# Still hidden under inner scene.
	assert(dlg2.visible == false and dlg2.is_paused() == true)
	# Pop inner scene -- outer is still on top so dialog stays under curtain.
	scene2_p.get_activity().finish()
	await process_frame
	await process_frame
	assert(dlg2.visible == false and dlg2.is_paused() == true,
			"dialog must stay hidden+paused while outer scene still on top")
	# Pop outer scene -- now dialog lifts.
	scene1_p.get_activity().finish()
	await process_frame
	await process_frame
	assert(dlg2.visible == true and dlg2.is_paused() == false,
			"dialog must lift after outer scene pop")
	print("[verify_scene] ok: nested scenes (inner pop stays curtained, outer pop lifts)")

	# Cleanup.
	app.shutdown()
	print("[verify_scene] === ALL ASSERTIONS PASSED ===")
	quit(0)
