#include "cwake.h"
#include "ui_font.h"

#include "Bitmap.h"
#include "Event.h"
#include "Graphics.h"
#include "Gui.h"
#include "Input.h"
#include "Options.h"
#include "Window.h"

#include <stdio.h>
#include <stdlib.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear.h"

static const char *GetKeyName(int key) {
// clang-format off
  static const char *const names[] = { "NONE",
    "F1", "F2", "F3", "F4", "F5", "F6",
    "F7", "F8", "F9", "F10", "F11", "F12",
    "GRAVE", "MINUS", "PLUS", 
    "LBRACKET", "RBRACKET", "SLASH", "SEMICOLON", "APOSTROPHE",
    "COMMA", "PERIOD", "BACKSLASH",
    "LSHIFT", "RSHIFT", "LCONTROL", "RCONTROL",
    "LALT", "RALT", "LWIN", "RWIN", 
    "UP", "DOWN", "LEFT", "RIGHT",
    "0", "1", "2", "3", "4",
    "5", "6", "7", "8", "9",
    "INSERT", "DELETE", "HOME", "END",
    "PRIOR","NEXT","MENU",
    "A", "B", "C", "D", "E", "F", "G", "H", "I",
    "J", "K", "L", "M", "N", "O", "P", "Q", "R",
    "S", "T", "U", "V", "W", "X", "Y", "Z",
    "RETURN", "SPACE","BACK","TAB","CAPITAL",
    "SCROLL", "PRINT", "PAUSE", "NUMLOCK",
    "NUMPAD0", "NUMPAD1", "NUMPAD2", "NUMPAD3", "NUMPAD4",
    "NUMPAD5", "NUMPAD6", "NUMPAD7", "NUMPAD8", "NUMPAD9",
    "DIVIDE", "MULTIPLY", "SUBTRACT", "ADD", "DECIMAL", "NUMPADENTER"
  };
// clang-format on

  if (key >= 0 && key < (sizeof(names) / sizeof(names[0]))) {
    return names[key];
  }
  return "Unknown";
}


static struct nk_context ctx;
static struct nk_font_atlas atlas;
static GfxResourceID cw_font_tex = NULL;
static GfxResourceID cw_ui_vb_col = NULL;
static GfxResourceID cw_ui_vb_tex = NULL;

static int cw_mouse_x = 0;
static int cw_mouse_y = 0;
static int cw_mouse_l_down = 0;
static float cw_scroll_delta = 0.0f;
static char cw_text_buf[256];
static int cw_text_len = 0;
cc_bool cw_ui_is_open = false;
static float cw_status_timer = 0.0f;
static char cw_status_msg[256] = {0};

static int cw_save_physics = 1;
static int cw_save_sounds = 1;
static int cw_save_ui = 1;

typedef enum { TAB_PHYSICS, TAB_UI, TAB_SOUNDS, TAB_PROFILES } CwakeUITab;
static CwakeUITab current_tab = TAB_PHYSICS;
static int cw_physics_enabled = 1;
static float cw_maxtilt = 15.0f;
static float cw_tiltmultiplier = 1.5f;

static int speedo_pos = 4;
static int speedo_color = 0;
int waiting_for_hotkey = 0;

static char profile_selected[64] = "";
static int profiles_scanned = 0;

static void Cwake_SetStatus(const char* msg) {
  strncpy(cw_status_msg, msg, 63);
  cw_status_msg[63] = '\0';
  cw_status_timer = 2.0f;
}
static void Cwake_UI_AllocateGPUResources(void) {
  struct nk_font* font;
  const void* image;
  int w, h;
  struct Bitmap bmp;
  nk_font_atlas_init_default(&atlas);
  nk_font_atlas_begin(&atlas);

  struct nk_font_config cfg = nk_font_config(14.0f);
  cfg.oversample_h = 1;
  cfg.oversample_v = 1;
  cfg.pixel_snap = nk_true;
  font = nk_font_atlas_add_from_memory(&atlas, (void*)cwake_custom_font, cwake_custom_font_size, 14.0f, &cfg);

  image = nk_font_atlas_bake(&atlas, &w, &h, NK_FONT_ATLAS_ALPHA8);
  BitmapCol* pixels = (BitmapCol*)malloc(w * h * sizeof(BitmapCol));
  const unsigned char* src = (const unsigned char*)image;

  for (int i = 0; i < w * h; i++) {
    pixels[i] = BitmapCol_Make(255, 255, 255, src[i]);
  }
  Bitmap_Init(bmp, w, h, pixels);
  cw_font_tex = Gfx_CreateTexture(&bmp, 0, false);
  free(pixels);
  nk_font_atlas_end(&atlas, nk_handle_id((int)(cc_uintptr)cw_font_tex), NULL);
  nk_init_default(&ctx, &font->handle);

  cw_ui_vb_col = Gfx_CreateDynamicVb(VERTEX_FORMAT_COLOURED, 2048);
  cw_ui_vb_tex = Gfx_CreateDynamicVb(VERTEX_FORMAT_TEXTURED, 4096);
}

