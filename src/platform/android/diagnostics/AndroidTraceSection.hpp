#pragma once
#include <android/trace.h>
#include <stdexcept>

namespace helengine::android {
    /// Emits one synchronous Perfetto/ATrace CPU span with lexical ownership on its calling thread.
    class AndroidTraceSection final {
    public:
        /// Checks the system tracing gate before emitting a fixed label; disabled scopes create no event or object graph.
        explicit AndroidTraceSection(const char* name) {
            if (!name || !*name) throw std::invalid_argument("Android trace sections require a nonempty label.");
            Active=ATrace_isEnabled();
            if (Active) ATrace_beginSection(name);
        }
        /// Balances an emitted begin even while unwinding an exception or after the capture gate changes.
        ~AndroidTraceSection() noexcept { if (Active) ATrace_endSection(); }
        /// A section cannot transfer its matching end to another scope or thread by copying.
        AndroidTraceSection(const AndroidTraceSection&)=delete;
        /// An existing scope cannot acquire another section's matching end.
        AndroidTraceSection& operator=(const AndroidTraceSection&)=delete;
        /// A section cannot transfer its matching end to another scope or thread by moving.
        AndroidTraceSection(AndroidTraceSection&&)=delete;
        /// An existing scope cannot acquire another section's matching end by moving.
        AndroidTraceSection& operator=(AndroidTraceSection&&)=delete;
    private:
        /// Remembers whether this scope emitted a begin rather than querying a potentially changed gate on teardown.
        bool Active=false;
    };
}
