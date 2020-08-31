//===== Hercules Plugin ======================================
//= Disable HP Bar for MVP/Mini and Emperium
//===== By: ==================================================
//=Samuel[Hercules]
//===== Current Version: =====================================
//= 1.0
//===== Compatible With: ===================================== 
//= Hercules
//===== Description: =========================================
//= Disable HP Bar on MVP
//= Disable HP Bar on Emperium
//===== Usage: ===============================================
//= conf/import/battle.conf
//= hpbar_emp : 0 (disable) : 1 (enable)
//= hpbar_mvp : 0 (disable) : 1 (enable) 
//============================================================

#include "common/hercules.h"

#include "common/timer.h"
#include "common/mmo.h"
#include "common/nullpo.h"
#include "common/strlib.h"

#include "map/achievement.h"
#include "map/battle.h"
#include "map/clif.h"
#include "map/map.h"
#include "map/mob.h"
#include "map/npc.h"
#include "map/pc.h"
#include "map/status.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

HPExport struct hplugin_info pinfo = {	
	"hidemobhpbar",
	SERVER_TYPE_MAP,
	"1.0",
	HPM_VERSION,
};

int bc_hpbaremp;
int bc_hpbarmvp;

void read_bc(const char *key, const char *val) {
	if (strcmpi(key, "battle_configuration/hpbar_emp") == 0) {
		bc_hpbaremp = config_switch(val);
		if (bc_hpbaremp > 1 || bc_hpbaremp < 0) {
			ShowDebug("Wrong Value for hpbar_emp: %d\n", config_switch(val));
			bc_hpbaremp = 0;
		}
	}
	else if (strcmpi(key, "battle_configuration/hpbar_mvp") == 0) {
		bc_hpbarmvp = config_switch(val);
		if (bc_hpbarmvp > 1 || bc_hpbarmvp < 0) {
			ShowDebug("Wrong Value for hpbar_mvp: %d\n", config_switch(val));
			bc_hpbarmvp = 0;
		}
	}
	return;
}

int read_return_bc(const char *key) {
	if (strcmpi(key, "battle_configuration/hpbar_emp") == 0) {
		return bc_hpbaremp;
	}
	else if (strcmpi(key, "battle_configuration/hpbar_mvp") == 0) {
		return bc_hpbarmvp;
	}
	return 0;
}

uint16 GetWord(uint32 val, int idx)
{
	switch (idx)
	{
	case 0: return (uint16)((val & 0x0000FFFF));
	case 1: return (uint16)((val & 0xFFFF0000) >> 0x10);
	default:
#if defined(DEBUG)
		ShowDebug("GetWord: invalid index (idx=%d)\n", idx);
#endif
		return 0;
	}
}

static inline void WBUFPOS(uint8 *p, unsigned short pos, short x, short y, unsigned char dir)
{
	p += pos;
	p[0] = (uint8)(x >> 2);
	p[1] = (uint8)((x << 6) | ((y >> 4) & 0x3f));
	p[2] = (uint8)((y << 4) | (dir & 0xf));
}

// client-side: x0+=sx0*0.0625-0.5 and y0+=sy0*0.0625-0.5
static inline void WBUFPOS2(uint8 *p, unsigned short pos, short x0, short y0, short x1, short y1, unsigned char sx0, unsigned char sy0)
{
	p += pos;
	p[0] = (uint8)(x0 >> 2);
	p[1] = (uint8)((x0 << 6) | ((y0 >> 4) & 0x3f));
	p[2] = (uint8)((y0 << 4) | ((x1 >> 6) & 0x0f));
	p[3] = (uint8)((x1 << 2) | ((y1 >> 8) & 0x03));
	p[4] = (uint8)y1;
	p[5] = (uint8)((sx0 << 4) | (sy0 & 0x0f));
}

//To make the assignation of the level based on limits clearer/easier. [Skotlex]
static int clif_setlevel_sub(int lv)
{
	if (lv < battle->bc->max_lv) {
		;
	}
	else if (lv < battle->bc->aura_lv) {
		lv = battle->bc->max_lv - 1;
	}
	else {
		lv = battle->bc->max_lv;
	}

	return lv;
}

static int clif_setlevel(struct block_list *bl)
{
	int lv = status->get_lv(bl);
	nullpo_retr(0, bl);
	if (battle->bc->client_limit_unit_lv&bl->type)
		return clif_setlevel_sub(lv);
	switch (bl->type) {
	case BL_NPC:
	case BL_PET:
		// npcs and pets do not have level
		return 0;
	}
	return lv;
}

