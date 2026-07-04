# Godot Adapter Framework

Reusable Godot-native adapter framework code.

## Dependency Policy

- This folder may depend on Godot headers and `godot-cpp`.
- The shared framework repo does not vendor `godot-cpp` by default.
- Each game repo should provide its own `third_party/godot-cpp/` or an equivalent configured dependency path.

## Expected Integration

A game repo should vendor this framework under:

```text
shared_framework/
  engines/
    godot/
```

Then the game build should point the Godot adapter target at the game's own `godot-cpp` checkout.
