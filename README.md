# Wardrobe - Transmog Your Equipment

Choose how your equipment looks from a dedicated **Wardrobe** tab in The Blood of Dawnwalker. Your equipped items and their stats stay in place.

Browse armour, legwear, gloves/wristbands, footwear and weapon appearances. Wristbands and gauntlets share the **Gloves / Wristbands** category. Switch between collected looks and the full appearance catalog, choose separate day and night outfits, and save three outfit presets. Gauntlets, feet and the sheathed weapon can be hidden. The drawn weapon remains visible.

Open Wardrobe with **End**, or select its tab immediately after **Inventory** in the character menu. Browse a six-column grid alongside a large character preview and compact equipment slots. The page uses the inventory background, game fonts, equipment icons, rarity frames and keyboard/controller glyphs, with gold selection markers and separate save/load dialogs. Smaller category controls and inset action prompts keep the layout close to Inventory, including on ultrawide screens. Mouse, keyboard and Xbox-style controller controls are supported.

## Installation

- **Vortex:** Install `Wardrobe-Transmog-Your-Equipment.zip` through Vortex, enable it and deploy.
- **Manual:** Copy `Data/WardrobeTransmog` into `The Blood of Dawnwalker/Dawnwalker/Binaries/Win64/ue4ss/Mods`, preserving the folder structure.

## Dependencies

Designed for game patch **1.0.5**. Requires UE4SS with C++ mod support and the native EngineTick, BeginPlay, EndPlay and ProcessLocalScriptFunction callbacks available. Mod Setting Menu **1.0.7.1 or later** provides the settings page. The loader and settings menu are separate downloads.

## Controls

| Action | Keyboard | Controller |
| --- | --- | --- |
| Open Wardrobe | End, configurable | Select the Wardrobe hub tab |
| Change category | A / D | Left / right trigger |
| Select a look | Arrow keys, Enter | Left stick or D-pad, A |
| Change page | Page Up / Page Down | Continue beyond the first or last tile |
| Day / night preview | P | Start / Menu |
| Collected / all looks | Y | Right stick click |
| Hide supported slot | F | Left stick click |
| Save / load outfit | T / S | X / Y |
| Reset displayed outfit | R | Hold X |
| Close or cancel | Escape | B |

All actions also have on-screen mouse controls. The action hints use the game's keyboard and Xbox glyphs and follow the active input device. Hold the left stick to repeat movement; vertical navigation preserves the column when crossing pages. Changing the previewed day/night set does not change your active equipment loadout. Each page holds up to 36 choices; focus a tile to see its name below the grid. The equipment panel shows the displayed outfit, with the selected category's name underneath. NPC appearances without an inventory image use their category symbol.

Within each category, **Player Outfits** with inventory icons appear first, followed by **NPC Outfits** without item icons. Both groups sort alphabetically by their displayed names, ignoring case. Each group starts on its own page and has a small heading above the grid. **Original look** and **Hide**, where available, stay at the front of the list. The same order applies when filtering collected looks.

## Settings and saved outfits

Use Mod Setting Menu to change **Enabled**, the **Wardrobe shortcut**, and **Logging**. Choose End, F7, F8 or F9 as the shortcut. Apply takes effect without restarting.

The mod creates `settings.ini` and `wardrobe.dat` inside `ue4ss/Mods/WardrobeTransmog`. These personal files are not included in the download. Keep a backup of both files. Outfits and collected appearances are shared across saves; collection starts with items observed while this mod is active. The All Looks filter gives access to the rest of the catalog.

Selecting **Original look** restores that slot's equipped appearance. **Reset Set** clears the displayed outfit. Presets store one complete outfit and can be loaded into either set.

The character preview stays in place while you change looks or save outfits. Selecting a look updates its highlight and outfit summary without rebuilding the grid. Item images load as their page is built. Ordinary pickups update collected looks without rebuilding your clothing. Saving runs in the background; close the game normally to let outstanding saves finish.

With Logging enabled, a completed page records its group, visible-entry and cached-image counts. Page construction is limited to two entries per continuation. Grouping uses saved icon references without loading preview textures for the full catalog.

Logging defaults to Off. Turn it on in Mod Setting Menu to record player attachment, game hub detection, tab creation and placement beside Inventory, page opening, aggregate catalog, inventory, refresh and appearance-override counts, total work time, and the longest active mod tick in `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. Operational failures include the failed action and error detail, once per distinct message, even when detailed logging is off. Failed player setup stops after twelve attempts; a later player event or shortcut press can retry it.

## Source

Source is maintained at [my-mods/wardrobe-transmog](https://github.com/my-mods/wardrobe-transmog). See [BUILD.md](BUILD.md) for the pinned SDK and build steps.

## Inspiration

[DawnwalkerWardrobe](https://www.nexusmods.com/thebloodofdawnwalker/mods/237) and [Transmog - Your Look Your Choice](https://www.nexusmods.com/thebloodofdawnwalker/mods/353) inspired the feature set. This project supplies its own implementation; their DLLs and bundled assets are not part of this package.
