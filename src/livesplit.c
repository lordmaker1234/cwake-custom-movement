#include "cwake.h"

#if defined(_WIN32)
#include <windows.h>

void (*cw_LiveSplit_SetSpeed)(cc_uint32 speed) = NULL;
cc_bool cw_send_livesplit = false;

cc_bool Cwake_LinkLiveSplit(void) {
  if (cw_LiveSplit_SetSpeed) return true;

  HMODULE mod = GetModuleHandleA("classicube_livesplit_plugin.dll");

  if (mod) {
    cw_LiveSplit_SetSpeed = (void (*)(cc_uint32))GetProcAddress(mod, "LiveSplit_SetCheckpointSpeed");
  }

  return cw_LiveSplit_SetSpeed != NULL;
}
#else
/* LiveSplit is currently Windows-only */
void (*cw_LiveSplit_SetSpeed)(cc_uint32 speed) = NULL;
cc_bool cw_send_livesplit = false;

cc_bool Cwake_LinkLiveSplit(void) {
  return false;
}
#endif