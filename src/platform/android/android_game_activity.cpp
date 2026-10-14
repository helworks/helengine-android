#include "runtime/AndroidGameHost.hpp"
#include "runtime/native_exceptions.hpp"
#include <android_native_app_glue.h>
#include <android/log.h>

namespace {
    /// Holds all native runtime state on the glue thread where lifecycle commands and drawing are serialized.
    struct AndroidGameActivity {
        android_app* App;
        helengine::android::AndroidVulkanRenderer Renderer;
        helengine::android::AndroidGameHost Game;
        bool WindowReady=false;
        bool Resumed=false;
        bool Failed=false;
        explicit AndroidGameActivity(android_app* app) : App(app),Renderer(app->activity->assetManager),Game(app->activity,Renderer) {}
    };
    /// Records the real native failure and requests Android teardown.
    void Fail(AndroidGameActivity& state,const char* message) {
        state.Failed=true;
        __android_log_print(ANDROID_LOG_ERROR,"HelengineAndroid","Android game failed: %s",message);
        ANativeActivity_finish(state.App->activity);
    }
    /// Handles synchronous window commands on the render thread, preserving the core when its surface is replaced.
    void HandleCommand(android_app* app,int32_t command) {
        auto& state=*static_cast<AndroidGameActivity*>(app->userData);
        try {
            if (command==APP_CMD_INIT_WINDOW && app->window) {
                state.Renderer.AttachWindow(app->window); state.WindowReady=true; state.Game.Initialize();
            } else if (command==APP_CMD_TERM_WINDOW && state.WindowReady) {
                state.WindowReady=false; state.Game.GetInput().Reset(); state.Renderer.DetachWindow();
            } else if (command==APP_CMD_RESUME) { state.Resumed=true; state.Game.Resume(); }
            else if (command==APP_CMD_PAUSE || command==APP_CMD_STOP) { state.Resumed=false; state.Game.Suspend(); }
            else if (command==APP_CMD_LOST_FOCUS || command==APP_CMD_INPUT_CHANGED) state.Game.GetInput().Reset();
            else if (command==APP_CMD_CONFIG_CHANGED && state.WindowReady) state.Game.Resize();
            else if (command==APP_CMD_CONTENT_RECT_CHANGED && state.WindowReady) {
                int width=app->contentRect.right-app->contentRect.left,height=app->contentRect.bottom-app->contentRect.top;
                if (width>0 && height>0) { state.Renderer.SetContentExtent(width,height); state.Game.Resize(); }
            } else if (command==APP_CMD_WINDOW_RESIZED && state.WindowReady) { state.Renderer.Resize(); state.Game.Resize(); }
        } catch (::Exception* error) { Fail(state,error->what()); delete error; }
        catch (const std::exception& error) { Fail(state,error.what()); }
    }
    /// Routes native pointer events through the engine input adapter.
    int32_t HandleInput(android_app* app,AInputEvent* event) {
        return static_cast<AndroidGameActivity*>(app->userData)->Game.GetInput().HandleEvent(event) ? 1 : 0;
    }
}

/// Runs the generated engine on Android's native glue thread without drawing from UI-thread callbacks.
extern "C" void android_main(android_app* app) {
    try {
        AndroidGameActivity state(app); app->userData=&state; app->onAppCmd=HandleCommand; app->onInputEvent=HandleInput;
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Android authored game host started: Vulkan.");
        while (!app->destroyRequested) {
            int events=0; android_poll_source* source=nullptr;
            int timeout=state.WindowReady && state.Resumed && !state.Failed ? 0 : -1;
            while (ALooper_pollOnce(timeout,nullptr,&events,reinterpret_cast<void**>(&source))>=0) {
                if (source) source->process(app,source);
                if (app->destroyRequested) break;
                timeout=0;
            }
            if (app->destroyRequested) break;
            if (state.WindowReady && state.Resumed && !state.Failed) {
                try { state.Game.Frame(); }
                catch (::Exception* error) { Fail(state,error->what()); delete error; }
                catch (const std::exception& error) { Fail(state,error.what()); }
            }
        }
        if (state.WindowReady) { state.WindowReady=false; state.Renderer.DetachWindow(); }
        app->userData=nullptr;
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Android authored game host destroyed.");
    } catch (::Exception* error) {
        __android_log_print(ANDROID_LOG_ERROR,"HelengineAndroid","Android engine initialization failed: %s",error->what()); delete error; ANativeActivity_finish(app->activity);
    } catch (const std::exception& error) {
        __android_log_print(ANDROID_LOG_ERROR,"HelengineAndroid","Android host initialization failed: %s",error.what()); ANativeActivity_finish(app->activity);
    }
}
