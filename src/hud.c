#include "cwake.h"

#include "Gui.h"
#include "Screens.h"
#include "Graphics.h"
#include "Window.h"
#include "String_.h"
#include "Entity.h"
#include "ExtMath.h"
#include "Drawer2D.h"
#include "PackedCol.h"

static struct Screen cwake_hud_screen;
static struct Texture speedo_tex;
static struct FontDesc speedo_font;
static float last_hzSpeed = -1.0f;

extern float cw_top_speed;

static void DrawTexture(const struct Texture *tex) {
  if (!tex->ID) return;
  Gfx_BindTexture(tex->ID);

  struct VertexTextured vertices[4];
  float x = (float)tex->x;
  float y = (float)tex->y;
  float width = (float)tex->width;
  float height = (float)tex->height;

  vertices[0].x = x;
  vertices[0].y = y;
  vertices[0].z = 0;
  vertices[0].Col = PACKEDCOL_WHITE;
  vertices[0].U = tex->uv.u1;
  vertices[0].V = tex->uv.v1;

  vertices[1].x = x;
  vertices[1].y = y + height;
  vertices[1].z = 0;
  vertices[1].Col = PACKEDCOL_WHITE;
  vertices[1].U = tex->uv.u1;
  vertices[1].V = tex->uv.v2;

  vertices[2].x = x + width;
  vertices[2].y = y + height;
  vertices[2].z = 0;
  vertices[2].Col = PACKEDCOL_WHITE;
  vertices[2].U = tex->uv.u2;
  vertices[2].V = tex->uv.v2;

  vertices[3].x = x + width;
  vertices[3].y = y;
  vertices[3].z = 0;
  vertices[3].Col = PACKEDCOL_WHITE;
  vertices[3].U = tex->uv.u2;
  vertices[3].V = tex->uv.v1;

  Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
  GfxResourceID vb = Gfx_CreateDynamicVb(VERTEX_FORMAT_TEXTURED, 4);
  Gfx_BindDynamicVb(vb);
  Gfx_SetDynamicVbData(vb, vertices, 4);
  Gfx_DrawVb_IndexedTris(4);
  Gfx_DeleteDynamicVb(&vb);
}

static void CwakeHud_Render(void *screen, float delta) {
  if (!cw_active_profile.show_speedo || !Entities.CurPlayer) return;

  struct LocalPlayer *p = Entities.CurPlayer;
  float vx = p->Base.Velocity.x;
  float vz = p->Base.Velocity.z;
  float hzSpeed = Math_SqrtF(vx * vx + vz * vz);
  float realBPS = hzSpeed * 20.0f;

  if (realBPS > cw_top_speed) cw_top_speed = realBPS;

  static char last_speedo_color[16] = "";
  if (Math_AbsF(hzSpeed - last_hzSpeed) > 0.005f || strcmp(cw_active_profile.speedo_color, last_speedo_color) != 0) {
    last_hzSpeed = hzSpeed;
    strcpy(last_speedo_color, cw_active_profile.speedo_color);

    char buf[128];
    sprintf(buf, "&%sSpeed: %.2f (Top: %.2f)", cw_active_profile.speedo_color, realBPS, cw_top_speed);

    if (speedo_tex.ID) {
      Gfx_DeleteTexture(&speedo_tex.ID);
      speedo_tex.ID = 0;
    }

    cc_string str = String_FromReadonly(buf);
    struct DrawTextArgs args;
    args.text = str;
    args.font = &speedo_font;
    args.useShadow = true;
    Drawer2D_MakeTextTexture(&speedo_tex, &args);
  }

  if (speedo_tex.ID) {
    int x = 10, y = 10;

    if (cw_active_profile.speedo_pos % 3 == 0) {
      x = 10;
    } else if (cw_active_profile.speedo_pos % 3 == 1) {
      x = (Window_UI.Width / 2) - (speedo_tex.width / 2);
    } else if (cw_active_profile.speedo_pos % 3 == 2) {
      x = Window_UI.Width - speedo_tex.width - 10;
    }

    if (cw_active_profile.speedo_pos / 3 == 0) {
      y = 10;
    } else if (cw_active_profile.speedo_pos / 3 == 1) {
      y = (Window_UI.Height / 2) - (speedo_tex.height / 2);
    } else if (cw_active_profile.speedo_pos / 3 == 2) {
      y = Window_UI.Height - speedo_tex.height - 10;
    }
    speedo_tex.x = x;
    speedo_tex.y = y;
    DrawTexture(&speedo_tex);
  }
}

static void CwakeHud_Init(void *screen) {
  Font_Make(&speedo_font, 16, FONT_FLAGS_NONE);
}
static void CwakeHud_Free(void *screen) {
  Font_Free(&speedo_font);
  if (speedo_tex.ID) {
    Gfx_DeleteTexture(&speedo_tex.ID);
    speedo_tex.ID = 0;
  }
}
static void CwakeHud_ContextLost(void *screen) {
  if (speedo_tex.ID) {
    Gfx_DeleteTexture(&speedo_tex.ID);
    speedo_tex.ID = 0;
  }
}
static void CwakeHud_NullFunc(void *screen) {}
static void CwakeHud_NullUpdate(void *screen, float delta) {}
static int CwakeHud_FInput(void *screen, int key, struct InputDevice *device) {
  return 0;
}
static void CwakeHud_InputUp(void *screen, int key, struct InputDevice *device) {}
static int CwakeHud_FKeyPress(void *screen, char keyChar) {
  return 0;
}
static int CwakeHud_FText(void *screen, const cc_string *str) {
  return 0;
}
static int CwakeHud_FPointer(void *screen, int id, int x, int y) {
  return 0;
}
static void CwakeHud_PointerUp(void *screen, int id, int x, int y) {}
static int CwakeHud_FMouseScroll(void *screen, float delta) {
  return 0;
}

static const struct ScreenVTABLE cwake_hud_vtable = {
    CwakeHud_Init,         CwakeHud_NullUpdate, CwakeHud_Free,        CwakeHud_Render,
    CwakeHud_NullFunc,     CwakeHud_FInput,     CwakeHud_InputUp,     CwakeHud_FKeyPress,
    CwakeHud_FText,        CwakeHud_FPointer,   CwakeHud_PointerUp,   CwakeHud_FPointer,
    CwakeHud_FMouseScroll, CwakeHud_NullFunc,   CwakeHud_ContextLost, CwakeHud_NullFunc};

void Cwake_Hud_Init(void) {
  cwake_hud_screen.VTABLE = &cwake_hud_vtable;
  cwake_hud_screen.grabsInput = false;
  cwake_hud_screen.blocksWorld = false;
  cwake_hud_screen.closable = false;

  Gui_Add(&cwake_hud_screen, 11);
}
