/* Serpent Isle SI.EXE, overlay segment 258 (file offsets 0x06f660 to 0x06f8b8, 600 bytes).
 * Borland C++ 2.0 -mm -O -G -P rebuilds it byte for byte as C++.
 */

#include "u7port.h"
#include "activity.h"
#include "lowlevel.h"
#include "u7npc.h"
#include "item.h"
#include "npcref.h"
#include "voolook.h"
#include "makemojo.h"
#include "combatai.h"
#include "missile.h"
#include "monsters.h"
#include "actitem.h"
#include "usehook.h"
#include "script.h"
#include "gtimer.h"
#include "sche.h"
#include "sprite.h"
#include "random.h"
#include "text.h"
#include "rules.h"

#define NPC(p) GetNpcBufferForIbo(p)
#define CUR_SCHED(p) (NPC(p)->schedules[NPC(p)->currentSchedule])
#define FLAG(v, m) ((uint8_t) ((v) & (m)))

/* Black Gate's approach: an NPC that cannot see the invisible Avatar, or reach him, loiters; one on
 * its way calls out now and then. */
static void ApproachAvatarToTalk(objref *npc, int16_t monster)
{
	if (FLAG(Item_getQualityFlags(&AvatarRef), QUALITY_INVISIBLE)
		&& !(uint8_t) MonsterRecords.get(monster)->seeInvisible
		|| !PathNextToItem(npc, AvatarRef, 75)) {
		CUR_SCHED(npc).state = 0;
		Npc_pushSchedule(npc, WORK_LOITER, -1, -1, 0);
		CUR_SCHED(npc).state = 1;
	} else if (Item_greatestDeltaToItem(*npc, AvatarRef) <= 3 && HasLineOfFire(*npc, AvatarRef))
		CUR_SCHED(npc).state++;
	else if (RollChance(20))
		/* "Stop!", "I would have words with thee." or "I wish to speak to thee." */
		SpriteManager_barkOnItem(&gSpriteManager, *npc,
			GetGameText(1, GenerateRandomIntegerInRange(3) + 20), 0, 15, 0);
}

/*
 * Talk: walk up to the avatar and start a conversation once close and in sight. An NPC that cannot
 * reach him goes back to what its schedule says it does at this hour, or wanders if it has none.
 */
void RunTalkSchedule(objref *npc)
{
	int16_t monster;
	uint16_t type;

	switch (CUR_SCHED(npc).state) {
	case 0:
		Npc_pushSchedule(npc, WORK_GOTO_SCH_D, -1, -1, 0);
		break;
	case 1:
		type = objref(npc->off).type();
		monster = MonsterLookup.get(type);
		if (TalkersLoiterAndCallOut())
			ApproachAvatarToTalk(npc, monster);
		else if (!PathNextToItem(npc, AvatarRef, 100) && (uint16_t)Item_getNpcNumber(npc) < 256) {
			uint8_t period, workType;

			period = GameTime.getHour() / 3;
			workType = Schedule_getWorkType(&ScheduleTable, Item_getNpcNumber(npc), period);
			if (workType == 0xff) {
				Npc_setSchedule(npc, WORK_WANDER);
				CUR_SCHED(npc).state = 1;
			}
			if (workType != 3 /* talk */) {
				CUR_SCHED(npc).state = 0;
				Npc_setSchedule(npc, workType);
				CUR_SCHED(npc).state = 1;
			}
		} else if (Item_greatestDeltaToItem(*npc, AvatarRef) <= 5 && HasLineOfFire(*npc, AvatarRef))
			CUR_SCHED(npc).state++;
		break;
	case 2:
		PostScheduleScript(npc, MakeScript(SCRIPT_STAND_FRAME, SCRIPT_END));
		break;
	case 3:
		RunUsable(TalkEvent(), *npc, -1);
		break;
	}
}
