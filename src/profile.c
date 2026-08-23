#include "cwake.h"

#include "Platform.h"
#include "String_.h"
#include "Options.h"
#include "Server.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CwakeProfile cw_active_profile;
void Cwake_Profile_SetDefaults(CwakeProfile* p) {
  strcpy(p->profile_name, "");
  p->enable_physics = 1;
  p->tilt_mode = 0;
  p->max_tilt = 15.0f;
  p->tilt_multiplier = 1.5f;
  p->gravity = 1.00f;
  p->friction = 8.00f;
  p->ground_speed = 0.28f;
  p->ground_accel = 10.00f;
  p->air_speed = 0.28f;
  p->air_accel = 10.00f;
  p->air_cap = 0.10f;
  p->jump_force = 1.00f;
  p->ricochet_hor = 0.00f;
  p->ricochet_up = 0.69f;
  p->ricochet_count = 1;
  p->bounce = 0.00f;
  p->show_speedo = 1;
  p->speedo_pos = 4;
  strcpy(p->speedo_color, "f");

  p->ui_bg = 0x1E1E1EE0;
  p->ui_text = 0xE6E6E6FF;
  p->ui_accent = 0x646464FF;
  p->ui_button = 0x323232FF;

  strcpy(p->jump_sound, "None");
  p->jump_vol = 0.5f;
  p->jump_vary_pitch = 0;
  strcpy(p->land_sound, "None");
  p->land_vol = 0.5f;
  p->land_vary_pitch = 0;
  strcpy(p->bounce_sound, "None");
  p->bounce_vol = 0.5f;
  p->bounce_vary_pitch = 0;
  strcpy(p->ricochet_sound, "None");
  p->ricochet_vol = 0.5f;
  p->ricochet_vary_pitch = 0;
}
typedef struct {
  const char* section;
  const char* key;
  /* 0 = int, 1 = float, 2 = string */
  int type;
  size_t offset;
} IniKeyMap;

// clang-format off
static const IniKeyMap ini_map[] = {
  {"Physics", "EnablePhysics", 0, offsetof(CwakeProfile, enable_physics)},
  {"Physics", "TiltMode", 0, offsetof(CwakeProfile, tilt_mode)},
  {"Physics", "MaxTilt", 1, offsetof(CwakeProfile, max_tilt)},
  {"Physics", "TiltMultiplier", 1, offsetof(CwakeProfile, tilt_multiplier)},
  {"Physics", "Gravity", 1, offsetof(CwakeProfile, gravity)},
  {"Physics", "Friction", 1, offsetof(CwakeProfile, friction)},
  {"Physics", "GroundSpeed", 1, offsetof(CwakeProfile, ground_speed)},
  {"Physics", "GroundAccel", 1, offsetof(CwakeProfile, ground_accel)},
  {"Physics", "AirSpeed", 1, offsetof(CwakeProfile, air_speed)},
  {"Physics", "AirAccel", 1, offsetof(CwakeProfile, air_accel)},
  {"Physics", "AirCap", 1, offsetof(CwakeProfile, air_cap)},
  {"Physics", "JumpForce", 1, offsetof(CwakeProfile, jump_force)},
  {"Physics", "RicochetHor", 1, offsetof(CwakeProfile, ricochet_hor)},
  {"Physics", "RicochetUp", 1, offsetof(CwakeProfile, ricochet_up)},
  {"Physics", "RicochetCount", 0, offsetof(CwakeProfile, ricochet_count)},
  {"Physics", "Bounce", 1, offsetof(CwakeProfile, bounce)},
  
  {"UI", "ShowSpeedo", 0, offsetof(CwakeProfile, show_speedo)},
  {"UI", "SpeedoPos", 0, offsetof(CwakeProfile, speedo_pos)},
  {"UI", "SpeedoColor", 2, offsetof(CwakeProfile, speedo_color)},
  {"Theme", "UI_Bg", 3, offsetof(CwakeProfile, ui_bg)},
  {"Theme", "UI_Text", 3, offsetof(CwakeProfile, ui_text)},
  {"Theme", "UI_Accent", 3, offsetof(CwakeProfile, ui_accent)},
  {"Theme", "UI_Button", 3, offsetof(CwakeProfile, ui_button)},

  {"Sounds", "JumpSound", 2, offsetof(CwakeProfile, jump_sound)},
  {"Sounds", "JumpVol", 1, offsetof(CwakeProfile, jump_vol)},
  {"Sounds", "JumpVaryPitch", 0, offsetof(CwakeProfile, jump_vary_pitch)},
  {"Sounds", "LandSound", 2, offsetof(CwakeProfile, land_sound)},
  {"Sounds", "LandVol", 1, offsetof(CwakeProfile, land_vol)},
  {"Sounds", "LandVaryPitch", 0, offsetof(CwakeProfile, land_vary_pitch)},
  {"Sounds", "BounceSound", 2, offsetof(CwakeProfile, bounce_sound)},
  {"Sounds", "BounceVol", 1, offsetof(CwakeProfile, bounce_vol)},
  {"Sounds", "BounceVaryPitch", 0, offsetof(CwakeProfile, bounce_vary_pitch)},
  {"Sounds", "RicochetSound", 2, offsetof(CwakeProfile, ricochet_sound)},
  {"Sounds", "RicochetVol", 1, offsetof(CwakeProfile, ricochet_vol)},
  {"Sounds", "RicochetVaryPitch", 0, offsetof(CwakeProfile, ricochet_vary_pitch)}
};
// clang-format on

