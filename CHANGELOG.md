# Changelog

## 0.1.0

- Added a Wardrobe tab with a character preview and mouse, keyboard and controller controls.
- Added separate day and night outfits, collected/all appearance filters and three outfit presets.
- Added hide options for gauntlets, feet and the sheathed weapon.
- Added configurable shortcuts and live Mod Setting Menu controls.
- Keep the character preview in place when changing looks or saving outfits.
- Avoid appearance rebuilds for ordinary pickups and unchanged outfit presets.
- Save outfits in the background and spread catalog/menu work across frames.
- Clarify armour, legwear, gloves/wristbands and footwear categories.
- Fix a clothing-layout check that could prevent the Wardrobe from initializing.
- Fix an initialization failure that could leave the tab and shortcut unavailable and repeatedly interrupt gameplay.
- Restore the Wardrobe page when reopening the character menu and stop failed menu actions from retrying continuously.
- Include the failed action in error messages to help diagnose missing game or menu functions.
