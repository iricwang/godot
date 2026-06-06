## Why

The Lua module currently provides a usable proof-of-concept integration, but script execution, Godot object access, Variant conversion, and tests are not stable enough for production use. This change hardens the module by making Lua script lifecycle behavior deterministic, improving runtime performance, expanding type interoperability, and adding regression coverage for the fixed behaviors.

## What Changes

- Add cached Lua chunk execution so `LuaScript` parses source once during reload and reuses the compiled chunk for instance creation where safe.
- Improve Lua instance lifecycle so `_init`, `_ready`, `_process`, `_physics_process`, and object fallback access behave consistently.
- Expand Variant interoperability for additional Godot types such as `StringName`, `NodePath`, `Quaternion`, `Basis`, `Transform2D`, `Transform3D`, `AABB`, `Plane`, `PackedInt64Array`, `PackedFloat64Array`, and `PackedVector4Array` where practical.
- Add optional per-script environment isolation to reduce accidental global state sharing between Lua scripts while preserving access to Godot bindings and standard Lua libraries.
- Improve Godot object binding stability, including method/property forwarding, ObjectID handling, callable lifetime safety, and reduced repeated closure/class table allocation.
- Add regression tests covering resource type reporting, `_init` execution, owner method fallback (`self:add_child(...)`), `Rect2` round-trip conversion, additional Variant conversions, and cached script instantiation.
- Keep `.lua` script format and existing class-table convention backward compatible.

## Capabilities

### New Capabilities
- `lua-script-runtime`: Covers Lua script loading, lifecycle callback execution, cached chunk reuse, instance environment behavior, and owner object access from Lua scripts.
- `lua-variant-interop`: Covers conversion behavior between Godot `Variant` values and Lua values, including additional engine math/path/name/packed-array types.
- `lua-godot-binding`: Covers Lua access to Godot classes, objects, properties, methods, and callable wrappers with stable lifetime and caching behavior.
- `lua-module-tests`: Covers regression and integration tests for the Lua module behaviors changed by this work.

### Modified Capabilities

None. There are no existing OpenSpec capabilities for the Lua module in `openspec/specs/`.

## Impact

- Affected code: `modules/lua/lua_language.*`, `modules/lua/lua_script.*`, `modules/lua/lua_script_instance.*`, `modules/lua/lua_variant_converter.*`, `modules/lua/lua_resource_loader.*`, `modules/lua/tests/test_lua.*`, and `modules/lua/SCsub`.
- Runtime behavior: Lua scripts gain more predictable lifecycle execution, better owner object access, broader Variant support, and improved performance from cached compilation and binding caches.
- Compatibility: Existing `.lua` scripts using `return { _base = "Node", ... }` remain supported. Global state leakage may be reduced by sandboxing, but core globals and Godot bindings remain accessible.
- Dependencies: No new external dependency. Existing bundled Lua 5.4.7 remains in use.
