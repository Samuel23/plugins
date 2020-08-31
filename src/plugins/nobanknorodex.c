//===== Hercules Plugin ======================================
//= nobank mapflag
//= norodex mapflag
//===== By: ==================================================
//= Originally by Samuel[Hercules]
//===== Current Version: =====================================
//= 1.0
//===== Compatible With: ===================================== 
//= Hercules
//===== Description: =========================================
//= Disable banks in a certain map
//= Disable rodex in a certain map
//===== Usage: ===============================================
//= alberta <tab> mapflag <tab> nobank
//= alberta <tab> mapflag <tab> norodex
//============================================================


#include "common/hercules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/memmgr.h"
#include "common/nullpo.h"
#include "common/socket.h"

#include "map/clif.h"
#include "map/pc.h"
#include "map/npc.h"
#include "map/rodex.h"


#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

HPExport struct hplugin_info pinfo = {	
	"nobanknorodex mapflag",
	SERVER_TYPE_MAP,
	"1.0",
	HPM_VERSION,
};

struct mapflag_data {
	unsigned nobank : 1;
	unsigned norodex : 1;
};

void npc_parse_unknown_mapflag_pre(const char **name, const char **w3, const char **w4, const char **start, const char **buffer, const char **filepath, int **retval)
{
	if (strcmpi(*w3,"nobank") == 0) {
		int16 m = map->mapname2mapid(*name);
		struct mapflag_data *mf;
		if (( mf = getFromMAPD(&map->list[m], 0)) == NULL) {
			CREATE(mf, struct mapflag_data, 1);
			addToMAPD(&map->list[m], mf, 0, true);
		}
		mf->nobank = 1;
		hookStop();
	}
	if (strcmpi(*w3, "norodex") == 0) {
		int16 m = map->mapname2mapid(*name);
		struct mapflag_data *mf;
		if ((mf = getFromMAPD(&map->list[m], 0)) == NULL) {
			CREATE(mf, struct mapflag_data, 1);
			addToMAPD(&map->list[m], mf, 0, true);
		}
		mf->norodex = 1;
		hookStop();
	}
	return;
}
	
void rodex_open_pre(struct map_session_data **sd, int8 *open_type, int64 *first_mail_id) {
	nullpo_retv(*sd);

	struct mapflag_data *mf;
	if (*sd == NULL)
		return;

	mf = getFromMAPD(&map->list[(*sd)->bl.m], 0);

	if (mf != NULL && mf->norodex == 1) {
		clif->messagecolor_self((*sd)->fd, COLOR_GREEN, "You cannot use rodex here!");
		hookStop();
	}

	return;
}

void clif_parse_BankCheck_pre(int *fd, struct map_session_data **sd)
{
	nullpo_retv(*sd);

	struct mapflag_data *mf;
	if (*sd == NULL)
		return;

	mf = getFromMAPD(&map->list[(*sd)->bl.m], 0);
	
	if (mf != NULL && mf->nobank == 1) {
		clif->messagecolor_self((*sd)->fd, COLOR_GREEN, "You cannot use bank here!");
		hookStop();
	}

	return;
}

void map_flags_init_pre(void)
{
	int i;
	for (i = 0; i < map->count; i++) {
		struct mapflag_data *mf = getFromMAPD(&map->list[i], 0);
		if (mf != NULL)
			removeFromMAPD(&map->list[i], 0);
	}
	return;
}

HPExport void plugin_init (void) {
	addHookPre(npc, parse_unknown_mapflag, npc_parse_unknown_mapflag_pre);
	addHookPre(rodex, open, rodex_open_pre);
	addHookPre(clif, pBankCheck, clif_parse_BankCheck_pre);
	addHookPre(map, flags_init, map_flags_init_pre);
}

HPExport void server_online(void)
{
	ShowInfo("'%s' Plugin by Samuel/Hercules. Version '%s'\n", pinfo.name, pinfo.version);
}
