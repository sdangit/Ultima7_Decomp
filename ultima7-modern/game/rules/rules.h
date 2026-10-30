#ifndef RULES_H
#define RULES_H

#include "gametype.h"

/* Whether type is a game's shape n, whichever game is played. */
static inline uint8_t IsGameShape(int16_t type, enum GameType game, int16_t n)
{
	return ShapeGame((uint16_t)type) == game && (type & 0x3ff) == n;
}

/* What differs between the played games: systems one game has and the other lacks, and numbers
 * that mean different things in each. */

static inline uint8_t PlayingBlackGate(void)
{
	return PlayedGame() == GAME_BG;
}

/* Serpent Isle systems. */

/* Schedules edited by script and saved in SCHEDULE.DAT; Black Gate keeps the static ones. */
static inline uint8_t HasSavedSchedules(void) { return !PlayingBlackGate(); }
/* PHILBERT.DAT: the keyring, infravision and a marked Avatar position. */
static inline uint8_t HasPhilbertFile(void) { return !PlayingBlackGate(); }
/* Flags and an id kept in the low bits of every NPC's mana and magic but the Avatar's, and
 * temperature, skin and more in the stat bytes' top bits. Black Gate stats are whole bytes. */
static inline uint8_t PacksNpcStats(void) { return !PlayingBlackGate(); }
/* Cold weather, warm clothing and the freeze meter. */
static inline uint8_t HasColdWeather(void) { return !PlayingBlackGate(); }
/* NPCs turned into other creatures, restored on load. */
static inline uint8_t HasPolymorph(void) { return !PlayingBlackGate(); }
/* While scripts freeze the Avatar, Black Gate takes him off the map; Serpent Isle draws him
 * with a blank shape from the start. */
static inline uint8_t TakesFrozenAvatarOffMap(void) { return PlayingBlackGate(); }
/* Black Gate also hides the pointer then. */
static inline uint8_t HidesPointerWhileFrozen(void) { return PlayingBlackGate(); }
/* Script gifts keep their temporary mark (Black Gate's inn keys). */
static inline uint8_t GiftsStayTemporary(void) { return PlayingBlackGate(); }
/* Every human regains mana hourly; in Serpent Isle only the Avatar. */
static inline uint8_t AllHumansRegainMana(void) { return PlayingBlackGate(); }
/* Items of shape 975 read inside a 581 come back as 792 (581 is ammunition in Black Gate). */
static inline uint8_t RepairsSavedShapes(void) { return !PlayingBlackGate(); }
/* NPCs who want a word give up while the Avatar is invisible, call out, and talk at 3 tiles. */
static inline uint8_t TalkersLoiterAndCallOut(void) { return PlayingBlackGate(); }
/* Guards stationed by region come when called; Black Gate spawns new ones of its guard shape. */
static inline uint8_t HasRegionalGuards(void) { return !PlayingBlackGate(); }
#define BG_GUARD_SHAPE 946
/* Goblins attack on sight around their camp. */
static inline uint8_t HasGoblinCamp(void) { return !PlayingBlackGate(); }
/* Avatar bodies by skin colour (shapes 1024-1027). */
static inline uint8_t HasSkinColours(void) { return !PlayingBlackGate(); }
/* Loading a save redoes the time-of-day palette, which also steps the colour cycling. */
static inline uint8_t ResetsPaletteOnLoad(void) { return !PlayingBlackGate(); }
/* Black Gate repaints the world as the save window closes. */
static inline uint8_t RedrawsAfterSaveGump(void) { return PlayingBlackGate(); }
/* With cheats on, Black Gate prints cheat screens and warnings on the game screen. */
static inline uint8_t PrintsCheatText(void) { return PlayingBlackGate(); }
/* Black Gate's F1 key list: a line fewer, after a gap below the cheat keys. */
static inline uint8_t HasBgCheatKeyList(void) { return PlayingBlackGate(); }

/* Numbers that differ. */

/* Black Gate's grandfather clocks (252 and 695) swing through three frames. */
static inline uint8_t ClocksSwingInThrees(void) { return PlayingBlackGate(); }
/* Black Gate's red loading screen: ENDSHAPE.FLX picture 19 over a red ramp built in code. */
static inline uint8_t HasBgRedScreen(void) { return PlayingBlackGate(); }
/* Swamps: Black Gate's bubbles both poison, and only its swamp boots protect; in Serpent Isle
 * any footwear on frame 6 does. */
