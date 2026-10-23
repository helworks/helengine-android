# HelEngine Android platform

This platform registers Android in the editor and exports debug APKs containing the generated native engine and cooked project content. DemoDisc starts at its authored splash and main menu, with rendering showcases, physics scenes, Tilt Trial levels, software ray tracers and the Max Objects benchmark packaged in the scene catalog. Vulkan handles textured standard materials, metallic/roughness lighting, directional shadows, fonts and clipped interfaces; AAudio mixes cooked audio. The standalone diagnostic triangle host remains available when no generated core is supplied.

## Register with the source editor

From the workspace root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\helengine-android\scripts\register-platform.ps1
```

The script publishes the .NET 9 metadata builder into `artifacts/builder/`, then adds Android to `helengine/user_settings/platforms.json` for the engine versions already registered there. It preserves other platform entries and can be run again to refresh the builder. Use `-EngineUserSettingsRoot`, `-HelengineRoot` and `-EngineVersion` to select another checkout or version. The registry is local engine configuration; it is not the project's `settings/platforms.json`.

Restart the editor after registration, then enable Android through the project's platform settings. The supported build profile is Debug, with the `android-native` Vulkan graphics profile. Registration requires .NET 9 or a newer compatible SDK; compiling an APK requires the Android toolchain below.

Inspect the metadata or run the focused integration checks:

```powershell
dotnet .\helengine-android\artifacts\builder\helengine.android.builder.dll --describe
dotnet test .\helengine-android\builder.tests\helengine.android.builder.tests.csproj
```

The tests exercise platform discovery, editor selection, material parameters and texture dependencies, invalid asset paths, and rejection of exports without a generated core.

When the builder is compiled outside the platform repository, set `HELENGINE_ANDROID_REPOSITORY_ROOT` to the absolute real Android source checkout. The override is validated before packaging; the installed `artifacts/builder` layout remains the fallback. `HELENGINE_BUILD_WORKSPACE_ROOT` selects the native and Gradle cache directories for editor/CLI builds. Direct `scripts/build-host.ps1` callers can supply `-NativeBuildRoot` and `-GradleCacheRoot` explicitly to keep native intermediates and caches in a chosen output location.

## Export an authored game

Enable Android in the project, choose its startup scene and select the graphics option `screen-orientation` (`unspecified`, `portrait` or `landscape`). Landscape maps to Android's `sensorLandscape` activity setting. From the workspace root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc -Platform android -BuildProfile debug -Output .\builds\helengine-android\demodisc\output
```

The builder stages cooked assets, emits their APK inventory and compiles the engine's generated C++ runtime. The game reads its generated startup catalog and loads cooked scenes through the shared scene manager. Material cooking supports `standard-shader` colors, diffuse and roughness textures, metallic/roughness/specular coefficients, alpha modes and shadow policies. Android stores its additional material parameters in a versioned extension after the shared material body. Imported models with no material bindings use the built-in white standard surface; additional submeshes reuse material slot zero, matching the shared player behavior. Unsupported schemas and invalid dependencies fail explicitly. Generated C++ must preserve enum underlying types, numeric formatting and virtual interface destructors.

Run DemoDisc's `menu.configure-android-demo-disc` editor command to prepare Android overrides on the selected scene package and material assets. It uses authoring transactions, preserves other platform settings, and applies the approved diagnostics scale across rendering, physics and benchmark scenes. The package includes 37 scene assets; the dual-screen menu and dual-screen Tilt selection scene use separate platforms and are excluded.

DemoDisc's `menu.validate-android-demo-disc` command checks the cooked package without launching a player. Set `HELENGINE_ANDROID_BUILD_REPORT_PATH` to the APK's `host-build-report.json` before running it. It rejects missing or duplicated entity IDs, dangling component references, unbound required Tilt references, and duplicate Tilt gameplay controllers or cameras.

## Reading packaged content

Game builds install `AndroidAssetContentStreamSource` through the existing `CoreInitializationOptions.ContentStreamSource`
API. Scenes, textures, meshes, fonts, materials and audio open directly from the APK's `cooked/` namespace when requested.
Startup no longer extracts the whole catalog into private app storage or changes the process working directory.
Build-time inventory verification still checks every cooked file before producing a verified APK.

