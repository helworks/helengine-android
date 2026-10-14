#pragma once
#include "system/io/stream.hpp"
/// Existing engine content interface mirrored so tests compile the real Android adapter independently of generated core code.
class IContentStreamSource {
public:
    /// Allows ordinary polymorphic destruction of a host-owned source.
    virtual ~IContentStreamSource()=default;
    /// Transfers one newly opened stream to the caller.
    virtual Stream* OpenRead(std::string path)=0;
};
