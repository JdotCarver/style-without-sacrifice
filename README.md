# Style Without Sacrifice - Your Transmogrification Wardrobe

Choose how your equipment looks from a dedicated **Wardrobe** tab in The Blood of Dawnwalker. Your equipped items and their stats stay in place.

Browse armour, legwear, gloves/wristbands, footwear and weapon appearances. Wristbands and gauntlets share the **Gloves / Wristbands** category. Switch between collected looks and the full appearance catalog, choose separate day and night outfits, and save three outfit presets. Gauntlets, feet and the sheathed weapon can be hidden. The drawn weapon remains visible.

Open Wardrobe with **End**, or select its tab immediately after **Inventory** in the character menu. Browse a six-column grid alongside a large character preview and compact equipment slots. The page uses the inventory background, game fonts, equipment icons, rarity frames and keyboard/controller glyphs, with gold selection markers and separate save/load dialogs. Smaller category controls and inset action prompts keep the layout close to Inventory, including on ultrawide screens. Mouse and keyboard controls are available alongside controller input routed through the game.

## Installation

- **Vortex:** Install `Style-Without-Sacrifice.zip` through Vortex, enable it and deploy.
- **Manual:** Copy `Data/WardrobeTransmog` into `The Blood of Dawnwalker/Dawnwalker/Binaries/Win64/ue4ss/Mods`, preserving the folder structure.

## Dependencies