bool clif_show_monster_hp_bar (const struct mob_data *md, struct block_list *bl);
static bool clif_show_monster_hp_bar(const struct mob_data *md, struct block_list *bl) {

	if (status_get_hp(bl) < status_get_max_hp(bl)) {
		if ((battle->bc->show_monster_hp_bar && !(md->class_ == MOBID_EMPELIUM) && !(md->status.mode & MD_BOSS))
			|| (bc_hpbaremp && (md->class_ == MOBID_EMPELIUM))
			|| (bc_hpbarmvp && (md->status.mode & MD_BOSS) && !(md->class_ == MOBID_EMPELIUM))
			) {
			return true;
		}
	}
	return false;
}

void clif_set_unit_idle_overload(struct block_list *bl, struct map_session_data *tsd, enum send_target target) {
	struct map_session_data* sd;
	struct status_change* sc = status->get_sc(bl);
	struct view_data* vd = status->get_viewdata(bl);
	struct packet_idle_unit p;
	int g_id = status->get_guild_id(bl);

	nullpo_retv(bl);
	nullpo_retv(vd);

#if PACKETVER < 20091103
	if (!pc->db_checkid(vd->class)) {
		clif->set_unit_idle2(bl, tsd, target);
		return;
	}
#endif

	sd = BL_CAST(BL_PC, bl);

	p.PacketType = idle_unitType;
#if PACKETVER >= 20091103
	p.PacketLength = sizeof(p);
	p.objecttype = clif->bl_type(bl);
#endif
#if PACKETVER >= 20131223
	p.AID = bl->id;
	p.GID = (sd) ? sd->status.char_id : 0; // CCODE
#else
	p.GID = bl->id;
#endif
	p.speed = status->get_speed(bl);
	p.bodyState = (sc) ? sc->opt1 : 0;
	p.healthState = (sc) ? sc->opt2 : 0;
	p.effectState = (sc != NULL) ? sc->option : ((bl->type == BL_NPC) ? BL_UCCAST(BL_NPC, bl)->option : 0);
	p.job = vd->class;
	p.head = vd->hair_style;
	p.weapon = vd->weapon;
	p.accessory = vd->head_bottom;
#if PACKETVER < 7 || PACKETVER_MAIN_NUM >= 20181121 || PACKETVER_RE_NUM >= 20180704 || PACKETVER_ZERO_NUM >= 20181114
	p.shield = vd->shield;
#endif
	p.accessory2 = vd->head_top;
	p.accessory3 = vd->head_mid;
	if (bl->type == BL_NPC && vd->class == FLAG_CLASS) {
		// The hell, why flags work like this?
		p.accessory = status->get_emblem_id(bl);
		p.accessory2 = GetWord(g_id, 1);
		p.accessory3 = GetWord(g_id, 0);
	}
	p.headpalette = vd->hair_color;
	p.bodypalette = vd->cloth_color;
	p.headDir = (sd) ? sd->head_dir : 0;
#if PACKETVER >= 20101124
	p.robe = vd->robe;
#endif
	p.GUID = g_id;
	p.GEmblemVer = status->get_emblem_id(bl);
	p.honor = (sd) ? sd->status.manner : 0;
	p.virtue = (sc) ? sc->opt3 : 0;
	p.isPKModeON = (sd && sd->status.karma) ? 1 : 0;
	p.sex = vd->sex;
	WBUFPOS(&p.PosDir[0], 0, bl->x, bl->y, unit->getdir(bl));
	p.xSize = p.ySize = (sd) ? 5 : 0;
	p.state = vd->dead_sit;
	p.clevel = clif_setlevel(bl);
#if PACKETVER >= 20080102
	p.font = (sd) ? sd->status.font : 0;
#endif
#if PACKETVER >= 20120221
	const struct mob_data *md = BL_UCCAST(BL_MOB, bl);
	if (clif_show_monster_hp_bar(md, bl) == true) {
		p.maxHP = status_get_max_hp(bl);
		p.HP = status_get_hp(bl);
	}
	else {
		p.maxHP = -1;
		p.HP = -1;
	}
	if (bl->type == BL_MOB) {
		p.isBoss = (md->spawn != NULL) ? md->spawn->state.boss : BTYPE_NONE;
	}
	else {
		p.isBoss = BTYPE_NONE;
	}
#endif
#if PACKETVER >= 20150513
	p.body = vd->body_style;
#endif
	/* Might be earlier, this is when the named item bug began */
#if PACKETVER >= 20131223
	safestrncpy(p.name, clif->get_bl_name(bl), NAME_LENGTH);
#endif
	clif->send(&p, sizeof(p), tsd ? &tsd->bl : bl, target);

	if (clif->isdisguised(bl)) {
#if PACKETVER >= 20091103
		p.objecttype = pc->db_checkid(status->get_viewdata(bl)->class) ? 0x0 : 0x5; //PC_TYPE : NPC_MOB_TYPE
#if PACKETVER >= 20131223
		p.AID = -bl->id;
#else
		p.GID = -bl->id;
#endif
#else
		p.GID = -bl->id;
#endif
		clif->send(&p, sizeof(p), bl, SELF);
	}

}

