#include "cwake.h"

const struct EntityVTABLE* cw_origVTABLE = NULL;
struct EntityVTABLE cw_hookedVTABLE;

void (*cw_orig_GetView)(struct Matrix* view) = NULL;
float cw_target_roll = 0.0f;
float cw_current_roll = 0.0f;

float cw_top_speed = 0.0f;
float cw_last_yaw = 0.0f;
int cw_current_ricochets = 0;

float cw_stopspeed = 0.10f;

void Cwake_GetView(struct Matrix* view) {
  if (cw_orig_GetView) {
    cw_orig_GetView(view);
  }

  if (cw_active_profile.enable_physics) {
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

static void Cwake_ApplyFriction(struct Entity* e, float delta, float cur_f) {
  float vx = e->Velocity.x;
  float vz = e->Velocity.z;
  float speed = sqrtf(vx * vx + vz * vz);
  float control, drop, newspeed;

  if (speed < 0.00001f) {
    e->Velocity.x = 0.0f;
    e->Velocity.z = 0.0f;
    return;
  }

  control = (speed < cw_stopspeed) ? cw_stopspeed : speed;
  drop = control * cur_f * delta;
  newspeed = speed - drop;
  if (newspeed < 0.0f) newspeed = 0.0f;
  newspeed /= speed;

  e->Velocity.x *= newspeed;
  e->Velocity.z *= newspeed;
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

  if (fm == 0.0f && sm == 0.0f) {
    *wx = 0.0f;
    *wz = 0.0f;
    return 0.0f;
  }

  yaw = p->Base.Yaw * 0.01745329251f;
  sinY = sinf(yaw);
  cosY = cosf(yaw);

  *wx = fm * sinY + sm * cosY;
  *wz = fm * -cosY + sm * sinY;

  len = sqrtf(*wx * *wx + *wz * *wz);
  if (len > 0.00001f) {
    *wx /= len;
    *wz /= len;
  }

  return cur_sp;
}

static void Cwake_Accelerate(struct Entity* e, float wx, float wz, float wishspeed, float accel, float delta) {
  float cur = e->Velocity.x * wx + e->Velocity.z * wz;
  float add = wishspeed - cur;
  float acc;
  if (add <= 0.0f) return;
  acc = accel * wishspeed * delta;
  if (acc > add) acc = add;
  e->Velocity.x += acc * wx;
  e->Velocity.z += acc * wz;
}

static void Cwake_AirAccelerate(struct Entity* e, float wx, float wz, float wishspeed, float accel, float aircap,
                                float delta) {
  float capped = (wishspeed > aircap) ? aircap : wishspeed;
  float cur = e->Velocity.x * wx + e->Velocity.z * wz;
  float add = capped - cur;
  float acc;
  if (add <= 0.0f) return;
  acc = accel * wishspeed * delta;
  if (acc > add) acc = add;
  e->Velocity.x += acc * wx;
  e->Velocity.z += acc * wz;
}

void Cwake_Tick(struct Entity* e, float delta) {
  struct LocalPlayer* p = (struct LocalPlayer*)e;
  Vec3 savedDrag, savedGndFric;
  float savedSpeedMulti, wx, wz, wishspeed;
  cc_bool onGround, tryingToJump, initiatedJump = false;
  float current_yaw, dyaw, cosD, sinD, vx, vz;

  CwakeProfile* prof = cw_motd_override_active ? &cw_motd_profile : &cw_active_profile;

  float cur_f = prof->friction;
  float cur_gaccel = prof->ground_accel;
  float cur_airacc = prof->air_accel;
  float cur_aircap = prof->air_cap;
  float cur_gspeed = prof->ground_speed;
  float cur_aspeed = prof->air_speed;
  float cur_grav = prof->gravity;
  float cur_ricochet = prof->ricochet_hor;
  float cur_ricochet_up = prof->ricochet_up;
  int cur_ricochet_count = prof->ricochet_count;
  float cur_bounce = prof->bounce;
  int cur_mode = prof->tilt_mode;

  float old_vx, old_vy, old_vz, lookX, lookZ, impactX, impactZ, imLen, wallDot;
  float total_hz, remain_ratio, bounce_vel;
  cc_bool hitX = false, hitZ = false;
  cc_bool can_manual;

  Cwake_CheckMOTD();

  if (cw_ui_is_open) {
    p->Base.Yaw = cw_last_yaw;
    cw_origVTABLE->Tick(e, delta);
    return;
  }

  if (Camera.Active && Camera.Active->GetView != Cwake_GetView) {
    cw_orig_GetView = Camera.Active->GetView;
    Camera.Active->GetView = Cwake_GetView;
  }

  can_manual = cw_active_profile.enable_physics && p->Hacks.CanSpeed;
  if ((!cw_motd_override_active && !can_manual) || p->Hacks.Flying || p->Hacks.Noclip) {
    cw_origVTABLE->Tick(e, delta);
    cw_last_yaw = p->Base.Yaw;
    cw_target_roll = 0.0f;
    cw_current_roll = 0.0f;
    if (cw_send_livesplit && cw_LiveSplit_SetSpeed) {
      cw_LiveSplit_SetSpeed(0);
    }
    return;
  }

  current_yaw = p->Base.Yaw;
  dyaw = current_yaw - cw_last_yaw;
  if (dyaw > 180.0f) dyaw -= 360.0f;
  if (dyaw < -180.0f) dyaw += 360.0f;
  dyaw *= 0.01745329251f;
  if (cur_mode == 1) {
    if (dyaw > 0.00001f || dyaw < -0.00001f) {
      cosD = cosf(dyaw);
      sinD = sinf(dyaw);
      vx = e->Velocity.x;
      vz = e->Velocity.z;
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
      if (curSpeed > 0.001f) {
        e->Velocity.x = curSpeed * wx;
        e->Velocity.z = curSpeed * wz;
      }
    }

    if (onGround && !tryingToJump)
      Cwake_Accelerate(e, wx, wz, wishspeed, cur_gaccel, delta);
    else
      Cwake_AirAccelerate(e, wx, wz, wishspeed, cur_airacc, cur_aircap, delta);
  }

  if (cw_active_profile.show_speedo) {
    float hzSpeed = sqrtf(e->Velocity.x * e->Velocity.x + e->Velocity.z * e->Velocity.z);
    float realBPS = hzSpeed * 20.0f;
    if (realBPS > cw_top_speed) cw_top_speed = realBPS;
  }

  if (cw_send_livesplit && cw_LiveSplit_SetSpeed) {
    float hzSpeed = sqrtf(e->Velocity.x * e->Velocity.x + e->Velocity.z * e->Velocity.z);
    float realBPS = hzSpeed * 20.0f;

    cc_uint32 scaled_speed = (cc_uint32)(realBPS * 100.0f);
    cw_LiveSplit_SetSpeed(scaled_speed);
  }

  savedDrag = p->Physics.drag;
  savedGndFric = p->Physics.groundFriction;
  savedSpeedMulti = p->Hacks.SpeedMultiplier;

  p->Physics.drag.x = 1.0f;
  p->Physics.drag.z = 1.0f;
  p->Physics.groundFriction.x = 1.0f;
  p->Physics.groundFriction.z = 1.0f;
  p->Hacks.SpeedMultiplier = 0.0f;

  old_vx = e->Velocity.x;
  old_vy = e->Velocity.y;
  old_vz = e->Velocity.z;

  if (!onGround && cur_grav != 1.0f) {
    e->Velocity.y += 0.08f * (1.0f - cur_grav);
  }

  cw_origVTABLE->Tick(e, delta);

  if (fabsf(old_vx) > 0.05f && fabsf(e->Velocity.x) < 0.01f) hitX = true;
  if (fabsf(old_vz) > 0.05f && fabsf(e->Velocity.z) < 0.01f) hitZ = true;

  if (!hitX) e->Velocity.x = old_vx;
  if (!hitZ) e->Velocity.z = old_vz;

  if (cur_ricochet > 0.0f && !onGround && tryingToJump && cw_current_ricochets < cur_ricochet_count) {
    impactX = 0.0f;
    impactZ = 0.0f;

    if (hitX) impactX = old_vx;
    if (hitZ) impactZ = old_vz;

    if (hitX || hitZ) {
      lookX = sinf(current_yaw * 0.01745329251f);
      lookZ = -cosf(current_yaw * 0.01745329251f);

      imLen = sqrtf(impactX * impactX + impactZ * impactZ);
      if (imLen > 0.0001f) {
        impactX /= imLen;
        impactZ /= imLen;
      }

      wallDot = (impactX * lookX) + (impactZ * lookZ);

      if (wallDot > 0.75f) {
        if (cur_ricochet_up > 0.0f) e->Velocity.y = cur_ricochet_up;
        if (cur_ricochet > 0.0f) {
          float cur_speed = sqrtf(e->Velocity.x * e->Velocity.x + e->Velocity.z * e->Velocity.z);
          e->Velocity.x = -e->Velocity.x;
          e->Velocity.z = -e->Velocity.z;
          float new_speed = sqrtf(e->Velocity.x * e->Velocity.x + e->Velocity.z * e->Velocity.z);
          if (new_speed > 0.0001f) {
            e->Velocity.x = (e->Velocity.x / new_speed) * cur_speed * cur_ricochet;
            e->Velocity.z = (e->Velocity.z / new_speed) * cur_speed * cur_ricochet;
          }
        }
        cw_current_ricochets++;
        if (cw_active_profile.ricochet_sound[0])
          Cwake_PlaySound(cw_active_profile.ricochet_sound, cw_active_profile.ricochet_vol,
                          cw_active_profile.ricochet_vary_pitch);
      }
    }
  }

  if (!onGround && e->OnGround && old_vy < -0.25f) {
    cc_bool bounced = false;
    if (cur_bounce > 0.0f) {
      bounce_vel = -old_vy * cur_bounce;
      if (bounce_vel > 0.15f) {
        e->Velocity.y = bounce_vel;
        bounced = true;
        if (cw_active_profile.bounce_sound[0])
          Cwake_PlaySound(cw_active_profile.bounce_sound, cw_active_profile.bounce_vol,
                          cw_active_profile.bounce_vary_pitch);
      }
    }
    if (!bounced) {
      if (cw_active_profile.land_sound[0])
        Cwake_PlaySound(cw_active_profile.land_sound, cw_active_profile.land_vol, cw_active_profile.land_vary_pitch);
    }
  }

  if (initiatedJump && e->Velocity.y > 0.001f) {
    float jumpBoost = prof->jump_force;
    if (jumpBoost > 0.0f) e->Velocity.y *= jumpBoost;
    if (cw_active_profile.jump_sound[0])
      Cwake_PlaySound(cw_active_profile.jump_sound, cw_active_profile.jump_vol, cw_active_profile.jump_vary_pitch);
  }

  p->Physics.drag = savedDrag;
  p->Physics.groundFriction = savedGndFric;
  p->Hacks.SpeedMultiplier = savedSpeedMulti;

  {
    if (cw_active_profile.tilt_mode) {
      float max_tilt_rad = cw_active_profile.max_tilt * 0.01745329251f;
      float raw_roll;

      if (cw_active_profile.tilt_mode == 1) {
        float yaw_rad = current_yaw * 0.01745329251f;
        float lateral_speed = (e->Velocity.x * cosf(yaw_rad)) + (e->Velocity.z * sinf(yaw_rad));
        raw_roll = lateral_speed * 0.35f * cw_active_profile.tilt_multiplier;
      } else {
        raw_roll = -dyaw * 3.0f;
      }

      if (raw_roll > max_tilt_rad) raw_roll = max_tilt_rad;
      if (raw_roll < -max_tilt_rad) raw_roll = -max_tilt_rad;

      cw_target_roll += (raw_roll - cw_target_roll) * 0.20f;
    } else {
      cw_target_roll = 0.0f;
    }
  }
}
