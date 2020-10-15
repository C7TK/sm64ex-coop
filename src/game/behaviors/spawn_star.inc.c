// spawn_default_star.c.inc

static struct ObjectHitbox sCollectStarHitbox = {
    /* interactType:      */ INTERACT_STAR_OR_KEY,
    /* downOffset:        */ 0,
    /* damageOrCoinValue: */ 0,
    /* health:            */ 0,
    /* numLootCoins:      */ 0,
    /* radius:            */ 80,
    /* height:            */ 50,
    /* hurtboxRadius:     */ 0,
    /* hurtboxHeight:     */ 0,
};

void bhv_collect_star_init(void) {
    s8 starId;
    u8 currentLevelStarFlags;

    starId = (o->oBehParams >> 24) & 0xFF;
    currentLevelStarFlags = save_file_get_star_flags(gCurrSaveFileNum - 1, gCurrCourseNum - 1);
    if (currentLevelStarFlags & (1 << starId)) {
        o->header.gfx.sharedChild = gLoadedGraphNodes[MODEL_TRANSPARENT_STAR];
    } else {
        o->header.gfx.sharedChild = gLoadedGraphNodes[MODEL_STAR];
    }

    obj_set_hitbox(o, &sCollectStarHitbox);
}

void bhv_collect_star_loop(void) {
    o->oFaceAngleYaw += 0x800;

    if (o->oInteractStatus & INT_STATUS_INTERACTED) {
        mark_obj_for_deletion(o);
        o->oInteractStatus = 0;
    }
}

void bhv_star_spawn_init(void) {
    o->oMoveAngleYaw = atan2s(o->oHomeZ - o->oPosZ, o->oHomeX - o->oPosX);
    o->oStarSpawnDisFromHome = sqrtf(sqr(o->oHomeX - o->oPosX) + sqr(o->oHomeZ - o->oPosZ));
    o->oVelY = (o->oHomeY - o->oPosY) / 30.0f;
    o->oForwardVel = o->oStarSpawnDisFromHome / 30.0f;
    o->oStarSpawnUnkFC = o->oPosY;

    if ((gMarioStates[0].action & ACT_GROUP_MASK) != ACT_GROUP_CUTSCENE) {
        if (o->oBehParams2ndByte == 0 || gCurrCourseNum == COURSE_BBH)
            cutscene_object(CUTSCENE_STAR_SPAWN, o);
        else
            cutscene_object(CUTSCENE_RED_COIN_STAR_SPAWN, o);

        gMarioStates[0].freeze = 60;
        set_time_stop_flags(TIME_STOP_ENABLED | TIME_STOP_MARIO_AND_DOORS);
        o->activeFlags |= ACTIVE_FLAG_INITIATED_TIME_STOP;
    }
    cur_obj_become_intangible();
}

void bhv_star_spawn_loop(void) {
    switch (o->oAction) {
        case 0:
            o->oFaceAngleYaw += 0x1000;
            if (o->oTimer > 20)
                o->oAction = 1;
            break;

        case 1:
            obj_move_xyz_using_fvel_and_yaw(o);
            o->oStarSpawnUnkFC += o->oVelY;
            o->oPosY = o->oStarSpawnUnkFC + sins((o->oTimer * 0x8000) / 30) * 400.0f;
            o->oFaceAngleYaw += 0x1000;
            spawn_object(o, MODEL_NONE, bhvSparkleSpawn);
            cur_obj_play_sound_1(SOUND_ENV_STAR);
            if (o->oTimer == 30) {
                o->oAction = 2;
                o->oForwardVel = 0;
                play_power_star_jingle(TRUE);
            }
            break;

        case 2:
            if (o->oTimer < 20)
                o->oVelY = 20 - o->oTimer;
            else
                o->oVelY = -10.0f;

            spawn_object(o, MODEL_NONE, bhvSparkleSpawn);
            obj_move_xyz_using_fvel_and_yaw(o);
            o->oFaceAngleYaw = o->oFaceAngleYaw - o->oTimer * 0x10 + 0x1000;
            cur_obj_play_sound_1(SOUND_ENV_STAR);

            if (o->oPosY < o->oHomeY) {
                cur_obj_play_sound_2(SOUND_GENERAL_STAR_APPEARS);
                cur_obj_become_tangible();
                o->oPosY = o->oHomeY;
                o->oAction = 3;
            }
            break;

        case 3:
            o->oFaceAngleYaw += 0x800;
            if (o->oTimer == 20) {
                gMarioStates[0].freeze = 0;
                gObjCutsceneDone = TRUE;
                clear_time_stop_flags(TIME_STOP_ENABLED | TIME_STOP_MARIO_AND_DOORS);
                o->activeFlags &= ~ACTIVE_FLAG_INITIATED_TIME_STOP;
            }

            if (o->oInteractStatus & INT_STATUS_INTERACTED) {
                mark_obj_for_deletion(o);
                o->oInteractStatus = 0;
            }
            break;
    }
}

