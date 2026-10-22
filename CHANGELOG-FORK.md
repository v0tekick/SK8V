# SK8V Story Mode fork changelog

## 1.1.9.3

- Object Dropper placements now feed their exact collision geometry through the same lip finder as Los Santos and register qualifying edges as Skate grind splines. Normal grind admission and scoring can then apply to ledges while ramps remain transitions.
- Added VHS 2 with the existing VHS crop, scan, tint and grain, minus moving tracking strips and most RGB separation.
- Replay Y now cycles follow, free and phone operator cameras. Phone mode creates a local skater ped with an attached phone prop, walks with the left stick, aims with the right stick and adds light handheld sway.
- Added five Replay edit looks: Super 8, Sunset, Night Spot, Bleach Bypass and Early Digital. Ctrl+Shift+E cycles all eleven looks.

## 1.1.9.2.1

- Restored GTA's native vehicle aim, fire and weapon switching by disabling the fork's LB weapon selector in vehicles and during vehicle entry. An open selector is cancelled on entering a vehicle without changing the selected weapon.
- Blocked the instant Skate controller chord and skateboard weapon selection while in or entering any vehicle. Both remain available immediately after exiting, including while airborne or in free fall.
- Allowed instant Skate entry during an active cutscene. Once the runtime and area are ready, entry ends the cutscene and hands the restored player character to Skate. Mission scripts may not continue normally after an interrupted cinematic.

## 1.1.9.2

- Added replay recording windows of 10, 15, 30, 45, 60, 90 and 120 seconds, selected with Ctrl+Shift+D during Replay. The rolling recorder retains the maximum two minutes, so a longer window is available immediately after changing it.
- Shaped free camera left-stick travel for finer low-speed movement near the center while preserving full-speed movement at the edge.
- Extended Replay FOV control to 5°–160° for every camera effect.
- Removed the oversized, noncolliding quarter pipe and unavailable rail/fence entries. Added benches, picnic tables, concrete barriers, pallet pads and box stacks with prepared collision templates.

## 1.1.9-story-fork.1

Based on Becaring's SK8V 1.1.9. GTA V Legacy Story Mode only.

- Enter Skate mode from airborne and other GTA character states with a controller chord, carrying the character's momentum into the board simulation.
- Use the fork's owned-weapon wheel outside Skate mode, including in vehicles.
- Use Object Dropper with GTA creator and stunt props.
- Capture a rolling Replay with free camera, camera FOV control, a HUD toggle, a cleaner replay interface, and keyboard camera effects including GoPro, VHS, and doorbell fisheye.
- Show control hints under the radar while the SK8V menu is open.
- Keep the upstream setup wizard and preparation process, with a fork label in the wizard.

The installer builds Skate 3 assets from the player's own disc and GTA data from the player's own GTA V Legacy installation. It does not bundle either game or ScriptHookV.