static struct nk_color UIntToColor(unsigned int c) {
  return nk_rgba((c >> 24) & 0xFF, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}
static unsigned int ColorToUInt(struct nk_color c) {
  return (c.r << 24) | (c.g << 16) | (c.b << 8) | c.a;
}
static struct nk_colorf UIntToColorF(unsigned int c) {
  struct nk_colorf cf;
  cf.r = ((c >> 24) & 0xFF) / 255.0f;
  cf.g = ((c >> 16) & 0xFF) / 255.0f;
  cf.b = ((c >> 8) & 0xFF) / 255.0f;
  cf.a = (c & 0xFF) / 255.0f;
  return cf;
}
static unsigned int ColorFToUInt(struct nk_colorf cf) {
  unsigned int r = (unsigned int)(cf.r * 255.0f);
  unsigned int g = (unsigned int)(cf.g * 255.0f);
  unsigned int b = (unsigned int)(cf.b * 255.0f);
  unsigned int a = (unsigned int)(cf.a * 255.0f);
  return (r << 24) | (g << 16) | (b << 8) | a;
}

static void Cwake_ApplyTheme(struct nk_context* ctx, CwakeProfile* prof) {
  struct nk_color bg = UIntToColor(prof->ui_bg);
  struct nk_color text = UIntToColor(prof->ui_text);
  struct nk_color accent = UIntToColor(prof->ui_accent);
  struct nk_color button = UIntToColor(prof->ui_button);

  struct nk_color bg_light =
      nk_rgba(bg.r < 215 ? bg.r + 40 : 255, bg.g < 215 ? bg.g + 40 : 255, bg.b < 215 ? bg.b + 40 : 255, bg.a);
  struct nk_color bg_dark =
      nk_rgba(bg.r > 20 ? bg.r - 20 : 0, bg.g > 20 ? bg.g - 20 : 0, bg.b > 20 ? bg.b - 20 : 0, bg.a);
  struct nk_color accent_hover = nk_rgba(accent.r < 225 ? accent.r + 30 : 255, accent.g < 225 ? accent.g + 30 : 255,
                                         accent.b < 225 ? accent.b + 30 : 255, accent.a);
  struct nk_color accent_active = nk_rgba(accent.r < 195 ? accent.r + 60 : 255, accent.g < 195 ? accent.g + 60 : 255,
                                          accent.b < 195 ? accent.b + 60 : 255, accent.a);

  struct nk_color button_hover = nk_rgba(button.r < 225 ? button.r + 30 : 255, button.g < 225 ? button.g + 30 : 255,
                                         button.b < 225 ? button.b + 30 : 255, button.a);
  struct nk_color button_active = nk_rgba(button.r < 195 ? button.r + 60 : 255, button.g < 195 ? button.g + 60 : 255,
                                          button.b < 195 ? button.b + 60 : 255, button.a);

  struct nk_color table[NK_COLOR_COUNT];
  table[NK_COLOR_TEXT] = text;
  table[NK_COLOR_WINDOW] = bg;
  table[NK_COLOR_HEADER] = bg_dark;
  table[NK_COLOR_BORDER] = bg_light;
  table[NK_COLOR_BUTTON] = button;
  table[NK_COLOR_BUTTON_HOVER] = button_hover;
  table[NK_COLOR_BUTTON_ACTIVE] = button_active;
  table[NK_COLOR_TOGGLE] = bg_light;
  table[NK_COLOR_TOGGLE_HOVER] = accent;
  table[NK_COLOR_TOGGLE_CURSOR] = text;
  table[NK_COLOR_SELECT] = bg_light;
  table[NK_COLOR_SELECT_ACTIVE] = accent;
  table[NK_COLOR_SLIDER] = bg_light;
  table[NK_COLOR_SLIDER_CURSOR] = accent;
  table[NK_COLOR_SLIDER_CURSOR_HOVER] = accent_hover;
  table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = accent_active;
  table[NK_COLOR_PROPERTY] = bg;
  table[NK_COLOR_EDIT] = bg;
  table[NK_COLOR_EDIT_CURSOR] = text;
  table[NK_COLOR_COMBO] = bg_light;
  table[NK_COLOR_CHART] = bg;
  table[NK_COLOR_CHART_COLOR] = accent;
  table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = accent_hover;
  table[NK_COLOR_SCROLLBAR] = bg_light;
  table[NK_COLOR_SCROLLBAR_CURSOR] = accent;
  table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = accent_hover;
  table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = accent_active;
  table[NK_COLOR_TAB_HEADER] = bg;
  nk_style_from_table(ctx, table);

  ctx->style.property.padding = nk_vec2(4, 4);
  ctx->style.combo.button_padding = nk_vec2(8, 8);
  ctx->style.combo.spacing = nk_vec2(4, 4);
  ctx->style.window.border = 0.0f;
}
static void Cwake_UI_FreeGPUResources(void) {
  if (cw_ui_vb_col) {
    Gfx_DeleteDynamicVb(&cw_ui_vb_col);
    cw_ui_vb_col = NULL;
  }
  if (cw_ui_vb_tex) {
    Gfx_DeleteDynamicVb(&cw_ui_vb_tex);
    cw_ui_vb_tex = NULL;
  }
  if (cw_font_tex) {
    Gfx_DeleteTexture(&cw_font_tex);
    cw_font_tex = NULL;
  }
  nk_font_atlas_clear(&atlas);
  nk_free(&ctx);
}
static void Cwake_OnContextLost(void* obj) {
  Cwake_UI_FreeGPUResources();
}
static void Cwake_OnContextRecreated(void* obj) {
  Cwake_UI_AllocateGPUResources();
}

static void Cwake_Draw2DFlat(float x, float y, float w, float h, PackedCol col) {
  struct VertexColoured verts[4];
  verts[0].x = x;
  verts[0].y = y;
  verts[0].z = 0.0f;
  verts[0].Col = col;
  verts[1].x = x + w;
  verts[1].y = y;
  verts[1].z = 0.0f;
  verts[1].Col = col;
  verts[2].x = x + w;
  verts[2].y = y + h;
  verts[2].z = 0.0f;
  verts[2].Col = col;
  verts[3].x = x;
  verts[3].y = y + h;
  verts[3].z = 0.0f;
  verts[3].Col = col;
  Gfx_SetVertexFormat(VERTEX_FORMAT_COLOURED);
  Gfx_SetTexturing(false);
  Gfx_SetDepthTest(false);
  Gfx_SetAlphaBlending(true);

  Gfx_BindDynamicVb(cw_ui_vb_col);
  Gfx_SetDynamicVbData(cw_ui_vb_col, verts, 4);
  Gfx_DrawVb_IndexedTris(4);
}

static void Cwake_Draw2DFlatMultiColor(float x, float y, float w, float h, PackedCol tl, PackedCol tr, PackedCol br,
                                       PackedCol bl) {
  struct VertexColoured verts[4];
  verts[0].x = x;
  verts[0].y = y;
  verts[0].z = 0.0f;
  verts[0].Col = tl;
  verts[1].x = x + w;
  verts[1].y = y;
  verts[1].z = 0.0f;
  verts[1].Col = tr;
  verts[2].x = x + w;
  verts[2].y = y + h;
  verts[2].z = 0.0f;
  verts[2].Col = br;
  verts[3].x = x;
  verts[3].y = y + h;
  verts[3].z = 0.0f;
  verts[3].Col = bl;
  Gfx_SetVertexFormat(VERTEX_FORMAT_COLOURED);
  Gfx_SetTexturing(false);
  Gfx_SetDepthTest(false);
  Gfx_SetAlphaBlending(true);

  Gfx_BindDynamicVb(cw_ui_vb_col);
  Gfx_SetDynamicVbData(cw_ui_vb_col, verts, 4);
  Gfx_DrawVb_IndexedTris(4);

  Gfx_SetDepthTest(true);
  Gfx_SetAlphaBlending(false);
  Gfx_SetTexturing(true);
}

static void Cwake_Draw2DFlatTriangle(float x1, float y1, float x2, float y2, float x3, float y3, PackedCol col) {
  struct VertexColoured verts[4];
  verts[0].x = x1;
  verts[0].y = y1;
  verts[0].z = 0.0f;
  verts[0].Col = col;
  verts[1].x = x2;
  verts[1].y = y2;
  verts[1].z = 0.0f;
  verts[1].Col = col;
  verts[2].x = x3;
  verts[2].y = y3;
  verts[2].z = 0.0f;
  verts[2].Col = col;
  verts[3].x = x3;
  verts[3].y = y3;
  verts[3].z = 0.0f;
  verts[3].Col = col;

  Gfx_SetVertexFormat(VERTEX_FORMAT_COLOURED);
  Gfx_SetTexturing(false);
  Gfx_SetDepthTest(false);
  Gfx_SetAlphaBlending(true);

  Gfx_BindDynamicVb(cw_ui_vb_col);
  Gfx_SetDynamicVbData(cw_ui_vb_col, verts, 4);
  Gfx_DrawVb_IndexedTris(4);

  Gfx_SetDepthTest(true);
  Gfx_SetAlphaBlending(false);
  Gfx_SetTexturing(true);
}
static void Cwake_Draw2DTextured(float x, float y, float w, float h, float u1, float v1, float u2, float v2,
                                 PackedCol col) {
  struct VertexTextured verts[4];

  verts[0].x = x;
  verts[0].y = y;
  verts[0].z = 0.0f;
  verts[0].Col = col;
  verts[0].U = u1;
  verts[0].V = v1;
  verts[1].x = x + w;
  verts[1].y = y;
  verts[1].z = 0.0f;
  verts[1].Col = col;
  verts[1].U = u2;
  verts[1].V = v1;
  verts[2].x = x + w;
  verts[2].y = y + h;
  verts[2].z = 0.0f;
  verts[2].Col = col;
  verts[2].U = u2;
  verts[2].V = v2;
  verts[3].x = x;
  verts[3].y = y + h;
  verts[3].z = 0.0f;
  verts[3].Col = col;
  verts[3].U = u1;
  verts[3].V = v2;
  Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
  Gfx_SetTexturing(true);
  Gfx_BindTexture(cw_font_tex);
  Gfx_SetDepthTest(false);
  Gfx_SetAlphaBlending(true);
  Gfx_BindDynamicVb(cw_ui_vb_tex);
  Gfx_SetDynamicVbData(cw_ui_vb_tex, verts, 4);
  Gfx_DrawVb_IndexedTris(4);
  Gfx_SetDepthTest(true);
  Gfx_SetAlphaBlending(false);
}

static int FindSoundIndex(const char* sound_name) {
  if (!cw_sound_file_ptrs) return 0;
  for (int i = 0; i < cw_sound_count; i++) {
    if (strcmp(cw_sound_file_ptrs[i], sound_name) == 0) return i;
  }
  return 0;
}

static void GetProfilePreview(const char* name, char* out_buf) {
  char filepath[256];
  sprintf(filepath, "plugins/cwake/profiles/%s.ini", name);
  FILE* f = fopen(filepath, "r");
  if (!f) {
    strcpy(out_buf, "File missing or empty");
    return;
  }
  int has_phys = 0, has_ui = 0, has_snd = 0;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    if (strstr(line, "[Physics]")) has_phys = 1;
    if (strstr(line, "[UI]")) has_ui = 1;
    if (strstr(line, "[Sounds]")) has_snd = 1;
  }
  fclose(f);
  sprintf(out_buf, "Contains: %s%s%s", has_phys ? "Physics " : "", has_ui ? "UI " : "", has_snd ? "Sounds" : "");
  if (!has_phys && !has_ui && !has_snd) strcpy(out_buf, "Contains: Nothing");
}

