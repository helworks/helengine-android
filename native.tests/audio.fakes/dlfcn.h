#pragma once
#define RTLD_DEFAULT nullptr
/// Supplies only the API-28 audio attribute setters requested by the production backend.
void* dlsym(void*,const char* name);