static inline uint8_t HasBgSwampRules(void) { return PlayingBlackGate(); }
/* Boots that keep swamp poison off, by their own game: Black Gate's swamp boots (588), Serpent
 * Isle's boots in frame 6. */
static inline uint8_t IsSwampBoots(int16_t type, uint16_t frame)
{
	return ShapeGame((uint16_t)type) == GAME_BG ? (type & 0x3ff) == 588 : frame == 6;
}
/* Serpent Isle's ring of reagents (ring 296 in frame 3): its wearer casts without reagents. */
static inline uint8_t IsReagentRing(int16_t type, uint16_t frame)
{
	return IsGameShape(type, GAME_SI, 296) && frame == 3;
}
/* Black Gate takes Serpent Isle's backpack (holds 220, not 150) and bag (0.2 stone, not 1.5). */
static inline uint8_t HasRoomierPacks(void) { return PlayingBlackGate(); }
#define BACKPACK_TYPE 801
#define BAG_TYPE 802
/* The jawbone (555) key; in Black Gate that shape is a hawk. */
static inline uint8_t HasJawbone(void) { return !PlayingBlackGate(); }
/* Serpent Isle's key ring, in either game. */
static inline uint16_t KeyRingShape(void) { return GameShape(GAME_SI, 485); }
/* Whether the key ring turns a key away. */
uint8_t KeyRingRefuses(uint8_t frame, uint8_t quality);
/* Whether shape n of one game is the same thing as the same number in another: keys, chests and
 * doors, which each game's usecode then treats as its own. */
uint8_t SameShapeIn(enum GameType game, uint16_t n, enum GameType other);
/* Items from another game that a game gets once, new or loaded: the item's own game, shape,
 * frame and quality, and where it goes. A container is found by its world cell and shape.
 * Saves remember each entry by its place in the list, so new ones go at the end. */
enum StartItemPlace { START_IN_PACK, START_IN_CONTAINER };
struct StartItem {
	enum GameType game;
	uint16_t shape;
	uint8_t frame, quality;
	enum StartItemPlace place;
	int16_t x, y;
	uint16_t container;
};
/* The played game's list; none while its setting is off. */
const struct StartItem *StartItems(uint16_t *count);
/* The pocket watch: Black Gate's own shape, or a frame of Serpent Isle's desk items. */
static inline int16_t PocketWatchShape(void) { return PlayingBlackGate() ? 159 : 675; }
static inline uint8_t PocketWatchFrame(void) { return PlayingBlackGate() ? 0 : 21; }
/* Black Gate's watch script barks the time over the item it is given, which inside a pack shows
 * nothing; given the Avatar it barks over him, as Serpent Isle's does. */
static inline uint8_t WatchBarksOverItem(void) { return PlayingBlackGate(); }
/* A creature that turns wild and leaves its schedule for good. */
static inline int16_t WildCreatureShape(void) { return PlayingBlackGate() ? 354 : 979; }
/* A shape that draws nothing. */
static inline int16_t BlankShape(void) { return PlayingBlackGate() ? 688 : 239; }

/* Combat. */

/* A mana-draining hit empties the Avatar's mana and strips anyone else of their spells; in
 * Black Gate a human keeps a random part of its mana. */
static inline uint8_t DrainStripsSpells(void) { return !PlayingBlackGate(); }
/* Serpent Isle leaves a no-damage hit's result unset; Black Gate counts it as a miss. */
static inline uint8_t NoDamageHitMisses(void) { return PlayingBlackGate(); }
/* The kill score counts the monster's gazer trait: one more XP for gazers and Hook. */
static inline uint8_t ScoresGazerTrait(void) { return PlayingBlackGate(); }
/* A companion's death may bring the Guardian's taunt (speech 22). */
static inline uint8_t GuardianTauntsDeaths(void) { return PlayingBlackGate(); }
/* Body 414's frame that may raise NPC 1: Iolo's in Black Gate, Dupre's in Serpent Isle. */
static inline int16_t Npc1BodyFrame(void) { return PlayingBlackGate() ? 18 : 20; }
/* Black Gate's cannon: its shots count as the Avatar's, and no NPC wields it. */
static inline uint8_t IsCannon(int16_t type) { return PlayingBlackGate() && type == 702; }
/* Black Gate's triple crossbow, whose missile is 948, needs and spends three bolts a shot. */
static inline uint8_t FiresThreeBolts(int16_t missile) { return IsGameShape(missile, GAME_BG, 948); }
/* Black Gate's flaming oil and starburst have no range of their own, so the last one is thrown
 * at the usual range. */
