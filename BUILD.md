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

The DLL is `build/Release/main.dll`. The archive is `build/Wardrobe-Transmog-Your-Equipment.zip`. Its explicit installation list includes the native DLL, Lua settings adapter, mod metadata, player documentation and license notices. It excludes personal settings and saved outfits.

`native/UE4SS.def` lists the host exports needed to link this implementation. Building the SDK itself is not required. The running UE4SS installation must supply the imported exports and declared C++ interfaces. Do not substitute an import library from an unrelated SDK revision.

The clothing hook identifies the specific accessor's instructions in executable memory. Reflected row structure and slot checks protect the data handed back to the game. A changed accessor disables that hook and reports the missing capability. No game version string or whole-file hash is used as a runtime gate.

Catalog processing is incremental; appearance work follows inventory and player events. The native UI is built from the game's loaded widget classes. Game assets are loaded from the installed game and are not redistributed.

Hard object parameters use the property's copy operation. The SDK's legacy `SetObjectPropertyValue` wrapper has no mapping in its UE5.5 virtual table definitions, even when the host exports the wrapper. The Wardrobe page is a native panel in the existing hub widget tree; hub activation and tab rebuilding drive its setup. Attachment exceptions consume the same finite retry budget as missing data, and other failed actions cancel their pending work.

Menu opening uses `UIFrontend.GetFrontend` to resolve the local player's active UI. Blueprint lifecycle bindings handle both direct events and their optimized event-graph calls. Graph targets and entry offsets come from the loaded functions, with a validated integer `EntryPoint` parameter; no fixed Blueprint offsets are assumed.

The source license is MIT. Preserve the bundled dependency notices when distributing binaries. The UE4SS SDK's MIT license is included under `LICENSES`.
