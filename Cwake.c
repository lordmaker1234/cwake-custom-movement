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

/* Default Parameters */
#define DEF_FRICTION      8.0f
#define DEF_GROUND_ACCEL  10.0f
#define DEF_AIR_ACCEL     10.0f
#define DEF_AIR_CAP       0.05f
#define DEF_GROUND_SPEED  0.28f
#define DEF_AIR_SPEED     0.28f
#define DEF_JUMP          1.0f
#define DEF_GRAV          1.0f
#define DEF_RICOCHET      0.0f
#define DEF_RICOCHET_UP   0.69f
#define DEF_RICOCHET_COUNT 1
#define DEF_BOUNCE        0.0f

static const struct EntityVTABLE* cw_origVTABLE = NULL;
static struct EntityVTABLE cw_hookedVTABLE;

static void (*cw_orig_GetView)(struct Matrix* view) = NULL;
static float cw_target_roll = 0.0f;
static float cw_current_roll = 0.0f;

static cc_bool cw_enabled = false;
static cc_bool cw_speedo = false;
static cc_bool cw_do_tilt = true;
static float cw_top_speed = 0.0f;
static float cw_last_yaw = 0.0f;
static int cw_current_ricochets = 0;

/* Player's Parameters */
static int cw_mode = 0;
static float cw_friction = DEF_FRICTION;
static float cw_gravity = DEF_GRAV;
static float cw_ground_speed = DEF_GROUND_SPEED;
static float cw_ground_accel = DEF_GROUND_ACCEL;
static float cw_bounce = DEF_BOUNCE;
static float cw_air_speed = DEF_AIR_SPEED;
static float cw_air_accel = DEF_AIR_ACCEL;
static float cw_air_cap = DEF_AIR_CAP;
static float cw_ricochet = DEF_RICOCHET;
static float cw_ricochet_up = DEF_RICOCHET_UP;
static int cw_ricochet_count = DEF_RICOCHET_COUNT;
static float cw_jump_boost = DEF_JUMP;
static float cw_stopspeed = 0.10f;

/* MOTD Parameters */
static cc_bool motd_override = false;
static int motd_mode = 0;
static float motd_friction = DEF_FRICTION;
static float motd_gravity = DEF_GRAV;
static float motd_ground_speed = DEF_GROUND_SPEED;
static float motd_ground_accel = DEF_GROUND_ACCEL;
static float motd_bounce = DEF_BOUNCE;
static float motd_air_speed = DEF_AIR_SPEED;
static float motd_air_accel = DEF_AIR_ACCEL;
static float motd_air_cap = DEF_AIR_CAP;
static float motd_ricochet = DEF_RICOCHET;
static float motd_ricochet_up = DEF_RICOCHET_UP;
static int motd_ricochet_count = DEF_RICOCHET_COUNT;
static float motd_jump_boost = DEF_JUMP;

static char cached_motd[256] = { 0 };

static void Cwake_GetView(struct Matrix* view) {
    if (cw_orig_GetView) {
        cw_orig_GetView(view);
    }

    if (cw_enabled) {
        cw_current_roll += (cw_target_roll - cw_current_roll) * 0.12f;

        if (fabsf(cw_current_roll) > 0.0001f) {
            struct Matrix rollM;
            Matrix_RotateZ(&rollM, cw_current_roll);
            Matrix_MulBy(view, &rollM);
        }
    } else {
        cw_current_roll = 0.0f;
    }
}

static void Cwake_ParseMOTD(const char* motdStr, int len) {
    char lowerMOTD[256] = { 0 };
    int i; char* ptr;

    motd_override = false;
    if (len == 0) return;

    memcpy(lowerMOTD, motdStr, len);
    for (i = 0; i < len; i++) {
        if (lowerMOTD[i] >= 'A' && lowerMOTD[i] <= 'Z') lowerMOTD[i] += 32;
    }

    motd_mode = 0; motd_friction = DEF_FRICTION; motd_gravity = DEF_GRAV;
    motd_ground_speed = DEF_GROUND_SPEED; motd_ground_accel = DEF_GROUND_ACCEL; motd_bounce = DEF_BOUNCE;
    motd_air_speed = DEF_AIR_SPEED; motd_air_accel = DEF_AIR_ACCEL; motd_air_cap = DEF_AIR_CAP;
    motd_ricochet = DEF_RICOCHET; motd_ricochet_up = DEF_RICOCHET_UP; motd_ricochet_count = DEF_RICOCHET_COUNT; 
    motd_jump_boost = DEF_JUMP;

    if ((ptr = strstr(lowerMOTD, "mode="))) { motd_mode = atoi(ptr + 5); motd_override = true; }
    if ((ptr = strstr(lowerMOTD, "friction="))) { motd_friction = (float)atof(ptr + 9); motd_override = true; }
    if ((ptr = strstr(lowerMOTD, "grav="))) { motd_gravity = (float)atof(ptr + 5); motd_override = true; }
    
    if ((ptr = strstr(lowerMOTD, "groundspeed="))) { motd_ground_speed = (float)atof(ptr + 12); motd_override = true; }
    if ((ptr = strstr(lowerMOTD, "groundaccel="))) { motd_ground_accel = (float)atof(ptr + 12); motd_override = true; }
    if ((ptr = strstr(lowerMOTD, "bounce="))) { motd_bounce = (float)atof(ptr + 7); motd_override = true; }
    
    if ((ptr = strstr(lowerMOTD, "airspeed="))) { motd_air_speed = (float)atof(ptr + 9); motd_override = true; }
    if ((ptr = strstr(lowerMOTD, "airaccel="))) { motd_air_accel = (float)atof(ptr + 9); motd_override = true; }
    if ((ptr = strstr(lowerMOTD, "aircap="))) { motd_air_cap = (float)atof(ptr + 7); motd_override = true; }
    
    if ((ptr = strstr(lowerMOTD, "ricochet="))) { motd_ricochet = (float)atof(ptr + 9); motd_override = true; }
    if ((ptr = strstr(lowerMOTD, "ricochetup="))) { motd_ricochet_up = (float)atof(ptr + 11); motd_override = true; }
    if ((ptr = strstr(lowerMOTD, "ricochetcount="))) { motd_ricochet_count = atoi(ptr + 14); motd_override = true; }
    if ((ptr = strstr(lowerMOTD, "jump="))) { motd_jump_boost = (float)atof(ptr + 5); motd_override = true; }
}