static inline uint8_t ThrownAtUsualRange(int16_t type)
{
	return PlayingBlackGate() && (type == 782 || type == 565);
}
/* Black Gate's sea serpent moves four tiles a step. */
static inline uint8_t MovesFourTiles(int16_t type) { return PlayingBlackGate() && type == 525; }
/* Penumbra (Black Gate's NPC 150) sleeps through attacks. */
static inline uint8_t SleepsThroughAttacks(int16_t npc) { return PlayingBlackGate() && npc == 150; }
/* Guards who call more guards when hit. Serpent Isle counts any NPC on a fighting schedule as a
 * guard already fighting; Black Gate only its guards. */
static inline uint8_t IsGuardShape(int16_t type)
{
	if (PlayingBlackGate())
		return type == BG_GUARD_SHAPE;
	return type == 228 || type == 259 || type == 461 || type == 381;
}
static inline uint8_t OnlyGuardsCountAsFighting(void) { return PlayingBlackGate(); }
/* Black Gate's ghosts and the liche never call for help. */
static inline uint8_t NeverCallsForHelp(int16_t type)
{
	return PlayingBlackGate() && (type == 299 || type == 317 || type == 519);
}
/* Serpent Isle's goblins bark their own lines. In Black Gate these shapes are the liche, Sherry
 * the Mouse, a lyre, a roulette wheel and a telescope. */
static inline uint8_t HasGoblins(void) { return !PlayingBlackGate(); }
/* Creatures that turn invisible, teleport or call up others in combat. */
uint8_t TurnsInvisible(int16_t type);
uint8_t TeleportsInCombat(int16_t type);
uint8_t SummonsInCombat(int16_t type);

/* Magic, items and the interface. */

/* Black Gate's first circle, Linear, needs no reagents or mana and its spellbook page has no
 * title or counts; a spell costs its circle's number counted from Linear. Serpent Isle's circles
 * count from one. */
static inline uint8_t HasLinearCircle(void) { return PlayingBlackGate(); }
/* Black Gate runs an item's put-on and take-off scripts at once; Serpent Isle queues them. By the
 * item's own game. */
static inline uint8_t QueuesEquipScripts(uint16_t type) { return ShapeGame(type) != GAME_BG; }
/* Amulets remember where they were marked, as virtue stones do. */
static inline uint8_t AmuletsKeepPosition(void) { return !PlayingBlackGate(); }
/* Signs in a runic font (styles past 46); Black Gate numbers its sign styles by its own gumps. */
static inline uint8_t HasRunicSigns(void) { return !PlayingBlackGate(); }
/* Signs spelled out in capitals, which every sign font draws as plain letters: always while the
 * avatar's read flag is set, otherwise as the runes setting says. */
uint8_t TranslatesSigns(uint8_t readFlag);
/* Shapes animate on every other world draw, near the five a second their art was made for. */
uint8_t AnimatesEveryOtherDraw(void);
/* Light changes switch at once and twilight takes the original hour steps. */
uint8_t KeepsOriginalLighting(void);
/* Lines on a book page. */
static inline uint8_t BookPageLines(void) { return PlayingBlackGate() ? 25 : 19; }
/* The second conversation face is hidden while the answers show. */
static inline uint8_t HidesSecondFaceForAnswers(void) { return !PlayingBlackGate(); }
/* Black Gate checks the world's items before saves, after talks and every few minutes. */
static inline uint8_t ChecksItemBuffer(void) { return PlayingBlackGate(); }
/* Black Gate reports an unowned item taken straight into the pack only after a theft was already
 * reported in that window. */