cc_bool Cwake_Profile_Load(const char* name, CwakeProfile* p) {
  strcpy(p->profile_name, name);
  char filepath[256];
  sprintf(filepath, "plugins/cwake/profiles/%s.ini", name);
  FILE* file = fopen(filepath, "r");
  if (!file) return false;

  char line[256];
  char current_section[64] = "";

  while (fgets(line, sizeof(line), file)) {
    line[strcspn(line, "\r\n")] = 0;

    if (line[0] == '\0' || line[0] == '#' || line[0] == ';') continue;

    if (line[0] == '[' && strchr(line, ']')) {
      char* end = strchr(line, ']');
      *end = '\0';
      strcpy(current_section, line + 1);

      CwakeProfile def;
      Cwake_Profile_SetDefaults(&def);
      for (int i = 0; i < sizeof(ini_map) / sizeof(IniKeyMap); i++) {
        if (strcmp(current_section, ini_map[i].section) == 0) {
          void* dest = (char*)p + ini_map[i].offset;
          void* src = (char*)&def + ini_map[i].offset;
          if (ini_map[i].type == 0) {
            *(int*)dest = *(int*)src;
          } else if (ini_map[i].type == 1) {
            *(float*)dest = *(float*)src;
          } else if (ini_map[i].type == 2) {
            strcpy((char*)dest, (char*)src);
          } else if (ini_map[i].type == 3) {
            *(unsigned int*)dest = *(unsigned int*)src;
          }
        }
      }
      continue;
    }

    /* Parse key-value pair */
    char* eq_ptr = strchr(line, '=');
    if (eq_ptr) {
      *eq_ptr = '\0';
      char* key_str = line;
      char* val_str = eq_ptr + 1;

      char* end = key_str + strlen(key_str) - 1;
      while (end > key_str && *end == ' ')
        *end-- = '\0';

      char* val_ptr = val_str;
      while (*val_ptr == ' ')
        val_ptr++;

      if (strcmp(key_str, "Name") == 0 || strcmp(key_str, "ProfileName") == 0) {
        continue;
      }

      for (int i = 0; i < sizeof(ini_map) / sizeof(IniKeyMap); i++) {
        if (strcmp(current_section, ini_map[i].section) == 0 && strcmp(key_str, ini_map[i].key) == 0) {
          void* dest = (char*)p + ini_map[i].offset;
          if (ini_map[i].type == 0) {
            *(int*)dest = atoi(val_ptr);
          } else if (ini_map[i].type == 1) {
            *(float*)dest = (float)atof(val_ptr);
          } else if (ini_map[i].type == 2) {
            strcpy((char*)dest, val_ptr);
          } else if (ini_map[i].type == 3) {
            *(unsigned int*)dest = (unsigned int)strtoul(val_ptr, NULL, 16);
          }
          break;
        }
      }
    }
  }
  fclose(file);
  return true;
}
cc_bool Cwake_Profile_Save(const char* name, const CwakeProfile* p, int flags) {
  char filepath[256];

  cc_string path = String_FromReadonly("plugins/cwake/profiles");
  cc_filepath fp;
  Platform_EncodePath(&fp, &path);
  Directory_Create2(&fp);

  sprintf(filepath, "plugins/cwake/profiles/%s.ini", name);
  FILE* file = fopen(filepath, "w");
  if (!file) return false;
  fprintf(file, "ProfileName = %s\n\n", p->profile_name);

  if (flags & PROFILE_SAVE_PHYSICS) {
    fprintf(file, "[Physics]\n");
    fprintf(file, "TiltMode = %d\n", p->tilt_mode);
    fprintf(file, "Gravity = %.6f\n", p->gravity);
    fprintf(file, "Friction = %.6f\n", p->friction);
    fprintf(file, "GroundSpeed = %.6f\n", p->ground_speed);
    fprintf(file, "GroundAccel = %.6f\n", p->ground_accel);
    fprintf(file, "AirSpeed = %.6f\n", p->air_speed);
    fprintf(file, "AirAccel = %.6f\n", p->air_accel);
    fprintf(file, "AirCap = %.6f\n", p->air_cap);
    fprintf(file, "JumpForce = %.6f\n", p->jump_force);
    fprintf(file, "RicochetHor = %.6f\n", p->ricochet_hor);
    fprintf(file, "RicochetUp = %.6f\n", p->ricochet_up);
    fprintf(file, "RicochetCount = %d\n", p->ricochet_count);
    fprintf(file, "Bounce = %.6f\n\n", p->bounce);
  }

  if (flags & PROFILE_SAVE_UI) {
    fprintf(file, "[UI]\n");
    fprintf(file, "ShowSpeedo = %d\n", p->show_speedo);
    fprintf(file, "SpeedoPos = %d\n", p->speedo_pos);
    fprintf(file, "SpeedoColor = %s\n\n", p->speedo_color);
    fprintf(file, "[Theme]\n");
    fprintf(file, "UI_Bg = %08X\n", p->ui_bg);
    fprintf(file, "UI_Text = %08X\n", p->ui_text);
    fprintf(file, "UI_Accent = %08X\n", p->ui_accent);
    fprintf(file, "UI_Button = %08X\n\n", p->ui_button);
  }

  if (flags & PROFILE_SAVE_SOUNDS) {
    fprintf(file, "[Sounds]\n");
    fprintf(file, "JumpSound = %s\n", p->jump_sound);
    fprintf(file, "JumpVol = %.2f\n", p->jump_vol);
    fprintf(file, "JumpVaryPitch = %d\n", p->jump_vary_pitch);
    fprintf(file, "LandSound = %s\n", p->land_sound);
    fprintf(file, "LandVol = %.2f\n", p->land_vol);
    fprintf(file, "LandVaryPitch = %d\n", p->land_vary_pitch);
    fprintf(file, "BounceSound = %s\n", p->bounce_sound);
    fprintf(file, "BounceVol = %.2f\n", p->bounce_vol);
    fprintf(file, "BounceVaryPitch = %d\n", p->bounce_vary_pitch);
    fprintf(file, "RicochetSound = %s\n", p->ricochet_sound);
    fprintf(file, "RicochetVol = %.2f\n", p->ricochet_vol);
    fprintf(file, "RicochetVaryPitch = %d\n", p->ricochet_vary_pitch);
  }

  fclose(file);
  return true;
}

