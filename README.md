# HelEngine Android platform

This first milestone registers Android in the editor and provides a native Android host scaffold. Game export, rendering, asset cooking, generated-core integration, input and audio are pending. Attempting material cooking or invoking the builder's game export produces an explicit unsupported-operation error.

## Register with the source editor

From the workspace root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\helengine-android\scripts\register-platform.ps1
```

The script publishes the .NET 9 metadata builder into `artifacts/builder/`, then adds Android to `helengine/user_settings/platforms.json` for the engine versions already registered there. It preserves other platform entries and can be run again to refresh the builder. Use `-EngineUserSettingsRoot`, `-HelengineRoot` and `-EngineVersion` to select another checkout or version. The registry is local engine configuration; it is not the project's `settings/platforms.json`.

Restart the editor after registration, then enable Android through the project's platform settings. Debug and Release profiles expose the native host scaffold. Registration requires .NET 9 or a newer compatible SDK; it does not require the Android SDK.

Inspect the metadata or run the focused integration checks:

```powershell
dotnet .\helengine-android\artifacts\builder\helengine.android.builder.dll --describe
dotnet test .\helengine-android\builder.tests\helengine.android.builder.tests.csproj
```

The tests use the engine's platform discovery, editor builder loader and editor selection model, and verify that incomplete export operations fail.

## Native Android project

Open `android/` in Android Studio. The project targets `arm64-v8a` devices and `x86_64` emulators, with minimum API 26 and compile/target API 35. It packages `libhelengine_android.so` as a `NativeActivity`. The activity logs window creation and destruction to `HelengineAndroid`; it does not render a scene or load game content.

The pinned toolchain is Android Gradle Plugin 8.11.1, Gradle 8.14.3, JDK 17, Android SDK platform 35, SDK Build Tools 35.0.0, NDK 27.0.12077973 and SDK CMake 3.22.1. The checked-in Gradle wrapper verifies the official Gradle distribution checksum. AGP's [compatibility documentation](https://developer.android.com/build/releases/agp-8-11-0-release-notes) specifies Gradle 8.13 or newer and JDK 17. NDK r27's flexible page-size option is enabled according to the [native page-size documentation](https://developer.android.com/guide/practices/page-sizes).

Install those SDK packages and accept their licenses using SDK Manager. The PowerShell build command accepts SDK/JDK paths explicitly, uses `ANDROID_HOME`/`ANDROID_SDK_ROOT` and `JAVA_HOME` when set, or finds the workspace-local toolchain under `builds/helengine-android/toolchain/`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\helengine-android\scripts\build-host.ps1
```

Optional parameters are `-SdkRoot`, `-JavaHome`, `-OutputDirectory`, `-WorkingDirectory` and `-GradleCacheRoot`. This build command restores its process environment afterwards and puts outputs and logs in `builds/helengine-android/native-host/` by default. It verifies APK signing, application identity, both native libraries and their machine types, exported `ANativeActivity_onCreate` entry points, and 16 KB APK/ELF alignment. On success it writes `output/helengine_android_host-debug.apk` and `output/host-build-report.json`. These checks verify the package and native entry points; running it on a device or emulator is a separate boot check.

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

Select a device with `adb -s <serial>` when more than one is connected. Expected log messages are `Android native host scaffold started.` and `Native window ready; rendering is pending.` The app should stay running without a native crash. A scene is not rendered at this milestone.

Validated on 2026-10-13: the debug APK passed signing, both ABI/entry-point checks, and 16 KB alignment checks. An Android 35 AOSP x86-64 emulator installed and launched it successfully; removing its task produced the native window-release and activity-destruction callbacks. ARM64 was compiled and inspected but was not boot-tested. Local package and boot evidence is in `builds/helengine-android/native-host/output/` and lifecycle logs are in `native-host/boot/`.