/* Menu Layout */
static void Cwake_DrawNuklear(void) {
  struct nk_color b_col = UIntToColor(cw_active_profile.ui_bg);
  struct nk_color bg_dark = nk_rgba(b_col.r > 20 ? b_col.r - 20 : 0, b_col.g > 20 ? b_col.g - 20 : 0,
                                    b_col.b > 20 ? b_col.b - 20 : 0, b_col.a);
  struct nk_color accent = UIntToColor(cw_active_profile.ui_accent);
  struct nk_color accent_hover = nk_rgba(accent.r < 225 ? accent.r + 30 : 255, accent.g < 225 ? accent.g + 30 : 255,
                                         accent.b < 225 ? accent.b + 30 : 255, accent.a);
  struct nk_color accent_active = nk_rgba(accent.r < 195 ? accent.r + 60 : 255, accent.g < 195 ? accent.g + 60 : 255,
                                          accent.b < 195 ? accent.b + 60 : 255, accent.a);
  if (nk_begin(&ctx, "Cwake Configuration", nk_rect(50, 50, 600, 550),
               NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)) {

    nk_layout_row_dynamic(&ctx, 30, 4);
    if (nk_button_label(&ctx, "Physics")) current_tab = TAB_PHYSICS;
    if (nk_button_label(&ctx, "UI")) current_tab = TAB_UI;
    if (nk_button_label(&ctx, "Sounds")) current_tab = TAB_SOUNDS;
    if (nk_button_label(&ctx, "Profiles")) current_tab = TAB_PROFILES;

    nk_layout_row_dynamic(&ctx, 5, 1);
    nk_spacing(&ctx, 1);

    if (cw_status_timer > 0.0f) {
      cw_status_timer -= 0.016f;
      nk_layout_row_dynamic(&ctx, 20, 1);
      nk_label_colored(&ctx, cw_status_msg, NK_TEXT_CENTERED, nk_rgb(100, 255, 100));
    }

    /* --- TAB 1: PHYSICS --- */
    if (current_tab == TAB_PHYSICS) {
      nk_layout_row_dynamic(&ctx, 30, 1);
      if (cw_active_profile.enable_physics) {
        nk_style_push_style_item(&ctx, &ctx.style.button.normal, nk_style_item_color(accent));
        nk_style_push_style_item(&ctx, &ctx.style.button.hover, nk_style_item_color(accent_hover));
        nk_style_push_style_item(&ctx, &ctx.style.button.active, nk_style_item_color(accent_active));
        if (nk_button_label(&ctx, "Physics Overrides: ON")) cw_active_profile.enable_physics = 0;
        nk_style_pop_style_item(&ctx);
        nk_style_pop_style_item(&ctx);
        nk_style_pop_style_item(&ctx);
      } else {
        if (nk_button_label(&ctx, "Physics Overrides: OFF")) cw_active_profile.enable_physics = 1;
      }

      if (cw_active_profile.enable_physics || cw_motd_override_active) {
        static int show_detailed_physics = 0;
        nk_layout_row_dynamic(&ctx, 20, 1);
        nk_checkbox_label(&ctx, "Show Detailed Physics Specs", &show_detailed_physics);

        if (show_detailed_physics) {
          nk_layout_row_dynamic(&ctx, 160, 1);
        } else {
          nk_layout_row_dynamic(&ctx, 70, 1);
        }

        if (nk_group_begin(&ctx, "ActiveOverrides", NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
          CwakeProfile* prof = cw_motd_override_active ? &cw_motd_profile : &cw_active_profile;

          nk_layout_row_dynamic(&ctx, 15, 1);
          if (cw_motd_override_active) {
            nk_label_colored(&ctx, "SERVER MOTD OVERRIDES ACTIVE:", NK_TEXT_LEFT, nk_rgb(255, 200, 50));
          } else {
            nk_label_colored(&ctx, "PROFILE PHYSICS ACTIVE:", NK_TEXT_LEFT, nk_rgb(100, 255, 100));
          }

          if (show_detailed_physics) {
            nk_layout_row_dynamic(&ctx, 15, 2);
            char buf[64];
            sprintf(buf, "Gravity: %.3f", prof->gravity);
            nk_label(&ctx, buf, NK_TEXT_LEFT);
            sprintf(buf, "Friction: %.3f", prof->friction);
            nk_label(&ctx, buf, NK_TEXT_LEFT);
            sprintf(buf, "Ground Speed: %.3f", prof->ground_speed);
            nk_label(&ctx, buf, NK_TEXT_LEFT);
            sprintf(buf, "Ground Accel: %.3f", prof->ground_accel);
            nk_label(&ctx, buf, NK_TEXT_LEFT);
            sprintf(buf, "Air Speed: %.3f", prof->air_speed);
            nk_label(&ctx, buf, NK_TEXT_LEFT);
            sprintf(buf, "Air Accel: %.3f", prof->air_accel);
            nk_label(&ctx, buf, NK_TEXT_LEFT);
            sprintf(buf, "Air Cap: %.3f", prof->air_cap);
            nk_label(&ctx, buf, NK_TEXT_LEFT);
            sprintf(buf, "Jump Boost: %.3f", prof->jump_force);
            nk_label(&ctx, buf, NK_TEXT_LEFT);
            sprintf(buf, "Bounce Cons.: %.3f", prof->bounce);
            nk_label(&ctx, buf, NK_TEXT_LEFT);
            sprintf(buf, "Ricochet Hor: %.3f", prof->ricochet_hor);
            nk_label(&ctx, buf, NK_TEXT_LEFT);
            sprintf(buf, "Ricochet Up: %.3f", prof->ricochet_up);
            nk_label(&ctx, buf, NK_TEXT_LEFT);
            sprintf(buf, "Ricochet Count: %d", prof->ricochet_count);
            nk_label(&ctx, buf, NK_TEXT_LEFT);
          } else {
            char info_buf[1024] = "";
            char temp[64];
            if (prof->gravity != DEF_GRAV) {
              sprintf(temp, "gravity: %.2f  ", prof->gravity);
              strcat(info_buf, temp);
            }
            if (prof->friction != DEF_FRICTION) {
              sprintf(temp, "friction: %.2f  ", prof->friction);
              strcat(info_buf, temp);
            }
            if (prof->jump_force != DEF_JUMP) {
              sprintf(temp, "jump_boost: %.2f  ", prof->jump_force);
              strcat(info_buf, temp);
            }
            if (prof->bounce != DEF_BOUNCE) {
              sprintf(temp, "bounce: %.2f  ", prof->bounce);
              strcat(info_buf, temp);
            }
            if (prof->ground_speed != DEF_GROUND_SPEED) {
              sprintf(temp, "groundspeed: %.2f  ", prof->ground_speed);
              strcat(info_buf, temp);
            }
            if (prof->ground_accel != DEF_GROUND_ACCEL) {
              sprintf(temp, "groundaccel: %.2f  ", prof->ground_accel);
              strcat(info_buf, temp);
            }
            if (prof->air_speed != DEF_AIR_SPEED) {
              sprintf(temp, "airspeed: %.2f  ", prof->air_speed);
              strcat(info_buf, temp);
            }
            if (prof->air_accel != DEF_AIR_ACCEL) {
              sprintf(temp, "airaccel: %.2f  ", prof->air_accel);
              strcat(info_buf, temp);
            }
            if (prof->air_cap != DEF_AIR_CAP) {
              sprintf(temp, "aircap: %.2f  ", prof->air_cap);
              strcat(info_buf, temp);
            }
            if (prof->ricochet_hor != DEF_RICOCHET) {
              sprintf(temp, "ricochet_hor: %.2f  ", prof->ricochet_hor);
              strcat(info_buf, temp);
            }
            if (prof->ricochet_up != DEF_RICOCHET_UP) {
              sprintf(temp, "ricochet_ver: %.2f  ", prof->ricochet_up);
              strcat(info_buf, temp);
            }
            if (prof->ricochet_count != DEF_RICOCHET_COUNT) {
              sprintf(temp, "ricochets: %d  ", prof->ricochet_count);
              strcat(info_buf, temp);
            }

            if (strlen(info_buf) == 0) {
              strcpy(info_buf, "All physics are set to default.");
            }
            nk_layout_row_dynamic(&ctx, 60, 1);
            nk_label_wrap(&ctx, info_buf);
          }
          nk_group_end(&ctx);
        }
      }

      nk_layout_row_dynamic(&ctx, 25, 1);

      const char* tilt_modes[] = {"No Tilt", "Velocity Based Tilt", "Delta Yaw Based Tilt"};
      cw_active_profile.tilt_mode = nk_combo(&ctx, tilt_modes, 3, cw_active_profile.tilt_mode, 25, nk_vec2(200, 150));

      if (cw_active_profile.tilt_mode != 0) {
        nk_layout_row_dynamic(&ctx, 25, 2);
        nk_property_float(&ctx, "Max Tilt:", 0.0f, &cw_active_profile.max_tilt, 90.0f, 0.5f, 0.1f);
        nk_property_float(&ctx, "Tilt Multiplier:", 0.0f, &cw_active_profile.tilt_multiplier, 10.0f, 0.1f, 0.05f);
      }
      nk_layout_row_dynamic(&ctx, 1, 1);
      nk_rule_horizontal(&ctx, accent, nk_true);

      // Group 1: World values
      nk_layout_row_dynamic(&ctx, 25, 2);
      nk_property_float(&ctx, "Gravity:", 0.0f, &cw_active_profile.gravity, 10000.0f, 0.01f, 0.002f);
      nk_property_float(&ctx, "Friction:", 0.0f, &cw_active_profile.friction, 10000.0f, 0.1f, 0.02f);
      nk_property_float(&ctx, "Bounce Cons.:", 0.0f, &cw_active_profile.bounce, 10000.0f, 0.01f, 0.002f);

      nk_layout_row_dynamic(&ctx, 1, 1);
      nk_rule_horizontal(&ctx, accent, n
        k_true);

      // Group 2: Ground Values
      nk_layout_row_dynamic(&ctx, 25, 2);
      nk_property_float(&ctx, "Ground Speed:", 0.0f, &cw_active_profile.ground_speed, 10000.0f, 0.01f, 0.002f);
      nk_property_float(&ctx, "Ground Accel:", 0.0f, &cw_active_profile.ground_accel, 10000.0f, 0.01f, 0.002f);

      nk_layout_row_dynamic(&ctx, 1, 1);
      nk_rule_horizontal(&ctx, accent, nk_true);

      // Group 3: Air Values
      nk_layout_row_dynamic(&ctx, 25, 2);
      nk_property_float(&ctx, "Air Speed:", 0.0f, &cw_active_profile.air_speed, 10000.0f, 0.05f, 0.01f);
      nk_property_float(&ctx, "Air Accel:", 0.0f, &cw_active_profile.air_accel, 10000.0f, 0.05f, 0.01f);
      nk_property_float(&ctx, "Air Cap:", 0.0f, &cw_active_profile.air_cap, 10000.0f, 0.05f, 0.01f);

      nk_layout_row_dynamic(&ctx, 1, 1);
      nk_rule_horizontal(&ctx, accent, nk_true);

      // Group 4: Ricochet Values
      nk_layout_row_dynamic(&ctx, 25, 2);
      nk_property_float(&ctx, "Ricochet Hor:", 0.0f, &cw_active_profile.ricochet_hor, 10000.0f, 0.05f, 0.01f);
      nk_property_float(&ctx, "Ricochet Up:", 0.0f, &cw_active_profile.ricochet_up, 10000.0f, 0.05f, 0.01f);
      nk_layout_row_dynamic(&ctx, 25, 1);
      nk_property_int(&ctx, "Ricochet Count:", 0, &cw_active_profile.ricochet_count, 100, 1, 1);
    }

    /* TAB 2: UI  */
    else if (current_tab == TAB_UI) {
      nk_layout_row_dynamic(&ctx, 30, 1);
      if (cw_active_profile.show_speedo) {
        nk_style_push_style_item(&ctx, &ctx.style.button.normal, nk_style_item_color(accent));
        nk_style_push_style_item(&ctx, &ctx.style.button.hover, nk_style_item_color(accent_hover));
        nk_style_push_style_item(&ctx, &ctx.style.button.active, nk_style_item_color(accent_active));
        if (nk_button_label(&ctx, "Show Speedometer: ON")) {
          cw_active_profile.show_speedo = 0;
          Cwake_SetStatus("Speedometer OFF");
        }
        nk_style_pop_style_item(&ctx);
        nk_style_pop_style_item(&ctx);
        nk_style_pop_style_item(&ctx);
      } else {
        if (nk_button_label(&ctx, "Show Speedometer: OFF")) {
          cw_active_profile.show_speedo = 1;
          Cwake_SetStatus("Speedometer ON");
        }
      }
      nk_layout_row_dynamic(&ctx, 15, 1);
      nk_label(&ctx, "Speedometer Color:", NK_TEXT_CENTERED);
      nk_layout_row_dynamic(&ctx, 25, 8);
      struct nk_color cc_colors[16] = {
          nk_rgb(0, 0, 0),     nk_rgb(0, 0, 170),    nk_rgb(0, 170, 0),    nk_rgb(0, 170, 170),
          nk_rgb(170, 0, 0),   nk_rgb(170, 0, 170),  nk_rgb(255, 170, 0),  nk_rgb(170, 170, 170),
          nk_rgb(85, 85, 85),  nk_rgb(85, 85, 255),  nk_rgb(85, 255, 85),  nk_rgb(85, 255, 255),
          nk_rgb(255, 85, 85), nk_rgb(255, 85, 255), nk_rgb(255, 255, 85), nk_rgb(255, 255, 255)};
      for (int i = 0; i < 16; i++) {
        struct nk_color c = cc_colors[i];
        nk_style_push_style_item(&ctx, &ctx.style.button.normal, nk_style_item_color(c));
        nk_style_push_style_item(
            &ctx, &ctx.style.button.hover,
            nk_style_item_color(nk_rgb(c.r > 200 ? c.r - 30 : c.r + 30, c.g > 200 ? c.g - 30 : c.g + 30,
                                       c.b > 200 ? c.b - 30 : c.b + 30)));
        nk_style_push_style_item(
            &ctx, &ctx.style.button.active,
            nk_style_item_color(nk_rgb(c.r > 200 ? c.r - 50 : c.r + 50, c.g > 200 ? c.g - 50 : c.g + 50,
                                       c.b > 200 ? c.b - 50 : c.b + 50)));

        float old_border = ctx.style.button.border;
        char codeStr[2] = {"0123456789abcdef"[i], '\0'};
        if (strcmp(cw_active_profile.speedo_color, codeStr) == 0) {
          nk_style_push_color(&ctx, &ctx.style.button.border_color, nk_rgb(255, 255, 255));
          ctx.style.button.border = 2.0f;
        } else {
          nk_style_push_color(&ctx, &ctx.style.button.border_color, nk_rgb(50, 50, 50));
          ctx.style.button.border = 1.0f;
        }

        if (nk_button_label(&ctx, "")) strcpy(cw_active_profile.speedo_color, codeStr);

        ctx.style.button.border = old_border;
        nk_style_pop_color(&ctx);       // border_color
        nk_style_pop_style_item(&ctx);  // active
        nk_style_pop_style_item(&ctx);  // hover
        nk_style_pop_style_item(&ctx);  // normal
      }
      nk_layout_row_dynamic(&ctx, 25, 2);
      nk_label(&ctx, "Custom Speedo Code:", NK_TEXT_LEFT);
      nk_edit_string_zero_terminated(&ctx, NK_EDIT_FIELD, cw_active_profile.speedo_color,
                                     sizeof(cw_active_profile.speedo_color), nk_filter_default);

      nk_layout_row_dynamic(&ctx, 20, 1);
      nk_label(&ctx, "Speedometer Screen Position:", NK_TEXT_CENTERED);
      nk_layout_row_dynamic(&ctx, 40, 3);
      const char* pos_labels[9] = {"Top-Left", "Top-Center", "Top-Right", "Middle-Left", "Center",
                                   "Middle-Right", "Bottom-Left", "Bottom-Center", "Bottom-Right"};
      for (int i = 0; i < 9; i++) {
        if (cw_active_profile.speedo_pos == i) {
          nk_style_push_style_item(&ctx, &ctx.style.button.normal, nk_style_item_color(accent));
          nk_style_push_style_item(&ctx, &ctx.style.button.hover, nk_style_item_color(accent_hover));
          nk_style_push_style_item(&ctx, &ctx.style.button.active, nk_style_item_color(accent_active));
          if (nk_button_label(&ctx, pos_labels[i])) cw_active_profile.speedo_pos = i;
          nk_style_pop_style_item(&ctx);
          nk_style_pop_style_item(&ctx);
          nk_style_pop_style_item(&ctx);
        } else {
          if (nk_button_label(&ctx, pos_labels[i])) cw_active_profile.speedo_pos = i;
        }
      }

      nk_layout_row_dynamic(&ctx, 15, 1);
      nk_layout_row_dynamic(&ctx, 1, 1);
      nk_rule_horizontal(&ctx, accent, nk_true);
      nk_layout_row_dynamic(&ctx, 15, 1);
      nk_label(&ctx, "UI Theme Colors:", NK_TEXT_CENTERED);

      nk_layout_row_begin(&ctx, NK_STATIC, 25, 4);

      nk_layout_row_push(&ctx, 110);
      nk_label(&ctx, "Background", NK_TEXT_RIGHT);
      nk_layout_row_push(&ctx, 40);
      struct nk_color ui_bg_col = UIntToColor(cw_active_profile.ui_bg);
      nk_style_push_style_item(&ctx, &ctx.style.combo.normal, nk_style_item_color(ui_bg_col));
      nk_style_push_style_item(&ctx, &ctx.style.combo.hover, nk_style_item_color(ui_bg_col));
      if (nk_combo_begin_label(&ctx, "", nk_vec2(250, 250))) {
        nk_layout_row_dynamic(&ctx, 240, 1);
        struct nk_colorf cf = UIntToColorF(cw_active_profile.ui_bg);
        nk_color_pick(&ctx, &cf, NK_RGBA);
        cw_active_profile.ui_bg = ColorFToUInt(cf);
        nk_combo_end(&ctx);
      }
      nk_style_pop_style_item(&ctx);
      nk_style_pop_style_item(&ctx);

      nk_layout_row_push(&ctx, 90);
      nk_label(&ctx, "Text", NK_TEXT_RIGHT);
      nk_layout_row_push(&ctx, 40);
      struct nk_color ui_text_col = UIntToColor(cw_active_profile.ui_text);
      nk_style_push_style_item(&ctx, &ctx.style.combo.normal, nk_style_item_color(ui_text_col));
      nk_style_push_style_item(&ctx, &ctx.style.combo.hover, nk_style_item_color(ui_text_col));
      if (nk_combo_begin_label(&ctx, "", nk_vec2(250, 250))) {
        nk_layout_row_dynamic(&ctx, 240, 1);
        struct nk_colorf cf = UIntToColorF(cw_active_profile.ui_text);
        nk_color_pick(&ctx, &cf, NK_RGBA);
        cw_active_profile.ui_text = ColorFToUInt(cf);
        nk_combo_end(&ctx);
      }
      nk_style_pop_style_item(&ctx);
      nk_style_pop_style_item(&ctx);
      nk_layout_row_end(&ctx);

      nk_layout_row_begin(&ctx, NK_STATIC, 25, 4);
      nk_layout_row_push(&ctx, 110);
      nk_label(&ctx, "Button", NK_TEXT_RIGHT);
      nk_layout_row_push(&ctx, 40);
      struct nk_color ui_btn_col = UIntToColor(cw_active_profile.ui_button);
      nk_style_push_style_item(&ctx, &ctx.style.combo.normal, nk_style_item_color(ui_btn_col));
      nk_style_push_style_item(&ctx, &ctx.style.combo.hover, nk_style_item_color(ui_btn_col));
      if (nk_combo_begin_label(&ctx, "", nk_vec2(250, 250))) {
        nk_layout_row_dynamic(&ctx, 240, 1);
        struct nk_colorf cf = UIntToColorF(cw_active_profile.ui_button);
        nk_color_pick(&ctx, &cf, NK_RGBA);
        cw_active_profile.ui_button = ColorFToUInt(cf);
        nk_combo_end(&ctx);
      }
      nk_style_pop_style_item(&ctx);
      nk_style_pop_style_item(&ctx);

      nk_layout_row_push(&ctx, 90);
      nk_label(&ctx, "Accent", NK_TEXT_RIGHT);
      nk_layout_row_push(&ctx, 40);
      struct nk_color ui_acc_col = UIntToColor(cw_active_profile.ui_accent);
      nk_style_push_style_item(&ctx, &ctx.style.combo.normal, nk_style_item_color(ui_acc_col));
      nk_style_push_style_item(&ctx, &ctx.style.combo.hover, nk_style_item_color(ui_acc_col));
      if (nk_combo_begin_label(&ctx, "", nk_vec2(250, 250))) {
        nk_layout_row_dynamic(&ctx, 240, 1);
        struct nk_colorf cf = UIntToColorF(cw_active_profile.ui_accent);
        nk_color_pick(&ctx, &cf, NK_RGBA);
        cw_active_profile.ui_accent = ColorFToUInt(cf);
        nk_combo_end(&ctx);
      }
      nk_style_pop_style_item(&ctx);
      nk_style_pop_style_item(&ctx);
      nk_layout_row_end(&ctx);

      nk_layout_row_dynamic(&ctx, 15, 1);
      nk_layout_row_dynamic(&ctx, 1, 1);
      nk_rule_horizontal(&ctx, accent, nk_true);

      nk_layout_row_dynamic(&ctx, 25, 2);
      nk_label(&ctx, "Menu Hotkey:", NK_TEXT_LEFT);
      if (waiting_for_hotkey) {
        nk_button_label(&ctx, "Press any key...");
      } else {
        char hotkey_buf[64];
        extern int cw_menu_hotkey;
        const char* name = GetKeyName(cw_menu_hotkey);
        if (name) {
          sprintf(hotkey_buf, "Change (Current Key: %s)", name);
        } else {
          sprintf(hotkey_buf, "Change (Current Key: ID %d)", cw_menu_hotkey);
        }

        if (nk_button_label(&ctx, hotkey_buf)) {
          waiting_for_hotkey = 1;
        }
      }
    }

    /* TAB 3: SOUNDS */
    else if (current_tab == TAB_SOUNDS) {
      nk_layout_row_dynamic(&ctx, 30, 1);
      if (nk_button_label(&ctx, "Rescan Sound Directory")) {
        Cwake_Audio_Rescan();
      }

      nk_layout_row_template_begin(&ctx, 25);
      nk_layout_row_template_push_static(&ctx, 110);
      nk_layout_row_template_push_dynamic(&ctx);
      nk_layout_row_template_push_static(&ctx, 35);
      nk_layout_row_template_push_static(&ctx, 130);
      nk_layout_row_template_push_static(&ctx, 90);
      nk_layout_row_template_end(&ctx);

      nk_label(&ctx, "Jump Sound:", NK_TEXT_LEFT);
      int j_idx = FindSoundIndex(cw_active_profile.jump_sound);
      j_idx = nk_combo(&ctx, (const char**)cw_sound_file_ptrs, cw_sound_count, j_idx, 25, nk_vec2(200, 200));
      if (cw_sound_file_ptrs && j_idx < cw_sound_count) strcpy(cw_active_profile.jump_sound, cw_sound_file_ptrs[j_idx]);
      if (j_idx > 0) {
        if (nk_button_symbol(&ctx, NK_SYMBOL_TRIANGLE_RIGHT))
          Cwake_PlaySound(cw_active_profile.jump_sound, cw_active_profile.jump_vol, cw_active_profile.jump_vary_pitch);
        nk_property_float(&ctx, "Vol:", 0.0f, &cw_active_profile.jump_vol, 2.0f, 0.05f, 0.05f);
        nk_checkbox_label(&ctx, "Vary Pitch", &cw_active_profile.jump_vary_pitch);
      } else {
        nk_label(&ctx, "", NK_TEXT_LEFT);
        nk_label(&ctx, "", NK_TEXT_LEFT);
        nk_label(&ctx, "", NK_TEXT_LEFT);
      }

      nk_label(&ctx, "Land Sound:", NK_TEXT_LEFT);
      int l_idx = FindSoundIndex(cw_active_profile.land_sound);
      l_idx = nk_combo(&ctx, (const char**)cw_sound_file_ptrs, cw_sound_count, l_idx, 25, nk_vec2(200, 200));
      if (cw_sound_file_ptrs && l_idx < cw_sound_count) strcpy(cw_active_profile.land_sound, cw_sound_file_ptrs[l_idx]);
      if (l_idx > 0) {
        if (nk_button_symbol(&ctx, NK_SYMBOL_TRIANGLE_RIGHT))
          Cwake_PlaySound(cw_active_profile.land_sound, cw_active_profile.land_vol, cw_active_profile.land_vary_pitch);
        nk_property_float(&ctx, "Vol:", 0.0f, &cw_active_profile.land_vol, 2.0f, 0.05f, 0.05f);
        nk_checkbox_label(&ctx, "Vary Pitch", &cw_active_profile.land_vary_pitch);
      } else {
        nk_label(&ctx, "", NK_TEXT_LEFT);
        nk_label(&ctx, "", NK_TEXT_LEFT);
        nk_label(&ctx, "", NK_TEXT_LEFT);
      }

      nk_label(&ctx, "Bounce Sound:", NK_TEXT_LEFT);
      int b_idx = FindSoundIndex(cw_active_profile.bounce_sound);
      b_idx = nk_combo(&ctx, (const char**)cw_sound_file_ptrs, cw_sound_count, b_idx, 25, nk_vec2(200, 200));
      if (cw_sound_file_ptrs && b_idx < cw_sound_count)
        strcpy(cw_active_profile.bounce_sound, cw_sound_file_ptrs[b_idx]);
      if (b_idx > 0) {
        if (nk_button_symbol(&ctx, NK_SYMBOL_TRIANGLE_RIGHT))
          Cwake_PlaySound(cw_active_profile.bounce_sound, cw_active_profile.bounce_vol,
                          cw_active_profile.bounce_vary_pitch);
        nk_property_float(&ctx, "Vol:", 0.0f, &cw_active_profile.bounce_vol, 2.0f, 0.05f, 0.05f);
        nk_checkbox_label(&ctx, "Vary Pitch", &cw_active_profile.bounce_vary_pitch);
      } else {
        nk_label(&ctx, "", NK_TEXT_LEFT);
        nk_label(&ctx, "", NK_TEXT_LEFT);
        nk_label(&ctx, "", NK_TEXT_LEFT);
      }

      nk_label(&ctx, "Ricochet Sound:", NK_TEXT_LEFT);
      int r_idx = FindSoundIndex(cw_active_profile.ricochet_sound);
      r_idx = nk_combo(&ctx, (const char**)cw_sound_file_ptrs, cw_sound_count, r_idx, 25, nk_vec2(200, 200));
      if (cw_sound_file_ptrs && r_idx < cw_sound_count)
        strcpy(cw_active_profile.ricochet_sound, cw_sound_file_ptrs[r_idx]);
      if (r_idx > 0) {
        if (nk_button_symbol(&ctx, NK_SYMBOL_TRIANGLE_RIGHT))
          Cwake_PlaySound(cw_active_profile.ricochet_sound, cw_active_profile.ricochet_vol,
                          cw_active_profile.ricochet_vary_pitch);
        nk_property_float(&ctx, "Vol:", 0.0f, &cw_active_profile.ricochet_vol, 2.0f, 0.05f, 0.05f);
        nk_checkbox_label(&ctx, "Vary Pitch", &cw_active_profile.ricochet_vary_pitch);
      } else {
        nk_label(&ctx, "", NK_TEXT_LEFT);
        nk_label(&ctx, "", NK_TEXT_LEFT);
        nk_label(&ctx, "", NK_TEXT_LEFT);
      }
    }
    /* TAB 4: PROFILES */
    else if (current_tab == TAB_PROFILES) {
      if (!profiles_scanned) {
        Cwake_Profile_ScanProfiles();
        profiles_scanned = 1;
      }
      nk_layout_row_dynamic(&ctx, 25, 2);
      nk_label(&ctx, "Default Profile:", NK_TEXT_LEFT);
      int def_idx = 0;
      const char* name_ptrs[32];
      for (int i = 0; i < cw_profile_count; i++) {
        name_ptrs[i] = cw_profile_names[i];
        if (strcmp(cw_profile_names[i], cw_default_profile_name) == 0) {
          def_idx = i;
        }
      }
      int new_def = nk_combo(&ctx, name_ptrs, cw_profile_count, def_idx, 25, nk_vec2(200, 200));
      if (new_def != def_idx) {
        strcpy(cw_default_profile_name, cw_profile_names[new_def]);
        Cwake_SaveConfig();
        Cwake_Profile_Load(cw_default_profile_name, &cw_active_profile);
        char s_buf[128];
        sprintf(s_buf, "Set Startup Profile: %s", cw_default_profile_name);
        Cwake_SetStatus(s_buf);
      }

      nk_layout_row_dynamic(&ctx, 25, 2);
      nk_label(&ctx, "Active Profile Name:", NK_TEXT_LEFT);
      nk_edit_string_zero_terminated(&ctx, NK_EDIT_FIELD, cw_active_profile.profile_name,
                                     sizeof(cw_active_profile.profile_name) - 1, nk_filter_default);

      nk_layout_row_dynamic(&ctx, 30, 2);
      if (nk_button_label(&ctx, "Create New Profile")) {
        char* new_name = cw_active_profile.profile_name;
        if (!Cwake_Profile_IsValidName(new_name)) {
          Cwake_SetStatus("Error: Invalid Profile Name");
        } else if (Cwake_Profile_Exists(new_name)) {
          Cwake_SetStatus("Error: Profile exists, use Overwrite");
        } else {
          int flags = (cw_save_physics ? PROFILE_SAVE_PHYSICS : 0) | (cw_save_sounds ? PROFILE_SAVE_SOUNDS : 0) |
                      (cw_save_ui ? PROFILE_SAVE_UI : 0);
          Cwake_Profile_Save(new_name, &cw_active_profile, flags);
          Cwake_Profile_ScanProfiles();
          char s_buf[128];
          sprintf(s_buf, "Created Profile: %s", new_name);
          Cwake_SetStatus(s_buf);
        }
      }
      if (nk_button_label(&ctx, "Reset to Defaults")) {
        extern void Cwake_Profile_SetDefaults(CwakeProfile * p);
        Cwake_Profile_SetDefaults(&cw_active_profile);
        Cwake_SetStatus("Reset to Defaults");
      }

      nk_layout_row_dynamic(&ctx, 25, 3);
      nk_checkbox_label(&ctx, "Save Physics", &cw_save_physics);
      nk_checkbox_label(&ctx, "Save Sounds", &cw_save_sounds);
      nk_checkbox_label(&ctx, "Save UI/Theme", &cw_save_ui);

      struct nk_rect bounds = nk_window_get_bounds(&ctx);
      float remaining_h = bounds.h - 180.0f;
      if (remaining_h < 150.0f) remaining_h = 150.0f;
      nk_layout_row_dynamic(&ctx, remaining_h, 2);

      if (nk_group_begin(&ctx, "ProfileList", NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(&ctx, 25, 1);
        for (int i = 0; i < cw_profile_count; i++) {
          char* p_name = cw_profile_names[i];
          int is_selected = (strcmp(profile_selected, p_name) == 0);
          char disp_buf[256];
          strcpy(disp_buf, p_name);
          if (nk_selectable_label(&ctx, disp_buf, NK_TEXT_LEFT, &is_selected)) {
            if (is_selected) {
              strcpy(profile_selected, p_name);
            } else {
              profile_selected[0] = '\0';
            }
          }
        }
        nk_group_end(&ctx);
      }

      if (nk_group_begin(&ctx, "ProfileDetails", NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(&ctx, 25, 1);
        if (profile_selected[0] != '\0') {
          char buf[512];
          sprintf(buf, "Selected: %s", profile_selected);
          nk_label(&ctx, buf, NK_TEXT_CENTERED);
          nk_spacing(&ctx, 1);

          char preview[128];
          GetProfilePreview(profile_selected, preview);
          nk_label_colored(&ctx, preview, NK_TEXT_CENTERED, nk_rgb(200, 200, 200));

          if (nk_button_label(&ctx, "Load Configuration")) {
            Cwake_Profile_Load(profile_selected, &cw_active_profile);
            char s_buf[128];
            sprintf(s_buf, "Loaded Profile: %s", profile_selected);
            Cwake_SetStatus(s_buf);
          }
          if (nk_button_label(&ctx, "Overwrite with Current")) {
            int flags = (cw_save_physics ? PROFILE_SAVE_PHYSICS : 0) | (cw_save_sounds ? PROFILE_SAVE_SOUNDS : 0) |
                        (cw_save_ui ? PROFILE_SAVE_UI : 0);
            Cwake_Profile_Save(profile_selected, &cw_active_profile, flags);
            char s_buf[128];
            sprintf(s_buf, "Saved Profile: %s", profile_selected);
            Cwake_SetStatus(s_buf);
            Cwake_Profile_ScanProfiles();
          }
          if (nk_button_label(&ctx, "Delete Profile")) {
            Cwake_Profile_Delete(profile_selected);
            char s_buf[128];
            sprintf(s_buf, "Deleted Profile: %s", profile_selected);
            Cwake_SetStatus(s_buf);
            profile_selected[0] = '\0';
            Cwake_Profile_ScanProfiles();
          }

        } else {
          nk_label(&ctx, "Select a profile...", NK_TEXT_CENTERED);
        }
        nk_group_end(&ctx);
      }
    }
  }
  nk_end(&ctx);

  const struct nk_command* cmd;
  nk_foreach(cmd, &ctx) {
    switch (cmd->type) {
      case NK_COMMAND_RECT_FILLED: {
        struct nk_command_rect_filled* r = (struct nk_command_rect_filled*)cmd;
        PackedCol col = PackedCol_Make(r->color.r, r->color.g, r->color.b, r->color.a);
        Cwake_Draw2DFlat(r->x, r->y, r->w, r->h, col);
        break;
      }
      case NK_COMMAND_RECT_MULTI_COLOR: {
        struct nk_command_rect_multi_color* r = (struct nk_command_rect_multi_color*)cmd;
        PackedCol tl = PackedCol_Make(r->left.r, r->left.g, r->left.b, r->left.a);
        PackedCol tr = PackedCol_Make(r->top.r, r->top.g, r->top.b, r->top.a);
        PackedCol br = PackedCol_Make(r->right.r, r->right.g, r->right.b, r->right.a);
        PackedCol bl = PackedCol_Make(r->bottom.r, r->bottom.g, r->bottom.b, r->bottom.a);
        Cwake_Draw2DFlatMultiColor(r->x, r->y, r->w, r->h, tl, tr, br, bl);
        break;
      }
      case NK_COMMAND_TRIANGLE_FILLED: {
        struct nk_command_triangle_filled* t = (struct nk_command_triangle_filled*)cmd;
        PackedCol col = PackedCol_Make(t->color.r, t->color.g, t->color.b, t->color.a);
        Cwake_Draw2DFlatTriangle((float)t->a.x, (float)t->a.y, (float)t->b.x, (float)t->b.y, (float)t->c.x,
                                 (float)t->c.y, col);
        break;
      }
      case NK_COMMAND_TEXT: {
        struct nk_command_text* t = (struct nk_command_text*)cmd;
        const struct nk_user_font* font = t->font;
        PackedCol fg = PackedCol_Make(t->foreground.r, t->foreground.g, t->foreground.b, t->foreground.a);

        float px = t->x;
        float py = t->y;
        struct nk_user_font_glyph g;

        for (int i = 0; i < t->length; i++) {
          font->query(font->userdata, font->height, &g, t->string[i], (i < t->length - 1) ? t->string[i + 1] : 0);

          Cwake_Draw2DTextured(px + g.offset.x, py + g.offset.y, g.width, g.height, g.uv[0].x, g.uv[0].y, g.uv[1].x,
                               g.uv[1].y, fg);
          px += g.xadvance;
        }
        break;
      }
      default:
        break;
    }
  }
  nk_clear(&ctx);
}
static void CwakeScreen_Init(void* screen) {}
static void CwakeScreen_Update(void* screen, float delta) {}
static void CwakeScreen_Free(void* screen) {
  cw_ui_is_open = false;
}
static void CwakeScreen_BuildMesh(void* screen) {}
static int cw_key_states[5] = {0};

static void CwakeScreen_Render(void* screen, float delta) {
  nk_input_begin(&ctx);
  nk_input_motion(&ctx, cw_mouse_x, cw_mouse_y);
  nk_input_button(&ctx, NK_BUTTON_LEFT, cw_mouse_x, cw_mouse_y, cw_mouse_l_down);

  nk_input_key(&ctx, NK_KEY_BACKSPACE, cw_key_states[0]);
  nk_input_key(&ctx, NK_KEY_DEL, cw_key_states[1]);
  nk_input_key(&ctx, NK_KEY_LEFT, cw_key_states[2]);
  nk_input_key(&ctx, NK_KEY_RIGHT, cw_key_states[3]);
  nk_input_key(&ctx, NK_KEY_ENTER, cw_key_states[4]);

  if (cw_scroll_delta != 0.0f) {
    nk_input_scroll(&ctx, nk_vec2(0, cw_scroll_delta));
    cw_scroll_delta = 0.0f;
  }

  for (int i = 0; i < cw_text_len; i++) {
    nk_input_unicode(&ctx, (nk_rune)cw_text_buf[i]);
  }
  cw_text_len = 0;
  if (!cw_ui_is_open) return;

  Cwake_ApplyTheme(&ctx, &cw_active_profile);

  nk_input_end(&ctx);

  for (int i = 0; i < 5; i++) {
    cw_key_states[i] = 0;
  }

  Cwake_DrawNuklear();
}

static int CwakeScreen_HandlesInputDown(void* screen, int key, struct InputDevice* device) {
  if (waiting_for_hotkey) {
    if (key != 1) { // 1 = CCKEY_ESCAPE
      extern int cw_menu_hotkey;
      cw_menu_hotkey = key;
      Options_SetInt("cwake-hotkey", cw_menu_hotkey);
    }
    waiting_for_hotkey = 0;
    return 1;
  }

  if (key == CCMOUSE_L)
    cw_mouse_l_down = 1;
  else if (key == CCKEY_BACKSPACE)
    cw_key_states[0] = 1;
  else if (key == CCKEY_DELETE)
    cw_key_states[1] = 1;
  else if (key == CCKEY_LEFT)
    cw_key_states[2] = 1;
  else if (key == CCKEY_RIGHT)
    cw_key_states[3] = 1;
  else if (key == CCKEY_ENTER)
    cw_key_states[4] = 1;

  return 1;
}
static void CwakeScreen_HandlesInputUp(void* screen, int key, struct InputDevice* device) {
  if (key == CCMOUSE_L)
    cw_mouse_l_down = 0;
  else if (key == CCKEY_BACKSPACE)
    cw_key_states[0] = 0;
  else if (key == CCKEY_DELETE)
    cw_key_states[1] = 0;
  else if (key == CCKEY_LEFT)
    cw_key_states[2] = 0;
  else if (key == CCKEY_RIGHT)
    cw_key_states[3] = 0;
  else if (key == CCKEY_ENTER)
    cw_key_states[4] = 0;
}
static int CwakeScreen_HandlesKeyPress(void* screen, char keyChar) {
  if (cw_text_len < 255) {
    cw_text_buf[cw_text_len++] = keyChar;
  }
  return 1;
}
static int CwakeScreen_HandlesMouseScroll(void* screen, float delta) {
  cw_scroll_delta += delta;
  return 1;
}
static int CwakeScreen_HandlesTextChanged(void* screen, const cc_string* str) {
  return 1;
}
static int CwakeScreen_HandlesPointerDown(void* screen, int id, int x, int y) {
  if (id == 0) {
    cw_mouse_l_down = 1;
  }
  cw_mouse_x = x;
  cw_mouse_y = y;
  return 1;
}
static void CwakeScreen_HandlesPointerUp(void* screen, int id, int x, int y) {
  if (id == 0) {
    cw_mouse_l_down = 0;
  }
  cw_mouse_x = x;
  cw_mouse_y = y;
}
static int CwakeScreen_HandlesPointerMove(void* screen, int id, int x, int y) {
  cw_mouse_x = x;
  cw_mouse_y = y;
  return 1;
}
static void CwakeScreen_Layout(void* screen) {}
static void CwakeScreen_ContextLost(void* screen) {}
static void CwakeScreen_ContextRecreated(void* screen) {}
static const struct ScreenVTABLE cwake_screen_vtable = {
  CwakeScreen_Init,
  CwakeScreen_Update,
  CwakeScreen_Free,
  CwakeScreen_Render,
  CwakeScreen_BuildMesh,
  CwakeScreen_HandlesInputDown,
  CwakeScreen_HandlesInputUp,
  CwakeScreen_HandlesKeyPress,
  CwakeScreen_HandlesTextChanged,
  CwakeScreen_HandlesPointerDown,
  CwakeScreen_HandlesPointerUp,
  CwakeScreen_HandlesPointerMove,
  CwakeScreen_HandlesMouseScroll,
  CwakeScreen_Layout,
  CwakeScreen_ContextLost,
  CwakeScreen_ContextRecreated
};
static struct Screen cwake_screen;

void Cwake_UI_Toggle(void) {
  cw_ui_is_open = !cw_ui_is_open;
  if (cw_ui_is_open) {
    Gui_Add((struct Screen*)&cwake_screen, 65);
    cw_status_timer = 0.0f;
    cc_string empty = String_FromReadonly("");
    Chat_AddOf(&empty, 11);
  } else {
    Gui_Remove((struct Screen*)&cwake_screen);
  }
}

void Cwake_UI_Init(void) {
  Cwake_UI_AllocateGPUResources();
  Event_Register_(&GfxEvents.ContextLost, NULL, Cwake_OnContextLost);
  Event_Register_(&GfxEvents.ContextRecreated, NULL, Cwake_OnContextRecreated);
  cwake_screen.VTABLE = &cwake_screen_vtable;
  cwake_screen.grabsInput = true;
  cwake_screen.blocksWorld = false;
  cwake_screen.closable = false;
  cwake_screen.dirty = true;
}
void Cwake_UI_Free(void) {
  Event_Unregister_(&GfxEvents.ContextLost, NULL, Cwake_OnContextLost);
  Event_Unregister_(&GfxEvents.ContextRecreated, NULL, Cwake_OnContextRecreated);

  if (cw_ui_is_open) Gui_Remove((struct Screen*)&cwake_screen);
  Cwake_UI_FreeGPUResources();
}