void Cwake_Profile_Delete(const char* name) {
  char filepath[256];
  sprintf(filepath, "plugins/cwake/profiles/%s.ini", name);
  remove(filepath);
}

cc_bool Cwake_Profile_Exists(const char* name) {
  char filepath[256];
  sprintf(filepath, "plugins/cwake/profiles/%s.ini", name);
  FILE* f = fopen(filepath, "r");
  if (f) {
    fclose(f);
    return true;
  }
  return false;
}

cc_bool Cwake_Profile_IsValidName(const char* name) {
  if (!name || name[0] == '\0') return false;
  for (int i = 0; name[i] != '\0'; i++) {
    char c = name[i];
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
      return false;
    }
  }
  return true;
}

cc_bool cw_motd_override_active = false;
CwakeProfile cw_motd_profile;
char cached_motd[256] = {0};

typedef struct {
  const char* key;
  /* 0 = int, 1 = float */
  int type;
  size_t offset;
} MotdKeyMap;

// clang-format off
static const MotdKeyMap motd_map[] = {
  {"mode=", 0, offsetof(CwakeProfile, tilt_mode)},
  {"friction=", 1, offsetof(CwakeProfile, friction)},
  {"gravity=", 1, offsetof(CwakeProfile, gravity)},
  {"groundspeed=", 1, offsetof(CwakeProfile, ground_speed)},
  {"groundaccel=", 1, offsetof(CwakeProfile, ground_accel)},
  {"bounce=", 1, offsetof(CwakeProfile, bounce)},
  {"airspeed=", 1, offsetof(CwakeProfile, air_speed)},
  {"airaccel=", 1, offsetof(CwakeProfile, air_accel)},
  {"aircap=", 1, offsetof(CwakeProfile, air_cap)},
  {"ricochet_hor=", 1, offsetof(CwakeProfile, ricochet_hor)},
  {"ricochet_ver=", 1, offsetof(CwakeProfile, ricochet_up)},
  {"ricochets=", 0, offsetof(CwakeProfile, ricochet_count)},
  {"jump_boost=", 1, offsetof(CwakeProfile, jump_force)}
};
// clang-format on

