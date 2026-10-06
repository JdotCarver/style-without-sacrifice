# Building

Use Windows x64, Visual Studio 2022 with the C++ desktop workload and Windows SDK, CMake 3.25 or newer, and Git. Release builds use the dynamic MSVC runtime.

Check out [RE-UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) at `97b7e501c19d8b2b7c662feee73aaa0dc1f0a4d1`, with recursive submodules. Its Unreal dependency must be at `eb40a05f49509bdeb1ac39287032b60af585cca8`. The CMake configuration checks these source revisions. MinHook and the SDK's header dependencies are pinned in `native/CMakeLists.txt`.

From an x64 Native Tools command prompt:

```bat
cmake -S native -B build -A x64 -DUE4SS_SDK=C:/src/RE-UE4SS
cmake --build build --config Release
cd build
cpack -C Release -G ZIP
```

The DLL is `build/Release/main.dll`. The archive is `build/Style-Without-Sacrifice.zip`. Its explicit installation list includes the native DLL, Lua settings adapter, mod metadata, player documentation and license notices. It excludes personal settings and saved outfits.

`native/UE4SS.def` lists the host exports needed to link this implementation. Building the SDK itself is not required. The running UE4SS installation must supply the imported exports and declared C++ interfaces. Do not substitute an import library from an unrelated SDK revision.

The clothing hook identifies the specific accessor's instructions in executable memory. Reflected row structure and slot checks protect the data handed back to the game. A changed accessor disables that hook and reports the missing capability. No game version string or whole-file hash is used as a runtime gate.

Hidden wrist rows explicitly set the reflected `AppearanceMesh.GauntletIndex` byte to zero, matching the game's unoccupied wrist slot. The native row constructor's value of 255 is passed directly to arm deformation when the wrist slot has a row. Missing or changed field metadata leaves that slot's equipped look in place; other appearances remain available.

Catalog processing is incremental; appearance work follows inventory and player events. The native UI is built from the game's loaded widget classes. Game assets are loaded from the installed game and are not redistributed.

Weapon sizing resolves the selected appearance's item and loads its soft `WeaponBlueprint` class reference through `LoadClassAsset_Blocking.AssetClass`. The reference is copied into owned call parameters; a hard class reference is also supported. It reads `RelativeScale3D` from the class default object's `BaseMesh` template, preserving the selected weapon's authored size. The game's `GetSheathedWeaponScale` can reduce greatswords and is not used for transmog sizing. Class-loader, object-reference and double-vector layouts are checked before use. The demo greatsword and sacrificial knife resolve sizing from their matching stock items. Fourteen identified catalog-only rows use unit scale; failure to load any other item keeps the equipped appearance instead of guessing unit scale. Inherited template scales are preserved, including the Blood Slave sword at 1.2 and Uriash's sword at 1.8. The resulting owned scale is cached with the choice and applied to drawn weapons, sheathed weapons, scabbards and character previews through `SetRelativeScale3D`. Scale restoration is conditional on the installed mesh and scale still matching; shared item assets and class defaults are never modified. Missing sizing capabilities leave the equipped weapon appearance in place and report the failed operation; Logging also records the selected row and scale. Loading is attempted only when preparing a changed choice or player context. Drawn weapons are registered after the game's `OnWeaponAppearanceSet` Blueprint event, which follows combat-owner, item-data and mesh initialization. The existing script post-callback filters the event name, parameter size and weapon class; repeated events on pooled actors coalesce into one deferred refresh. The refresh consumes callbacks raised by its own appearance update without scheduling another pass. Ownership accepts the player or preview actor directly, or the reflected owner of `OwningCombatComponent`. BeginPlay no longer requires item data to have been assigned. The fixed weapon list reuses deleted or no-longer-owned slots. Transmog leaves drawn and sheathed weapon visibility under game control, so retained inactive actors remain hidden. Visibility restoration for Hide/scabbards is conditional on the same mesh still being installed. Player teardown and rebinding restore surviving edits before clearing caches; ClientRestart also resets context when the same pawn is reused across saves. Preview teardown precedes rebinding. Loader-thread shutdown only discards records and does not call into gameplay.

Hard object parameters use the property's copy operation. The SDK's legacy `SetObjectPropertyValue` wrapper has no mapping in its UE5.5 virtual table definitions, even when the host exports the wrapper. The Wardrobe page is a native panel in the existing hub widget tree; hub activation and tab rebuilding drive its setup. Attachment exceptions consume the same finite retry budget as missing data, and other failed actions cancel their pending work.

Menu opening uses `UIFrontend.GetFrontend` to resolve the local player's active UI. Blueprint lifecycle bindings handle both direct events and their optimized event-graph calls. Graph targets and entry offsets come from the loaded functions, with a validated integer `EntryPoint` parameter; no fixed Blueprint offsets are assumed.

The source license is MIT. Preserve the bundled dependency notices when distributing binaries. The UE4SS SDK's MIT license is included under `LICENSES`.