static inline uint8_t ReportsTakingAfterTheftOnly(void) { return PlayingBlackGate(); }
/* Serpent Isle's filari, monetari and guilders weigh a tenth each, as gold does; in Black Gate
 * 948 is bolts. */
static inline uint8_t HasSiMoney(void) { return !PlayingBlackGate(); }
/* Black Gate's cyclops shake the screen as they walk. */
static inline uint8_t StepShakesScreen(int16_t type) { return PlayingBlackGate() && (type == 501 || type == 380); }
/* Party inventories that open: Black Gate never opens NPC 6's, nor any past NPC 10. */
static inline uint8_t PartyInventoryOpens(int16_t npc) { return !PlayingBlackGate() || (npc != 6 && npc <= 10); }
/* The ring of invisibility frame that wears off: any in Black Gate (255), frame 0 in Serpent Isle. */
static inline uint8_t InvisibilityRingFrame(void) { return PlayingBlackGate() ? 255 : 0; }
/* Speech numbers an item may play. */
static inline uint8_t IsItemSpeech(uint8_t n) { return PlayingBlackGate() ? n > 0 && n <= 32 : n <= 26; }
/* The world map's top left corner on screen; Black Gate's map is larger. */
static inline int16_t WorldMapLeft(void) { return PlayingBlackGate() ? 65 : 78; }
static inline int16_t WorldMapTop(void) { return PlayingBlackGate() ? 5 : 10; }
/* Containers: the locked chest and Black Gate's sealed box take nothing and their contents are
 * not counted; Serpent Isle's urn takes nothing and its jawbone only serpent teeth; bodies take
 * nothing. */
uint8_t IsSealedContainer(int16_t type);
uint8_t ContainerRefuses(int16_t container, int16_t item);
/* Serpent Isle's corpses refuse items; Black Gate's take them, as its Forge of Virtue needs. */
uint8_t CorpseRefusesItems(int16_t type);
uint8_t IsShieldShape(int16_t type);
/* Lit items that light their surroundings as a torch does. */
uint8_t GlowsLikeTorch(int16_t type);
#define BG_LIGHTNING_TYPE 179
#define BG_UNSEALED_BOX_TYPE 799

/* Text numbered differently in each game. */
enum GameText {
	TEXT_AFTER_SAVING,      /* the Avatar's bark after a save */
	TEXT_CIRCLE,            /* "Circle" on a spellbook page */
	TEXT_CIRCLE_NAMES,      /* the first circle's name; the others follow */
	GAME_TEXT_COUNT
};
int16_t TextNumber(enum GameText text);

/* Equipment. The engine has Serpent Isle's 18 slots; Black Gate's 12 map onto them. */
#define BG_SLOT_COUNT 12
extern const uint8_t BgSlotToSi[BG_SLOT_COUNT];
uint8_t SiReadyCode(int16_t type, uint8_t bgCode);
/* Black Gate's cloak: body armour there, worn in the cape slot here. */
uint8_t IsBgCloak(int16_t type);
uint8_t SlotTakesShape(uint8_t slot, int16_t type);
uint8_t LitShapeFitsAnySlot(int16_t type);
uint8_t ShapeHasEquipScript(int16_t type);

/* Black Gate's containers, bodies and spellbook: the things a double-click opens. */
uint8_t BgShapeOpens(int16_t type);

/* Gumps are numbered as in Serpent Isle; these give the record of GUMPS.VGA to read, -1 for
 * none, and where one of Black Gate's own gumps is numbered. */
int16_t GumpRecord(int16_t gump);
int16_t BgGump(int16_t record);
/* Serpent Isle gumps Black Gate lacks, read from Serpent Isle's GUMPS.VGA: the paperdoll's
 * hands and its combat status button. */
uint8_t IsSerpentIsleGump(int16_t gump);
/* A helm dropped on a doll's face goes on the head; Serpent Isle puts it in a pack whose
 * picture lies behind the face. */
static inline uint8_t HelmDropsOnFace(void) { return PlayingBlackGate(); }

