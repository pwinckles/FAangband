/* cave.h - cave interface */

#ifndef CAVE_H
#define CAVE_H

#include "defines.h"
#include "z-type.h"

struct player;
struct monster;

/**
 * Maximum size of the "temp" array (see "cave.c")
 * Note that we must be as large as "VIEW_MAX" for proper functioning
 * of the "update_view()" function, and we must also be as large as the
 * largest illuminatable room, but no room is larger than 800 grids.  We
 * must also be large enough to allow "good enough" use as a circular queue,
 * to calculate monster flow, but note that the flow code is "paranoid".
 */
#define TEMP_MAX 1536

/**
 * Maximum distance from the character to store flow (noise) information
 */
#define NOISE_STRENGTH 45

/**
 * Character turns it takes for smell to totally dissipate
 */
#define SMELL_STRENGTH 60

/*** Feature Indexes (see "lib/edit/terrain.txt") ***/

/** Nothing */
#define FEAT_NONE	0x00

/* Various */
#define FEAT_FLOOR	0x01
#define FEAT_OPEN	0x03
#define FEAT_BROKEN	0x04
#define FEAT_MORE	0x05
#define FEAT_LESS	0x06
#define FEAT_MORE_SHAFT	0x07
#define FEAT_LESS_SHAFT	0x08

/* Doors */
#define FEAT_DOOR_HEAD	0x20
#define FEAT_DOOR_TAIL	0x2F

/* Extra */
#define FEAT_SECRET	0x30
#define FEAT_RUBBLE	0x31

/* Seams */
#define FEAT_MAGMA	0x32
#define FEAT_QUARTZ	0x33
#define FEAT_MAGMA_H	0x34
#define FEAT_QUARTZ_H	0x35
#define FEAT_MAGMA_K	0x36
#define FEAT_QUARTZ_K	0x37

/* Walls */
#define FEAT_WALL_EXTRA	0x38
#define FEAT_WALL_INNER	0x39
#define FEAT_WALL_OUTER	0x3A
#define FEAT_WALL_SOLID	0x3B
#define FEAT_PERM_EXTRA	0x3C
#define FEAT_PERM_INNER	0x3D
#define FEAT_PERM_OUTER	0x3E
#define FEAT_PERM_SOLID	0x3F

/* Shops */
#define FEAT_SHOP_HEAD 0x40
#define FEAT_SHOP_HOME 0x47
#define FEAT_SHOP_TAIL 0x49

/* "stairs" in wilderness -NRM- */
#define FEAT_LESS_NORTH		0x60
#define FEAT_MORE_NORTH		0x61
#define FEAT_LESS_EAST		0x62
#define FEAT_MORE_EAST		0x63
#define FEAT_LESS_SOUTH		0x64
#define FEAT_MORE_SOUTH		0x65
#define FEAT_LESS_WEST		0x66
#define FEAT_MORE_WEST		0x67

/* Special dungeon/wilderness features. -LM- Expanded for FAangband 0.3.4 */
#define FEAT_MIN_SPECIAL        0x70
#define FEAT_LAVA		0x70
#define FEAT_WATER      	0x71
#define FEAT_TREE               0x72
#define FEAT_TREE2		0x73
#define FEAT_GRASS              0x74  
#define FEAT_ROAD               0x75
#define FEAT_VOID               0x77  
#define FEAT_PIT                0x78  
#define FEAT_DUNE               0x7a

/*** Terrain flags ***/

enum
{
	#define TF(a,b) TF_##a,
	#include "list-terrain-flags.h"
	#undef TF
	TF_MAX
};

#define TF_SIZE                FLAG_SIZE(TF_MAX)

#define tf_has(f, flag)        flag_has_dbg(f, TF_SIZE, flag, #f, #flag)


/**
 * Determine if a legal grid can be projected through
 * This is a pretty feeble hack -NRM-
 */
#define cave_project(Y,X) \
    (tf_has(f_info[cave->feat[Y][X]].flags, TF_PROJECT)) 


/**
 * Determine if a legal grid is a clean floor grid
 * Used for placing objects
 *
 * Line 1 -- check can hold an object
 * Line 2 -- forbid normal objects
 */
#define cave_clean_bold(Y,X)		     \
    (tf_has(f_info[cave->feat[Y][X]].flags, TF_OBJECT) && \
     (cave->o_idx[Y][X] == 0))

