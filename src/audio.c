#include "cwake.h"

#include "Chat.h"
#include "String_.h"
#include "Platform.h"
#include <stdlib.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

ma_engine cw_audio_engine;
int cw_audio_initialized = 0;

char** cw_sound_file_ptrs = NULL;
int cw_sound_count = 0;

static void FindSoundsCallback(const cc_string* filename, void* obj, int isDirectory) {
  if (isDirectory) return;

  char buf[256];
  int str_len = filename->length < 255 ? filename->length : 255;
  for (int i = 0; i < str_len; i++) {
    buf[i] = filename->buffer[i];
  }
  buf[str_len] = '\0';

  int len = strlen(buf);
  if (len > 4 && (strcmp(buf + len - 4, ".wav") == 0 || strcmp(buf + len - 4, ".mp3") == 0)) {
    char* last_slash = strrchr(buf, '/');
    char* last_backslash = strrchr(buf, '\\');
    char* filename_only = buf;
    if (last_slash) filename_only = last_slash + 1;
    if (last_backslash && last_backslash > last_slash) filename_only = last_backslash + 1;

    int fn_len = strlen(filename_only);

    if (cw_sound_count == 0) {
      cw_sound_file_ptrs = malloc(sizeof(char*));
    } else {
      cw_sound_file_ptrs = realloc(cw_sound_file_ptrs, (cw_sound_count + 1) * sizeof(char*));
    }
    cw_sound_file_ptrs[cw_sound_count] = malloc(fn_len + 1);
    strcpy(cw_sound_file_ptrs[cw_sound_count], filename_only);
    cw_sound_count++;
  }
}

void Cwake_Audio_Init(void) {
  if (cw_audio_initialized) return;

  ma_result result = ma_engine_init(NULL, &cw_audio_engine);
  if (result != MA_SUCCESS) {
    cc_string str = String_FromReadonly("&c[Cwake] Failed to initialize miniaudio engine!");
    Chat_AddOf(&str, 11);
    return;
  }
  ma_engine_start(&cw_audio_engine);
  cw_audio_initialized = 1;

  if (cw_sound_count == 0) {
    cw_sound_file_ptrs = malloc(sizeof(char*));
    cw_sound_file_ptrs[0] = malloc(16);
    strcpy(cw_sound_file_ptrs[0], "None");
    cw_sound_count = 1;
  }

  cc_string path = String_FromReadonly("plugins/cwake/sounds/");
  Directory_Enum(&path, NULL, FindSoundsCallback);
}

void Cwake_Audio_Rescan(void) {
  if (!cw_audio_initialized) return;
  if (cw_sound_file_ptrs) {
    for (int i = 0; i < cw_sound_count; i++) {
      free(cw_sound_file_ptrs[i]);
    }
    free(cw_sound_file_ptrs);
    cw_sound_file_ptrs = NULL;
  }
  cw_sound_count = 0;

  cw_sound_file_ptrs = malloc(sizeof(char*));
  cw_sound_file_ptrs[0] = malloc(16);
  strcpy(cw_sound_file_ptrs[0], "None");
  cw_sound_count = 1;

  cc_string path = String_FromReadonly("plugins/cwake/sounds/");
  Directory_Enum(&path, NULL, FindSoundsCallback);
}

#define MAX_CONCURRENT_SOUNDS 16
ma_sound cw_active_sounds[MAX_CONCURRENT_SOUNDS];
int cw_sound_slots[MAX_CONCURRENT_SOUNDS] = {0};

void Cwake_PlaySound(const char* sound_name, float volume, int vary_pitch) {
  if (!cw_audio_initialized || !sound_name || strcmp(sound_name, "None") == 0 || strlen(sound_name) == 0) return;

  char filepath[512];
  sprintf(filepath, "plugins/cwake/sounds/%s", sound_name);

  int slot = -1;
  for (int i = 0; i < MAX_CONCURRENT_SOUNDS; i++) {
    if (cw_sound_slots[i] == 0) {
      slot = i;
      break;
    } else {
      if (!ma_sound_is_playing(&cw_active_sounds[i])) {
        ma_sound_uninit(&cw_active_sounds[i]);
        slot = i;
        break;
      }
    }
  }

  if (slot != -1) {
    ma_result init_res = ma_sound_init_from_file(&cw_audio_engine, filepath, 0, NULL, NULL, &cw_active_sounds[slot]);
    if (init_res == MA_SUCCESS) {
      ma_sound_set_volume(&cw_active_sounds[slot], volume);
      if (vary_pitch) {
        float pitch = 1.0f + (((rand() % 100) / 100.0f) * 0.3f - 0.15f);  // 0.85 to 1.15
        ma_sound_set_pitch(&cw_active_sounds[slot], pitch);
      }
      ma_sound_start(&cw_active_sounds[slot]);
      cw_sound_slots[slot] = 1;
    } else {
      char err_buf[256];
      sprintf(err_buf, "&c[Cwake] Failed to load sound: %s (Error: %d)", sound_name, init_res);
      cc_string str = String_FromReadonly(err_buf);
      Chat_AddOf(&str, 11);
    }
  }
}