/* Music and sound effects numbered differently in each game. */
enum GameSound {
	MUSIC_BATTLE,           /* a fight starts */
	MUSIC_GUARDS,           /* guards set on the Avatar */
	MUSIC_RETREAT,          /* the Avatar leaves a fight with enemies about */
	MUSIC_AVATAR_DIED,
	MUSIC_HARD_WON,         /* the party came off worse than its enemies */
	MUSIC_VICTORY,
	MUSIC_FIGHT_OVER,       /* the party was never attacked */
	MUSIC_DUNGEON,
	MUSIC_OUTDOORS,         /* leaving a dungeon */
	MUSIC_RAIN,
	MUSIC_SNOW,
	SFX_ARMOUR_HIT,         /* a blow on armour or the body */
	SFX_MISSILE_HIT,        /* also something dropped on a lower floor */
	SFX_GLASS_SWORD_HIT,
	SFX_WEAPON_HIT,
	SFX_TURN_INVISIBLE,
	SFX_TELEPORT,
	SFX_SKELETONS_RISE,
	SFX_SUMMON,
	SFX_NO_CAN_DO,
	SFX_QUAKE,
	SFX_THUNDER,
	SFX_ITEM_SET_DOWN,      /* a schedule puts an item down */
	SFX_LOADING_SCREEN,
	SFX_WINDOW_OPEN,
	SFX_BUTTON_CLICK,
	SFX_BUTTON_PRESS,
	SFX_FENCING_SWING,
	SFX_BELLOWS,
	SFX_HAMMER,
	SFX_SPLASH,             /* hot metal quenched, a trough filled */
	SFX_SPINNING_WHEEL,
	SFX_LOOM,
	SFX_FIELD,              /* a field appears or a trap fires */
	SFX_DOOR_OPEN,
	SFX_FALL_ASLEEP,
	GAME_SOUND_COUNT
};
int16_t SoundNumber(enum GameSound sound);
/* Black Gate stores weapon attack sounds one higher than it plays them. */
static inline int16_t AttackSoundNumber(int16_t stored) { return PlayingBlackGate() ? stored - 1 : stored; }
/* Black Gate keeps speech playing while the game runs. */
static inline uint8_t SpeechPlaysDuringGame(void) { return PlayingBlackGate(); }

/* World and schedules. */

/* Gargoyles, too wide for a bed, sleep on futons (312, 363). */
static inline uint8_t GargoylesUseFutons(void) { return PlayingBlackGate(); }
/* How many of a bed's frames a sleeper looks for. */
static inline uint8_t BedFramesSearched(void) { return PlayingBlackGate() ? 3 : 7; }
/* Black Gate's ghosts (299, 317) and liche (519) sleep for good and never witness a crime. */
static inline uint8_t IsDeepSleepingSpirit(int16_t type)
{
	return PlayingBlackGate() && (type == 299 || type == 317 || type == 519);
}
/* Penumbra (Black Gate's NPC 150) is never woken. */
static inline uint8_t NeverWoken(int16_t npc) { return PlayingBlackGate() && npc == 150; }
/* Serpent Isle's goblins, gwani, monks, Xenkans and penguin have their own barks when woken or
 * witnessing a crime, and its gargoyles wake hostile. In Black Gate these shapes are other
 * things: 480 is Draxinusom. */
static inline uint8_t HasSiCreatureBarks(void) { return !PlayingBlackGate(); }
/* Rats hound Mosh (NPC 27) and wolves give up; in Black Gate every hound follows the Avatar. */
static inline uint8_t HoundsPickQuarryByShape(void) { return !PlayingBlackGate(); }
/* Dancers sing in one hall, and anyone dancing outside a dance job stops after one step. */
static inline uint8_t HasSiDancers(void) { return !PlayingBlackGate(); }
/* A patroller with no route loiters, then fights the Avatar once within 10 tiles; in Serpent
 * Isle it wanders and a named NPC goes back to its job. */