void clif_set_unit_walking_overload(struct block_list *bl, struct map_session_data *tsd, struct unit_data *ud, enum send_target target) {
	struct map_session_data* sd;
	struct status_change* sc = status->get_sc(bl);
	struct view_data* vd = status->get_viewdata(bl);
	struct packet_unit_walking p;
	int g_id = status->get_guild_id(bl);

	nullpo_retv(bl);
	nullpo_retv(ud);
	nullpo_retv(vd);

	sd = BL_CAST(BL_PC, bl);

	p.PacketType = unit_walkingType;
#if PACKETVER >= 20091103
	p.PacketLength = sizeof(p);
#endif
#if PACKETVER >= 20071106
	p.objecttype = clif->bl_type(bl);
#endif
#if PACKETVER >= 20131223
	p.AID = bl->id;
	p.GID = (sd) ? sd->status.char_id : 0; // CCODE
#else
	p.GID = bl->id;
#endif
	p.speed = status->get_speed(bl);
	p.bodyState = (sc) ? sc->opt1 : 0;
	p.healthState = (sc) ? sc->opt2 : 0;
	p.effectState = (sc != NULL) ? sc->option : ((bl->type == BL_NPC) ? BL_UCCAST(BL_NPC, bl)->option : 0);
	p.job = vd->class;
	p.head = vd->hair_style;
	p.weapon = vd->weapon;
	p.accessory = vd->head_bottom;
	p.moveStartTime = (unsigned int)timer->gettick();
#if PACKETVER < 7 || PACKETVER_MAIN_NUM >= 20181121 || PACKETVER_RE_NUM >= 20180704 || PACKETVER_ZERO_NUM >= 20181114
	p.shield = vd->shield;
#endif
	p.accessory2 = vd->head_top;
	p.accessory3 = vd->head_mid;
	p.headpalette = vd->hair_color;
	p.bodypalette = vd->cloth_color;
	p.headDir = (sd) ? sd->head_dir : 0;
#if PACKETVER >= 20101124
	p.robe = vd->robe;
#endif
	p.GUID = g_id;
	p.GEmblemVer = status->get_emblem_id(bl);
	p.honor = (sd) ? sd->status.manner : 0;
	p.virtue = (sc) ? sc->opt3 : 0;
	p.isPKModeON = (sd && sd->status.karma) ? 1 : 0;
	p.sex = vd->sex;
	WBUFPOS2(&p.MoveData[0], 0, bl->x, bl->y, ud->to_x, ud->to_y, 8, 8);
	p.xSize = p.ySize = (sd) ? 5 : 0;
	p.clevel = clif_setlevel(bl);
#if PACKETVER >= 20080102
	p.font = (sd) ? sd->status.font : 0;
#endif
#if PACKETVER >= 20120221
	const struct mob_data *md = BL_UCCAST(BL_MOB, bl);
	if (clif_show_monster_hp_bar(md, bl) == true) {
		p.maxHP = status_get_max_hp(bl);
		p.HP = status_get_hp(bl);
	}
	else {
		p.maxHP = -1;
		p.HP = -1;
	}
	if (bl->type == BL_MOB) {
		p.isBoss = (md->spawn != NULL) ? md->spawn->state.boss : BTYPE_NONE;
	}
	else {
		p.isBoss = BTYPE_NONE;
	}
#endif
#if PACKETVER >= 20150513
	p.body = vd->body_style;
#endif
	/* Might be earlier, this is when the named item bug began */
#if PACKETVER >= 20131223
	safestrncpy(p.name, clif->get_bl_name(bl), NAME_LENGTH);
#endif

	clif->send(&p, sizeof(p), tsd ? &tsd->bl : bl, target);

	if (clif->isdisguised(bl)) {
#if PACKETVER >= 20091103
		p.objecttype = pc->db_checkid(status->get_viewdata(bl)->class) ? 0x0 : 0x5; //PC_TYPE : NPC_MOB_TYPE
#if PACKETVER >= 20131223
		p.AID = -bl->id;
#else
		p.GID = -bl->id;
#endif
#else
		p.GID = -bl->id;
#endif
		clif->send(&p, sizeof(p), bl, SELF);
	}
}

