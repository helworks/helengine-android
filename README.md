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

The pinned toolchain is Android Gradle Plugin 8.11.1, Gradle 8.14.3, JDK 17, Android SDK platform 35, NDK 27.0.12077973 and SDK CMake 3.22.1. AGP's [compatibility documentation](https://developer.android.com/build/releases/agp-8-11-0-release-notes) specifies Gradle 8.13 or newer and JDK 17. NDK r27's flexible page-size option is enabled according to the [native page-size documentation](https://developer.android.com/guide/practices/page-sizes).

Set `ANDROID_HOME` or create ignored `android/local.properties` with `sdk.dir` pointing to the SDK. Select JDK 17 using `JAVA_HOME`. Until a Gradle wrapper is generated, use the installed Gradle 8.14.3 distribution:

```powershell
gradle -p .\helengine-android\android assembleDebug
```

The scaffold APK is written to `android/app/build/outputs/apk/debug/app-debug.apk`. Native lifecycle setup follows Android's [NativeActivity sample](https://developer.android.com/ndk/samples/sample_na). Building this standalone host does not export an authored HelEngine game.
