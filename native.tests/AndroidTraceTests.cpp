#include "../src/platform/android/diagnostics/AndroidTraceSection.hpp"
#include "../src/platform/android/runtime/AndroidAssetStream.hpp"
#include "../src/platform/android/runtime/AndroidProcessMemoryReader.hpp"
#include "trace.fakes/FakeTraceApi.hpp"
#include "memory.fakes/FakeMemoryApi.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <type_traits>

/// Verifies the actual tracing guard and traced native IO boundaries without a device or a Perfetto session.
class AndroidTraceTests {
public:
    /// Runs scope lifetime, capture-gate, thread pairing and production IO integration scenarios.
    static void Run() {
        Disabled(); Nested(); Exceptions(); CaptureGateChanges(); IndependentThreads(); InvalidLabels(); AssetReads(); AssetFailure(); MemoryCapture();
        std::cout<<"9 Android tracing scenarios passed.\n";
    }
private:
    /// Native scope type retains only its begin decision, with no stored name or object graph.
    using Section=helengine::android::AndroidTraceSection;
    /// Requires an observable tracing or IO contract to hold even in release builds.
    static void Check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
    /// Verifies every emitted SDK begin has an end and the main thread retains no orphan sections.
    static void Balanced() {
        Check(FakeTraceApi::Begins==FakeTraceApi::Ends && FakeTraceApi::InvalidEnds==0 && FakeTraceApi::Stack.empty(),"Tracing calls must be paired on their owning thread.");
    }
    /// Disabled guards avoid all begin/end events and retain no names, lists or heap storage.
    static void Disabled() {
        static_assert(sizeof(Section)==sizeof(bool));
        static_assert(!std::is_copy_constructible_v<Section> && !std::is_move_constructible_v<Section>);
        FakeTraceApi::Reset();
        for (int index=0;index<10000;++index) { const Section section("disabled"); }
        Check(FakeTraceApi::Gates==10000 && FakeTraceApi::Calls.empty() && FakeTraceApi::Begins==0 && FakeTraceApi::Ends==0,"Disabled scopes must emit no events or orphan ends.");
    }
    /// Ordinary nested spans close in exact stack order.
    static void Nested() {
        FakeTraceApi::Reset(true);
        { const Section outer("outer"); { const Section inner("inner"); } }
        Check(FakeTraceApi::Calls==std::vector<std::string>{"B:outer","B:inner","E:inner","E:outer"},"Nested spans must follow lexical begin/end order."); Balanced();
    }
    /// Exception unwinding must close child spans before their parent, preserving the original failure.
    static void Exceptions() {
        FakeTraceApi::Reset(true); bool caught=false;
        try { const Section outer("outer"); const Section inner("inner"); throw std::runtime_error("native failure"); }
        catch (const std::runtime_error& error) { caught=std::string(error.what())=="native failure"; }
        Check(caught && FakeTraceApi::Calls.back()=="E:outer","Scope cleanup must preserve a native error while closing spans."); Balanced();
    }
    /// A gate change never creates an unmatched SDK end for a scope that began while tracing was disabled.
    static void CaptureGateChanges() {
        FakeTraceApi::Reset(true);
        { const Section active("active"); FakeTraceApi::Enabled=false; { const Section disabled("disabled"); } }
        Check(FakeTraceApi::Begins==1 && FakeTraceApi::Ends==1,"A started scope must issue its matching end after the system gate turns off."); Balanced();
        FakeTraceApi::Reset();
        { const Section inactive("inactive"); FakeTraceApi::Enabled=true; { const Section active("active"); } }
        Check(FakeTraceApi::Begins==1 && FakeTraceApi::Ends==1,"A disabled parent must not end a child or fabricate a later begin."); Balanced();
    }
    /// Simulates parallel scene-loading workers without transferring a trace guard between threads.
    static void Worker() { for (int index=0;index<100;++index) { const Section outer("worker"); const Section inner("asset"); } }
    /// Each thread pairs its own spans while the test process observes balanced total SDK calls.
    static void IndependentThreads() {
        FakeTraceApi::Reset(true);
        { const Section main("main"); std::thread first(Worker),second(Worker); first.join(); second.join(); }
        Check(FakeTraceApi::Begins==401 && FakeTraceApi::Ends==401 && FakeTraceApi::Calls.back()=="E:main","Workers must not close another thread's parent span."); Balanced();
    }
    /// Missing labels fail before touching SDK state rather than silently substituting a default section.
    static void InvalidLabels() {
        FakeTraceApi::Reset(true); int caught=0;
        try { const Section invalid(nullptr); } catch (const std::invalid_argument&) { ++caught; }
        try { const Section invalid(""); } catch (const std::invalid_argument&) { ++caught; }
        Check(caught==2 && FakeTraceApi::Gates==0 && FakeTraceApi::Calls.empty(),"Invalid trace labels must fail before emitting SDK calls.");
    }
    /// Actual asset streams trace native buffer fills rather than emitting a span for each deserialized byte.
    static void AssetReads() {
        FakeTraceApi::Reset(true); AAssetManager manager{}; auto& bytes=manager.Files["cooked/test.bin"]; bytes.resize(50000);
        { helengine::android::AndroidAssetStream stream(&manager,"cooked/test.bin");
            for (int index=0;index<50000;++index) Check(stream.InternalReadByte()==0,"Traced asset data must retain its contents.");
            Check(FakeTraceApi::Begins==5 && manager.Reads==4,"Native tracing must follow one open and four buffered reads, not 50000 byte reads.");
            stream.SetPosition(10); Check(FakeTraceApi::Begins==6,"An out-of-cache seek must trace its native call.");
            Check(stream.InternalReadByte()==0,"Read after a traced seek must remain valid.");
            const int before=FakeTraceApi::Begins; stream.SetPosition(20); Check(FakeTraceApi::Begins==before,"A cached seek must avoid native trace events.");
        }
        Check(manager.Handles==0,"Tracing must not change asset ownership."); Balanced();
    }
    /// Failing native IO ends its trace scope before propagating the actual stream error.
    static void AssetFailure() {
        FakeTraceApi::Reset(true); AAssetManager manager{}; manager.Files["cooked/test.bin"]={1,2}; manager.FailRead=true; bool failed=false;
        try { helengine::android::AndroidAssetStream stream(&manager,"cooked/test.bin"); stream.InternalReadByte(); }
        catch (const std::runtime_error&) { failed=true; }
        Check(failed && manager.Handles==0 && FakeTraceApi::Begins==2,"Traced read failures must still close spans and native handles."); Balanced();
    }
    /// The real memory reader emits one balanced sampling span without modifying native failure behavior.
    static void MemoryCapture() {
        FakeTraceApi::Reset(true); FakeMemoryApi::Reset(); helengine::android::AndroidProcessMemoryReader reader;
        Check(reader.Capture(false).ResidentBytes==819200,"Tracing must preserve sampled process memory.");
        FakeMemoryApi::ReadFails=true; bool failed=false; try { reader.Capture(false); } catch (const std::runtime_error&) { failed=true; }
        Check(failed && FakeMemoryApi::Handles==0 && FakeTraceApi::Calls==std::vector<std::string>{"B:HelEngine.Memory.Capture","E:HelEngine.Memory.Capture","B:HelEngine.Memory.Capture","E:HelEngine.Memory.Capture"},"Sampling failures must unwind the same trace label as successful captures."); Balanced();
    }
};
/// Reports any tracing assertion failure as a nonzero process exit status.
int main() { try { AndroidTraceTests::Run(); return 0; } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; } }
