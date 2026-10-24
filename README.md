![SK8V Gameplay](media/sk8v.webp)

# SK8V Story Mode Fork

Based on Becaring's SK8V 1.1.9. This installer prepares data from **your own** GTA V Legacy and Skate 3 copies, then installs the Story Mode fork. It does not contain the games or ScriptHookV.

**Skate 3 gameplay. GTA V's open world.**


SK8V brings *Skate 3*'s Flick-It controls, board/skater physics, grinds, manuals, skitching, tricks, combos, bails, scoring, session markers, and Hall of Meat into **GTA V Legacy Story Mode**. Skate's gameplay runs alongside GTA's characters, traffic, weapons, radio, and world interactions. Skate 3’s own audio engine runs inside GTA’s mixer, so wheels sound different on every GTA surface. Things like concrete, asphalt, wood, metal, tile all sound different, and your wheels click over the cracks and seams in the pavement, with almost complete coverage.

## Requirements
- **Controller is Required**
- **SSD is recommended for installation**
- **Windows x64**, **GTA V Legacy 1.0.3889.0**, and **ScriptHookV v3889.0 / 1158.13** with its ASI loader. (https://www.dev-c.com/gtav/scripthookv/)
- **A controller:** Xbox, or PlayStation (DualShock 4 / DualSense, USB or Bluetooth). Keyboard skating is not supported. On a PlayStation pad, Share (or the touchpad) is Back and Options is Start.
- Your own Skate 3 Xbox 360 disc ISO or extracted disc folder (tested: title ID 454108E6, media ID 5C087C2C, no title update)
- About **7 GB free** on your GTA drive for setup; roughly **2 GB** remains afterward.

**Story Mode only.** GTA V Enhanced, GTA Online, and FiveM are not supported.

## Install

1. Extract this fork's release ZIP to a normal folder. Do not run setup from inside the ZIP.
2. Close GTA V and run **`SK8V Setup.cmd`**. (If your GTA installation is in a protected folder (Program Files, etc.) setup will offer to restart as administrator.)
3. Select your Skate 3 disc, choose **Regular** or **Goofy** stance, and press **Install**. Setup detects your GTA V folder when possible.
4. Launch GTA V Legacy **Story Mode with BattlEye disabled**. Select the **skateboard** in GTA's weapon wheel, then press `Y` to mount it.

Setup prepares data from your copies of both games. It can download a local Python environment if needed, and rerunning it resumes interrupted preparation.

Command-line alternative (Python 3.11+, NumPy, Pillow):

```powershell
python tools/prepare.py --skate "<Skate 3.iso or folder>" --gta "<GTA V folder>" --stance Goofy --install --clean
```

(from the extracted release folder; omit --stance for Regular).

## Controls

**All regular skating controls are Skate 3's.** SK8V adds:

| Input | Action |
|---|---|
| GTA weapon wheel → **Skateboard** | Enter Skate mode |
| `Y` / hold `Y` | Mount or dismount / put the board away and return to GTA |
| Keyboard `/` or controller `Back + LB` | Open SK8V menu |
| `Back` / hold `Back` | Equip or holster a weapon / open weapon wheel when armed |
| `LB` / `RB` when armed | Aim / fire |
| Hold `LB` + D-pad left/right, gun holstered | Change radio station (Skate 3's layout) |
| D-pad | Emotes, one per direction (set them in the SK8V menu) |
| D-pad left/right, gun equipped | Change weapon |
| Double-tap `X` on foot near a grabbable wall | GTA mantle, then return to Skate |

Swimming switches to GTA automatically and returns to Skate afterward. **Hall of Meat is on by default** and can be disabled in the SK8V menu, alongside other gameplay settings.

### Fork controls

| Input | Action |
|---|---|
| RS click + D-pad left | Enter Skate immediately, including during a jump, fall or active cutscene; carries the character's momentum |
| Hold LB outside Skate while on foot or airborne | Open the fork's owned-weapon wheel; select Skateboard to enter Skate when the character is out of a vehicle |
| LB while in or entering any vehicle | GTA's normal vehicle aim and fire controls; use GTA's normal vehicle weapon switching |
| RS click + D-pad right while skating | Open Object Dropper |
| RS click + D-pad up while skating | Open the 30-second Replay |
| Y during Replay | Cycle follow, free and phone operator cameras |
| Left/right sticks in phone camera | Move the phone operator from a precise walk to a run at full stick / aim the handheld view |
| RS click + LT/RT during Replay | Adjust camera FOV smoothly from 5° to 160° |
| Ctrl+Shift+D during Replay | Cycle the recording window: 10, 15, 30, 45, 60, 90 or 120 seconds |
| Ctrl+Shift+E during Replay | Cycle 11 effects: clean, GoPro, VHS, film noir, doorbell, VHS 2, Super 8, Sunset, Night Spot, Bleach Bypass and Early Digital |
| Ctrl+Shift+H during Replay | Hide or show all HUD |

Object Dropper offers ramps, benches, picnic tables, concrete barriers, pallet pads and box stacks. These are GTA props with prepared collision templates, as substitutes for Skate 3's full park catalog. Placed props now contribute valid top edges to Skate's grind spline provider, so ledges can trigger normal grind tricks and scoring. Replay retains up to two minutes of skating, with a finer left-stick movement curve near the center in free camera mode. The phone mode adds a local camera operator who holds a phone prop and can walk while filming. It is the fork's own rolling recording, not Rockstar Editor.

### Unable to save settings in the menu?

1. Right-click your game install folder and select Properties.
2. Click the Security tab and click Edit.
3. Click Add, type your specific user account name (or less preferably, Everyone), then click Check Names and OK.
4. Select the newly added user or group, check the box for Full control under the Allow column, then click Apply and OK

### Known Limitations & Future Support

- **Rockstar Editor:** Recording and playback are not currently functional with SK8V. Support is planned for a future update.

- **Teleporting while skating:** Teleporting directly while Skate mode is active may cause the skater to fall through the map. Switch back to GTA control before teleporting, then resume skating at your destination.

- **GTA V Enhanced:** SK8V currently supports GTA V Legacy only. Enhanced Edition compatibility is a long-term goal, but may require significant additional work and is not guaranteed.

## Contributing

This fork is based on [Becaring's original SK8V](https://github.com/becaring/SK8V). Bug reports and feature requests for fork changes belong in this fork's issues.

## Support Me
[<img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" height="40">](https://buymeacoffee.com/becaring) [<img src="https://storage.ko-fi.com/cdn/kofi3.png?v=3" height="36">](https://ko-fi.com/becaring)

If you had fun with SK8V, a donation is NEVER required but always appreciated <3 

## Uninstall

With GTA V closed, remove these from its folder:

- `SkateVLegacy.asi`, `SkateVRuntime.dll`, `SkateVLegacy.ini`, `SkateVLegacy.installed.json`
- `SK8V/`
- `update/x64/dlcpacks/skatev/`

Logs and records are in `%LOCALAPPDATA%\SkateV` and can be deleted separately. ScriptHookV is left untouched.

## Building from Source

Run BOOTSTRAP.bat to build the code, then follow BUILDING.md to package a release. The baked world collision data is copied from a published release.

## License

Original SK8V code is [Apache-2.0](LICENSE); third-party components keep their own licenses and are credited in [NOTICE](NOTICE). Fork changes were developed with OpenAI Codex. SK8V is an unofficial fan project, unaffiliated with EA, Rockstar Games, or Take-Two Interactive. No GTA V or Skate 3 retail assets, executables, keys, or proprietary SDK redistributables are included; supply your own game copies.