struct Object *spawn_star(struct Object *sp30, f32 sp34, f32 sp38, f32 sp3C) {
    sp30 = spawn_object_abs_with_rot(o, 0, MODEL_STAR, bhvStarSpawnCoordinates, o->oPosX, o->oPosY,
                                     o->oPosZ, 0, 0, 0);
    sp30->oBehParams = o->oBehParams;
    sp30->oHomeX = sp34;
    sp30->oHomeY = sp38;
    sp30->oHomeZ = sp3C;
    sp30->oFaceAnglePitch = 0;
    sp30->oFaceAngleRoll = 0;
    return sp30;
}

static u8 spawn_star_deduplication(u32* array, u8* count, u32 behParams) {
    for (int i = 0; i < *count; i++) {
        if (array[i] == behParams) { return TRUE; }
    }
    if (*count < 8) {
        array[*count] = behParams;
        *count = *count + 1;
        return FALSE;
    }
    return TRUE;
}

struct Object* spawn_default_star(f32 x, f32 y, f32 z) {
    if (sCurrPlayMode != PLAY_MODE_NORMAL && sCurrPlayMode != PLAY_MODE_PAUSED) { return NULL; }
    u32 behParams = o->oBehParams;

    // de-duplication checking
    if (spawn_star_deduplication(gSpawnedStarDefault, &gSpawnedStarDefaultCount, behParams)) {
        return NULL;
    }

    struct Object *star;
    star = spawn_star(star, x, y, z);
    star->oBehParams2ndByte = 0;
    network_send_spawn_star(star, 0, x, y, z, behParams);
    return star;
}

struct Object* spawn_red_coin_cutscene_star(f32 x, f32 y, f32 z) {
    u32 behParams = o->oBehParams;

    // de-duplication checking
    if (spawn_star_deduplication(gSpawnedStarRed, &gSpawnedStarRedCount, behParams)) {
        return NULL;
    }

    struct Object * star;
    star = spawn_star(star, x, y, z);
    star->oBehParams2ndByte = 1;
    network_send_spawn_star(star, 1, x, y, z, behParams);
    return star;
}

struct Object* spawn_no_exit_star(f32 x, f32 y, f32 z) {
    u32 behParams = o->oBehParams;

    // de-duplication checking
    if (spawn_star_deduplication(gSpawnedStarHidden, &gSpawnedStarHiddenCount, behParams)) {
        return NULL;
    }

    struct Object * star;
    star = spawn_star(star, x, y, z);
    star->oBehParams2ndByte = 1;
    star->oInteractionSubtype |= INT_SUBTYPE_NO_EXIT;
    network_send_spawn_star(star, 2, x, y, z, behParams);
    return star;
}

void bhv_hidden_red_coin_star_init(void) {
    s16 sp36;
    struct Object *sp30;

    if (gCurrCourseNum != COURSE_JRB)
        spawn_object(o, MODEL_TRANSPARENT_STAR, bhvRedCoinStarMarker);

    sp36 = count_objects_with_behavior(bhvRedCoin);
    if (sp36 == 0) {
        sp30 =
            spawn_object_abs_with_rot(o, 0, MODEL_STAR, bhvStar, o->oPosX, o->oPosY, o->oPosZ, 0, 0, 0);
        sp30->oBehParams = o->oBehParams;
        o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
    }

    o->oHiddenStarTriggerCounter = 8 - sp36;
}

void bhv_hidden_red_coin_star_loop(void) {
    gRedCoinsCollected = o->oHiddenStarTriggerCounter;
    switch (o->oAction) {
        case 0:
            if (o->oHiddenStarTriggerCounter == 8)
                o->oAction = 1;
            break;

        case 1:
            if (o->oTimer > 2) {
                spawn_red_coin_cutscene_star(o->oPosX, o->oPosY, o->oPosZ);
                spawn_mist_particles();
                o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
            }
            break;
    }
}