Each reader owns a separate [NDK asset handle](https://developer.android.com/ndk/reference/group/asset), supporting compressed
and uncompressed assets without requiring a direct file descriptor. Streams expose length, position, seek, EOF and explicit
disposal to existing engine deserializers. Small reads share a fixed 16 KB buffer; large reads use bounded native chunks.
The source can open independent streams on scene-loading workers; an individual stream belongs to its calling thread.
Read and seek errors, missing assets and unexpectedly truncated files fail explicitly. Packaged assets remain read-only.

Content paths are relative to APK assets and must name files beneath `cooked/`. Ordinary `./` segments are normalized;
absolute paths, parent traversal and embedded nulls are rejected. User save data belongs in a separate writable storage
service; this content source does not persist player preferences. Old extracted content from previous APKs is left unused.

The manifest enables Java code loading for the custom `HelengineActivity` and SDK bridges. Packaging verifies this flag
in the compiled binary manifest. `native.tests/AndroidAssetStreamTests.cpp` exercises the real adapter in eight CPU scenarios
covering buffering, partial reads, seek, independent readers, ownership, invalid paths and native failures.

## Thermal status diagnostics

Rich snapshots from `Core.RuntimeDiagnosticsService.CaptureSnapshot()` expose Android's reported thermal throttling
state through `DetailMetrics`. `android_thermal_status_available` is `1` on devices with the API 30 native contract and
`0` on API 26-29. A supported capture also publishes `android_thermal_status`; unsupported captures omit that metric,
so an older OS is distinguishable from a measured NONE state.

The [native Android thermal status API](https://developer.android.com/ndk/reference/group/thermal) reports these values:

| Value | Android status |
| --- | --- |
| 0 | NONE |
| 1 | LIGHT |
| 2 | MODERATE |
| 3 | SEVERE |
| 4 | CRITICAL |
| 5 | EMERGENCY |
| 6 | SHUTDOWN |

The host resolves thermal functions dynamically only on supported runtime OS versions. It acquires the manager on the
first requested rich capture, serializes acquisition/queries and releases it during host teardown. It does not register
a thermal listener or start a polling loop. `CaptureMemoryCounters` remains a memory-only scalar path and never opens
or queries the thermal service. Perfetto marks actual queries with `HelEngine.Thermal.ReadStatus`.

Thermal state is a framework throttling observation, not a temperature in degrees or a forecast of remaining headroom.
The game engineer decides whether to query it and how to react through the existing `Core.PlatformSettings` API;
the host does not select quality presets, change FPS or render scale, or persist a player choice automatically.
Missing required entry points on a supported OS, manager acquisition failure and negative status results fail explicitly.
Failed rich captures dispose their temporary snapshot graph rather than returning a fabricated NONE state.

`native.tests/AndroidThermalDiagnosticsTests.cpp` covers older OS capability, all seven current states, lazy ownership,
missing entry points, failed acquisition/query, concurrent callers, shared snapshots, failure cleanup and scalar memory
independence. Packaging verifies the reader is present in both game ABIs and rejects direct imports of API 30 thermal
functions, preserving the APK's API 26 minimum. A hardware observation still needs validation on a supported real device.

## Memory pressure and renderer caches

`HelengineActivity.onTrimMemory` forwards notifications from the historical RUNNING_LOW threshold upward, including
UI_HIDDEN, BACKGROUND and intermediate/future stronger levels. The modern callback matters because Android no longer
delivers [onLowMemory](https://developer.android.com/reference/android/content/ComponentCallbacks2) since API 34.
Older `APP_CMD_LOW_MEMORY` glue commands remain supported through the same deferred cleanup queue.

The UI callback checks the live activity registration, atomically retains the strongest pending level and wakes the
owning native looper. It performs no Vulkan operations. The host consumes the request between frames even while paused
or without a native window. Registration destruction retires the callback target before releasing the retained looper;
late callbacks and diagnostic-probe notifications have no game frame-cache target to access.

The renderer drops retained CPU frame batches, command planning storage and per-batch offsets, then waits for device
work before retiring GPU resources. It releases vertex and lighting arenas and clears their handles, mappings and
capacities. The next `Prepare` rebuilds them at the new frame's required capacity and updates the retained lighting
descriptor. Expired texture-cache entries are removed; textures whose source still has an owner remain resident.
This preserves loaded scenes, authored assets, active quality settings, pipelines, shadow resources and scaled targets.
The routine is valid before logical-device initialization and across native surface loss.

Logs report notification level, released buffer/texture counts and requested arena capacities. Capacity figures exclude
Vulkan alignment and driver overhead and do not promise a matching decrease in process RSS. Perfetto spans
`HelEngine.Memory.TrimRenderer` and `HelEngine.Memory.TrimGpu` expose synchronization and cleanup cost.
Native wait failures propagate before GPU resources are freed; repeated notifications cannot double-free retired arenas.
CPU frame-cache destruction remains serialized with render visitors on the host thread.

`native.tests/AndroidSceneGpuMemoryTests.cpp` compiles the actual cleanup and JNI queue against official Vulkan headers,
fake driver/bootstrap boundaries and a fake looper. Nine scenarios verify resource ownership, destruction order, cleared
rebuild gates, idempotence, wait failure, cold initialization, live Activity routing, wakeup/coalescing and late callbacks.
Three Java policy scenarios cover weak, intermediate and strong notification levels and superclass forwarding;
the existing audio-focus scenarios also exercise the updated production Activity. Both APK flavors export the JNI bridge.

## Native CPU tracing

The game and diagnostic renderer emit synchronous [Android native ATrace sections](https://developer.android.com/ndk/reference/group/tracing).
They appear as thread spans in [Perfetto](https://perfetto.dev/docs/data-sources/atrace) when a system capture enables this
application. With tracing disabled, each scope checks the SDK gate and emits no begin/end event. The guard retains one
boolean, creates no object graph and uses fixed labels. Lexical ownership balances emitted sections during ordinary
returns and exceptions; guards cannot be copied or moved to another scope or thread.

Game spans include `HelEngine.Initialize`, `HelEngine.Frame`, `HelEngine.Core.Update`, `HelEngine.Core.Draw`,
`HelEngine.Overlay.Build` and `HelEngine.Scene.Commit`. Vulkan spans distinguish `HelEngine.Vulkan.Frame`, `WaitFence`,
`PrepareResources`, `AcquireImage`, `RecordCommands`, `QueueSubmit`, `Present` and `RecreateSwapchain`, all with the
`HelEngine.Vulkan.` prefix. These measure CPU work and blocking calls, including waits for previously submitted GPU
work and Swappy presentation. They do not measure GPU execution duration through Vulkan timestamp queries.

Content streams mark `HelEngine.Content.Open`, `HelEngine.Content.ReadNative` and `HelEngine.Content.SeekNative`.
Cached bytes and cached seeks create no additional spans; a small-read deserializer traces native buffer fills rather
than every byte. Process-memory sampling marks `HelEngine.Memory.Capture`, exposing diagnostic overhead in the same trace.
The host emits events only into an explicitly enabled system capture and does not start a recorder or write trace files.

`diagnostics/helengine-perfetto.pbtxt` prepares a ten-second capture of the current debug application, CPU scheduling
and Android graphics/input events. On Android 9+ with a chosen connected device and the player already running, an engineer
can record it explicitly with the [Perfetto command-line workflow](https://perfetto.dev/docs/reference/perfetto-cli):

```powershell
adb -s <serial> push .\helengine-android\diagnostics\helengine-perfetto.pbtxt /data/local/tmp/helengine-perfetto.pbtxt
adb -s <serial> shell "cat /data/local/tmp/helengine-perfetto.pbtxt | perfetto --txt -c - -o /data/misc/perfetto-traces/helengine.pftrace"
adb -s <serial> pull /data/misc/perfetto-traces/helengine.pftrace .\builds\helengine-android\perfetto\capture.pftrace
```

Open the resulting file in the Perfetto viewer and search for `HelEngine.`. Choose the device explicitly; these instructions
do not boot, install or launch a player. Native sections use API 23 functions supported by the host's API 26 minimum;
API 26/27 require tracing tools available on those OS versions rather than the Android 9+ Perfetto service.
`native.tests/AndroidTraceTests.cpp` covers nesting, exceptions, gate changes, independent threads and actual asset/memory
reader integration. Packaging checks all three native tracing imports in both ABIs, for game and diagnostic APKs.

## Runtime memory diagnostics

Android installs `AndroidRuntimeDiagnosticsProvider` through `CoreInitializationOptions.RuntimeDiagnosticsProvider`.
The existing `Core.RuntimeDiagnosticsService.CaptureSnapshot()` API reports process resident bytes, peak resident bytes
and process page faults. The shared service still attaches loaded scene IDs and engine collection counts/capacities.
`CaptureMemoryCounters(counters)` repopulates an existing `RuntimeMemoryCounters` object through the provider's scalar
interface, without creating a snapshot, lists or detail metrics, and without querying the allocator heap.
Sampling occurs only when the engine or gameplay requests it; the host does not create a new periodic sampler.

Current resident size comes from the process-local [Linux statm counters](https://docs.kernel.org/filesystems/proc.html),
converted with the real `sysconf(_SC_PAGESIZE)` value, supporting both 4 KB and 16 KB pages. RSS is an approximate kernel
counter rather than a precise mapping audit. Peak RSS and minor/major fault totals come from process-wide
[getrusage](https://man7.org/linux/man-pages/man2/getrusage.2.html), excluding child processes; peak RSS uses KiB independently
of page width. The separate reads are observations taken close together, not an atomic system-wide memory snapshot.

Rich snapshots add these stable detail metrics:

- `android_process_virtual_bytes`: virtual address-space size, not committed memory.
- `android_system_page_size_bytes`: actual process page width.
- `android_native_heap_allocated_bytes` and `android_native_heap_free_bytes`: allocator-reported live/free space.
- `android_native_heap_arena_bytes` and `android_native_heap_mapped_bytes`: allocator-reported arena/mapped regions.
- `android_process_resident_is_estimate`: `1`, identifying the approximate RSS measurement.

Heap details use [Bionic mallinfo](https://android.googlesource.com/platform/bionic/+/main/libc/include/malloc.h), whose counters
are `size_t` on Android and retain values above 4 GiB on both supported 64-bit ABIs. These are allocator observations,
not a precise allocation audit or measurements of Java heap, GPU memory, PSS or whole-process committed memory.
`CommittedBytes`, `PeakCommittedBytes` and `AvailablePhysicalBytes` remain `0` to mean unknown under the shared API.
Permission/read errors, malformed counters, invalid page sizes and numeric overflow fail explicitly; stale observations
are not published as successful captures. Process-counter descriptors and partially constructed snapshot graphs close
on failure. Snapshot callers own disposal; the reusable scalar path owns no transient engine diagnostics objects.

`native.tests/AndroidMemoryDiagnosticsTests.cpp` compiles the real reader/provider against simulated OS and engine ABI
boundaries. Seven scenarios cover page/KiB conversion, reusable sampling, rich details, EINTR and short reads, invalid
records, native failures, overflow and snapshot cleanup. Packaging verifies native diagnostics imports for both ABIs.

## Viewport density

Android reads the activity's current [DisplayMetrics.densityDpi](https://developer.android.com/reference/android/util/DisplayMetrics#densityDpi) through JNI and reports it with the shared renderer's `SetWindowDensityDpi` API, including configuration changes. Reading the current activity resources avoids retaining the density of the native asset manager captured at startup. Scaling is owned by the engine's `ViewportComponent`:

- `ScalingMode = ViewportComponent.DensityScalingMode` enables density conversion.
- `PixelsPerCentimeter` specifies authored pixels per centimeter, initially `160 / 2.54`.
- `UiScale` is an additional size multiplier, initially `1`.

The viewport computes `DPI / (PixelsPerCentimeter * 2.54) * UiScale`. At 420 DPI the initial configuration produces a 2.625 scale. Rendering and pointer input retain physical pixels; the viewport scales layout, glyphs, decorations and hit areas together. Use edge anchors for panels and titles so their positions follow the available viewport bounds. Set `NativeFontPixels = Disabled` for glyphs that should follow viewport scale; native pixel fonts explicitly retain their cooked pixel size. Host-reported logical DPI gives an approximate physical size, consistent with Android's [density documentation](https://developer.android.com/training/multiscreen/screendensities).

DemoDisc's `menu.configure-cube-test-density` editor command adds Android-only settings to the existing Cube Test and its shared instruction Blueprint through an authoring transaction. Android uses the Blueprint panel with density scaling and edge anchors; the legacy scene instruction viewport is excluded to prevent duplicate panels. The Cube Test factory retains that selection when the scene is regenerated. Common scene content and other platform overrides remain authored.

The Cube Test uses `UiScale = 0.5625` for its Android interface viewports. At 420 DPI the effective scale is 1.4765625, producing an approximately 650x348 instruction panel. Fonts, icons and the separate light-state indicator use the same authored size multiplier.

## Audio and input

The audio backend owns PCM16 mono/stereo voices and mixes gains, loops and buses on a worker thread. Sources are resampled
to the fixed 48 kHz stereo float client format; Android routes that stream to the default output device. The real-time
callback consumes a bounded ring buffer without locks or allocation.

Activity pause, stop or native-window loss freezes the mixer and closes the output stream. The suspended worker waits
without polling. Resume requires a drawable window and granted audio focus, then opens the current output route and resets the game frame clock.
Voices, loops, bus gains, bus pauses and PCM queued in the engine ring survive suspension and device replacement.
Scene initialization may register or stop voices before output is resumed.

An [AAudio disconnect](https://developer.android.com/ndk/guides/audio/aaudio/aaudio#disconnected_audio_stream) is published
by the error callback and handled on the engine thread: the old stream closes before its replacement opens. A route that
disconnects again during opening or starting is retried at the next active maintenance frame, with one attempt per frame.
Other device failures remain explicit errors; candidates and builders are released when opening, starting or format
validation fails. Fixed client format keeps queued PCM valid on the new route. Audio already submitted to a disconnected
device can be lost; the game does not restart voices to compensate.

`HelengineActivity` extends `NativeActivity` to bridge [Android audio focus](https://developer.android.com/media/optimize/audio-focus).
The host requests focus while resumed with a drawable window, accepts delayed permission, and remains silent if the request
is denied. Temporary loss, including a request to duck, pauses audio instead of modifying authored bus gains. Permission
callbacks immediately gate the real-time consumer and mixer; only engine-thread maintenance opens or closes streams.
A current gain resumes audio only while the host is active. Permanent loss retires the request and requires a later host
activation; old callbacks cannot grant permission to a replacement request or resurrect a suspended activity.
API 28+ uses matching GAME/MUSIC client and focus attributes. API 26/27 uses AAudio's MEDIA/MUSIC defaults without importing
newer native symbols. The diagnostic host has no game audio and leaves the focus controller inactive.

Menu pointer input supports touch and mouse. Hardware controllers expose sticks, triggers and face buttons through the engine input system. On the emulator, arrows or WASD map to the directional pad/left stick, Enter or Space confirms, Back/Escape returns, Q/E map to shoulders and J/K/U/I map to face buttons. Gameplay requiring analog input can use a controller; an on-screen gamepad is not supplied yet.

Touch input also exposes independent fingers through the shared engine API. `Core.Input.TouchCount` counts ordered
samples in the captured frame; `Core.Input.GetTouch(index)` returns an `InputTouchState` with stable `Id`, physical-pixel
coordinates, sample deltas, pressure and `Began`, `Moved`, `Stationary`, `Ended` or `Canceled` phase. The index is not an ID.
A contact can appear more than once in a frame, preserving taps and ID reuse even when several native events arrive
before the next update. Frame storage is borrowed from the backend until the next capture; copy individual samples
when retaining them. The tracker reuses alternating buffers and grows only when an event batch exceeds their capacity.

`InteractableComponent.TouchEvent` delivers each captured finger in coordinates relative to its control, using the
existing viewport/camera hit test, visual ordering and clipping. Fingers remain captured when dragged outside a control;
another finger can operate a different control simultaneously. Existing cursor-based controls use their first captured
finger and do not receive a duplicate click from the legacy projected pointer. Custom controls can subscribe to
`TouchEvent` or implement `ITouchInteractable2D`; the game defines its on-screen layout and maps those contacts to actions.
Focus loss, activity suspension, input-queue replacement and surface loss cancel contacts. Cancellation abandons control
state without a release activation and discards undelivered gestures. Removing a control or changing input views also
cancels its captures. A stationary finger cannot acquire a newly appearing control without a fresh touch-down.

The forward renderer supports up to eight lights per camera and one 1024-pixel shadow map when the first directional light enables shadows. Point and spot lighting are supported without point/spot shadow maps. These are the current Android renderer limits, so desktop rendering parity and performance are separate validation targets.

## Native Android project

Open `android/` in Android Studio. The project targets `arm64-v8a` devices and `x86_64` emulators, with minimum API 26 and compile/target API 35, and requires Vulkan 1.0. It packages `libhelengine_android.so` in `com.helengine.android.HelengineActivity`, a `NativeActivity` subclass. With generated content it runs the engine update/draw loop using native app glue; otherwise it draws the diagnostic triangle. Both paths log device selection, swapchain creation, presentation and teardown to `HelengineAndroid`. Surface replacement releases presentation resources while retaining the running engine scene.

Device, swapchain and pipeline resources have separate owners. The probe uses FIFO presentation, one submission fence, and one presentation semaphore per swapchain image following the [Khronos semaphore guidance](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html). It preserves the blue background for UNORM and sRGB attachments. A Vulkan initialization or rendering failure is reported and ends the activity; another graphics backend is not substituted.

The pinned toolchain is Android Gradle Plugin 8.11.1, Gradle 8.14.3, JDK 17, Android SDK platform 35, SDK Build Tools 35.0.0, NDK 27.0.12077973 and SDK CMake 3.22.1. The checked-in Gradle wrapper verifies the official Gradle distribution checksum. AGP's [compatibility documentation](https://developer.android.com/build/releases/agp-8-11-0-release-notes) specifies Gradle 8.13 or newer and JDK 17. NDK r27's flexible page-size option is enabled according to the [native page-size documentation](https://developer.android.com/guide/practices/page-sizes).

Install those SDK packages and accept their licenses using SDK Manager. The PowerShell build command accepts SDK/JDK paths explicitly, uses `ANDROID_HOME`/`ANDROID_SDK_ROOT` and `JAVA_HOME` when set, or finds the workspace-local toolchain under `builds/helengine-android/toolchain/`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\helengine-android\scripts\build-host.ps1
```

Optional parameters include `-SdkRoot`, `-JavaHome`, `-OutputDirectory`, `-WorkingDirectory`, `-GradleCacheRoot` and `-ScreenOrientation`. Supply `-GeneratedCoreRoot` and `-ContentRoot` together to compile an exported game. This command restores its process environment afterwards and puts outputs and logs in `builds/helengine-android/native-host/` by default. Short hashed Gradle/CMake cache paths avoid Windows native build path limits. It verifies APK signing, application identity, both native libraries and their machine types, exported entry points, packaged SPIR-V shaders and 16 KB APK/ELF alignment. Game builds additionally verify every asset in the cooked inventory. On success it writes `output/helengine_android_host-debug.apk` and `output/host-build-report.json`.

For a direct Gradle build instead, configure `ANDROID_HOME` or ignored `android/local.properties` and select JDK 17 with `JAVA_HOME`:

```powershell
.\helengine-android\android\gradlew.bat -p .\helengine-android\android assembleDebug
```

The direct Gradle command writes `android/app/build/outputs/apk/debug/app-debug.apk`. Native lifecycle setup follows Android's [NativeActivity sample](https://developer.android.com/ndk/samples/sample_na). Building this standalone host does not export an authored HelEngine game.

## Boot check

With a connected Android device or running emulator, install and launch the verified APK:

```powershell
adb install -r .\builds\helengine-android\native-host\output\helengine_android_host-debug.apk
adb shell am start -W -n com.helengine.android.scaffold/com.helengine.android.HelengineActivity
adb logcat -d -s HelengineAndroid:I
```

Select a device with `adb -s <serial>` when more than one is connected. A standalone probe logs `Vulkan triangle frame presented:`. An exported game logs its generated startup scene, extracted asset inventory and `Authored runtime update:` with camera and mesh counts. The Cube Test should show its spinning cube and interface over the scene's cornflower-blue background.

## Runtime settings and quality presets

In **Profiles → Android → Runtime**, create complete independent presets, duplicate them, edit their IDs and names,
and select the preset used at startup. Presets have no base or inheritance and are saved with the project in
`settings/platform.android.json` under `runtimeSettings`. The same workflow is available for other platforms.
The engine/platform declares setting contracts; the engine user authors each platform's preset values.

The initial common controls are global shadows, maximum visible lights and the scene loading budget.
Android caps the requested light budget at its eight-light forward shader limit. Disabling shadows preserves lighting
and the authored per-light/material choices. Platform builders may declare additional typed settings through
`PlatformDefinition.RuntimeSettings`; a host supplies `IPlatformSettingsBackend` for platform-specific validation and application.

Gameplay code controls when settings are applied through `Core.PlatformSettings`:

```csharp
core.PlatformSettings.ApplyPreset("low");
core.PlatformSettings.SetBoolean(CommonPlatformRuntimeSettings.ShadowsEnabled, false);
var values = core.PlatformSettings.CopyActiveValues();
values[CommonPlatformRuntimeSettings.MaximumVisibleLights] = PlatformSettingValue.FromInteger(4);
core.PlatformSettings.ApplyValues(values);
```

`Configuration` lists the current platform's definitions and presets; `GetValue` reads an active value.
Runtime edits affect only the active configuration. The game owns its quality menu, confirmation flow and preference persistence.
Settings marked as startup-only reject runtime changes. Native Android and Windows hosts load the exported catalog;
desktop Play receives the same portable catalog. Other native hosts can opt in by installing
`he_create_runtime_platform_settings()` into `CoreInitializationOptions.PlatformSettingsConfiguration` before initializing the core.

Android also declares the numeric **3D Render Scale** (`render-scale`) runtime setting, from `0.25` to `1.0`, initially `1.0`.
Its value belongs to each complete authored preset. Existing presets must provide an explicit value when adopting this contract.
The game can change it through the normal settings API:

```csharp
core.PlatformSettings.SetNumber("render-scale", 0.75);
```

On a 1920×1080 output, `0.5` renders the world at 960×540 and `0.75` at 1440×810. Text, sprites, UI clipping,
viewport layout, DPI and input coordinates retain the physical output dimensions. Each camera's world is rendered into
scaled color/depth targets and composited before that camera's native-resolution overlays, preserving camera order,
clear policies, depth reuse and transparent coverage. Full scale uses the direct rendering path.
Settings application prepares a complete replacement before publishing the value; allocation failure retains the previous
target and active settings. The game engineer chooses when to incur this synchronous GPU resource change. The chosen scale
survives native surface replacement and resize; it does not select a preset or persist player preferences automatically.

Android declares **Target Frame Rate** (`target-frame-rate`) as a runtime-mutable integer from `0` to `240`, initially `0`.
Zero requests the fastest available display cadence; explicit values request a target such as 30, 60, 90 or 120 FPS:

```csharp
core.PlatformSettings.SetInteger("target-frame-rate", 30);
core.PlatformSettings.SetInteger("target-frame-rate", 0); // Display-rate mode.
```

Presentation uses [Android Frame Pacing (Swappy)](https://developer.android.com/games/sdk/frame-pacing/vulkan/add-functions),
statically linked from `androidx.games:games-frame-pacing:2.1.3`. Swappy negotiates timing extensions and available refresh
periods, and receives each replacement native window. Automatic swap-interval selection is disabled: gameplay owns the
requested FPS. The achievable rate depends on display modes and CPU/GPU workload; this setting is a request, not a measured
FPS guarantee. Timing history resets after resume and FPS changes. Surface recreation retains the selected rate.
Every authored Android preset must explicitly include both `render-scale` and `target-frame-rate`; no preset inherits values.

## Physical controllers and keyboard navigation

Android game builds use [Android Game Controller (Paddleboat)](https://developer.android.com/games/sdk/game-controller/controller),
linked from `androidx.games:games-controller:2.0.2`. The input backend publishes up to eight independent controllers through
the existing engine input API. SDK slot indices remain stable while a device is connected, including holes after another
device disconnects. `GetGamepadCount()` is the slot extent: inspect `GetGamepadState(index).Connected` before using a slot.
Bindings select a player through `InputControlId.DeviceIndex`; the engine user owns player assignment and control bindings.

Standardized face buttons, shoulders, stick clicks, d-pad, start/select/home, both sticks and L2/R2 analog triggers map to
the portable input state. Hardware flat regions are respected; gameplay binding dead zones remain authored by the engineer.
Controller key events preserve held analog values. A button tap drained entirely before an engine frame is retained for
one capture, then released. Disconnection releases its slot, and replacement publishes a disconnected frame before new input.
Focus loss clears pending actions and suppresses held physical buttons and axes until they are released or return to neutral.

Existing WASD/d-pad keyboard navigation, Enter/Space/J confirmation, Back/Escape/K cancellation, U/I face buttons and Q/E
shoulders overlay slot zero. Individual aliases retain independent key lifetimes. Keyboard release never clears a physical
button or analog axis, and physical controllers in other slots remain independent. Mouse and multitouch events continue
through their pointer/contact adapters. Device listeners follow activity start/stop, independently from Vulkan surface lifetime.

`native.tests/AndroidGamepadTests.cpp` compiles the production adapter against the SDK's actual public header and fake JNI,
Android input and SDK boundaries. Its twelve scenarios cover slot identity, all button mappings, normalized axes, quick taps,
keyboard overlap, reset gates, hotplug, pointer routing and failure propagation. Device compatibility still requires a physical
controller run; these checks do not boot an emulator.

## Vulkan lifecycle check

The focused check runs only against the private `helengine_android_host` emulator. It verifies drawing, background/resume surface replacement, a temporary display resize, and activity teardown, then restores display overrides and GPU-debug settings and reopens the app. It uses logs and Vulkan results; it captures no screenshots.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\helengine-android\scripts\test-vulkan-host.ps1
```

For a game APK, pass `-ApkPath` and `-ExpectedStartupScene cube_test`; the check also requires scene draw batches, mesh draws and changing authored cube orientation. For a full disc whose normal startup is the splash, add `-StartupSceneOverride cube_test`. The debug activity accepts a validated scene catalog ID through the `helengine.startup_scene` string intent extra for focused cold-start tests. Use `-ValidationLayerPath` with an official x86-64 `libVkLayer_khronos_validation.so` to require Khronos validation. The script confirms the layer is loaded and rejects validation errors. The layer is supplied externally according to the [Android validation-layer documentation](https://developer.android.com/ndk/guides/graphics/validation-layer); it is not packaged in the APK. Optional `-Serial` and `-SdkRoot` select the private emulator. Reports go to `builds/helengine-android/native-host/output/` and logs to `native-host/boot/`.

Validated on 2026-10-13 on Android 35 AOSP x86-64 using SwiftShader Vulkan 1.2: authored Cube Test startup, rotation, background/resume, landscape resize and teardown passed with Khronos validation layer 1.4.363.0 loaded and no validation errors. Both ARM64 and x86-64 libraries, shaders, signatures and 16 KB alignment passed package verification; ARM64 was not boot-tested. Landscape renders at 1920x1080 using identity swapchain presentation; Android's compositor handles physical display rotation. The resize path uses Android's content-layout callback, since native producer buffers can retain previous swapchain dimensions.

For the entire exported catalog, `scripts/test-demodisc-scenes.ps1 -ApkPath <verified-apk>` cold-starts each scene and saves its startup log and result alongside the APK. Add `-ValidationLayerPath <layer>` to check all rendering paths with Khronos validation. It restores GPU debug settings and returns the app to its normal splash/menu startup. This check verifies successful scene initialization and presentation; it does not assert a game's win condition or visual parity with desktop.