/**
 * Determine if a legal grid is an empty floor grid
 * Used for safely placing the player or a monster
 *
 * Line 1 -- check easily passed through
 * Line 2 -- forbid player/monsters
 */
#define cave_empty_bold(Y,X) \
    (tf_has(f_info[cave->feat[Y][X]].flags, TF_EASY) && \
     (cave->m_idx[Y][X] == 0))

/*
 * Cave flags
 */

enum
{
	#define SQUARE(a,b) SQUARE_##a,
	#include "list-square-flags.h"
	#undef SQUARE
	SQUARE_MAX
};

#define SQUARE_SIZE                FLAG_SIZE(SQUARE_MAX)

#define sqinfo_has(f, flag)        flag_has_dbg(f, SQUARE_SIZE, flag, #f, #flag)
#define sqinfo_next(f, flag)       flag_next(f, SQUARE_SIZE, flag)
#define sqinfo_is_empty(f)         flag_is_empty(f, SQUARE_SIZE)
#define sqinfo_is_full(f)          flag_is_full(f, SQUARE_SIZE)
#define sqinfo_is_inter(f1, f2)    flag_is_inter(f1, f2, SQUARE_SIZE)
#define sqinfo_is_subset(f1, f2)   flag_is_subset(f1, f2, SQUARE_SIZE)
#define sqinfo_is_equal(f1, f2)    flag_is_equal(f1, f2, SQUARE_SIZE)
#define sqinfo_on(f, flag)         flag_on_dbg(f, SQUARE_SIZE, flag, #f, #flag)
#define sqinfo_off(f, flag)        flag_off(f, SQUARE_SIZE, flag)
#define sqinfo_wipe(f)             flag_wipe(f, SQUARE_SIZE)
#define sqinfo_setall(f)           flag_setall(f, SQUARE_SIZE)
#define sqinfo_negate(f)           flag_negate(f, SQUARE_SIZE)
#define sqinfo_copy(f1, f2)        flag_copy(f1, f2, SQUARE_SIZE)
#define sqinfo_union(f1, f2)       flag_union(f1, f2, SQUARE_SIZE)
#define sqinfo_comp_union(f1, f2)  flag_comp_union(f1, f2, SQUARE_SIZE)
#define sqinfo_inter(f1, f2)       flag_inter(f1, f2, SQUARE_SIZE)
#define sqinfo_diff(f1, f2)        flag_diff(f1, f2, SQUARE_SIZE)

/**
 * Determine if a "legal" grid is within "los" of the player
 *
 * Note the use of comparison to zero to force a "boolean" result
 */
#define player_has_los_bold(Y,X) \
    (sqinfo_has(cave->info[Y][X], SQUARE_VIEW))


/**
 * Determine if a "legal" grid can be "seen" by the player
 *
 * Note the use of comparison to zero to force a "boolean" result
 */
#define player_can_see_bold(Y,X) \
    (sqinfo_has(cave->info[Y][X], SQUARE_SEEN))


/**
 * Information about terrain "features"
 */
typedef struct feature {
    char *name;		/**< Name  */
    char *text;		/**< Text  */
    int fidx;

    struct feature *next;

    byte mimic;		/**< Feature to mimic */
    byte priority; /**< Display priority */
    
    byte locked;   /**< How locked is it? */
    byte jammed;   /**< How jammed is it? */
    byte shopnum;  /**< Which shop does it take you to? */
    random_value dig;      /**< How hard is it to dig through? */

    bitflag flags[TF_SIZE];		/**< Bitflags */


    byte d_attr;	/**< Default feature attribute */
    wchar_t d_char;	/**< Default feature character */


    byte x_attr[4];   /**< Desired feature attribute (set by user/pref file) */
    wchar_t x_char[4];   /**< Desired feature character (set by user/pref file) */
} feature_type;


enum grid_light_level
{
	FEAT_LIGHTING_LOS = 0,   /* line of sight */
	FEAT_LIGHTING_TORCH,     /* torchlight */
	FEAT_LIGHTING_LIT,       /* permanently lit (when not in line of sight) */
	FEAT_LIGHTING_DARK,      /* dark */
	FEAT_LIGHTING_MAX
};

