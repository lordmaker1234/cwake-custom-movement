#include "cwake.h"

#if defined(_WIN64)
#include <windows.h>
#elif defined(_LP64)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#endif

void (*cw_LiveSplit_SetSpeed)(cc_uint32 speed) = NULL;
cc_bool cw_send_livesplit = false;

cc_bool Cwake_LinkLiveSplit(void) {
  if (cw_LiveSplit_SetSpeed) return true;

#if defined(_WIN64)
  HMODULE mod = GetModuleHandleA("classicube_livesplit_plugin.dll");

  if (mod) {
    cw_LiveSplit_SetSpeed = (void (*)(cc_uint32))GetProcAddress(mod, "LiveSplit_SetCheckpointSpeed");
  }

#elif defined(_LP64)
  void* handle = dlopen("classicube_livesplit_plugin.so", RTLD_LAZY | RTLD_NOLOAD);
  if (handle) {
    cw_LiveSplit_SetSpeed = (void (*)(cc_uint32))dlsym(handle, "LiveSplit_SetCheckpointSpeed");
  }

#endif
  return cw_LiveSplit_SetSpeed != NULL;
}