#pragma once
/// Simulates the system capture gate while preserving the SDK's public native tracing shape.
bool ATrace_isEnabled();
/// Observes a synchronous begin on the calling thread.
void ATrace_beginSection(const char* name);
/// Observes a synchronous end on the same calling thread.
void ATrace_endSection();