static void Cwake_CheckMOTD(void) {
    char current_motd[256] = { 0 };
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

static void Cwake_OnNewMapLoaded(void) {
    cached_motd[0] = '\0';
}

/* Profiles */
static void Cwake_SaveConfig(const char* profile) {
    FILE* f; cc_string msg; char path[128]; char buf[128];

    sprintf(path, "plugins/cwake/%.80s.txt", profile);
    f = fopen(path, "w");
    if (!f) {
        String_InitArray(msg, buf); String_AppendConst(&msg, "&c[Cwake] Failed to save! Create the 'cwake' folder inside 'plugins'."); Chat_Add(&msg);
        return;
    }

    if (motd_override) {
        String_InitArray(msg, buf); String_AppendConst(&msg, "&c[Cwake] Cannot save: Server is enforcing MOTD physics."); Chat_Add(&msg);
        fclose(f);
        return;
    }
    
    fprintf(f, "mode %d\nfriction %f\ngrav %f\ngroundspeed %f\ngroundaccel %f\nbounce %f\nairspeed %f\nairaccel %f\naircap %f\nricochet %f\nricochetup %f\nricochetcount %d\njump %f\n",
        cw_mode, cw_friction, cw_gravity, cw_ground_speed, cw_ground_accel, cw_bounce, cw_air_speed, cw_air_accel, cw_air_cap, cw_ricochet, cw_ricochet_up, cw_ricochet_count, cw_jump_boost);
    fclose(f);

    sprintf(buf, "&a[Cwake] Profile '%.80s' saved!", profile);
    String_InitArray(msg, buf); String_AppendConst(&msg, buf); Chat_Add(&msg);
}

static void Cwake_LoadConfig(const char* profile) {
    FILE* f; cc_string msg; char path[128]; char buf[128]; char key[32]; float val;

    if (motd_override) {
        String_InitArray(msg, buf); String_AppendConst(&msg, "&c[Cwake] Cannot load: Server is enforcing MOTD physics."); Chat_Add(&msg);
        return;
    }

    sprintf(path, "plugins/cwake/%.80s.txt", profile);
    f = fopen(path, "r");
    if (!f) {
        sprintf(buf, "&c[Cwake] Profile '%.80s.txt' not found in plugins/cwake/.", profile);
        String_InitArray(msg, buf); String_AppendConst(&msg, buf); Chat_Add(&msg);
        return;
    }

    while (fscanf(f, "%31s %f", key, &val) == 2) {
        if (strcmp(key, "mode") == 0) cw_mode = (int)val;
        else if (strcmp(key, "friction") == 0) cw_friction = val;
        else if (strcmp(key, "grav") == 0) cw_gravity = val;
        else if (strcmp(key, "groundspeed") == 0) cw_ground_speed = val;
        else if (strcmp(key, "groundaccel") == 0) cw_ground_accel = val;
        else if (strcmp(key, "bounce") == 0) cw_bounce = val;
        else if (strcmp(key, "airspeed") == 0) cw_air_speed = val;
        else if (strcmp(key, "airaccel") == 0) cw_air_accel = val;
        else if (strcmp(key, "aircap") == 0) cw_air_cap = val;
        else if (strcmp(key, "ricochet") == 0) cw_ricochet = val;
        else if (strcmp(key, "ricochetup") == 0) cw_ricochet_up = val;
        else if (strcmp(key, "ricochetcount") == 0) cw_ricochet_count = (int)val;
        else if (strcmp(key, "jump") == 0) cw_jump_boost = val;
    }
    fclose(f);

    sprintf(buf, "&a[Cwake] Profile '%.80s' loaded!", profile);
    String_InitArray(msg, buf); String_AppendConst(&msg, buf); Chat_Add(&msg);
}

static void CwakeCmd_Execute(const cc_string* args, int argsCount) {
    char buf[256]; cc_string msg; char cmd[32] = { 0 }; char valStr[32] = { 0 }; char profile[32] = { 0 };
    int len, vLen; cc_bool hasVal; struct LocalPlayer* p = Entities.CurPlayer;

    if (argsCount == 0 || args[0].length == 0) {
        String_InitArray(msg, buf); String_AppendConst(&msg, "&b--- Cwake Commands ---"); Chat_Add(&msg);
        String_InitArray(msg, buf); String_AppendConst(&msg, "&e/cwake toggle &f- Toggle Cwake physics"); Chat_Add(&msg);
        String_InitArray(msg, buf); String_AppendConst(&msg, "&e/cwake display &f- Show current parameter values"); Chat_Add(&msg);
        String_InitArray(msg, buf); String_AppendConst(&msg, "&e/cwake params &f- List all parameter names"); Chat_Add(&msg);
        String_InitArray(msg, buf); String_AppendConst(&msg, "&e/cwake help <param> &f- Explains a parameter"); Chat_Add(&msg);
        String_InitArray(msg, buf); String_AppendConst(&msg, "&e/cwake speedo <reset> &f- Toggle speedometer or reset top speed"); Chat_Add(&msg);
        String_InitArray(msg, buf); String_AppendConst(&msg, "&e/cwake tilt &f- Toggle camera rolling"); Chat_Add(&msg);
        String_InitArray(msg, buf); String_AppendConst(&msg, "&e/cwake [save|load] <name> &f- Save/Load cwake profiles."); Chat_Add(&msg);
        String_InitArray(msg, buf); String_AppendConst(&msg, "&e/cwake motd &f- Show MOTD flags for current physics"); Chat_Add(&msg);
        String_InitArray(msg, buf); String_AppendConst(&msg, "&e/cwake [param] <value> &f- Set parameter (omit <value> to reset)");
        return;
    }

    len = args[0].length < 31 ? args[0].length : 31;
    memcpy(cmd, args[0].buffer, len); cmd[len] = '\0';

    // Parameter help
    if (strcmp(cmd, "help") == 0) {
        if (argsCount > 1) {
            char param[32];
            vLen = args[1].length < 31 ? args[1].length : 31;
            memcpy(param, args[1].buffer, vLen); param[vLen] = '\0';

            if (strcmp(param, "mode") == 0) { sprintf(buf, "&eMode (Default: 0): &f0 = Relative to the world, 1 = Relative to the camera"); }
            else if (strcmp(param, "friction") == 0) { sprintf(buf, "&eFriction (Default: %.1f): &fGround deceleration rate.", DEF_FRICTION); }
            else if (strcmp(param, "grav") == 0) { sprintf(buf, "&eGrav (Default: %.1f): &fGravity scale", DEF_GRAV); }
            else if (strcmp(param, "groundspeed") == 0) { sprintf(buf, "&eGroundSpeed (Default: %.2f): &fMax walking speed.", DEF_GROUND_SPEED); }
            else if (strcmp(param, "groundaccel") == 0) { sprintf(buf, "&eGroundAccel (Default: %.1f): &fHow quickly you reach max speed.", DEF_GROUND_ACCEL); }
            else if (strcmp(param, "bounce") == 0) { sprintf(buf, "&eBounce (Default: %.1f): &fConservation of velocity when bouncing off the floor", DEF_BOUNCE); }
            else if (strcmp(param, "airspeed") == 0) { sprintf(buf, "&eAirSpeed (Default: %.2f): &fMid-Air target max speed.", DEF_AIR_SPEED); }
            else if (strcmp(param, "airaccel") == 0) { sprintf(buf, "&eAirAccel (Default: %.1f): &fMid-air turning control.", DEF_AIR_ACCEL); }
            else if (strcmp(param, "aircap") == 0) { sprintf(buf, "&eAirCap (Default: %.2f): &fMid-air speed increase limit per tick. ", DEF_AIR_CAP); }
            else if (strcmp(param, "ricochet") == 0) { sprintf(buf, "&eRicochet (Default: %.1f): &fConservation of velocity when bouncing off a wall", DEF_RICOCHET); }
            else if (strcmp(param, "ricochetup") == 0) { sprintf(buf, "&eRicochetUp (Default: %.1f): &fRatio of horizontal speed converted into vertical when hitting a wall. (ricochet cannot equal 0 for this effect to work!)", DEF_RICOCHET_UP); }
            else if (strcmp(param, "ricochetcount") == 0) { sprintf(buf, "&eRicochetCount (Default: %d): &fMax chainable ricochets.", DEF_RICOCHET_COUNT); }
            else if (strcmp(param, "jump") == 0) { sprintf(buf, "&eJump (Default: %.1f): &fVertical jump scale", DEF_JUMP); }
            else { sprintf(buf, "&cUnknown parameter."); }
            String_InitArray(msg, buf); String_AppendConst(&msg, buf); Chat_Add(&msg);
        }
        else {
            String_InitArray(msg, buf); String_AppendConst(&msg, "&eType &a/cwake help <param> &efor details."); Chat_Add(&msg);
        }
        return;
    }

    if (strcmp(cmd, "motd") == 0) {
        String_InitArray(msg, buf); String_AppendConst(&msg, "&b--- Cwake MOTD Strings ---"); Chat_Add(&msg);

        sprintf(buf, "mode=%d friction=%.2f grav=%.2f",
            motd_override ? motd_mode : cw_mode,
            motd_override ? motd_friction : cw_friction,
            motd_override ? motd_gravity : cw_gravity);
        String_InitArray(msg, buf); String_AppendConst(&msg, buf); Chat_Add(&msg);

        sprintf(buf, "groundspeed=%.3f groundaccel=%.2f bounce=%.2f",
            motd_override ? motd_ground_speed : cw_ground_speed,
            motd_override ? motd_ground_accel : cw_ground_accel,
            motd_override ? motd_bounce : cw_bounce);
        String_InitArray(msg, buf); String_AppendConst(&msg, buf); Chat_Add(&msg);
        
        sprintf(buf, "airspeed=%.3f airaccel=%.2f aircap=%.3f",
            motd_override ? motd_air_speed : cw_air_speed,
            motd_override ? motd_air_accel : cw_air_accel,
            motd_override ? motd_air_cap : cw_air_cap);
        String_InitArray(msg, buf); String_AppendConst(&msg, buf); Chat_Add(&msg);
        
        sprintf(buf, "ricochet=%.2f ricochetup=%.2f ricochetcount=%d",
            motd_override ? motd_ricochet : cw_ricochet,
            motd_override ? motd_ricochet_up : cw_ricochet_up,
            motd_override ? motd_ricochet_count : cw_ricochet_count);
        String_InitArray(msg, buf); String_AppendConst(&msg, buf); Chat_Add(&msg);

        return;
    }

    if (strcmp(cmd, "display") == 0) {
        String_InitArray(msg, buf); String_AppendConst(&msg, "&b--- Cwake Parameter Values ---"); Chat_Add(&msg);

        sprintf(buf, "&fMode: &7%d &b| &fFriction: &7%.1f &b| &fGravity: &7%.2f",
            motd_override ? motd_mode : cw_mode,
            motd_override ? motd_friction : cw_friction,
            motd_override ? motd_gravity : cw_gravity);
        String_InitArray(msg, buf); String_AppendConst(&msg, buf); Chat_Add(&msg);

        sprintf(buf, "&fGroundSpeed: &7%.2f &b| &fGroundAccel: &7%.1f &b| &fBounce: &7%.2f",
            motd_override ? motd_ground_speed : cw_ground_speed,
            motd_override ? motd_ground_accel : cw_ground_accel,
            motd_override ? motd_bounce : cw_bounce);
        String_InitArray(msg, buf); String_AppendConst(&msg, buf); Chat_Add(&msg);

        sprintf(buf, "&fAirSpeed: &7%.2f &b| &fAirAccel: &7%.1f &b| &fAirCap: &7%.3f",
            motd_override ? motd_air_speed : cw_air_speed,
            motd_override ? motd_air_accel : cw_air_accel,
            motd_override ? motd_air_cap : cw_air_cap);
        String_InitArray(msg, buf);  String_AppendConst(&msg, buf); Chat_Add(&msg);

        sprintf(buf, "&fRicochet: &7%.2f &b| &fRicochetUp: &7%.2f &b| &fRicochetCount: &7%d",
            motd_override ? motd_ricochet : cw_ricochet,
            motd_override ? motd_ricochet_up : cw_ricochet_up,
            motd_override ? motd_ricochet_count : cw_ricochet_count);
        String_InitArray(msg, buf); String_AppendConst(&msg, buf); Chat_Add(&msg);

        return;
    }

    if (strcmp(cmd, "toggle") == 0) {
        if (motd_override) {
            String_InitArray(msg, buf); String_AppendConst(&msg, "&c[Cwake] Cannot toggle: Map is using custom physics."); Chat_Add(&msg);
            return;
        }
        if (!cw_enabled && p && !p->Hacks.CanSpeed) {
            String_InitArray(msg, buf); String_AppendConst(&msg, "&c[Cwake] Cannot toggle: Speedhax are disabled."); Chat_Add(&msg);
            return;
        }
        cw_enabled = !cw_enabled;
        if (cw_enabled && p) cw_last_yaw = p->Base.Yaw;

        String_InitArray(msg, buf); String_AppendConst(&msg, cw_enabled ? "&a[Cwake] Custom Physics ENABLED" : "&c[Cwake] Custom Physics DISABLED"); Chat_Add(&msg);
        return;
    }

    if (strcmp(cmd, "params") == 0) {
        String_InitArray(msg, buf); String_AppendConst(&msg, "&bParams: &fmode, friction, grav, groundspeed, groundaccel, bounce"); Chat_Add(&msg);
        String_InitArray(msg, buf); String_AppendConst(&msg, "             airspeed, airaccel, aircap, ricochet, ricochetup, ricochetcount, jump"); Chat_Add(&msg);
        return;
    }

    if (strcmp(cmd, "speedo") == 0) {
        if (argsCount > 1 && args[1].length >= 5 && args[1].buffer[0] == 'r') {
            cw_top_speed = 0.0f;
            String_InitArray(msg, buf); String_AppendConst(&msg, "&a[Cwake] Top speed reset!"); Chat_Add(&msg);
            return;
        }
        cw_speedo = !cw_speedo;
        String_InitArray(msg, buf); String_AppendConst(&msg, cw_speedo ? "&a[Cwake] Speedometer ON" : "&c[Cwake] Speedometer OFF"); Chat_Add(&msg);
        if (!cw_speedo) { String_InitArray(msg, buf); String_AppendConst(&msg, ""); Chat_AddOf(&msg, 11); }
        return;
    }

    if (strcmp(cmd, "tilt") == 0) {
        cw_do_tilt = !cw_do_tilt;
        String_InitArray(msg, buf); String_AppendConst(&msg, cw_do_tilt ? "&a[Cwake] Camera Tilt ON" : "&c[Cwake] Camera Tilt OFF"); Chat_Add(&msg);
        if (!cw_do_tilt) cw_target_roll = 0.0f;
        return;
    }

    if (strcmp(cmd, "save") == 0 || strcmp(cmd, "load") == 0) {
        strncpy(profile, "default", sizeof(profile) - 1);
        profile[sizeof(profile) - 1] = '\0';
        if (argsCount > 1) {
            vLen = args[1].length < 31 ? args[1].length : 31;
            memcpy(profile, args[1].buffer, vLen); profile[vLen] = '\0';
        }
        if (strcmp(cmd, "save") == 0) Cwake_SaveConfig(profile);
        else Cwake_LoadConfig(profile);
        return;
    }

    if (motd_override) {
        String_InitArray(msg, buf); String_AppendConst(&msg, "&cCannot edit params: Server is enforcing MOTD physics."); Chat_Add(&msg);
        return;
    }

    hasVal = argsCount > 1;
    if (hasVal) {
        vLen = args[1].length < 31 ? args[1].length : 31;
        memcpy(valStr, args[1].buffer, vLen); valStr[vLen] = '\0';
    }

    if (strcmp(cmd, "mode") == 0) {
        cw_mode = hasVal ? atoi(valStr) : 0;
        if (cw_mode < 0 || cw_mode > 1) cw_mode = 0;
        sprintf(buf, "&e[Cwake] Mode set to %s", cw_mode == 1 ? "1 (Camera-Locked)" : "0 (Source)");
    }
    else if (strcmp(cmd, "friction") == 0) {
        cw_friction = hasVal ? fabsf((float)atof(valStr)) : DEF_FRICTION;
        sprintf(buf, "&e[Cwake] Friction set to %.2f", cw_friction);
    }
    else if (strcmp(cmd, "grav") == 0 || strcmp(cmd, "gravity") == 0) {
        cw_gravity = hasVal ? (float)atof(valStr) : DEF_GRAV;
        sprintf(buf, "&e[Cwake] Gravity set to %.2f", cw_gravity);
    }
    else if (strcmp(cmd, "groundspeed") == 0) {
        cw_ground_speed = hasVal ? fabsf((float)atof(valStr)) : DEF_GROUND_SPEED;
        sprintf(buf, "&e[Cwake] GroundSpeed set to %.4f", cw_ground_speed);
    }
    else if (strcmp(cmd, "groundaccel") == 0) {
        cw_ground_accel = hasVal ? fabsf((float)atof(valStr)) : DEF_GROUND_ACCEL;
        sprintf(buf, "&e[Cwake] GroundAccel set to %.2f", cw_ground_accel);
    }
    else if (strcmp(cmd, "bounce") == 0) {
        cw_bounce = hasVal ? fabsf((float)atof(valStr)) : DEF_BOUNCE;
        sprintf(buf, "&e[Cwake] Bounce set to %.2f", cw_bounce);
    }
    else if (strcmp(cmd, "airspeed") == 0) {
        cw_air_speed = hasVal ? fabsf((float)atof(valStr)) : DEF_AIR_SPEED;
        sprintf(buf, "&e[Cwake] AirSpeed set to %.4f", cw_air_speed);
    }
    else if (strcmp(cmd, "airaccel") == 0) {
        cw_air_accel = hasVal ? fabsf((float)atof(valStr)) : DEF_AIR_ACCEL;
        sprintf(buf, "&e[Cwake] AirAccel set to %.2f", cw_air_accel);
    }
    else if (strcmp(cmd, "aircap") == 0) {
        cw_air_cap = hasVal ? fabsf((float)atof(valStr)) : DEF_AIR_CAP;
        sprintf(buf, "&e[Cwake] AirCap set to %.4f", cw_air_cap);
    }
    else if (strcmp(cmd, "ricochet") == 0 || strcmp(cmd, "walljump") == 0) {
        cw_ricochet = hasVal ? fabsf((float)atof(valStr)) : DEF_RICOCHET;
        sprintf(buf, "&e[Cwake] Ricochet bounce set to %.2f", cw_ricochet);
    }
    else if (strcmp(cmd, "ricochetup") == 0 || strcmp(cmd, "wjup") == 0) {
        cw_ricochet_up = hasVal ? fabsf((float)atof(valStr)) : DEF_RICOCHET_UP;
        sprintf(buf, "&e[Cwake] Ricochet Up Ratio set to %.2f", cw_ricochet_up);
    }
    else if (strcmp(cmd, "ricochetcount") == 0 || strcmp(cmd, "riccount") == 0) {
        cw_ricochet_count = hasVal ? atoi(valStr) : DEF_RICOCHET_COUNT;
        sprintf(buf, "&e[Cwake] RicochetCount set to %d", cw_ricochet_count);
    }
    else if (strcmp(cmd, "jump") == 0) {
        cw_jump_boost = hasVal ? fabsf((float)atof(valStr)) : DEF_JUMP;
        sprintf(buf, "&e[Cwake] Jump Power set to %.2f", cw_jump_boost);
    }
    else {
        String_InitArray(msg, buf); String_AppendConst(&msg, "&c[Cwake] Unknown parameter."); Chat_Add(&msg);
        return;
    }

    String_InitArray(msg, buf); String_AppendConst(&msg, buf); Chat_Add(&msg);
}

static struct ChatCommand cwake_cmd = {
    "cwake", CwakeCmd_Execute, false,
    {
        "&e/cwake &f- Shows command help menu",
        "&e/cwake toggle &f- Toggles Cwake physics",
        "&e/cwake tilt &f- Toggles camera rolling",
        "&e/cwake [save|load] [name] &f- Disk presets",
        "&e/cwake [param] <val> &f- Sets a parameter"
    }
};

/*  The Actual Physics Mathsy Bit */
static void Cwake_ApplyFriction(struct Entity* e, float delta, float cur_f) {
    float vx = e->Velocity.x; float vz = e->Velocity.z;
    float speed = sqrtf(vx * vx + vz * vz);
    float control, drop, newspeed;

    if (speed < 0.00001f) { e->Velocity.x = 0.0f; e->Velocity.z = 0.0f; return; }

    control = (speed < cw_stopspeed) ? cw_stopspeed : speed;
    drop = control * cur_f * delta;
    newspeed = speed - drop;
    if (newspeed < 0.0f) newspeed = 0.0f;
    newspeed /= speed;

    e->Velocity.x *= newspeed; e->Velocity.z *= newspeed;
}

static float Cwake_WishDir(struct LocalPlayer* p, float* wx, float* wz, float cur_sp) {
    float fm = 0.0f, sm = 0.0f, yaw, sinY, cosY, len;

    if (!Gui.InputGrab) {
        cc_bool fwd = KeyBind_IsPressed(BIND_FORWARD);
        cc_bool back = KeyBind_IsPressed(BIND_BACK);
        cc_bool left = KeyBind_IsPressed(BIND_LEFT);
        cc_bool right = KeyBind_IsPressed(BIND_RIGHT);
        fm = (fwd ? 1.0f : 0.0f) - (back ? 1.0f : 0.0f);
        sm = (right ? 1.0f : 0.0f) - (left ? 1.0f : 0.0f);
    }

    if (fm == 0.0f && sm == 0.0f) { *wx = 0.0f; *wz = 0.0f; return 0.0f; }

    yaw = p->Base.Yaw * 0.01745329251f;
    sinY = sinf(yaw); cosY = cosf(yaw);

    *wx = fm * sinY + sm * cosY;
    *wz = fm * -cosY + sm * sinY;

    len = sqrtf(*wx * *wx + *wz * *wz);
    if (len > 0.00001f) { *wx /= len; *wz /= len; }

    return cur_sp;
}

static void Cwake_Accelerate(struct Entity* e, float wx, float wz, float wishspeed, float accel, float delta) {
    float cur = e->Velocity.x * wx + e->Velocity.z * wz;
    float add = wishspeed - cur;
    float acc;
    if (add <= 0.0f) return;
    acc = accel * wishspeed * delta;
    if (acc > add) acc = add;
    e->Velocity.x += acc * wx; e->Velocity.z += acc * wz;
}

static void Cwake_AirAccelerate(struct Entity* e, float wx, float wz, float wishspeed, float accel, float aircap, float delta) {
    float capped = (wishspeed > aircap) ? aircap : wishspeed;
    float cur = e->Velocity.x * wx + e->Velocity.z * wz;
    float add = capped - cur;
    float acc;
    if (add <= 0.0f) return;
    acc = accel * wishspeed * delta;
    if (acc > add) acc = add;
    e->Velocity.x += acc * wx; e->Velocity.z += acc * wz;
}

static void Cwake_Tick(struct Entity* e, float delta) {
    struct LocalPlayer* p = (struct LocalPlayer*)e;
    Vec3 savedDrag, savedGndFric;
    float savedSpeedMulti, wx, wz, wishspeed;
    cc_bool onGround, tryingToJump, initiatedJump = false;
    float current_yaw, dyaw, cosD, sinD, vx, vz;

    float cur_f = motd_override ? motd_friction : cw_friction;
    float cur_gaccel = motd_override ? motd_ground_accel : cw_ground_accel;
    float cur_airacc = motd_override ? motd_air_accel : cw_air_accel;
    float cur_aircap = motd_override ? motd_air_cap : cw_air_cap;
    float cur_gspeed = motd_override ? motd_ground_speed : cw_ground_speed;
    float cur_aspeed = motd_override ? motd_air_speed : cw_air_speed;
    float cur_grav = motd_override ? motd_gravity : cw_gravity;
    float cur_ricochet = motd_override ? motd_ricochet : cw_ricochet;
    float cur_ricochet_up = motd_override ? motd_ricochet_up : cw_ricochet_up;
    int   cur_ricochet_count = motd_override ? motd_ricochet_count : cw_ricochet_count;
    float cur_bounce = motd_override ? motd_bounce : cw_bounce;
    int   cur_mode = motd_override ? motd_mode : cw_mode;

    float old_vx, old_vy, old_vz, lookX, lookZ, impactX, impactZ, imLen, wallDot;
    float total_hz, remain_ratio, bounce_vel;
    cc_bool hitX = false, hitZ = false;
    cc_bool can_manual;

    Cwake_CheckMOTD();

    if (Camera.Active && Camera.Active->GetView != Cwake_GetView) {
        cw_orig_GetView = Camera.Active->GetView;
        Camera.Active->GetView = Cwake_GetView;
    }

    can_manual = cw_enabled && p->Hacks.CanSpeed;
    if ((!motd_override && !can_manual) || p->Hacks.Flying || p->Hacks.Noclip) {
        cw_origVTABLE->Tick(e, delta);
        cw_last_yaw = p->Base.Yaw;
        cw_target_roll = 0.0f;
        cw_current_roll = 0.0f;
        return;
    }

    current_yaw = p->Base.Yaw;
    if (cur_mode == 1) {
        dyaw = (current_yaw - cw_last_yaw) * 0.01745329251f;
        if (dyaw > 0.00001f || dyaw < -0.00001f) {
            cosD = cosf(dyaw); sinD = sinf(dyaw);
            vx = e->Velocity.x; vz = e->Velocity.z;
            e->Velocity.x = vx * cosD - vz * sinD;
            e->Velocity.z = vx * sinD + vz * cosD;
        }
    }
    cw_last_yaw = current_yaw;

    onGround = e->OnGround;
    if (onGround) cw_current_ricochets = 0;
    tryingToJump = !Gui.InputGrab && KeyBind_IsPressed(BIND_JUMP);

    if (onGround && !tryingToJump) Cwake_ApplyFriction(e, delta, cur_f);
    if (onGround && tryingToJump) initiatedJump = true;

    wishspeed = Cwake_WishDir(p, &wx, &wz, onGround ? cur_gspeed : cur_aspeed);
    if (wishspeed > 0.0f) {
        if (cur_mode == 1) {
            float curSpeed = sqrtf(e->Velocity.x * e->Velocity.x + e->Velocity.z * e->Velocity.z);
            if (curSpeed > 0.001f) { e->Velocity.x = curSpeed * wx; e->Velocity.z = curSpeed * wz; }
        }

        if (onGround && !tryingToJump) Cwake_Accelerate(e, wx, wz, wishspeed, cur_gaccel, delta);
        else Cwake_AirAccelerate(e, wx, wz, wishspeed, cur_airacc, cur_aircap, delta);
    }

    if (cw_speedo) {
        float hzSpeed = sqrtf(e->Velocity.x * e->Velocity.x + e->Velocity.z * e->Velocity.z);
        float realBPS = hzSpeed * 20.0f;
        char rawBuf[64]; char msgBuf[128]; cc_string msg;

        if (realBPS > cw_top_speed) cw_top_speed = realBPS;

        sprintf(rawBuf, "&bSpeed: %.1f | Top: %.1f", realBPS, cw_top_speed);
        String_InitArray(msg, msgBuf); String_AppendConst(&msg, rawBuf); Chat_AddOf(&msg, 11);
    }

    savedDrag = p->Physics.drag;
    savedGndFric = p->Physics.groundFriction;
    savedSpeedMulti = p->Hacks.SpeedMultiplier;

    p->Physics.drag.x = 1.0f; p->Physics.drag.z = 1.0f;
    p->Physics.groundFriction.x = 1.0f; p->Physics.groundFriction.z = 1.0f;
    p->Hacks.SpeedMultiplier = 0.0f;

    old_vx = e->Velocity.x;
    old_vy = e->Velocity.y;
    old_vz = e->Velocity.z;

    if (!onGround && cur_grav != 1.0f) { e->Velocity.y += 0.08f * (1.0f - cur_grav); }

    cw_origVTABLE->Tick(e, delta);

    if (fabsf(old_vx) > 0.05f && fabsf(e->Velocity.x) < 0.01f) hitX = true;
    if (fabsf(old_vz) > 0.05f && fabsf(e->Velocity.z) < 0.01f) hitZ = true;

    if (!hitX) e->Velocity.x = old_vx;
    if (!hitZ) e->Velocity.z = old_vz;

    if (cur_ricochet > 0.0f && !onGround && tryingToJump && cw_current_ricochets < cur_ricochet_count) {
        impactX = 0.0f; impactZ = 0.0f;

        if (hitX) impactX = old_vx;
        if (hitZ) impactZ = old_vz;

        if (hitX || hitZ) {
            lookX = sinf(current_yaw * 0.01745329251f);
            lookZ = -cosf(current_yaw * 0.01745329251f);

            imLen = sqrtf(impactX * impactX + impactZ * impactZ);
            if (imLen > 0.0001f) { impactX /= imLen; impactZ /= imLen; }

            wallDot = (impactX * lookX) + (impactZ * lookZ);

            if (wallDot > 0.75f) {
                total_hz = sqrtf(old_vx * old_vx + old_vz * old_vz);

                remain_ratio = 1.0f - cur_ricochet_up;
                if (remain_ratio < 0.0f) remain_ratio = 0.0f;

                if (hitX) e->Velocity.x = -old_vx * cur_ricochet * remain_ratio;
                if (hitZ) e->Velocity.z = -old_vz * cur_ricochet * remain_ratio;

                e->Velocity.y = (0.42f * (motd_override ? 1.0f : cw_jump_boost)) + (total_hz * cur_ricochet_up);
                cw_current_ricochets++;
            }
        }
    }

    if (cur_bounce > 0.0f && !onGround && e->OnGround && old_vy < -0.25f) {
        bounce_vel = -old_vy * cur_bounce;
        if (bounce_vel > 0.15f) {
            e->Velocity.y = bounce_vel;
        }
    }

    if (initiatedJump && e->Velocity.y > 0.001f) {
        e->Velocity.y *= (motd_override ? 1.0f : cw_jump_boost);
    }

    p->Physics.drag = savedDrag; 
    p->Physics.groundFriction = savedGndFric; 
    p->Hacks.SpeedMultiplier = savedSpeedMulti;
    
    /* Camera Roll */
    {
        float yaw_rad = current_yaw * 0.01745329251f;
        float lateral_speed = (e->Velocity.x * cosf(yaw_rad)) + (e->Velocity.z * sinf(yaw_rad));
        
        if (cw_do_tilt) {
            cw_target_roll = lateral_speed * 0.35f;
            
            /* ~4.5 degrees tilt limit*/
            if (cw_target_roll > 0.08f) cw_target_roll = 0.08f;
            if (cw_target_roll < -0.08f) cw_target_roll = -0.08f;
        } else {
            cw_target_roll = 0.0f;
        }
    }
}

static void CwakePlugin_Init(void) {
    cc_string msg;
    char buf[80];

    Commands_Register(&cwake_cmd);

    String_InitArray(msg, buf);
    String_AppendConst(&msg, "&fLoading Cwake v1.0-pre");
    Chat_Add(&msg);

    String_AppendConst(&Server.AppName, " + Cwake v1.0-pre");

    cw_origVTABLE = Entities.CurPlayer->Base.VTABLE;
    cw_hookedVTABLE = *cw_origVTABLE;
    cw_hookedVTABLE.Tick = Cwake_Tick;
    Entities.CurPlayer->Base.VTABLE = &cw_hookedVTABLE;
}

static void CwakePlugin_Free(void) {
    if (cw_origVTABLE) Entities.CurPlayer->Base.VTABLE = cw_origVTABLE;
    
    if (Camera.Active && cw_orig_GetView && Camera.Active->GetView == Cwake_GetView) {
        Camera.Active->GetView = cw_orig_GetView;
    }
}

PLUGIN_EXPORT int Plugin_ApiVersion = 1;
PLUGIN_EXPORT struct IGameComponent Plugin_Component = {
    CwakePlugin_Init,
    CwakePlugin_Free,
    NULL,
    NULL,
    Cwake_OnNewMapLoaded
};