void clif_spawn_unit_overload(struct block_list *bl, enum send_target target) {
	struct map_session_data* sd;
	struct status_change* sc = status->get_sc(bl);
	struct view_data* vd = status->get_viewdata(bl);
	struct packet_spawn_unit p;
	int g_id = status->get_guild_id(bl);

	nullpo_retv(bl);
	nullpo_retv(vd);

#if PACKETVER < 20091103
	if (!pc->db_checkid(vd->class)) {
		clif->spawn_unit2(bl, target);
		return;
	}
#endif

	sd = BL_CAST(BL_PC, bl);

	p.PacketType = spawn_unitType;
#if PACKETVER >= 20091103
	p.PacketLength = sizeof(p);
	p.objecttype = clif->bl_type(bl);
#endif
#if PACKETVER >= 20131223
	p.AID = bl->id;
	p.GID = (sd) ? sd->status.char_id : 0; // CCODE
#else
	p.GID = bl->id;
#endif
	p.speed = status->get_speed(bl);
	p.bodyState = (sc) ? sc->opt1 : 0;
	p.healthState = (sc) ? sc->opt2 : 0;
	p.effectState = (sc != NULL) ? sc->option : ((bl->type == BL_NPC) ? BL_UCCAST(BL_NPC, bl)->option : 0);
	p.job = vd->class;
	p.head = vd->hair_style;
	p.weapon = vd->weapon;
	p.accessory = vd->head_bottom;
#if PACKETVER < 7 || PACKETVER_MAIN_NUM >= 20181121 || PACKETVER_RE_NUM >= 20180704 || PACKETVER_ZERO_NUM >= 20181114
	p.shield = vd->shield;
#endif
	p.accessory2 = vd->head_top;
	p.accessory3 = vd->head_mid;
	if (bl->type == BL_NPC && vd->class == FLAG_CLASS) {
		// The hell, why flags work like this?
		p.accessory = status->get_emblem_id(bl);
		p.accessory2 = GetWord(g_id, 1);
		p.accessory3 = GetWord(g_id, 0);
	}
	p.headpalette = vd->hair_color;
	p.bodypalette = vd->cloth_color;
	p.headDir = (sd) ? sd->head_dir : 0;
#if PACKETVER >= 20101124
	p.robe = vd->robe;
#endif
	p.GUID = g_id;
	p.GEmblemVer = status->get_emblem_id(bl);
	p.honor = (sd) ? sd->status.manner : 0;
	p.virtue = (sc) ? sc->opt3 : 0;
	p.isPKModeON = (sd && sd->status.karma) ? 1 : 0;
	p.sex = vd->sex;
	WBUFPOS(&p.PosDir[0], 0, bl->x, bl->y, unit->getdir(bl));
	p.xSize = p.ySize = (sd) ? 5 : 0;
	p.clevel = clif_setlevel(bl);
#if PACKETVER >= 20080102
	p.font = (sd) ? sd->status.font : 0;
#endif
#if PACKETVER >= 20120221
	const struct mob_data *md = BL_UCCAST(BL_MOB, bl);
	if (clif_show_monster_hp_bar(md, bl) == true) {
		p.maxHP = status_get_max_hp(bl);
		p.HP = status_get_hp(bl);
	}
	else {
		p.maxHP = -1;
		p.HP = -1;
	}
	if (bl->type == BL_MOB) {
		p.isBoss = (md->spawn != NULL) ? md->spawn->state.boss : BTYPE_NONE;
	}
	else {
		p.isBoss = BTYPE_NONE;
	}
#endif
#if PACKETVER >= 20150513
	p.body = vd->body_style;
#endif
	/* Might be earlier, this is when the named item bug began */
#if PACKETVER >= 20131223
	safestrncpy(p.name, clif->get_bl_name(bl), NAME_LENGTH);
#endif
	if (clif->isdisguised(bl)) {
		nullpo_retv(sd);
		if (sd->status.class != sd->disguise)
			clif->send(&p, sizeof(p), bl, target);
#if PACKETVER >= 20091103
		p.objecttype = pc->db_checkid(status->get_viewdata(bl)->class) ? 0x0 : 0x5; //PC_TYPE : NPC_MOB_TYPE
#if PACKETVER >= 20131223
		p.AID = -bl->id;
#else
		p.GID = -bl->id;
#endif
#else
		p.GID = -bl->id;
#endif
		clif->send(&p, sizeof(p), bl, SELF);
	}
	else
		clif->send(&p, sizeof(p), bl, target);

}