typedef struct
{
    u32b m_idx;		/* Monster index */
    u32b f_idx;		/* Feature index */
    u32b first_k_idx;	/* The "Kind" of the first item on the grid */
    u32b trap;          /* Trap index */
    bool multiple_objects;	/* Is there more than one item there? */
    
    enum grid_light_level lighting; /* Light level */
    bool in_view; /* TRUE when the player can currently see the grid. */
    bool is_player;
    bool hallucinate;
    bool trapborder;
} grid_data;


struct cave {
	s32b created_at;
	int depth;

	byte feeling;

	int height;
	int width;
	
	int *feat_count;

	bitflag ***info;
	byte **feat;
	byte **cost;
	byte **when;
	s16b **m_idx;
	s16b **o_idx;

	struct monster *monsters;
	int mon_max;
	int mon_cnt;

	struct trap_type *traps;
	s16b trap_max;
	int trap_cnt;
};


extern int distance(int y1, int x1, int y2, int x2);
extern bool los(int y1, int x1, int y2, int x2);
extern bool no_light(void);
extern bool cave_valid_bold(int y, int x);
extern byte get_color(byte a, int attr, int n);
extern void grid_data_as_text(grid_data *g, int *ap, wchar_t *cp, byte *tap, wchar_t *tcp);
extern void map_info(unsigned x, unsigned y, grid_data *g);
extern void move_cursor_relative(int y, int x);
extern void big_putch(int x, int y, byte a, char c);
extern void print_rel(wchar_t c, byte a, int y, int x);
extern void note_spot(int y, int x);
extern void light_spot(int y, int x);
extern void prt_map(void);
extern void display_map(int *cy, int *cx);
extern void do_cmd_view_map(void);
extern void forget_view(void);
extern void update_view(void);
extern void update_noise(void);
extern void update_smell(void);
extern void map_area(int y, int x, bool extended);
extern void wiz_light(bool wizard);
extern void wiz_dark(void);
extern void illuminate(void);
extern void square_set_feat(struct cave *c, int y, int x, int feat);
extern int project_path(u16b *gp, int range, \
                         int y1, int x1, int y2, int x2, int flg);
extern byte projectable(int y1, int x1, int y2, int x2, int flg);
extern void scatter(int *yp, int *xp, int y, int x, int d, int m);
extern void health_track(int m_idx);
extern void monster_race_track(int r_idx);
extern void track_object(int item);
extern void track_object_kind(int k_idx);
extern void disturb(int stop_search, int unused_flag);
extern bool dtrap_edge(int y, int x);

extern struct cave *cave;

extern struct cave *cave_new(void);
extern void cave_free(struct cave *c);

extern struct feature *square_feat(struct cave *c, int y, int x);
extern void square_set_feat(struct cave *c, int y, int x, int feat);
extern void square_note_spot(struct cave *c, int y, int x);
extern void square_light_spot(struct cave *c, int y, int x);
extern void cave_update_flow(struct cave *c);
extern void cave_forget_flow(struct cave *c);
extern void cave_illuminate(struct cave *c, bool daytime);

/**
 * square_predicate is a function pointer which tests a given square to
 * see if the predicate in question is true.
 */
typedef bool (*square_predicate)(struct cave *c, int y, int x);

/* FEATURE PREDICATES */
extern bool square_isfloor(struct cave *c, int y, int x);
extern bool square_isrock(struct cave *c, int y, int x);
extern bool square_isperm(struct cave *c, int y, int x);
extern bool feat_is_magma(int feat);
extern bool square_ismagma(struct cave *c, int y, int x);
extern bool feat_is_quartz(int feat);
extern bool square_isquartz(struct cave *c, int y, int x);
extern bool square_ismineral(struct cave *c, int y, int x);
extern bool square_hassecretvein(struct cave *c, int y, int x);
extern bool square_hasgoldvein(struct cave *c, int y, int x);
extern bool feat_is_treasure(int feat);
extern bool square_issecretdoor(struct cave *c, int y, int x);
extern bool square_isopendoor(struct cave *c, int y, int x);
extern bool square_iscloseddoor(struct cave *c, int y, int x);
extern bool square_islockeddoor(struct cave *c, int y, int x);
extern bool square_isbrokendoor(struct cave *c, int y, int x);
extern bool square_isdoor(struct cave *c, int y, int x);
extern bool square_issecrettrap(struct cave *c, int y, int x);
extern bool feat_is_wall(int feat);
extern bool square_isknowntrap(struct cave *c, int y, int x);
extern bool square_istrap(struct cave *c, int y, int x);
extern bool feature_isshop(int feat);
extern bool square_isstairs(struct cave *c, int y, int x);
extern bool square_isupstairs(struct cave *c, int y, int x);
extern bool square_isdownstairs(struct cave *c, int y, int x);
extern bool square_isshop(struct cave *c, int y, int x);
extern bool square_isglyph(struct cave *c, int y, int x);