static inline uint8_t RoutelessPatrollersFight(void) { return PlayingBlackGate(); }
/* Patrol marker 4: lay a flower (999) at a tombstone (715). */
static inline uint8_t MournersLayFlowers(void) { return PlayingBlackGate(); }
/* Preachers pray at the Fellowship Icon (724). */
static inline uint8_t PreachersPrayAtIcon(void) { return PlayingBlackGate(); }
/* Miners hack at lead and iron ore. */
static inline uint8_t HasMiners(void) { return PlayingBlackGate(); }
/* Companions left loitering stay put instead of checking lights and shutters. */
static inline uint8_t CompanionsStayPut(void) { return PlayingBlackGate(); }
/* Work type 24 walks an NPC somewhere for a script; in Black Gate it does nothing. */
static inline uint8_t HasPathfindSchedule(void) { return !PlayingBlackGate(); }
/* Serpent Isle's shutters are frames 0-3 closed, 4-7 open; Black Gate's open ones are shapes of
 * their own (372, 322 open; 291, 290 closed). */
static inline uint8_t ShuttersOpenByFrame(void) { return !PlayingBlackGate(); }
static inline int16_t WellShape(void) { return PlayingBlackGate() ? 740 : 470; }
static inline uint8_t WellFrame(void) { return PlayingBlackGate() ? 0xff : 1; }
/* What a seamstress makes: pants, or a cloak (403 is Batlin in Black Gate). */
static inline int16_t SewnShape(void) { return PlayingBlackGate() ? 738 : 403; }
/* Black Gate's farmers may work with a knife (615) instead of a scythe. */
static inline uint8_t FarmersUseKnives(void) { return PlayingBlackGate(); }
/* The sword blank a smith fetches: frame 0, or any. */
static inline uint8_t SwordBlankFrame(void) { return PlayingBlackGate() ? 0 : 0xff; }
/* Bread dough: Black Gate's own shape 658, frame 2 once risen; Serpent Isle's kitchen items
 * 863, frames 16 and 18. */
static inline int16_t DoughShape(void) { return PlayingBlackGate() ? 658 : 863; }
static inline uint8_t DoughFrame(uint8_t risen) { return (PlayingBlackGate() ? 0 : 16) + risen * 2; }
/* Poison fields poison from this frame of their cycle. */
static inline uint8_t PoisonFieldFrame(void) { return PlayingBlackGate() ? 6 : 4; }
/* Serpent Isle's blue flame (561) burns like a fire field; Black Gate's 561 is a great dagger. */
static inline uint8_t HasBlueFlame(void) { return !PlayingBlackGate(); }
/* Waterfalls, snowy mountains and roofs that make other kinds of dungeon. In Black Gate those
 * shapes are Lord British, wagon parts, a water wheel, a guard and others. */
static inline uint8_t HasSiDungeonShapes(void) { return !PlayingBlackGate(); }
/* Black Gate's Abbey doors (392, 225, 250, 246) in place of Serpent Isle's two-piece doors. */
static inline uint8_t HasBgAbbeyDoors(void) { return PlayingBlackGate(); }
/* The attack of a trap firing in a set direction. */
static inline uint8_t DirectionTrapAttack(void) { return PlayingBlackGate() ? 25 : 60; }
/* Cart wheels and draft horses turn while the cart moves. */
static inline uint8_t AnimatesCarts(void) { return PlayingBlackGate(); }
/* Black Gate's new-game byte is nonzero for a female Avatar; in Serpent Isle bit 0 means male
 * and bits 1-2 give the skin. */
static inline uint8_t GameArgsMarkFemale(void) { return PlayingBlackGate(); }
/* The first frame of a stack whose frame shows its size: 24 for missiles, 0 for coins and the
 * like, -1 for other items. */
int16_t QuantityFrameBase(int16_t type);
/* The meat a creature's carcass leaves, -1 for a random frame. */
int16_t MeatFrame(int16_t type);
/* Where NPCs sent off the map wait: x, y and the size of the box. */
static inline int16_t OffMapX(void) { return PlayingBlackGate() ? 1440 : 130; }
static inline int16_t OffMapY(void) { return PlayingBlackGate() ? 1280 : 35; }
static inline int16_t OffMapWidth(void) { return PlayingBlackGate() ? 16 : 44; }
static inline int16_t OffMapHeight(void) { return PlayingBlackGate() ? 16 : 27; }
/* The food (377, 17 frames) and kitchen item (863) frames an NPC picks from when it fetches any. */
uint8_t FoodFrame(uint8_t n);
static inline uint8_t KitchenFrameCount(void) { return PlayingBlackGate() ? 6 : 5; }
uint8_t KitchenFrame(uint8_t n);

#endif
