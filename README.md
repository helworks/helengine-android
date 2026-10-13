# HelEngine Android platform

This platform registers Android in the editor and exports debug APKs containing the generated native engine and cooked project content. The DemoDisc Cube Test runs its authored camera, spinning cube, directional light, fonts and interface through Vulkan. The standalone diagnostic triangle host remains available when no generated core is supplied. Audio, textured 3D materials, shadows and the wider DemoDisc scene set remain future work.

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

The tests exercise platform discovery, editor selection, cooked material colors and rejection of exports without a generated core.

## Export an authored game

Enable Android in the project, choose its startup scene and select the graphics option `screen-orientation` (`unspecified`, `portrait` or `landscape`). Landscape maps to Android's `sensorLandscape` activity setting. From the workspace root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc -Platform android -BuildProfile debug -Output .\builds\helengine-android\cube-test\output
```

The builder stages cooked assets, emits their APK inventory and compiles the engine's generated C++ runtime. The game reads its generated startup catalog and loads the real cooked scene through the shared scene manager. Material cooking currently supports colored `standard-shader` materials without textures; unsupported material inputs fail explicitly. Generated C++ enums must preserve their C# underlying integer type, including unsigned values.

## Viewport density

Android reads the activity's current [DisplayMetrics.densityDpi](https://developer.android.com/reference/android/util/DisplayMetrics#densityDpi) through JNI and reports it with the shared renderer's `SetWindowDensityDpi` API, including configuration changes. Reading the current activity resources avoids retaining the density of the native asset manager captured at startup. Scaling is owned by the engine's `ViewportComponent`:

- `ScalingMode = ViewportComponent.DensityScalingMode` enables density conversion.
- `PixelsPerCentimeter` specifies authored pixels per centimeter, initially `160 / 2.54`.
- `UiScale` is an additional size multiplier, initially `1`.

The viewport computes `DPI / (PixelsPerCentimeter * 2.54) * UiScale`. At 420 DPI the initial configuration produces a 2.625 scale. Rendering and pointer input retain physical pixels; the viewport scales layout, glyphs, decorations and hit areas together. Use edge anchors for panels and titles so their positions follow the available viewport bounds. Set `NativeFontPixels = Disabled` for glyphs that should follow viewport scale; native pixel fonts explicitly retain their cooked pixel size. Host-reported logical DPI gives an approximate physical size, consistent with Android's [density documentation](https://developer.android.com/training/multiscreen/screendensities).

DemoDisc's `menu.configure-cube-test-density` editor command adds Android-only settings to the existing Cube Test and its shared instruction Blueprint through an authoring transaction. Android uses the Blueprint panel with density scaling and edge anchors; the legacy scene instruction viewport is excluded to prevent duplicate panels. The Cube Test factory retains that selection when the scene is regenerated. Common scene content and other platform overrides remain authored.

The Cube Test uses `UiScale = 0.5625` for its Android interface viewports. At 420 DPI the effective scale is 1.4765625, producing an approximately 650x348 instruction panel. Fonts, icons and the separate light-state indicator use the same authored size multiplier.

## Native Android project

Open `android/` in Android Studio. The project targets `arm64-v8a` devices and `x86_64` emulators, with minimum API 26 and compile/target API 35, and requires Vulkan 1.0. It packages `libhelengine_android.so` as a `NativeActivity`. With generated content it runs the engine update/draw loop using native app glue; otherwise it draws the diagnostic triangle. Both paths log device selection, swapchain creation, presentation and teardown to `HelengineAndroid`. Surface replacement releases presentation resources while retaining the running engine scene.

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
adb shell am start -W -n com.helengine.android.scaffold/android.app.NativeActivity
adb logcat -d -s HelengineAndroid:I
```

Select a device with `adb -s <serial>` when more than one is connected. A standalone probe logs `Vulkan triangle frame presented:`. An exported game logs its generated startup scene, extracted asset inventory and `Authored runtime update:` with camera and mesh counts. The Cube Test should show its spinning cube and interface over the scene's cornflower-blue background.

## Vulkan lifecycle check

The focused check runs only against the private `helengine_android_host` emulator. It verifies drawing, background/resume surface replacement, a temporary display resize, and activity teardown, then restores display overrides and GPU-debug settings and reopens the app. It uses logs and Vulkan results; it captures no screenshots.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\helengine-android\scripts\test-vulkan-host.ps1
```

For a game APK, pass `-ApkPath` and `-ExpectedStartupScene cube_test`; the check also requires scene draw batches, mesh draws and changing authored cube orientation. Use `-ValidationLayerPath` with an official x86-64 `libVkLayer_khronos_validation.so` to require Khronos validation. The script confirms the layer is loaded and rejects validation errors. The layer is supplied externally according to the [Android validation-layer documentation](https://developer.android.com/ndk/guides/graphics/validation-layer); it is not packaged in the APK. Optional `-Serial` and `-SdkRoot` select the private emulator. Reports go to `builds/helengine-android/native-host/output/` and logs to `native-host/boot/`.

Validated on 2026-10-13 on Android 35 AOSP x86-64 using SwiftShader Vulkan 1.2: authored Cube Test startup, rotation, background/resume, landscape resize and teardown passed with Khronos validation layer 1.4.363.0 loaded and no validation errors. Both ARM64 and x86-64 libraries, shaders, signatures and 16 KB alignment passed package verification; ARM64 was not boot-tested. Landscape renders at 1920x1080 using identity swapchain presentation; Android's compositor handles physical display rotation. The resize path uses Android's content-layout callback, since native producer buffers can retain previous swapchain dimensions.