/* BEHAVIOR PREDICATES */
extern bool square_isopen(struct cave *c, int y, int x);
extern bool square_isempty(struct cave *c, int y, int x);
extern bool square_canputitem(struct cave *c, int y, int x);
extern bool square_isdiggable(struct cave *c, int y, int x);
extern bool feat_is_monster_walkable(feature_type *feature);
extern bool square_is_monster_walkable(struct cave *c, int y, int x);
extern bool feat_ispassable(feature_type *f_ptr);
extern bool square_ispassable(struct cave *c, int y, int x);
extern bool feat_isprojectable(feature_type *f_ptr);
extern bool square_isprojectable(struct cave *c, int y, int x);
extern bool square_iswall(struct cave *c, int y, int x);
extern bool square_isstrongwall(struct cave *c, int y, int x);
extern bool square_isvault(struct cave *c, int y, int x);
extern bool square_isroom(struct cave *c, int y, int x);
extern bool square_isrubble(struct cave *c, int y, int x);
extern bool square_isfeel(struct cave *c, int y, int x);
extern bool feat_isboring(feature_type *f_ptr);
extern bool square_isboring(struct cave *c, int y, int x);
extern bool square_isview(struct cave *c, int y, int x);
extern bool square_isseen(struct cave *c, int y, int x);
extern bool square_wasseen(struct cave *c, int y, int x);
extern bool square_isglow(struct cave *c, int y, int x);
extern bool square_iswarded(struct cave *c, int y, int x);
extern bool square_canward(struct cave *c, int y, int x);

extern bool square_seemslikewall(struct cave *c, int y, int x);
/* interesting to memorize when mapping */
extern bool square_isinteresting(struct cave *c, int y, int x);
/* noticeable when running */
extern bool square_noticeable(struct cave *c, int y, int x);

/* Feature placers */
extern void square_add_trap(struct cave *c, int y, int x);
extern void square_add_ward(struct cave *c, int y, int x);
extern void square_add_stairs(struct cave *c, int y, int x, int depth);
extern void square_add_door(struct cave *c, int y, int x, bool closed);

extern void square_remove_ward(struct cave *c, int y, int x);

extern void cave_generate(struct cave *c, struct player *p);

extern bool square_in_bounds(struct cave *c, int y, int x);
extern bool square_in_bounds_fully(struct cave *c, int y, int x);

extern bool square_in_bounds(struct cave *c, int y, int x);
extern bool square_in_bounds_fully(struct cave *c, int y, int x);

extern struct monster *cave_monster(struct cave *c, int idx);
extern struct monster *square_monster(struct cave *c, int y, int x);
extern int cave_monster_max(struct cave *c);
extern int cave_monster_count(struct cave *c);

void upgrade_mineral(struct cave *c, int y, int x);

/* Feature modifiers */
int square_door_power(struct cave *c, int y, int x);
void square_open_door(struct cave *c, int y, int x);
void square_close_door(struct cave *c, int y, int x);
void square_smash_door(struct cave *c, int y, int x);
void square_lock_door(struct cave *c, int y, int x, int power);
void square_unlock_door(struct cave *c, int y, int x);
void square_destroy_door(struct cave *c, int y, int x);

void square_show_trap(struct cave *c, int y, int x, int type);
void square_destroy_trap(struct cave *c, int y, int x);

void square_tunnel_wall(struct cave *c, int y, int x);
void square_destroy_wall(struct cave *c, int y, int x);

void square_show_vein(struct cave *c, int y, int x);

/* destroy this cell, as destruction spell */
void square_destroy(struct cave *c, int y, int x);
void square_earthquake(struct cave *c, int y, int x);

int square_shopnum(struct cave *c, int y, int x);
const char *square_apparent_name(struct cave *c, struct player *p, int y, int x);

void square_destroy_rubble(struct cave *c, int y, int x);

void square_force_floor(struct cave *c, int y, int x);

int count_feats(int *y, int *x, bool (*test)(struct cave *cave, int y, int x), bool under);

#endif /* !CAVE_H */