void mob_damage_overload(struct mob_data *md, struct block_list *src, int damage)
{
	nullpo_retv(md);
	if (damage > 0) { //Store total damage...
		if (UINT_MAX - (unsigned int)damage > md->tdmg)
			md->tdmg += damage;
		else if (md->tdmg == UINT_MAX)
			damage = 0; //Stop recording damage once the cap has been reached.
		else { //Cap damage log...
			damage = (int)(UINT_MAX - md->tdmg);
			md->tdmg = UINT_MAX;
		}
		if (md->state.aggressive) { //No longer aggressive, change to retaliate AI.
			md->state.aggressive = 0;
			if (md->state.skillstate == MSS_ANGRY)
				md->state.skillstate = MSS_BERSERK;
			if (md->state.skillstate == MSS_FOLLOW)
				md->state.skillstate = MSS_RUSH;
		}
		//Log damage
		if (src)
			mob->log_damage(md, src, damage);
		md->dmgtick = timer->gettick();

		// Achievements [Smokexyz/Hercules]
		if (src != NULL && src->type == BL_PC)
			achievement->validate_mob_damage(BL_UCAST(BL_PC, src), damage, false);
	}

	if (battle->bc->show_mob_info & 3)
		clif->blname_ack(0, &md->bl);

#if PACKETVER >= 20131223
	// Resend ZC_NOTIFY_MOVEENTRY to Update the HP
	if (clif_show_monster_hp_bar(md, &md->bl) == true)
		clif->set_unit_walking(&md->bl, NULL, unit->bl2ud(&md->bl), AREA);
#endif

	if (!src)
		return;

#if (PACKETVER >= 20120404 && PACKETVER < 20131223)
	if (battle_config.show_monster_hp_bar && !(md->status.mode&MD_BOSS)) {
		int i;
		for (i = 0; i < DAMAGELOG_SIZE; i++) { // must show hp bar to all char who already hit the mob.
			if (md->dmglog[i].id) {
				struct map_session_data *sd = map->charid2sd(md->dmglog[i].id);
				if (sd && check_distance_bl(&md->bl, &sd->bl, AREA_SIZE)) // check if in range
					clif->monster_hp_bar(md, sd);
			}
		}
	}
#endif
}

void mob_heal_overload(struct mob_data *md, unsigned int heal)
{
	nullpo_retv(md);
	if (battle->bc->show_mob_info & 3)
		clif->blname_ack(0, &md->bl);
#if PACKETVER >= 20131223
	// Resend ZC_NOTIFY_MOVEENTRY to Update the HP
	if (clif_show_monster_hp_bar(md, &md->bl) == true)
		clif->set_unit_walking(&md->bl, NULL, unit->bl2ud(&md->bl), AREA);
#endif

#if (PACKETVER >= 20120404 && PACKETVER < 20131223)
	if (battle_config.show_monster_hp_bar && !(md->status.mode&MD_BOSS)) {
		int i;
		for (i = 0; i < DAMAGELOG_SIZE; i++) { // must show hp bar to all char who already hit the mob.
			if (md->dmglog[i].id) {
				struct map_session_data *sd = map->charid2sd(md->dmglog[i].id);
				if (sd && check_distance_bl(&md->bl, &sd->bl, AREA_SIZE)) // check if in range
					clif->monster_hp_bar(md, sd);
			}
		}
	}
#endif
}

HPExport void plugin_init(void) {
	clif->spawn_unit = clif_spawn_unit_overload;
	clif->set_unit_idle = clif_set_unit_idle_overload;
	clif->spawn_unit = clif_spawn_unit_overload;

	mob->damage = mob_damage_overload;
	mob->heal = mob_heal_overload;
}

HPExport void server_preinit(void) {
	addBattleConf("battle_configuration/hpbar_emp", read_bc, read_return_bc, false);
	addBattleConf("battle_configuration/hpbar_mvp", read_bc, read_return_bc, false);
}

HPExport void server_online(void)
{
	ShowInfo("'%s' Plugin by Samuel/Hercules. Version '%s'\n", pinfo.name, pinfo.version);
}