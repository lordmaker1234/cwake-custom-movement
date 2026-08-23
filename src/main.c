#include "cwake.h"

#include "Event.h"
#include "Input.h"
#include "Options.h"

int cw_menu_hotkey = CCKEY_HOME;

static void Cwake_OnInputDown(void* obj, int key, cc_bool repeating, struct InputDevice* device) {
  extern int waiting_for_hotkey;
  extern cc_bool cw_ui_is_open;

  if (waiting_for_hotkey) return;

  if (!repeating) {
    if (key == cw_menu_hotkey) {
      Cwake_UI_Toggle();
    } else if (key == 1 && cw_ui_is_open) {
      // 1 = ESCAPE
      Cwake_UI_Toggle();
    }
  }
}

static void CwakePlugin_Init(void) {
  cc_string msg;
  char buf[80];

  cw_menu_hotkey = Options_GetInt("cwake-hotkey", 0, 255, CCKEY_HOME);
  Event_Register_(&InputEvents.Down2, NULL, Cwake_OnInputDown);

  String_InitArray(msg, buf);
  String_AppendConst(&msg, "&fLoading Cwake 2.0pre");
  Chat_Add(&msg);

  String_AppendConst(&Server.AppName, " Cwake 2.0pre");

  Cwake_LoadConfig();
  Cwake_Profile_SetDefaults(&cw_active_profile);
  Cwake_Profile_Load(cw_default_profile_name, &cw_active_profile);

  cw_origVTABLE = Entities.CurPlayer->Base.VTABLE;
  cw_hookedVTABLE = *cw_origVTABLE;
  cw_hookedVTABLE.Tick = Cwake_Tick;
  Entities.CurPlayer->Base.VTABLE = &cw_hookedVTABLE;

  Cwake_Audio_Init();
  Cwake_UI_Init();
  Cwake_Hud_Init();
}

static void CwakePlugin_Free(void) {
  Event_Unregister_(&InputEvents.Down2, NULL, Cwake_OnInputDown);

  if (cw_origVTABLE) Entities.CurPlayer->Base.VTABLE = cw_origVTABLE;

  if (Camera.Active && cw_orig_GetView && Camera.Active->GetView == Cwake_GetView) {
    Camera.Active->GetView = cw_orig_GetView;
  }
}

PLUGIN_EXPORT int Plugin_ApiVersion = 1;
PLUGIN_EXPORT struct IGameComponent Plugin_Component = {CwakePlugin_Init, CwakePlugin_Free, NULL, NULL,
                                                        Cwake_OnNewMapLoaded};
