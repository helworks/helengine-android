#pragma once
#define RTLD_DEFAULT nullptr
/// Resolves fake function addresses using the same dynamic lookup boundary as supported Android devices.
void* dlsym(void* library,const char* name);