Designed for game patch **1.0.5**. Requires [UE4SS for Dawnwalker by Vercadi](https://www.nexusmods.com/thebloodofdawnwalker/mods/18) **1.3 (RC6) or later** with C++ mod support and the native EngineTick, BeginPlay, EndPlay and ProcessLocalScriptFunction callbacks available. Mod Setting Menu **1.0.7.1 or later** provides the settings page. The loader and settings menu are separate downloads.

## Controls

| Action | Keyboard | Controller |
| --- | --- | --- |
| Open Wardrobe | End, configurable | Select the Wardrobe hub tab |
| Change category | A / D | Left / right trigger |
| Select a look | Arrow keys, Enter | D-pad or left stick, A |
| Change page | Page Up / Page Down | Right stick Up / Down |
| Day / night preview | P | Start / Menu |
| Collected / all looks | Y | Right stick click |
| Hide supported slot | F | Left stick click |
| Save / load outfit | T / S | X / Y |
| Reset displayed outfit | R | Hold X |
| Close or cancel | Escape | B |

Wardrobe uses the game's controller input, including while the character menu is paused. It does not require a particular XInput slot or add controller drivers. The game must recognize your controller first. Controller names in this table and the on-screen prompts remain **Xbox-style**, including when using a PlayStation controller; PlayStation symbols are not provided. You can browse items and change pages as soon as Wardrobe opens, without clicking a tile first. If the stick or a trigger was held while entering the tab or reconnecting, release it before navigating.

All actions also have on-screen mouse controls. The action hints use the game's keyboard and Xbox glyphs and follow the active input device. D-pad Left / Right moves one item horizontally; Up / Down moves one row vertically. Tilt the right stick up for Previous or down for Next; the page buttons show matching indicators. Each tilt changes one page, so release the stick before paging again. Moving beyond a page with the D-pad or left stick also continues onto the adjacent page. Hold the left stick to repeat item movement; vertical navigation preserves the column when crossing pages. Changing the previewed day/night set does not change your active equipment loadout. Each page holds up to 36 choices; focus a tile to see its name below the grid. The equipment panel shows the displayed outfit, with the selected category's name underneath. NPC appearances without an inventory image use their category symbol.

Within each category, **Player Outfits** with inventory icons appear first, followed by **NPC Outfits** without item icons. Both groups sort alphabetically by their displayed names, ignoring case. Each group starts on its own page and has a small heading above the grid. **Original look** and **Hide**, where available, stay at the front of the list. The same order applies when filtering collected looks. These group names describe whether an appearance has an inventory icon; they do not indicate ownership.

## Settings and saved outfits

Use Mod Setting Menu to change **Enabled**, the **Wardrobe shortcut**, and **Logging**. Choose End, F7, F8 or F9 as the shortcut. Apply takes effect without restarting.

If you encounter an issue, set **Logging** to **Debug** in this mod's Mod Setting Menu settings, apply the change, reproduce the issue, and send me `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log` from your game folder.

The mod creates `settings.ini` and `wardrobe.dat` inside `ue4ss/Mods/WardrobeTransmog`. These personal files are not included in the download. Keep a backup of both files. Outfits and collected appearances are shared across saves. Collected looks include supported equipment in your backpack, equipped slots and stash when you load a game or open Wardrobe, plus items picked up while the mod is active. Equipping or transferring items also refreshes the collection. Previously recorded looks remain available after an item is sold or stored. Items sold before the mod first observed them are not recovered from save history. The All Looks filter gives access to the rest of the catalog.

Selecting **Original look** restores that slot's equipped appearance. **Reset Set** clears the displayed outfit. Presets store one complete outfit and can be loaded into either set.

Selected weapon appearances keep their authored size while drawn or sheathed, with matching scabbards and character previews. Choosing a greatsword look keeps its greatsword size regardless of the weapon equipped for its stats. The [DB] Heavy Weapon look uses a pickaxe. Changing looks keeps inactive swords hidden, and loading another save clears old weapon and sheath edits before applying your wardrobe. Vampire claws and fists retain their own appearance. Saved weapon looks reapply after the character finishes loading. Selected weapon and scabbard looks are restored immediately when the game resets them during a consumable animation, while normal draw and sheath visibility is preserved.

Hiding gloves or wristbands restores the natural forearm shape in both the character preview and gameplay.

The character preview stays in place while you change looks or save outfits. Selecting a look updates its highlight and outfit summary without rebuilding the grid. Item images load as their page is built. Ordinary pickups update collected looks without rebuilding your clothing. Saving runs in the background; close the game normally to let outstanding saves finish.

With Logging set to Debug, a completed page records its group, visible-entry and cached-image counts. Page construction is limited to two entries per continuation. Grouping uses saved icon references without loading preview textures for the full catalog.

Logging offers Off, Error, Warning, Info and Debug; Warning is the default. Levels include messages from the preceding levels, and Off suppresses all mod-owned output. Select Debug in Mod Setting Menu and apply it to record player attachment, menu creation, controller ownership and event counts, appearance work, and aggregate timings in `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. Controller compatibility failures identify the required entry point or layout that could not be verified. Repeated failures are suppressed. Loading completion restores player tracking and refreshes collected appearances. Main-menu pawns do not start player setup retries. Once a local gameplay player exists, delayed component setup is limited to twelve attempts; a later load, player event or shortcut press can retry it.

## Source

Source is maintained at [my-mods/style-without-sacrifice](https://github.com/my-mods/style-without-sacrifice). See [BUILD.md](BUILD.md) for the pinned SDK and build steps.

## Inspiration

Credit to **RyroNZ**, author of [Dawnwalker Wardrobe (Transmog)](https://www.nexusmods.com/thebloodofdawnwalker/mods/237), for the original Wardrobe/transmog idea. **MIK**'s [Transmog - Your Look Your Choice](https://www.nexusmods.com/thebloodofdawnwalker/mods/353) also inspired the feature set.

This mod is an independent implementation. **Not a single line of code from either author's mod was copied into this project.** Separately, this project uses the RE-UE4SS SDK, MinHook, fmt and the provided Mod Setting Menu integration helper; see [third-party notices](THIRD-PARTY-NOTICES.md).

## Performance and diagnostics

Wardrobe reuses deletion-aware references to loaded helpers and UI classes. Weapon catalog discovery runs in bounded slices; equipped appearances still refresh before optional catalog work. Asset loading remains synchronous when an appearance is first needed. Logging adds per-operation counts, total time and maximum time for attachment, catalog work, asset loading, refresh and menu work. These nested measurements overlap and must not be added together.

Select **Debug** in the final **Logging** setting for diagnostics in `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. Warning is the default; choose Off to suppress all mod-owned output. Timings and offline checks do not establish an in-game frame-rate improvement.

### Logging

Logging is the final diagnostic setting: **Off**, **Error**, **Warning** (default), **Info**, or **Debug**. Levels include all more severe messages. Off silences this mod; Debug includes detailed events and timing summaries in `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. Old Logging On preferences become Debug; old Off preferences become Warning.
