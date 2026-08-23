#ifndef CWAKE_H
#define CWAKE_H

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(push)
#pragma warning(disable : 4005)
#pragma warning(disable : 4996)
#include "Core.h"
#include "PluginAPI.h"
#pragma warning(pop)

#include "Chat.h"
#include "Commands.h"
#include "Camera.h"
#include "Entity.h"
#include "EntityComponents.h"
#include "Game.h"
#include "Gui.h"
#include "Input.h"
#include "InputHandler.h"
#include "Model.h"
#include "Server.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DEF_FRICTION 8.0f
#define DEF_GROUND_ACCEL 10.0f
#define DEF_AIR_ACCEL 10.0f
#define DEF_AIR_CAP 0.05f
#define DEF_GROUND_SPEED 0.28f
#define DEF_AIR_SPEED 0.28f
#define DEF_JUMP 1.0f
#define DEF_GRAV 1.0f
#define DEF_RICOCHET 0.0f
#define DEF_RICOCHET_UP 0.0f
#define DEF_RICOCHET_COUNT 0
#define DEF_BOUNCE 0.0f
#define DEF_MAX_TILT 4.5f
#define DEF_TILT_MODE 1

typedef struct {
  char profile_name[64];
  /* [Physics] */
  int enable_physics;
  int tilt_mode;
  float max_tilt;
  float tilt_multiplier;
  float gravity;
  float friction;
  float ground_speed;
  float ground_accel;
  float air_speed;
  float air_accel;
  float air_cap;
  float jump_force;
  float ricochet_hor;
  float ricochet_up;
  int ricochet_count;
  float bounce;
  /* [UI] */
  int show_speedo;
  int speedo_pos;
  char speedo_color[16];
  /* [Theme] */
  unsigned int ui_bg;
  unsigned int ui_text;
  unsigned int ui_accent;
  unsigned int ui_button;
  /* [Audio] */
  char jump_sound[64];
  float jump_vol;
  int jump_vary_pitch;
  char land_sound[64];
  float land_vol;
  int land_vary_pitch;
  char bounce_sound[64];
  float bounce_vol;
  int bounce_vary_pitch;
  char ricochet_sound[64];
  float ricochet_vol;
  int ricochet_vary_pitch;
} CwakeProfile;

/* --- GLOBAL STATE --- */
extern CwakeProfile cw_active_profile;
extern CwakeProfile cw_motd_profile;
extern cc_bool cw_motd_override_active;

extern cc_bool cw_ui_is_open;
extern cc_bool cw_enabled;
extern float cw_top_speed;
extern float cw_last_yaw;
extern int cw_current_ricochets;

extern char cw_profile_names[32][64];
extern char cw_default_profile_name[64];
extern int cw_profile_count;

extern char** cw_sound_file_ptrs;
extern int cw_sound_count;

/* Core Hooks & VTABLE */
extern const struct EntityVTABLE* cw_origVTABLE;
extern struct EntityVTABLE cw_hookedVTABLE;
extern void (*cw_orig_GetView)(struct Matrix* view);

/* Camera Tilt State */
extern float cw_target_roll;
extern float cw_current_roll;

/* LiveSplit Integration */
extern cc_bool cw_send_livesplit;
extern void (*cw_LiveSplit_SetSpeed)(cc_uint32 speed);

/* --- FUNCTION DECLARATIONS --- */
/* Core & Physics */
void Cwake_GetView(struct Matrix* view);
void Cwake_Tick(struct Entity* e, float delta);

void Cwake_Profile_SetDefaults(CwakeProfile* p);
#define PROFILE_SAVE_PHYSICS 1
#define PROFILE_SAVE_SOUNDS 2
#define PROFILE_SAVE_UI 4

cc_bool Cwake_Profile_Load(const char* name, CwakeProfile* p);
cc_bool Cwake_Profile_Save(const char* name, const CwakeProfile* p, int flags);
void Cwake_Profile_Delete(const char* name);
cc_bool Cwake_Profile_Exists(const char* name);
cc_bool Cwake_Profile_IsValidName(const char* name);
void Cwake_Profile_ScanProfiles(void);
void Cwake_ParseMOTD(const char* motdStr, int len);
void Cwake_CheckMOTD(void);
void Cwake_OnNewMapLoaded(void);
void Cwake_LoadConfig(void);
void Cwake_SaveConfig(void);

/* UI & HUD */
void Cwake_UI_Init(void);
void Cwake_UI_Toggle(void);
void Cwake_UI_Draw(void);
void Cwake_HUD_Draw(void);
void Cwake_Hud_Init(void);

/* Audio */
void Cwake_Audio_Init(void);
void Cwake_Audio_Rescan(void);
void Cwake_PlaySound(const char* sound_name, float volume, int vary_pitch);

/* LiveSplit */
cc_bool Cwake_LinkLiveSplit(void);

#endif