void Cwake_ParseMOTD(const char* motdStr, int len) {
  char lowerMOTD[256] = {0};
  int i;
  char* ptr;
  cw_motd_override_active = false;

  if (len == 0) return;
  memcpy(lowerMOTD, motdStr, len);
  for (i = 0; i < len; i++) {
    if (lowerMOTD[i] >= 'A' && lowerMOTD[i] <= 'Z') lowerMOTD[i] += 32;
  }

  cw_motd_profile = cw_active_profile;
  Cwake_Profile_SetDefaults(&cw_motd_profile);

  for (i = 0; i < sizeof(motd_map) / sizeof(MotdKeyMap); i++) {
    if ((ptr = strstr(lowerMOTD, motd_map[i].key))) {
      ptr += strlen(motd_map[i].key);
      void* dest = (char*)&cw_motd_profile + motd_map[i].offset;
      if (motd_map[i].type == 0) {
        *(int*)dest = atoi(ptr);
      } else {
        *(float*)dest = (float)atof(ptr);
      }
      cw_motd_override_active = true;
    }
  }
}
void Cwake_CheckMOTD(void) {
  char current_motd[256] = {0};
  int len = Server.MOTD.length < 255 ? Server.MOTD.length : 255;
  if (len > 0 && Server.MOTD.buffer) {
    memcpy(current_motd, Server.MOTD.buffer, len);
  }
  current_motd[len] = '\0';
  if (strcmp(cached_motd, current_motd) != 0) {
    strncpy(cached_motd, current_motd, sizeof(cached_motd) - 1);
    cached_motd[sizeof(cached_motd) - 1] = '\0';
    Cwake_ParseMOTD(current_motd, len);
  }
}
void Cwake_OnNewMapLoaded(void) {
  cached_motd[0] = '\0';
}

char cw_profile_names[32][64];
int cw_profile_count = 0;

static void ProfileEnumCallback(const cc_string* filename, void* obj, int isDirectory) {
  if (isDirectory) return;

  char buf[256];
  int len = filename->length < 255 ? filename->length : 255;
  for (int i = 0; i < len; i++) {
    buf[i] = filename->buffer[i];
  }
  buf[len] = '\0';

  char* last_slash = strrchr(buf, '/');
  char* last_backslash = strrchr(buf, '\\');
  char* filename_only = buf;
  if (last_slash) filename_only = last_slash + 1;
  if (last_backslash && last_backslash > last_slash) filename_only = last_backslash + 1;

  char* dot = strrchr(filename_only, '.');
  if (dot && strcmp(dot, ".ini") == 0) {
    if (cw_profile_count < 32) {
      *dot = '\0';
      strncpy(cw_profile_names[cw_profile_count], filename_only, 63);
      cw_profile_names[cw_profile_count][63] = '\0';
      cw_profile_count++;
    }
  }
}

void Cwake_Profile_ScanProfiles(void) {
  cw_profile_count = 0;

  cc_string dir_path = String_FromReadonly("plugins/cwake/profiles");
  cc_filepath fp;
  Platform_EncodePath(&fp, &dir_path);
  Directory_Create2(&fp);

  cc_string path = String_FromReadonly("plugins/cwake/profiles/");
  Directory_Enum(&path, NULL, ProfileEnumCallback);
}

char cw_default_profile_name[64] = "Default";

void Cwake_LoadConfig(void) {
  FILE* f = fopen("plugins/cwake/config.txt", "r");
  if (f) {
    if (fgets(cw_default_profile_name, sizeof(cw_default_profile_name), f)) {
      cw_default_profile_name[strcspn(cw_default_profile_name, "\r\n")] = '\0';
    }
    fclose(f);
  }
}

void Cwake_SaveConfig(void) {
  FILE* f = fopen("plugins/cwake/config.txt", "w");
  if (f) {
    fputs(cw_default_profile_name, f);
    fclose(f);
  }
}
