/** \file cave.c 
    \brief Map visuals

 * distance, LOS (and targetting), destruction of a square, legal object
 * and monster codes, hallucination, code for dungeon display, memorization
 * of objects and features, small-scale dungeon maps,  and management,
 * magic mapping, wizard light the dungeon, forget the dungeon, the pro-
 * jection code, disturb player, check for quest level.
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cave.h"
#include "game-event.h"
#include "game-cmd.h"
#include "mapmode.h"
#include "option.h"
#include "spells.h"
#include "squelch.h"
#include "trap.h"


/**
 * Approximate Distance between two points.
 *
 * When either the X or Y component dwarfs the other component,
 * this function is almost perfect, and otherwise, it tends to
 * over-estimate about one grid per fifteen grids of distance.
 *
 * Algorithm: hypot(dy,dx) = max(dy,dx) + min(dy,dx) / 2
 */
int distance(int y1, int x1, int y2, int x2)
{
	int ay, ax;

	/* Find the absolute y/x distance components */
	ay = (y1 > y2) ? (y1 - y2) : (y2 - y1);
	ax = (x1 > x2) ? (x1 - x2) : (x2 - x1);

	/* Hack -- approximate the distance */
	return ((ay > ax) ? (ay + (ax >> 1)) : (ax + (ay >> 1)));
}


/**
 * A simple, fast, integer-based line-of-sight algorithm.  By Joseph Hall,
 * 4116 Brewster Drive, Raleigh NC 27606.  Email to jnh@ecemwl.ncsu.edu.
 *
 * This function returns TRUE if a "line of sight" can be traced from the
 * center of the grid (x1,y1) to the center of the grid (x2,y2), with all
 * of the grids along this path (except for the endpoints) being non-wall
 * grids, that are also not trees or rubble.  Actually, the "chess knight 
 * move" situation is handled by some special case code which allows the 
 * grid diagonally next to the player to be obstructed, because this 
 * yields better gameplay semantics.  This algorithm is totally reflexive, 
 * except for "knight move" situations.
 *
 * Because this function uses (short) ints for all calculations, overflow
 * may occur if dx and dy exceed 90.
 *
 * Once all the degenerate cases are eliminated, we determine the "slope"
 * ("m"), and we use special "fixed point" mathematics in which we use a
 * special "fractional component" for one of the two location components
 * ("qy" or "qx"), which, along with the slope itself, are "scaled" by a
 * scale factor equal to "abs(dy*dx*2)" to keep the math simple.  Then we
 * simply travel from start to finish along the longer axis, starting at
 * the border between the first and second tiles (where the y offset is
 * thus half the slope), using slope and the fractional component to see
 * when motion along the shorter axis is necessary.  Since we assume that
 * vision is not blocked by "brushing" the corner of any grid, we must do
 * some special checks to avoid testing grids which are "brushed" but not
 * actually "entered".
 *
 * Angband three different "line of sight" type concepts, including this
 * function (which is used almost nowhere), the "project()" method (which
 * is used for determining the paths of projectables and spells and such),
 * and the "update_view()" concept (which is used to determine which grids
 * are "viewable" by the player, which is used for many things, such as
 * determining which grids are illuminated by the player's torch, and which
 * grids and monsters can be "seen" by the player, etc).
 */
bool los(int y1, int x1, int y2, int x2)
{
	/* Delta */
	int dx, dy;

	/* Absolute */
	int ax, ay;

	/* Signs */
	int sx, sy;

	/* Fractions */
	int qx, qy;

	/* Scanners */
	int tx, ty;

	/* Scale factors */
	int f1, f2;

	/* Slope, or 1/Slope, of LOS */
	int m;


	/* Extract the offset */
	dy = y2 - y1;
	dx = x2 - x1;

	/* Extract the absolute offset */
	ay = ABS(dy);
	ax = ABS(dx);


	/* Handle adjacent (or identical) grids */
	if ((ax < 2) && (ay < 2))
		return (TRUE);


	/* Directly South/North */
	if (!dx) {
		/* South -- check for walls */
		if (dy > 0) {
			for (ty = y1 + 1; ty < y2; ty++) {
				if (!cave_project(ty, x1))
					return (FALSE);
			}
		}

		/* North -- check for walls */
		else {
			for (ty = y1 - 1; ty > y2; ty--) {
				if (!cave_project(ty, x1))
					return (FALSE);
			}
		}

		/* Assume los */
		return (TRUE);
	}

	/* Directly East/West */
	if (!dy) {
		/* East -- check for walls */
		if (dx > 0) {
			for (tx = x1 + 1; tx < x2; tx++) {
				if (!cave_project(y1, tx))
					return (FALSE);
			}
		}

		/* West -- check for walls */
		else {
			for (tx = x1 - 1; tx > x2; tx--) {
				if (!cave_project(y1, tx))
					return (FALSE);
			}
		}

		/* Assume los */
		return (TRUE);
	}


	/* Extract some signs */
	sx = (dx < 0) ? -1 : 1;
	sy = (dy < 0) ? -1 : 1;


	/* Vertical "knights" */
	if (ax == 1) {
		if (ay == 2) {
			if (cave_project(y1 + sy, x1))
				return (TRUE);
		}
	}

	/* Horizontal "knights" */
	else if (ay == 1) {
		if (ax == 2) {
			if (cave_project(y1, x1 + sx))
				return (TRUE);
		}
	}


	/* Calculate scale factor div 2 */
	f2 = (ax * ay);

	/* Calculate scale factor */
	f1 = f2 << 1;


	/* Travel horizontally */
	if (ax >= ay) {
		/* Let m = dy / dx * 2 * (dy * dx) = 2 * dy * dy */
		qy = ay * ay;
		m = qy << 1;

		tx = x1 + sx;

		/* Consider the special case where slope == 1. */
		if (qy == f2) {
			ty = y1 + sy;
			qy -= f1;
		} else {
			ty = y1;
		}

		/* Note (below) the case (qy == f2), where */
		/* the LOS exactly meets the corner of a tile. */
		while (x2 - tx) {
			if (!cave_project(ty, tx))
				return (FALSE);

			qy += m;

			if (qy < f2) {
				tx += sx;
			} else if (qy > f2) {
				ty += sy;
				if (!cave_project(ty, tx))
					return (FALSE);
				qy -= f1;
				tx += sx;
			} else {
				ty += sy;
				qy -= f1;
				tx += sx;
			}
		}
	}

	/* Travel vertically */
	else {
		/* Let m = dx / dy * 2 * (dx * dy) = 2 * dx * dx */
		qx = ax * ax;
		m = qx << 1;

		ty = y1 + sy;

		if (qx == f2) {
			tx = x1 + sx;
			qx -= f1;
		} else {
			tx = x1;
		}

		/* Note (below) the case (qx == f2), where */
		/* the LOS exactly meets the corner of a tile. */
		while (y2 - ty) {
			if (!cave_project(ty, tx))
				return (FALSE);

			qx += m;

			if (qx < f2) {
				ty += sy;
			} else if (qx > f2) {
				tx += sx;
				if (!cave_project(ty, tx))
					return (FALSE);
				qx -= f1;
				ty += sy;
			} else {
				tx += sx;
				qx -= f1;
				ty += sy;
			}
		}
	}

	/* Assume los */
	return (TRUE);
}




/**
 * Returns true if the player's grid is dark
 * Players with the UNLIGHT ability don't need light and always
 * return false.
 */
bool no_light(void)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	if (player_has(PF_UNLIGHT) || p_ptr->state.darkness)
		return (FALSE);

	return (!player_can_see_bold(py, px));
}




/**
 * Determine if a given location may be "destroyed"
 *
 * Used by destruction spells, and for placing stairs, etc.
 */
bool cave_valid_bold(int y, int x)
{
	object_type *o_ptr;
	feature_type *f_ptr = &f_info[cave->feat[y][x]];

	/* Forbid perma-grids */
	if (tf_has(f_ptr->flags, TF_PERMANENT))
		return (FALSE);

	/* Check objects */
	for (o_ptr = get_first_object(y, x); o_ptr;
		 o_ptr = get_next_object(o_ptr)) {
		/* Forbid artifact grids */
		if (artifact_p(o_ptr))
			return (FALSE);
	}

	/* Accept */
	return (TRUE);
}


/** 
 * Table of breath colors.  Must match listings in a single set of 
 * monster spell flags.
 *
 * The value "255" is special.  Monsters with that kind of breath 
 * may be any color.
 */
static byte breath_to_attr[32][2] = {
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{TERM_SLATE, TERM_L_DARK},	/* RSF_BRTH_ACID */
	{TERM_BLUE, TERM_L_BLUE},	/* RSF_BRTH_ELEC */
	{TERM_RED, TERM_L_RED},		/* RSF_BRTH_FIRE */
	{TERM_WHITE, TERM_L_WHITE},	/* RSF_BRTH_COLD */
	{TERM_GREEN, TERM_L_GREEN},	/* RSF_BRTH_POIS */
	{TERM_ORANGE, TERM_RED},	/* RSF_BRTH_PLAS */
	{TERM_YELLOW, TERM_ORANGE},	/* RSF_BRTH_LIGHT */
	{TERM_L_DARK, TERM_SLATE},	/* RSF_BRTH_DARK */
	{TERM_L_UMBER, TERM_UMBER},	/* RSF_BRTH_CONFU */
	{TERM_YELLOW, TERM_L_UMBER},	/* RSF_BRTH_SOUND */
	{TERM_UMBER, TERM_L_UMBER},	/* RSF_BRTH_SHARD */
	{TERM_L_WHITE, TERM_SLATE},	/* RSF_BRTH_INER */
	{TERM_L_WHITE, TERM_SLATE},	/* RSF_BRTH_GRAV */
	{TERM_UMBER, TERM_L_UMBER},	/* RSF_BRTH_FORCE */
	{TERM_L_RED, TERM_VIOLET},	/* RSF_BRTH_NEXUS */
	{TERM_L_GREEN, TERM_GREEN},	/* RSF_BRTH_NETHR */
	{255, 255},					/* (any color) *//* RSF_BRTH_CHAOS */
	{TERM_VIOLET, TERM_VIOLET},	/* RSF_BRTH_DISEN */
	{TERM_L_BLUE, TERM_L_BLUE},	/* RSF_BRTH_TIME */
	{TERM_BLUE, TERM_SLATE},	/* RSF_BRTH_STORM */
	{TERM_RED, TERM_GREEN},		/* RSF_BRTH_DFIRE */
	{TERM_WHITE, TERM_L_WHITE},	/* RSF_BRTH_ICE */
	{255, 255}					/* (any color) *//* RSF_BRTH_ALL */
};


/**
 * Multi-hued monsters shimmer acording to their breaths.
 *
 * If a monster has only one kind of breath, it uses both colors 
 * associated with that breath.  Otherwise, it just uses the first 
 * color for any of its breaths.
 *
 * If a monster does not breath anything, it can be any color.
 */
static byte multi_hued_attr(monster_race * r_ptr)
{
	byte allowed_attrs[15];

	int i, j;

	int stored_colors = 0;
	int breaths = 0;
	int first_color = 0;
	int second_color = 0;


	/* Monsters with no ranged attacks can be any color */
	if (!r_ptr->freq_ranged)
		return (randint1(BASIC_COLORS - 1));

	/* Check breaths */
	for (i = 0; i < 32; i++) {
		bool stored = FALSE;

		/* Don't have that breath */
		if (!rsf_has(r_ptr->spell_flags, i))
			continue;

		/* Get the first color of this breath */
		first_color = breath_to_attr[i][0];

		/* Breath has no color associated with it */
		if (first_color == 0)
			continue;

		/* Monster can be of any color */
		if (first_color == 255)
			return (randint1(BASIC_COLORS - 1));

		/* Increment the number of breaths */
		breaths++;

		/* Monsters with lots of breaths may be any color. */
		if (breaths == 6)
			return (randint1(BASIC_COLORS - 1));

		/* Always store the first color */
		for (j = 0; j < stored_colors; j++) {
			/* Already stored */
			if (allowed_attrs[j] == first_color)
				stored = TRUE;
		}
		if (!stored) {
			allowed_attrs[stored_colors] = first_color;
			stored_colors++;
		}

		/* 
		 * Remember (but do not immediately store) the second color 
		 * of the first breath.
		 */
		if (breaths == 1) {
			second_color = breath_to_attr[i][1];
		}
	}

	/* Monsters with no breaths may be of any color. */
	if (breaths == 0)
		return (randint1(BASIC_COLORS - 1));

	/* If monster has one breath, store the second color too. */
	if (breaths == 1) {
		allowed_attrs[stored_colors] = second_color;
		stored_colors++;
	}

	/* Pick a color at random */
	return (allowed_attrs[randint0(stored_colors)]);
}

/**
 * Hack -- Hallucinatory monster
 */
static void hallucinatory_monster(int *a, wchar_t * c)
{
	while (1) {
		/* Select a random monster */
		monster_race *r_ptr = &r_info[randint0(z_info->r_max)];

		/* Skip non-entries */
		if (!r_ptr->name)
			continue;

		/* Retrieve attr/char */
		*a = r_ptr->x_attr;
		*c = r_ptr->x_char;
		return;
	}
}


/**
 * Hack -- Hallucinatory object
 */
static void hallucinatory_object(int *a, wchar_t * c)
{
	while (1) {
		/* Select a random object */
		object_kind *k_ptr = &k_info[randint0(z_info->k_max - 1) + 1];

		/* Skip non-entries */
		if (!k_ptr->name)
			continue;

		/* Retrieve attr/char (HACK - without flavors) */
		*a = k_ptr->x_attr;
		*c = k_ptr->x_char;

		/* HACK - Skip empty entries */
		if ((*a == 0) || (*c == 0))
			continue;

		return;
	}
}



/*
 * Translate text colours.
 *
 * This translates a color based on the attribute. We use this to set terrain to
 * be lighter or darker, make metallic monsters shimmer, highlight text under the
 * mouse, and reduce the colours on mono colour or 16 colour terms to the correct
 * colour space.
 *
 * TODO: Honour the attribute for the term (full color, mono, 16 color) but ensure
 * that e.g. the lighter version of yellow becomes white in a 16 color term, but
 * light yellow in a full colour term.
 */
byte get_color(byte a, int attr, int n)
{
	/* Accept any graphical attr (high bit set) */
	if (a & (0x80))
		return (a);

	/* TODO: Honour the attribute for the term (full color, mono, 16 color) */
	if (!attr)
		return (a);

	/* Translate the color N times */
	while (n > 0) {
		a = color_table[a].color_translate[attr];
		n--;
	}

	/* Return the modified color */
	return (a);
}



/* 
 * Checks if a square is at the (inner) edge of a trap detect area 
 */
bool dtrap_edge(int y, int x)
{
	int i, j;

	/* Check if the square is a dtrap in the first place */
	if (!sqinfo_has(cave->info[y][x], SQUARE_DTRAP))
		return FALSE;

	/* Check for non-dtrap adjacent grids */
	for (i = -1; i <= 1; i++)
		for (j = -1; j <= 1; j++)
			if (square_in_bounds_fully(cave, y + i, x + j) &&
				(!sqinfo_has(cave->info[y + i][x + j], SQUARE_DTRAP)))
				return TRUE;
	
	return FALSE;
}


/**
 * Apply text lighting effects
 */
static void grid_get_attr(grid_data * g, int *a)
{
	feature_type *f_ptr = &f_info[g->f_idx];

	/* Trap detect edge, but don't colour traps themselves, or treasure */
	if (g->trapborder && tf_has(f_ptr->flags, TF_FLOOR) &&
		!((int) g->trap < trap_max)) {
		*a += MAX_COLORS * BG_TRAP;
	} else if (tf_has(f_ptr->flags, TF_TORCH)) {
		if (g->lighting == FEAT_LIGHTING_TORCH) {
			if (*a == TERM_WHITE)
				*a = TERM_YELLOW;
		} else if (g->lighting == FEAT_LIGHTING_LIT) {
			if (*a == TERM_WHITE)
				*a = TERM_L_DARK;
		} else if (g->lighting == FEAT_LIGHTING_DARK) {
			if (*a == TERM_WHITE)
				*a = TERM_L_DARK;
		}
		if (OPT(hybrid_walls) && tf_has(f_ptr->flags, TF_WALL)) {
			*a = *a + (MAX_COLORS * BG_DARK);
		} else if (OPT(solid_walls) && tf_has(f_ptr->flags, TF_WALL)) {
			*a = *a + (MAX_COLORS * BG_SAME);
		}
	} else {
		if ((g->lighting == FEAT_LIGHTING_DARK || g->lighting == FEAT_LIGHTING_LIT)) {
			if (*a == TERM_WHITE)
				*a = TERM_SLATE;
		}
		if (OPT(hybrid_walls) && tf_has(f_ptr->flags, TF_WALL)) {
			*a = *a + (MAX_COLORS * BG_DARK);
		} else if (OPT(solid_walls) && tf_has(f_ptr->flags, TF_WALL)) {
			*a = *a + (MAX_COLORS * BG_SAME);
		}
	}
}


/**
 * This function takes a pointer to a grid info struct describing the 
 * contents of a grid location (as obtained through the function map_info)
 * and fills in the character and attr pairs for display.
 *
 * ap and cp are filled with the attr/char pair for the monster, object or 
 * floor tile that is at the "top" of the grid (monsters covering objects, 
 * which cover floor, assuming all are present).
 *
 * tap and tcp are filled with the attr/char pair for the floor, regardless
 * of what is on it.  This can be used by graphical displays with
 * transparency to place an object onto a floor tile, is desired.
 *
 * Any lighting effects are also applied to these pairs, clear monsters allow
 * the underlying colour or feature to show through (ATTR_CLEAR and
 * CHAR_CLEAR), multi-hued colour-changing (ATTR_MULTI) is applied, and so on.
 * Technically, the flag "CHAR_MULTI" is supposed to indicate that a monster 
 * looks strange when examined, but this flag is currently ignored.
 *
 * NOTES:
 * This is called pretty frequently, whenever a grid on the map display
 * needs updating, so don't overcomplicate it.
 *
 * The "zero" entry in the feature/object/monster arrays are
 * used to provide "special" attr/char codes, with "monster zero" being
 * used for the player attr/char, "object zero" being used for the "pile"
 * attr/char, and "feature zero" being used for the "darkness" attr/char.
 *
 * TODO:
 * The transformations for tile colors, or brightness for the 16x16
 * tiles should be handled differently.  One possibility would be to
 * extend feature_type with attr/char definitions for the different states.
 * This will probably be done outside of the current text->graphics mappings
 * though.
 */
void grid_data_as_text(grid_data * g, int *ap, wchar_t * cp, byte * tap,
					   wchar_t * tcp)
{
	feature_type *f_ptr = &f_info[g->f_idx];

	int a = f_ptr->x_attr[g->lighting];
	wchar_t c = f_ptr->x_char[g->lighting];

	/* Don't display hidden objects */
	bool ignore_objects = tf_has(f_ptr->flags, TF_HIDE_OBJ);

	/* Neutral monsters get shaded background */
	bool neutral = FALSE;

	/* Check for trap detection boundaries */
	if (use_graphics == GRAPHICS_NONE)
		grid_get_attr(g, &a);

	/* Save the terrain info for the transparency effects */
	(*tap) = a;
	(*tcp) = c;


	/* There is a trap in this grid, and we are not hallucinating */
	if (((int) g->trap < trap_max) && (!g->hallucinate)) {
		/* Change graphics to indicate a trap (if visible) */
		if (get_trap_graphics(cave, g->trap, &a, &c, TRUE)) {
			/* Ignore objects stacked on top of this trap */
			ignore_objects = TRUE;
		}
	}


	/* If there's an object, deal with that. */
	if ((g->first_k_idx) && !ignore_objects) {
		if (g->hallucinate) {
			/* Just pick a random object to display. */
			hallucinatory_object(&a, &c);
		} else {
			object_kind *k_ptr = &k_info[g->first_k_idx];

			/* Normal attr and char */
			a = object_kind_attr(g->first_k_idx);
			c = object_kind_char(g->first_k_idx);

			if (g->multiple_objects) {
				/* Get the "pile" feature instead */
				k_ptr = &k_info[0];

				a = k_ptr->x_attr;
				c = k_ptr->x_char;
			}
		}
	}

	/* If there's a monster */
	if (g->m_idx > 0) {
		if (g->hallucinate) {
			/* Just pick a random monster to display. */
			hallucinatory_monster(&a, &c);
		} else {
			monster_type *m_ptr = &m_list[g->m_idx];
			monster_race *r_ptr = &r_info[m_ptr->r_idx];

			byte da;
			wchar_t dc;

			/* Desired attr & char */
			da = r_ptr->x_attr;
			dc = r_ptr->x_char;

			/* Neutral monster get shaded background */
			if (m_ptr->hostile >= 0)
				neutral = TRUE;

			/* Special attr/char codes */
			if (da & 0x80) {
				/* Use attr */
				a = da;

				/* Use char */
				c = dc;
			}

			/* Multi-hued monster */
			else if (rf_has(r_ptr->flags, RF_ATTR_MULTI) ||
					 rf_has(r_ptr->flags, RF_ATTR_FLICKER)) {
				/* Multi-hued attr */
				a = multi_hued_attr(r_ptr);

				/* Normal char */
				c = dc;
			}

			/* Normal monster (not "clear" in any way) */
			else if (!flags_test(r_ptr->flags, RF_SIZE,
								 RF_ATTR_CLEAR, RF_CHAR_CLEAR, FLAG_END)) {
				/* Use attr */
				a = da;

				/* Desired attr & char */
				da = r_ptr->x_attr;
				dc = r_ptr->x_char;

				/* Use char */
				c = dc;
			}

			/* Hack -- Bizarre grid under monster */
			else if ((a & 0x80) || (c & 0x80)) {
				/* Use attr */
				a = da;

				/* Use char */
				c = dc;
			}

			/* Normal char, Clear attr, monster */
			else if (!rf_has(r_ptr->flags, RF_CHAR_CLEAR)) {
				/* Normal char */
				c = dc;
			}

			/* Normal attr, Clear char, monster */
			else if (!rf_has(r_ptr->flags, RF_ATTR_CLEAR)) {
				/* Normal attr */
				a = da;
			}

			/* Store the drawing attr so we can use it elsewhere */
			m_ptr->attr = a;
		}
	}

	/* Handle "player" */
	else if (g->is_player) {
		monster_race *r_ptr = &r_info[0];

		/* Get the "player" attr */
		a = r_ptr->x_attr;
		if ((OPT(hp_changes_colour)) && (arg_graphics == GRAPHICS_NONE)) {
			switch (p_ptr->chp * 10 / p_ptr->mhp) {
			case 10:
			case 9:
				{
					a = TERM_WHITE;
					break;
				}
			case 8:
			case 7:
				{
					a = TERM_YELLOW;
					break;
				}
			case 6:
			case 5:
				{
					a = TERM_ORANGE;
					break;
				}
			case 4:
			case 3:
				{
					a = TERM_L_RED;
					break;
				}
			case 2:
			case 1:
			case 0:
				{
					a = TERM_RED;
					break;
				}
			default:
				{
					a = TERM_WHITE;
					break;
				}
			}
		}

		/* Get the "player" char */
		c = r_ptr->x_char;
	}

	/* Shaded for neutrals */
	if (neutral)
		a += MAX_COLORS * BG_DARK;

	/* Result */
	(*ap) = a;
	(*cp) = c;
}

/*
 * This function takes a grid location (x, y) and extracts information the
 * player is allowed to know about it, filling in the grid_data structure
 * passed in 'g'.
 *
 * The information filled in is as follows:
 *  - g->f_idx is filled in with the terrain's feature type, or FEAT_NONE
 *    if the player doesn't know anything about the grid.  The function
 *    makes use of the "mimic" field in terrain in order to allow one
 *    feature to look like another (hiding secret doors, invisible traps,
 *    etc).  This will return the terrain type the player "Knows" about,
 *    not necessarily the real terrain.
 *  - g->m_idx is set to the monster index, or 0 if there is none (or the
 *    player doesn't know it).
 *  - g->first_k_idx is set to the index of the first object in a grid
 *    that the player knows (and cares, as per OPT(hide_squelchable)) about,
 *    or zero for no object in the grid.
 *  - g->muliple_objects is TRUE if there is more than one object in the
 *    grid that the player knows and cares about (to facilitate any special
 *    floor stack symbol that might be used).
 *  - g->in_view is TRUE if the player can currently see the grid - this can
 *    be used to indicate field-of-view, such as through the OPT(view_bright_light)
 *    option.
 *  - g->lighting is set to indicate the lighting level for the grid:
 *    FEAT_LIGHTING_DARK for unlit grids, FEAT_LIGHTING_LIT for inherently light
 *    grids (lit rooms, etc), FEAT_LIGHTING_TORCH for grids lit by the player's
 *    light source, and FEAT_LIGHTING_LOS for grids in the player's line of sight.
 *    LIGHT_DARK for unlit grids, LIGHT_TORCH for those lit by the player's
 *    light source, and LIGHT_GLOW for inherently light grids (lit rooms, etc).
 *    Note that lighting is always FEAT_LIGHTING_LIT for known "interesting" grids
 *    like walls.
 *  - g->is_player is TRUE if the player is on the given grid.
 *  - g->hallucinate is TRUE if the player is hallucinating something "strange"
 *    for this grid - this should pick a random monster to show if the m_idx
 *    is non-zero, and a random object if first_k_idx is non-zero.
 *       
 * NOTES:
 * This is called pretty frequently, whenever a grid on the map display
 * needs updating, so don't overcomplicate it.
 *
 * Terrain is remembered separately from objects and monsters, so can be
 * shown even when the player can't "see" it.  This leads to things like
 * doors out of the player's view still change from closed to open and so on.
 *
 * TODO:
 * Hallucination is currently disabled (it was a display-level hack before,
 * and we need it to be a knowledge-level hack).  The idea is that objects
 * may turn into different objects, monsters into different monsters, and
 * terrain may be objects, monsters, or stay the same.
 */
void map_info(unsigned y, unsigned x, grid_data * g)
{
	object_type *o_ptr;
	feature_type *f_ptr;

	assert(x < DUNGEON_WID);
	assert(y < DUNGEON_HGT);

	/* Default "clear" values, others will be set later where appropriate. */
	g->first_k_idx = 0;
	g->trap = trap_max;
	g->multiple_objects = FALSE;
	g->lighting = FEAT_LIGHTING_DARK;

	/* Set things we can work out right now */
	g->f_idx = cave->feat[y][x];
	g->in_view = sqinfo_has(cave->info[y][x], SQUARE_SEEN) ? TRUE : FALSE;
	g->is_player = (cave->m_idx[y][x] < 0) ? TRUE : FALSE;
	g->m_idx = (g->is_player) ? 0 : cave->m_idx[y][x];
	g->hallucinate = p_ptr->timed[TMD_IMAGE] ? TRUE : FALSE;
	g->trapborder = (sqinfo_has(cave->info[y][x], SQUARE_DEDGE)) ? TRUE : FALSE;
	f_ptr = &f_info[g->f_idx];

	/* Apply "mimic" field */
	g->f_idx = f_ptr->mimic;

	/* If the grid is memorised or can currently be seen */
	if (g->in_view) {
		g->lighting = FEAT_LIGHTING_LOS;

		if (!sqinfo_has(cave->info[y][x], SQUARE_GLOW)
			&& OPT(view_yellow_light))
			g->lighting = FEAT_LIGHTING_TORCH;

	}
	/* Unknown */
	else if (!sqinfo_has(cave->info[y][x], SQUARE_MARK)) {
		g->f_idx = FEAT_NONE;
	}

	/* There is a trap in this grid */
	if (sqinfo_has(cave->info[y][x], SQUARE_TRAP) &&
		sqinfo_has(cave->info[y][x], SQUARE_MARK)) {
		int i;

		/* Scan the current trap list */
		for (i = 0; i < trap_max; i++) {
			/* Point to this trap */
			trap_type *t_ptr = &trap_list[i];

			/* Find a trap in this position */
			if ((t_ptr->fy == y) && (t_ptr->fx == x)) {
				/* Get the trap */
				g->trap = i;
				break;
			}
		}
	}

	/* Objects */
	for (o_ptr = get_first_object(y, x); o_ptr;
		 o_ptr = get_next_object(o_ptr)) {
		/* Memorized objects */
		if (o_ptr->marked && !squelch_hide_item(o_ptr)) {
			/* First item found */
			if (g->first_k_idx == 0) {
				g->first_k_idx = o_ptr->k_idx;
			} else {
				g->multiple_objects = TRUE;

				/* And we know all we need to know. */
				break;
			}
		}
	}

	/* Monsters */
	if (g->m_idx > 0) {
		/* If the monster isn't "visible", make sure we don't list it. */
		monster_type *m_ptr = &m_list[g->m_idx];
		if (!m_ptr->ml)
			g->m_idx = 0;

	}

	/* Rare random hallucination on non-outer walls */
	if (g->hallucinate && g->m_idx == 0 && g->first_k_idx == 0) {
		if (one_in_(256) && (g->f_idx != FEAT_PERM_SOLID)) {
			/* Normally, make an imaginary monster */
			if (randint0(100) < 75) {
				g->m_idx = 1;
			}
			/* Otherwise, an imaginary object */
			else {
				g->first_k_idx = 1;
			}
		} else {
			g->hallucinate = FALSE;
		}
	}

	assert(g->f_idx <= z_info->f_max);
	if (!g->hallucinate)
		assert(g->m_idx < (u32b) m_max);
	assert(g->first_k_idx < z_info->k_max);
	/* All other g fields are 'flags', mostly booleans. */
}



/*
 * Move the cursor to a given map location.
 */
static void move_cursor_relative_map(int y, int x)
{
	int ky, kx;

	term *old;

	int j;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++) {
		term *t = angband_term[j];

		/* No window */
		if (!t)
			continue;

		/* No relevant flags */
		if (!(op_ptr->window_flag[j] & (PW_MAP)))
			continue;

		/* Location relative to panel */
		ky = y - t->offset_y;

		if (tile_height > 1) {
			ky = tile_height * ky;
		}

		/* Verify location */
		if ((ky < 0) || (ky >= t->hgt))
			continue;

		/* Location relative to panel */
		kx = x - t->offset_x;

		if (tile_width > 1) {
			kx = tile_width * kx;
		}

		/* Verify location */
		if ((kx < 0) || (kx >= t->wid))
			continue;

		/* Go there */
		old = Term;
		Term_activate(t);
		(void) Term_gotoxy(kx, ky);
		Term_activate(old);
	}
}

/*
 * Move the cursor to a given map location.
 *
 * The main screen will always be at least 24x80 in size.
 */
void move_cursor_relative(int y, int x)
{
	int ky, kx;
	int vy, vx;

	/* Move the cursor on map sub-windows */
	move_cursor_relative_map(y, x);

	/* Location relative to panel */
	ky = y - Term->offset_y;

	/* Verify location */
	if ((ky < 0) || (ky >= SCREEN_HGT))
		return;

	/* Location relative to panel */
	kx = x - Term->offset_x;

	/* Verify location */
	if ((kx < 0) || (kx >= SCREEN_WID))
		return;

	/* Location in window */
	vy = ky + ROW_MAP;

	/* Location in window */
	vx = kx + COL_MAP;

	if (tile_width > 1) {
		vx += (tile_width - 1) * kx;
	}
	if (tile_height > 1) {
		vy += (tile_height - 1) * ky;
	}

	/* Go there */
	(void) Term_gotoxy(vx, vy);
}


/*
 * Display an attr/char pair at the given map location
 *
 * Note the inline use of "panel_contains()" for efficiency.
 *
 * Note the use of "Term_queue_char()" for efficiency.
 */
static void print_rel_map(wchar_t c, byte a, int y, int x)
{
	int ky, kx;

	int j;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++) {
		term *t = angband_term[j];

		/* No window */
		if (!t)
			continue;

		/* No relevant flags */
		if (!(op_ptr->window_flag[j] & (PW_MAP)))
			continue;

		/* Location relative to panel */
		ky = y - t->offset_y;

		if (tile_height > 1) {
			ky = tile_height * ky;
			if (ky + 1 >= t->hgt)
				continue;
		}

		/* Verify location */
		if ((ky < 0) || (ky >= t->hgt))
			continue;

		/* Location relative to panel */
		kx = x - t->offset_x;

		if (tile_width > 1) {
			kx = tile_width * kx;
			if (kx + 1 >= t->wid)
				continue;
		}

		/* Verify location */
		if ((kx < 0) || (kx >= t->wid))
			continue;

		/* Hack -- Queue it */
		Term_queue_char(t, kx, ky, a, c, 0, 0);

		if ((tile_width > 1) || (tile_height > 1)) {
			/* Mega-Hack : Queue dummy chars */
			Term_big_queue_char(Term, kx, ky, a, c, 0, 0);
		}
	}
}



/*
 * Display an attr/char pair at the given map location
 *
 * Note the inline use of "panel_contains()" for efficiency.
 *
 * Note the use of "Term_queue_char()" for efficiency.
 *
 * The main screen will always be at least 24x80 in size.
 */
void print_rel(wchar_t c, byte a, int y, int x)
{
	int ky, kx;
	int vy, vx;

	/* Print on map sub-windows */
	print_rel_map(c, a, y, x);

	/* Location relative to panel */
	ky = y - Term->offset_y;

	/* Verify location */
	if ((ky < 0) || (ky >= SCREEN_HGT))
		return;

	/* Location relative to panel */
	kx = x - Term->offset_x;

	/* Verify location */
	if ((kx < 0) || (kx >= SCREEN_WID))
		return;

	/* Get right position */
	vx = COL_MAP + (tile_width * kx);
	vy = ROW_MAP + (tile_height * ky);

	/* Hack -- Queue it */
	Term_queue_char(Term, vx, vy, a, c, 0, 0);

	if ((tile_width > 1) || (tile_height > 1)) {
		/* Mega-Hack : Queue dummy chars */
		Term_big_queue_char(Term, vx, vy, a, c, 0, 0);
	}

}

/*
 * Memorize interesting viewable object/features in the given grid
 *
 * This function should only be called on "legal" grids.
 *
 * This function will memorize the object and/or feature in the given grid,
 * if they are (1) see-able and (2) interesting.  Note that all objects are
 * interesting, all terrain features except floors (and invisible traps) are
 * interesting, and floors (and invisible traps) are interesting sometimes
 * (depending on various options involving the illumination of floor grids).
 *
 * The automatic memorization of all objects and non-floor terrain features
 * as soon as they are displayed allows incredible amounts of optimization
 * in various places, especially "map_info()" and this function itself.
 *
 * Note that the memorization of objects is completely separate from the
 * memorization of terrain features, preventing annoying floor memorization
 * when a detected object is picked up from a dark floor, and object
 * memorization when an object is dropped into a floor grid which is
 * memorized but out-of-sight.
 *
 * This function should be called every time the "memorization" of a grid
 * (or the object in a grid) is called into question, such as when an object
 * is created in a grid, when a terrain feature "changes" from "floor" to
 * "non-floor", and when any grid becomes "see-able" for any reason.
 *
 * This function is called primarily from the "update_view()" function, for
 * each grid which becomes newly "see-able".
 */
void note_spot(int y, int x)
{
	object_type *o_ptr;

	/* Require "seen" flag */
	if (!sqinfo_has(cave->info[y][x], SQUARE_SEEN))
		return;


	/* Hack -- memorize objects */
	for (o_ptr = get_first_object(y, x); o_ptr;
		 o_ptr = get_next_object(o_ptr)) {
		/* Memorize objects */
		o_ptr->marked = TRUE;
	}


	/* Hack -- memorize grids */
	if (sqinfo_has(cave->info[y][x], SQUARE_MARK))
		return;

	/* Memorize */
	sqinfo_on(cave->info[y][x], SQUARE_MARK);
}




/**
 * Redraw (on the screen) a given MAP location
 *
 * This function should only be called on "legal" grids
 */
void light_spot(int y, int x)
{
	event_signal_point(EVENT_MAP, x, y);
}



static void prt_map_aux(void)
{
	int a;
	wchar_t c;
	byte ta;
	wchar_t tc;
	grid_data g;

	int y, x;
	int vy, vx;
	int ty, tx;

	int j;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++) {
		term *t = angband_term[j];

		/* No window */
		if (!t)
			continue;

		/* No relevant flags */
		if (!(op_ptr->window_flag[j] & (PW_MAP)))
			continue;

		/* Assume screen */
		ty = t->offset_y + (t->hgt / tile_height);
		tx = t->offset_x + (t->wid / tile_width);

		/* Dump the map */
		for (y = t->offset_y, vy = 0; y < ty; vy++, y++) {
			if (vy + tile_height - 1 >= t->hgt)
				continue;
			for (x = t->offset_x, vx = 0; x < tx; vx++, x++) {
				/* Check bounds */
				if (!square_in_bounds(cave, y, x))
					continue;

				if (vx + tile_width - 1 >= t->wid)
					continue;

				/* Determine what is there */
				map_info(y, x, &g);
				grid_data_as_text(&g, &a, &c, &ta, &tc);
				Term_queue_char(t, vx, vy, a, c, ta, tc);

				if ((tile_width > 1) || (tile_height > 1)) {
					/* Mega-Hack : Queue dummy chars */
					Term_big_queue_char(t, vx, vy, 255, -1, 0, 0);
				}
			}
		}
	}
}



/*
 * Redraw (on the screen) the current map panel
 *
 * Note the inline use of "light_spot()" for efficiency.
 *
 * The main screen will always be at least 24x80 in size.
 */
void prt_map(void)
{
	int a;
	wchar_t c;
	byte ta;
	wchar_t tc;
	grid_data g;

	int y, x;
	int vy, vx;
	int ty, tx;

	/* Redraw map sub-windows */
	prt_map_aux();

	/* Assume screen */
	ty = Term->offset_y + SCREEN_HGT;
	tx = Term->offset_x + SCREEN_WID;

	/* Dump the map */
	for (y = Term->offset_y, vy = ROW_MAP; y < ty; vy++, y++) {
		for (x = Term->offset_x, vx = COL_MAP; x < tx; vx++, x++) {
			/* Check bounds */
			if (!square_in_bounds(cave, y, x))
				continue;

			/* Determine what is there */
			map_info(y, x, &g);
			grid_data_as_text(&g, &a, &c, &ta, &tc);

			/* Hack -- Queue it */
			Term_queue_char(Term, vx, vy, a, c, ta, tc);

			if ((tile_width > 1) || (tile_height > 1)) {
				Term_big_queue_char(Term, vx, vy, a, c, TERM_WHITE, L' ');

				if (tile_width > 1) {
					vx += tile_width - 1;
				}
			}
		}

		if (tile_height > 1)
			vy += tile_height - 1;

	}
}



/**
 * Display highest priority object in the RATIO by RATIO area
 */
#define RATIO 3

/**
 * Display the entire map
 */
#define MAP_HGT (DUNGEON_HGT / RATIO)
#define MAP_WID (DUNGEON_WID / RATIO)


/**
 * Display a "small-scale" map of the dungeon in the active Term
 *
 * Note that the "map_info()" function must return fully colorized
 * data or this function will not work correctly.
 *
 * Note that this function must "disable" the special lighting
 * effects so that the "priority" function will work.
 *
 * Note the use of a specialized "priority" function to allow this
 * function to work with any graphic attr/char mappings, and the
 * attempts to optimize this function where possible.
 *
 * cx and cy are offsets from the position of the player.  This
 * allows the map to be shifted around - but only works in the
 * wilderness.  cx and cy return the position of the player on the
 * possibly shifted map.
 */
void display_map(int *cy, int *cx)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	int map_hgt, map_wid;
	int dungeon_hgt, dungeon_wid, top_row, left_col;
	int row, col;

	int x, y;
	grid_data g;

	int a;
	byte ta;
	wchar_t c, tc;

	byte tp;

	/* Large array on the stack */
	byte mp[DUNGEON_HGT][DUNGEON_WID];

	monster_race *r_ptr = &r_info[0];

	/* Desired map height */
	map_hgt = Term->hgt - 2;
	map_wid = Term->wid - 2;

	/* Adjust for town */
	dungeon_hgt = (p_ptr->depth ? DUNGEON_HGT : 2 * DUNGEON_HGT / 3);
	dungeon_wid = (p_ptr->depth ? DUNGEON_WID : 2 * DUNGEON_WID / 3);
	if (!(p_ptr->depth) && (p_ptr->stage < KHAZAD_DUM_TOWN) &&
		!MAP(DUNGEON) && !MAP(FANILLA))
		dungeon_wid = DUNGEON_WID / 2;
	top_row = (p_ptr->depth ? 0 : DUNGEON_HGT / 3);
	left_col = (p_ptr->depth ? 0 : DUNGEON_WID / 3);

	/* Prevent accidents */
	if (map_hgt > dungeon_hgt)
		map_hgt = dungeon_hgt;
	if (map_wid > dungeon_wid)
		map_wid = dungeon_wid;

	/* Prevent accidents */
	if ((map_wid < 1) || (map_hgt < 1))
		return;

	/* Nothing here */
	a = TERM_WHITE;
	c = L' ';
	ta = TERM_WHITE;
	tc = L' ';

	/* Clear the priorities */
	for (y = 0; y < map_hgt; ++y) {
		for (x = 0; x < map_wid; ++x) {
			/* No priority */
			mp[y][x] = 0;
		}
	}

	/* Draw a box around the edge of the term */
	window_make(0, 0, map_wid + 1, map_hgt + 1);

	/* Analyze the actual map */
	for (y = top_row; y < dungeon_hgt; y++) {
		for (x = left_col; x < dungeon_wid; x++) {
			row = ((y - top_row) * map_hgt / dungeon_hgt);
			col = ((x - left_col) * map_wid / dungeon_wid);

			if (tile_width > 1) {
				col = col - (col % tile_width);
			}
			if (tile_height > 1) {
				row = row - (row % tile_height);
			}

			/* Get the attr/char at that map location */
			map_info(y, x, &g);
			grid_data_as_text(&g, &a, &c, &ta, &tc);

			/* Get the priority of that feature */
			tp = f_info[g.f_idx].priority;

			/* Stuff on top of terrain gets higher priority */
			if ((a != ta) || (c != tc))
				tp = 20;

			/* Save "best" */
			if (mp[row][col] < tp) {
				/* Hack - make every grid on the map lit */
				g.lighting = FEAT_LIGHTING_LIT;	/*FEAT_LIGHTING_BRIGHT; */
				grid_data_as_text(&g, &a, &c, &ta, &tc);

				Term_queue_char(Term, col + 1, row + 1, a, c, ta, tc);

				if ((tile_width > 1) || (tile_height > 1))
					Term_big_queue_char(Term, col + 1, row + 1, 255, -1, 0, 0);

				/* Save priority */
				mp[row][col] = tp;
			}
		}
	}


	/* Player location */
	row = ((py - top_row) * map_hgt / dungeon_hgt);
	col = ((px - left_col) * map_wid / dungeon_wid);

	if (tile_width > 1) {
		col = col - (col % tile_width);
	}
	if (tile_height > 1) {
		row = row - (row % tile_height);
	}

  /*** Make sure the player is visible ***/

	/* Get the "player" attr */
	ta = r_ptr->x_attr;

	/* Get the "player" char */
	tc = r_ptr->x_char;

	/* Draw the player */
	Term_putch(col + 1, row + 1, ta, tc);

	if ((tile_width > 1) || (tile_height > 1)) {
		Term_big_putch(col + 1, row + 1, ta, tc);
	}

	/* Return player location */
	if (cy != NULL)
		(*cy) = row + 1;
	if (cx != NULL)
		(*cx) = col + 1;
}

/**
 * Display a map of the type of wilderness surrounding the current stage
 */
void regional_map(int num, int size, int centre_stage)
{
	int i, j, col, row;
	int *stage = malloc(size * sizeof(*stage));
	int north, east, south, west;
	const char *lev;

	/* Get the side length */
	num = 2 * num + 1;

	/* Initialise */
	for (i = 0; i < size; i++)
		stage[i] = 0;

	/* Set the centre */
	stage[size / 2] = centre_stage;

	/* Redo the right number of times */
	for (j = 0; j < num; j++) {
		/* Pass across the whole array */
		for (i = 0; i < size; i++) {
			/* See what's adjacent */
			north = (i > (num - 1) ? (i - num) : -1);
			east = ((i % num) != (num - 1) ? (i + 1) : -1);
			south = (i < (size - num) ? (i + num) : -1);
			west = ((i % num) ? (i - 1) : -1);

			/* Set them */
			if ((north >= 0) && (stage_map[stage[i]][NORTH])
				&& (!stage[north]))
				stage[north] = stage_map[stage[i]][NORTH];
			if ((east >= 0) && (stage_map[stage[i]][EAST])
				&& (!stage[east]))
				stage[east] = stage_map[stage[i]][EAST];
			if ((south >= 0) && (stage_map[stage[i]][SOUTH])
				&& (!stage[south]))
				stage[south] = stage_map[stage[i]][SOUTH];
			if ((west >= 0) && (stage_map[stage[i]][WEST])
				&& (!stage[west]))
				stage[west] = stage_map[stage[i]][WEST];
		}
	}

	/* Now print the info */
	for (i = 0; i < size; i++) {
		/* Nowhere */
		if (!stage[i])
			continue;

		/* Get the place */
		col = (i % num) * 10 + COL_MAP;
		row = (i / num) * 4 + ROW_MAP;

		/* Get the level string */
		lev = format("Level %d", stage_map[stage[i]][DEPTH]);

		switch (stage_map[stage[i]][STAGE_TYPE]) {
		case TOWN:
			{
				c_put_str(TERM_SLATE, "Town", row, col);
				break;
			}
		case PLAIN:
			{
				c_put_str(TERM_UMBER, "Plain", row, col);
				break;
			}
		case FOREST:
			{
				c_put_str(TERM_GREEN, "Forest", row, col);
				break;
			}
		case MOUNTAIN:
			{
				c_put_str(TERM_L_DARK, "Mountain", row, col);
				break;
			}
		case SWAMP:
			{
				c_put_str(TERM_L_GREEN, "Swamp", row, col);
				break;
			}
		case RIVER:
			{
				c_put_str(TERM_BLUE, "River", row, col);
				break;
			}
		case DESERT:
			{
				c_put_str(TERM_L_UMBER, "Desert", row, col);
				break;
			}
		case VALLEY:
			{
				c_put_str(TERM_RED, "Valley", row, col);
				break;
			}
		default:
			break;
		}
		if (stage[i] == p_ptr->stage)
			c_put_str(TERM_VIOLET, "Current ", row, col);
		c_put_str(i == size / 2 ? TERM_WHITE : TERM_L_DARK, lev, row + 1,
				  col);
		if (stage_map[stage[i]][EAST])
			c_put_str(TERM_WHITE, "-", row + 1, col + 8);
		if (stage_map[stage[i]][DOWN]) {
			if (stage_map[stage[i]][STAGE_TYPE] == MOUNTAINTOP)
				switch (stage_map[stage_map[stage[i]][DOWN]][STAGE_TYPE]) {
				case TOWN:
					{
						c_put_str(TERM_SLATE, "(Town)", row + 2, col);
						break;
					}
				case PLAIN:
					{
						c_put_str(TERM_UMBER, "(Plain)", row + 2, col);
						break;
					}
				case FOREST:
					{
						c_put_str(TERM_GREEN, "(Forest)", row + 2, col);
						break;
					}
				case MOUNTAIN:
					{
						c_put_str(TERM_L_DARK, "(Mountain)", row + 2, col);
						break;
					}
				case RIVER:
					{
						c_put_str(TERM_BLUE, "(River)", row + 2, col);
						break;
					}
				case DESERT:
					{
						c_put_str(TERM_L_UMBER, "(Desert)", row + 2, col);
						break;
					}
				case VALLEY:
					{
						c_put_str(TERM_RED, "(Valley)", row + 2, col);
						break;
					}
				default:
					break;
			} else
				if (stage_map[stage_map[stage[i]][DOWN]][STAGE_TYPE] != UNDERWORLD)
					c_put_str(TERM_L_RED, "(Dungeon)", row + 2, col);
		}
		if (stage_map[stage[i]][SOUTH])
			c_put_str(TERM_WHITE, "|", row + 3, col + 3);
		if ((stage_map[stage[i]][SOUTH]) && (!stage_map[stage[i]][DOWN]))
			c_put_str(TERM_WHITE, "|", row + 2, col + 3);
	}
	free(stage);

}

/**
 * Display a "small-scale" map of the dungeon.
 *
 * Note that the "player" is always displayed on the map.
 */
void do_cmd_view_map(void)
{
	int cy, cx;
	int wid, hgt;
	/* variables for regional map */
	int num_down, num_across, num, centre_stage, next_stage;
	ui_event ke;

	/* Get size */
	Term_get_size(&wid, &hgt);

	/* Get dimensions for the regional map */
	num_down = (hgt - 6) / 8;
	num_across = (wid - 24) / 20;
	num = (num_down < num_across ? num_down : num_across);

	/* Hack - limit range for now */
	num = 2;
	centre_stage = p_ptr->stage;

	/* Save screen */
	screen_save();

	/* Note */
	prt("Please wait...", 0, 0);

	/* Flush */
	Term_fresh();

	/* Clear the screen */
	Term_clear();

	/* Display the map */
	display_map(&cy, &cx);

	/* Wait for it */
	put_str("Hit any key to continue", hgt - 1, (wid - COL_MAP) / 2);

	/* Hilight the player */
	Term_gotoxy(cx, cy);

	/* Get any key */
	(void) inkey_ex();

	/* Regional map if not in the dungeon */
	if (stage_map[p_ptr->stage][STAGE_TYPE] != CAVE) {
		for (;;) {
			/* Flush */
			Term_fresh();

			/* Clear the screen */
			Term_clear();

			/* Display the regional map */
			regional_map(num, (2 * num + 1) * (2 * num + 1), centre_stage);

			/* Wait for it */
			put_str("Move keys to scroll, other input to continue",
					hgt - 1, (wid - 40) / 2);

			/* Get any key */
			ke = inkey_ex();
			next_stage = -1;
			switch (ke.key.code) {
			case 'k':
			case ARROW_UP:
				next_stage = stage_map[centre_stage][NORTH];
				break;
			case 'j':
			case ARROW_DOWN:
				next_stage = stage_map[centre_stage][SOUTH];
				break;
			case 'h':
			case ARROW_LEFT:
				next_stage = stage_map[centre_stage][WEST];
				break;
			case 'l':
			case ARROW_RIGHT:
				next_stage = stage_map[centre_stage][EAST];
				break;
			}
			if (next_stage == -1)
				break;
			if (next_stage)
				centre_stage = next_stage;


		}						/* for(;;) */
	}

	/* Load screen */
	screen_load();
}


/**
 * Some comments on the dungeon related data structures and functions...
 *
 * Angband is primarily a dungeon exploration game, and it should come as
 * no surprise that the internal representation of the dungeon has evolved
 * over time in much the same way as the game itself, to provide semantic
 * changes to the game itself, to make the code simpler to understand, and
 * to make the executable itself faster or more efficient in various ways.
 *
 * There are a variety of dungeon related data structures, and associated
 * functions, which store information about the dungeon, and provide methods
 * by which this information can be accessed or modified.
 *
 * Some of this information applies to the dungeon as a whole, such as the
 * list of unique monsters which are still alive.  Some of this information
 * only applies to the current dungeon level, such as the current depth, or
 * the list of monsters currently inhabiting the level.   And some of the
 * information only applies to a single grid of the current dungeon level,
 * such as whether the grid is illuminated, or whether the grid contains a
 * monster, or whether the grid can be seen by the player.  If Angband was
 * to be turned into a multi-player game, some of the information currently
 * associated with the dungeon should really be associated with the player,
 * such as whether a given grid is viewable by a given player.
 *
 * Currently, a lot of the information about the dungeon is stored in ways
 * that make it very efficient to access or modify the information, while
 * still attempting to be relatively conservative about memory usage, even
 * if this means that some information is stored in multiple places, or in
 * ways which require the use of special code idioms.  For example, each
 * monster record in the monster array contains the location of the monster,
 * and each cave grid has an index into the monster array, or a zero if no
 * monster is in the grid.  This allows the monster code to efficiently see
 * where the monster is located, while allowing the dungeon code to quickly
 * determine not only if a monster is present in a given grid, but also to
 * find out which monster.  The extra space used to store the information
 * twice is inconsequential compared to the speed increase.
 *
 * Several pieces of information about each cave grid are stored in the
 * "cave->info" array, which is a special two dimensional array of bitflags.
 *
 * The "SQUARE_ROOM" flag is saved in the savefile and is used to determine
 * which grids are part of "rooms", and thus which grids are affected by
 * "illumination" spells.
 *
 * The "SQUARE_ICKY" flag is saved in the savefile and is used to determine
 * which grids are part of "vaults", and thus which grids cannot serve as
 * the destinations of player teleportation.
 *
 * The "SQUARE_MARK" flag is saved in the savefile and is used to determine
 * which grids have been "memorized" by the player.  This flag is used by
 * the "map_info()" function to determine if a grid should be displayed.
 * This flag is used in a few other places to determine if the player can
 * "know" about a given grid.
 *
 * The "SQUARE_GLOW" flag is saved in the savefile and is used to determine
 * which grids are "permanently illuminated".  This flag is used by the
 * "update_view()" function to help determine which viewable flags may
 * be "seen" by the player.  This flag is used by the "map_info" function
 * to determine if a grid is only lit by the player's torch.  This flag
 * has special semantics for wall grids (see "update_view()").
 *
 * The "SQUARE_VIEW" flag is used to determine which grids are currently in
 * line of sight of the player.   This flag is set by (and used by) the
 * "update_view()" function.  This flag is used by any code which needs to
 * know if the player can "view" a given grid.  This flag is used by the
 * "map_info()" function for some optional special lighting effects.  The
 * "player_has_los_bold()" macro wraps an abstraction around this flag, but
 * certain code idioms are much more efficient.   This flag is used to check
 * if a modification to a terrain feature might affect the player's field of
 * view.  This flag is used to see if certain monsters are "visible" to the
 * player.  This flag is used to allow any monster in the player's field of
 * view to "sense" the presence of the player.
 *
 * The "SQUARE_SEEN" flag is used to determine which grids are currently in
 * line of sight of the player and also illuminated in some way.  This flag
 * is set by the "update_view()" function, using computations based on the
 * "SQUARE_VIEW" and "SQUARE_WALL" and "SQUARE_GLOW" flags of various grids.  
 * This flag is used by any code which needs to know if the player can "see" a
 * given grid.  This flag is used by the "map_info()" function both to see
 * if a given "boring" grid can be seen by the player, and for some optional
 * special lighting effects.  The "player_can_see_bold()" macro wraps an
 * abstraction around this flag, but certain code idioms are much more
 * efficient.  This flag is used to see if certain monsters are "visible" to
 * the player.  This flag is never set for a grid unless "SQUARE_VIEW" is also
 * set for the grid.  Whenever the "SQUARE_WALL" or "SQUARE_GLOW" flag changes
 * for a grid which has the "SQUARE_VIEW" flag set, the "SQUARE_SEEN" flag must
 * be recalculated.  The simplest way to do this is to call "forget_view()"
 * and "update_view()" whenever the "SQUARE_WALL" or "SQUARE_GLOW" flags change
 * for a grid which has "SQUARE_VIEW" set.
 *
 * The "SQUARE_WASSEEN" flag is used for a variety of temporary purposes.  This
 * flag is used to determine if the "SQUARE_SEEN" flag for a grid has changed
 * during the "update_view()" function.   This flag is used to "spread" light
 * or darkness through a room.  This flag is used by the "monster flow code".
 * This flag must always be cleared by any code which sets it.
 *
 * Note that the "SQUARE_MARK" flag is used for many reasons, some of which
 * are strictly for optimization purposes.  The "SQUARE_MARK" flag means that
 * even if the player cannot "see" the grid, he "knows" about the terrain in
 * that grid.  This is used to "memorize" grids when they are first "seen" by
 * the player, and to allow certain grids to be "detected" by certain magic.
 * Note that most grids are always memorized when they are first "seen", but
 * "boring" grids (floor grids) are only memorized if the "view_torch_grids"
 * option is set, or if the "view_perma_grids" option is set, and the grid
 * in question has the "SQUARE_GLOW" flag set.
 *
 * Objects are "memorized" in a different way, using a special "marked" flag
 * on the object itself, which is set when an object is observed or detected.
 * This allows objects to be "memorized" independant of the terrain features.
 *
 * The "update_view()" function is an extremely important function.  It is
 * called only when the player moves, significant terrain changes, or the
 * player's blindness or torch radius changes.  Note that when the player
 * is resting, or performing any repeated actions (like digging, disarming,
 * farming, etc), there is no need to call the "update_view()" function, so
 * even if it was not very efficient, this would really only matter when the
 * player was "running" through the dungeon.  It sets the "SQUARE_VIEW" flag
 * on every cave grid in the player's field of view, and maintains an array
 * of all such grids in the global "view_g" array.  It also checks the torch
 * radius of the player, and sets the "SQUARE_SEEN" flag for every grid which
 * is in the "field of view" of the player and which is also "illuminated",
 * either by the players torch (if any) or by any permanent light source.
 * It could use and help maintain information about multiple light sources,
 * which would be helpful in a multi-player version of Angband.
 *
 * Note that the "update_view()" function allows, among other things, a room
 * to be "partially" seen as the player approaches it, with a growing cone
 * of floor appearing as the player gets closer to the door.  Also, by not
 * turning on the "memorize perma-lit grids" option, the player will only
 * "see" those floor grids which are actually in line of sight.   And best
 * of all, you can now activate the special lighting effects to indicate
 * which grids are actually in the player's field of view by using dimmer
 * colors for grids which are not in the player's field of view, and/or to
 * indicate which grids are illuminated only by the player's torch by using
 * the color yellow for those grids.
 *
 * It seems as though slight modifications to the "update_view()" functions
 * would allow us to determine "reverse" line-of-sight as well as "normal"
 * line-of-sight", which would allow monsters to have a more "correct" way
 * to determine if they can "see" the player, since right now, they "cheat"
 * somewhat and assume that if the player has "line of sight" to them, then
 * they can "pretend" that they have "line of sight" to the player.  But if
 * such a change was attempted, the monsters would actually start to exhibit
 * some undesirable behavior, such as "freezing" near the entrances to long
 * hallways containing the player, and code would have to be added to make
 * the monsters move around even if the player was not detectable, and to
 * "remember" where the player was last seen, to avoid looking stupid.
 *
 * Note that the "SQUARE_GLOW" flag means that a grid is permanently lit in
 * some way.  However, for the player to "see" the grid, as determined by
 * the "SQUARE_SEEN" flag, the player must not be blind, the grid must have
 * the "SQUARE_VIEW" flag set, and if the grid is a "wall" grid, and it is
 * not lit by the player's torch, then it must touch a grid which does not
 * have the "SQUARE_WALL" flag set, but which does have both the "SQUARE_GLOW"
 * and "SQUARE_VIEW" flags set.  This last part about wall grids is induced
 * by the semantics of "SQUARE_GLOW" as applied to wall grids, and checking
 * the technical requirements can be very expensive, especially since the
 * grid may be touching some "illegal" grids.  Luckily, it is more or less
 * correct to restrict the "touching" grids from the eight "possible" grids
 * to the (at most) three grids which are touching the grid, and which are
 * closer to the player than the grid itself, which eliminates more than
 * half of the work, including all of the potentially "illegal" grids, if
 * at most one of the three grids is a "diagonal" grid.   In addition, in
 * almost every situation, it is possible to ignore the "SQUARE_VIEW" flag
 * on these three "touching" grids, for a variety of technical reasons.
 * Finally, note that in most situations, it is only necessary to check
 * a single "touching" grid, in fact, the grid which is strictly closest
 * to the player of all the touching grids, and in fact, it is normally
 * only necessary to check the "SQUARE_GLOW" flag of that grid, again, for
 * various technical reasons.  However, one of the situations which does
 * not work with this last reduction is the very common one in which the
 * player approaches an illuminated room from a dark hallway, in which the
 * two wall grids which form the "entrance" to the room would not be marked
 * as "SQUARE_SEEN", since of the three "touching" grids nearer to the player
 * than each wall grid, only the farthest of these grids is itself marked
 * "SQUARE_GLOW". 
 *
 *
 * Here are some pictures of the legal "light source" radius values, in
 * which the numbers indicate the "order" in which the grids could have
 * been calculated, if desired.   Note that the code will work with larger
 * radiuses, though currently yields such a radius, and the game would
 * become slower in some situations if it did.
 *<pre>
 *	 Rad=0      Rad=1      Rad=2          Rad=3         
 *  No-Light  Torch,etc   Lantern       Artifacts         
 *    
 *                                         333         
 *	                        333           43334         
 *	            212	       32123         3321233         
 *     @        1@1	       31@13         331@133         
 *	            212	       32123         3321233         
 *	                        333           43334         
 *                                         333         
 *</pre>
 *
 */



/**
 * Forget the "SQUARE_VIEW" grids, redrawing as needed
 */
void forget_view(void)
{
	int x, y;

	for (y = 0; y < DUNGEON_HGT; y++) {
		for (x = 0; x < DUNGEON_WID; x++) {
			if (!sqinfo_has(cave->info[y][x], SQUARE_VIEW))
				continue;
			sqinfo_off(cave->info[y][x], SQUARE_VIEW);
			sqinfo_off(cave->info[y][x], SQUARE_SEEN);
			light_spot(y, x);
		}
	}
}



/**
 * Calculate the complete field of view using a new algorithm
 */
static void mark_wasseen(void)
{
	int x, y;
	/* Save the old "view" grids for later */
	for (y = 0; y < DUNGEON_HGT; y++) {
		for (x = 0; x < DUNGEON_WID; x++) {
			if (sqinfo_has(cave->info[y][x], SQUARE_SEEN))
				sqinfo_on(cave->info[y][x], SQUARE_TEMP);
			sqinfo_off(cave->info[y][x], SQUARE_VIEW);
			sqinfo_off(cave->info[y][x], SQUARE_SEEN);
		}
	}
}

static void update_one(int y, int x, int blind)
{
	if (blind)
		sqinfo_off(cave->info[y][x], SQUARE_SEEN);

	/* Square went from unseen -> seen */
	if (sqinfo_has(cave->info[y][x], SQUARE_SEEN) &&
		!sqinfo_has(cave->info[y][x], SQUARE_TEMP)) {
		note_spot(y, x);
		light_spot(y, x);
	}

	/* Square went from seen -> unseen */
	if (!sqinfo_has(cave->info[y][x], SQUARE_SEEN) &&
		sqinfo_has(cave->info[y][x], SQUARE_TEMP))
		light_spot(y, x);

	sqinfo_off(cave->info[y][x], SQUARE_TEMP);
}

/**
 * True if the square is a wall square (impedes the player's los).
 *
 */
bool cave_iswall(int y, int x)
{
	/* Terrain */
	feature_type *f_ptr;

	if (!square_in_bounds(cave, y, x))
		return FALSE;

	f_ptr = &f_info[cave->feat[y][x]];

	return !tf_has(f_ptr->flags, TF_LOS);
}

static void become_viewable(int y, int x, int lit, int py, int px)
{
	int xc = x;
	int yc = y;
	if (sqinfo_has(cave->info[y][x], SQUARE_VIEW))
		return;

	sqinfo_on(cave->info[y][x], SQUARE_VIEW);

	if (lit)
		sqinfo_on(cave->info[y][x], SQUARE_SEEN);

	if (sqinfo_has(cave->info[y][x], SQUARE_GLOW)) {
		if (cave_iswall(y, x)) {
			/* For walls, move a bit towards the player.
			 * TODO(elly): huh? why?
			 */
			xc = (x < px) ? (x + 1) : (x > px) ? (x - 1) : x;
			yc = (y < py) ? (y + 1) : (y > py) ? (y - 1) : y;
		}
		if (sqinfo_has(cave->info[yc][xc], SQUARE_GLOW))
			sqinfo_on(cave->info[y][x], SQUARE_SEEN);
	}
}

static void update_view_one(int y, int x, int radius, int py, int px)
{
	int xc = x;
	int yc = y;

	int d = distance(y, x, py, px);
	int lit = d < radius;

	if (d > MAX_SIGHT)
		return;

	/* Special case for wall lighting. If we are a wall and the square in
	 * the direction of the player is in LOS, we are in LOS. This avoids
	 * situations like:
	 * #1#############
	 * #............@#
	 * ###############
	 * where the wall cell marked '1' would not be lit because the LOS
	 * algorithm runs into the adjacent wall cell.
	 */
	if (cave_iswall(y, x)) {
		int dx = x - px;
		int dy = y - py;
		int ax = ABS(dx);
		int ay = ABS(dy);
		int sx = dx > 0 ? 1 : -1;
		int sy = dy > 0 ? 1 : -1;

		xc = (x < px) ? (x + 1) : (x > px) ? (x - 1) : x;
		yc = (y < py) ? (y + 1) : (y > py) ? (y - 1) : y;

		/* Check that the cell we're trying to steal LOS from isn't a
		 * wall. If we don't do this, double-thickness walls will have
		 * both sides visible.
		 */
		if (cave_iswall(yc, xc)) {
			xc = x;
			yc = y;
		}

		/* Check that we got here via the 'knight's move' rule. If so,
		 * don't steal LOS. */
		if (ax == 2 && ay == 1) {
			if (!cave_iswall(y, x - sx) && cave_iswall(y - sy, x - sx)) {
				xc = x;
				yc = y;
			}
		} else if (ax == 1 && ay == 2) {
			if (cave_iswall(y - sy, x) && cave_iswall(y - sy, x - sx)) {
				xc = x;
				yc = y;
			}
		}
	}


	if (los(py, px, yc, xc))
		become_viewable(y, x, lit, py, px);
}

void update_view(void)
{
	int x, y;

	int radius;

	mark_wasseen();

	/* Extract "radius" value */
	if ((player_has(PF_UNLIGHT) || p_ptr->state.darkness)
		&& (p_ptr->cur_light <= 0))
		radius = 2;
	else
		radius = p_ptr->cur_light;

	/* Handle real light */
	if (radius > 0)
		++radius;

	/* Assume we can view the player grid */
	sqinfo_on(cave->info[p_ptr->py][p_ptr->px], SQUARE_VIEW);
	if (radius > 0
		|| sqinfo_has(cave->info[p_ptr->py][p_ptr->px], SQUARE_GLOW))
		sqinfo_on(cave->info[p_ptr->py][p_ptr->px], SQUARE_SEEN);

	/* View squares we have LOS to */
	for (y = 0; y < DUNGEON_HGT; y++)
		for (x = 0; x < DUNGEON_WID; x++)
			update_view_one(y, x, radius, p_ptr->py, p_ptr->px);

	/*** Step 3 -- Complete the algorithm ***/

	for (y = 0; y < DUNGEON_HGT; y++)
		for (x = 0; x < DUNGEON_WID; x++)
			update_one(y, x, p_ptr->timed[TMD_BLIND]);
}




/**
 * Every so often, the character makes enough noise that nearby 
 * monsters can use it to home in on him.
 *
 * Fill in the "cave->cost" field of every grid that the player can 
 * reach with the number of steps needed to reach that grid.  This 
 * also yields the route distance of the player from every grid.
 *
 * Monsters use this information by moving to adjacent grids with 
 * lower flow costs, thereby homing in on the player even though 
 * twisty tunnels and mazes.  Monsters can also run away from loud 
 * noises.
 *
 * The biggest limitation of this code is that it does not easily 
 * allow for alternate ways around doors (not all monsters can handle 
 * doors) and lava/water (many monsters are not allowed to enter 
 * water, lava, or both).
 *
 * The flow table is three-dimensional.  The first dimension allows the 
 * table to both store and overwrite grids safely.  The second indicates 
 * whether this value is that for x or for y.  The third is the number 
 * of grids able to be stored at any flow distance.
 */
void update_noise(void)
{
	int cost;
	int route_distance = 0;

	int i, d;
	int y, x, y2, x2;
	int last_index;
	int grid_count = 0;

	int dist;
	bool full = FALSE;

	/* Note where we get information from, and where we overwrite */
	int this_cycle = 0;
	int next_cycle = 1;

	byte flow_table[2][2][8 * NOISE_STRENGTH];

	/* The character's grid has no flow info.  Do a full rebuild. */
	if (cave->cost[p_ptr->py][p_ptr->px] == 0)
		full = TRUE;

	/* Determine when to rebuild, update, or do nothing */
	if (!full) {
		dist = ABS(p_ptr->py - flow_center_y);
		if (ABS(p_ptr->px - flow_center_x) > dist)
			dist = ABS(p_ptr->px - flow_center_x);

		/* 
		 * Character is far enough away from the previous flow center - 
		 * do a full rebuild.
		 */
		if (dist >= 15)
			full = TRUE;

		else {
			/* Get axis distance to center of last update */
			dist = ABS(p_ptr->py - update_center_y);
			if (ABS(p_ptr->px - update_center_x) > dist)
				dist = ABS(p_ptr->px - update_center_x);

			/* 
			 * We probably cannot decrease the center cost any more.
			 * We should assume that we have to do a full rebuild.
			 */
			if (cost_at_center - (dist + 5) <= 0)
				full = TRUE;


			/* Less than five grids away from last update */
			else if (dist < 5) {
				/* We're in LOS of the last update - don't update again */
				if (los
					(p_ptr->py, p_ptr->px, update_center_y,
					 update_center_x))
					return;

				/* We're not in LOS - update */
				else
					full = FALSE;
			}

			/* Always update if at least five grids away */
			else
				full = FALSE;
		}
	}

	/* Update */
	if (!full) {
		bool found = FALSE;

		/* Start at the character's location */
		flow_table[this_cycle][0][0] = p_ptr->py;
		flow_table[this_cycle][1][0] = p_ptr->px;
		grid_count = 1;

		/* Erase outwards until we hit the previous update center */
		for (cost = 0; cost <= NOISE_STRENGTH; cost++) {
			/* 
			 * Keep track of the route distance to the previous 
			 * update center.
			 */
			route_distance++;


			/* Get the number of grids we'll be looking at */
			last_index = grid_count;

			/* Clear the grid count */
			grid_count = 0;

			/* Get each valid entry in the flow table in turn */
			for (i = 0; i < last_index; i++) {
				/* Get this grid */
				y = flow_table[this_cycle][0][i];
				x = flow_table[this_cycle][1][i];

				/* Look at all adjacent grids */
				for (d = 0; d < 8; d++) {
					/* Child location */
					y2 = y + ddy_ddd[d];
					x2 = x + ddx_ddd[d];

					/* Check Bounds */
					if (!square_in_bounds(cave, y2, x2))
						continue;

					/* Ignore illegal grids */
					if (cave->cost[y2][x2] == 0)
						continue;

					/* Ignore previously erased grids */
					if (cave->cost[y2][x2] == 255)
						continue;

					/* Erase previous info, mark grid */
					cave->cost[y2][x2] = 255;

					/* Store this grid in the flow table */
					flow_table[next_cycle][0][grid_count] = y2;
					flow_table[next_cycle][1][grid_count] = x2;

					/* Increment number of grids stored */
					grid_count++;

					/* If this is the previous update center, we can stop */
					if ((y2 == update_center_y) && (x2 == update_center_x))
						found = TRUE;
				}
			}

			/* Stop when we find the previous update center. */
			if (found)
				break;


			/* Swap write and read portions of the table */
			if (this_cycle == 0) {
				this_cycle = 1;
				next_cycle = 0;
			} else {
				this_cycle = 0;
				next_cycle = 1;
			}
		}

		/* 
		 * Reduce the flow cost assigned to the new center grid by 
		 * enough to maintain the correct cost slope out to the range 
		 * we have to update the flow.
		 */
		cost_at_center -= route_distance;

		/* We can't reduce the center cost any more.  Do a full rebuild. */
		if (cost_at_center < 0)
			full = TRUE;

		else {
			/* Store the new update center */
			update_center_y = p_ptr->py;
			update_center_x = p_ptr->px;
		}
	}


	/* Full rebuild */
	if (full) {
		/* 
		 * Set the initial cost to 100; updates will progressively 
		 * lower this value.  When it reaches zero, another full 
		 * rebuild has to be done.
		 */
		cost_at_center = 100;

		/* Save the new noise epicenter */
		flow_center_y = p_ptr->py;
		flow_center_x = p_ptr->px;
		update_center_y = p_ptr->py;
		update_center_x = p_ptr->px;


		/* Erase all of the current flow (noise) information */
		for (y = 0; y < DUNGEON_HGT; y++) {
			for (x = 0; x < DUNGEON_WID; x++) {
				cave->cost[y][x] = 0;
			}
		}
	}


	/*** Update or rebuild the flow ***/


	/* Store base cost at the character location */
	cave->cost[p_ptr->py][p_ptr->px] = cost_at_center;

	/* Store this grid in the flow table, note that we've done so */
	flow_table[this_cycle][0][0] = p_ptr->py;
	flow_table[this_cycle][1][0] = p_ptr->px;
	grid_count = 1;

	/* Extend the noise burst out to its limits */
	for (cost = cost_at_center + 1;
		 cost <= cost_at_center + NOISE_STRENGTH; cost++) {
		/* Get the number of grids we'll be looking at */
		last_index = grid_count;

		/* Stop if we've run out of work to do */
		if (last_index == 0)
			break;

		/* Clear the grid count */
		grid_count = 0;

		/* Get each valid entry in the flow table in turn. */
		for (i = 0; i < last_index; i++) {
			/* Get this grid */
			y = flow_table[this_cycle][0][i];
			x = flow_table[this_cycle][1][i];

			/* Look at all adjacent grids */
			for (d = 0; d < 8; d++) {
				/* Child location */
				y2 = y + ddy_ddd[d];
				x2 = x + ddx_ddd[d];

				/* Check Bounds */
				if (!square_in_bounds(cave, y2, x2))
					continue;

				/* When doing a rebuild... */
				if (full) {
					/* Ignore previously marked grids */
					if (cave->cost[y2][x2])
						continue;

					/* Ignore walls.  Do not ignore rubble. */
					if (tf_has
						(f_info[cave->feat[y2][x2]].flags, TF_NO_NOISE))
						continue;
				}

				/* When doing an update... */
				else {
					/* Ignore all but specially marked grids */
					if (cave->cost[y2][x2] != 255)
						continue;
				}

				/* Store cost at this location */
				cave->cost[y2][x2] = cost;

				/* Store this grid in the flow table */
				flow_table[next_cycle][0][grid_count] = y2;
				flow_table[next_cycle][1][grid_count] = x2;

				/* Increment number of grids stored */
				grid_count++;
			}
		}

		/* Swap write and read portions of the table */
		if (this_cycle == 0) {
			this_cycle = 1;
			next_cycle = 0;
		} else {
			this_cycle = 0;
			next_cycle = 1;
		}
	}
}


/**
 * Characters leave scent trails for perceptive monsters to track.
 *
 * Smell is rather more limited than sound.  Many creatures cannot use 
 * it at all, it doesn't extend very far outwards from the character's 
 * current position, and monsters can use it to home in the character, 
 * but not to run away from him.
 *
 * Smell is valued according to age.  When a character takes his turn, 
 * scent is aged by one, and new scent of the current age is laid down.  
 * Speedy characters leave more scent, true, but it also ages faster, 
 * which makes it harder to hunt them down.
 *
 * Whenever the age count loops, most of the scent trail is erased and 
 * the age of the remainder is recalculated.
 */
void update_smell(void)
{
	int i, j;
	int y, x;

	int py = p_ptr->py;
	int px = p_ptr->px;

	feature_type *f_ptr = NULL;

	/* Create a table that controls the spread of scent */
	int scent_adjust[5][5] = {
		{250, 2, 2, 2, 250},
		{2, 1, 1, 1, 2},
		{2, 1, 0, 1, 2},
		{2, 1, 1, 1, 2},
		{250, 2, 2, 2, 250},
	};

	/* Scent becomes "younger" */
	scent_when--;

	/* Loop the age and adjust scent values when necessary */
	if (scent_when <= 0) {
		/* Scan the entire dungeon */
		for (y = 0; y < DUNGEON_HGT; y++) {
			for (x = 0; x < DUNGEON_WID; x++) {
				/* Ignore non-existent scent */
				if (cave->when[y][x] == 0)
					continue;

				/* Erase the earlier part of the previous cycle */
				if (cave->when[y][x] > SMELL_STRENGTH)
					cave->when[y][x] = 0;

				/* Reset the ages of the most recent scent */
				else
					cave->when[y][x] =
						250 - SMELL_STRENGTH + cave->when[y][x];
			}
		}

		/* Reset the age value */
		scent_when = 250 - SMELL_STRENGTH;
	}


	/* Lay down new scent */
	for (i = 0; i < 5; i++) {
		for (j = 0; j < 5; j++) {
			/* Translate table to map grids */
			y = i + py - 2;
			x = j + px - 2;

			/* Check Bounds */
			if (!square_in_bounds(cave, y, x))
				continue;

			/* Get the feature */
			f_ptr = &f_info[cave->feat[y][x]];

			/* Walls, water, and lava cannot hold scent. */
			if (tf_has(f_ptr->flags, TF_NO_SCENT)) {
				continue;
			}

			/* Grid must not be blocked by walls from the character */
			if (!los(p_ptr->py, p_ptr->px, y, x))
				continue;

			/* Note grids that are too far away */
			if (scent_adjust[i][j] == 250)
				continue;

			/* Mark the grid with new scent */
			cave->when[y][x] = scent_when + scent_adjust[i][j];
		}
	}
}

/**
 * Map around a given point, or the current panel (plus some) 
 * ala "magic mapping".   Staffs of magic mapping map more than 
 * rods do, because staffs affect larger areas in general.
 *
 * We must never attempt to map the outer dungeon walls, or we
 * might induce illegal cave grid references.
 */
void map_area(int y, int x, bool extended)
{
	int i, y_c, x_c;
	int rad = DETECT_RAD_DEFAULT;

	if (extended)
		rad += 10;

	/* Map around a location, if given. */
	if ((y) && (x)) {
		y_c = y;
		x_c = x;
	}

	/* Normally, pick an area to map around the player */
	else {
		y_c = p_ptr->py;
		x_c = p_ptr->px;
	}

	/* Scan the maximal area of mapping */
	for (y = y_c - rad; y <= y_c + rad; y++) {
		for (x = x_c - rad; x <= x_c + rad; x++) {
			feature_type *f_ptr = &f_info[cave->feat[y][x]];

			/* Ignore "illegal" locations */
			if (!square_in_bounds(cave, y, x))
				continue;

			/* Enforce a "circular" area */
			if (distance(y_c, x_c, y, x) > rad)
				continue;

			/* All passable grids are checked */
			if (square_ispassable(cave, y, x)) {
				/* Memorize interesting features */
				if (!tf_has(f_ptr->flags, TF_FLOOR) ||
					tf_has(f_ptr->flags, TF_INTERESTING)) {
					/* Memorize the object */
					sqinfo_on(cave->info[y][x], SQUARE_MARK);
				}

				/* Memorize known walls */
				for (i = 0; i < 8; i++) {
					int yy = y + ddy_ddd[i];
					int xx = x + ddx_ddd[i];

					/* All blockages are checked */
					f_ptr = &f_info[cave->feat[yy][xx]];
					if (!tf_has(f_ptr->flags, TF_LOS)) {
						/* Memorize the walls */
						sqinfo_on(cave->info[yy][xx], SQUARE_MARK);
					}
				}
			}
		}
	}

	/* Redraw map */
	p_ptr->redraw |= (PR_MAP);
}



/**
 * Light up the dungeon using "claravoyance"
 *
 * This function "illuminates" every grid in the dungeon, memorizes all
 * "objects", memorizes all grids as with magic mapping, and, under the
 * standard option settings (view_perma_grids but not view_torch_grids)
 * memorizes all floor grids too.
 *
 * In Oangband, greater and lesser vaults only become fully known if the 
 * player has accessed this function from the debug commands.  Otherwise, 
 * they act like magically mapped permenantly lit rooms.
 *
 * Note that if "view_perma_grids" is not set, we do not memorize floor
 * grids, since this would defeat the purpose of "view_perma_grids", not
 * that anyone seems to play without this option.
 *
 * Note that if "view_torch_grids" is set, we do not memorize floor grids,
 * since this would prevent the use of "view_torch_grids" as a method to
 * keep track of what grids have been observed directly.
 */
void wiz_light(bool wizard)
{
	int i, y, x;

	/* Memorize objects */
	for (i = 1; i < o_max; i++) {
		object_type *o_ptr = &o_list[i];

		/* Skip dead objects */
		if (!o_ptr->k_idx)
			continue;

		/* Skip held objects */
		if (o_ptr->held_m_idx)
			continue;

		/* Skip objects in vaults, if not a wizard. */
		if ((wizard == FALSE)
			&& sqinfo_has(cave->info[o_ptr->iy][o_ptr->ix], SQUARE_ICKY))
			continue;

		/* Memorize */
		o_ptr->marked = TRUE;
	}

	/* Scan all normal grids */
	for (y = 1; y < DUNGEON_HGT - 1; y++) {
		/* Scan all normal grids */
		for (x = 1; x < DUNGEON_WID - 1; x++) {
			feature_type *f_ptr = &f_info[cave->feat[y][x]];

			/* Process all passable grids (or all grids, if a wizard) */
			if (square_ispassable(cave, y, x)) {
				/* Paranoia -- stay in bounds */
				if (!square_in_bounds_fully(cave, y, x))
					continue;

				/* Scan the grid and all neighbors */
				for (i = 0; i < 9; i++) {
					int yy = y + ddy_ddd[i];
					int xx = x + ddx_ddd[i];

					f_ptr = &f_info[cave->feat[yy][xx]];

					/* Perma-light the grid (always) */
					sqinfo_on(cave->info[yy][xx], SQUARE_GLOW);

					/* If not a wizard, do not mark passable grids in vaults */
					if ((!wizard)
						&& sqinfo_has(cave->info[yy][xx], SQUARE_ICKY)) {
						if (square_ispassable(cave, yy, xx))
							continue;
					}

					/* Memorize features other than ordinary floor */
					if (!tf_has(f_ptr->flags, TF_FLOOR) ||
						square_visible_trap(cave, yy, xx)) {
						/* Memorize the grid */
						sqinfo_on(cave->info[yy][xx], SQUARE_MARK);
					}
				}
			}
		}
	}

	/* Fully update the visuals */
	p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redraw whole map, monster list */
	p_ptr->redraw |= (PR_MAP | PR_MONLIST | PR_ITEMLIST);
}



/**
 * Forget the dungeon map (ala "Thinking of Maud...").
 */
void wiz_dark(void)
{
	int i, y, x;


	/* Forget every grid */
	for (y = 0; y < DUNGEON_HGT; y++) {
		for (x = 0; x < DUNGEON_WID; x++) {
			int idx;

			/* Process the grid */
			sqinfo_off(cave->info[y][x], SQUARE_MARK);
			sqinfo_off(cave->info[y][x], SQUARE_DTRAP);
			sqinfo_off(cave->info[y][x], SQUARE_DEDGE);

			/* Traps become invisible */
			idx = visible_trap_idx(cave, y, x);
			if (idx > -1)
				hide_trap_idx(idx);
		}
	}

	/* Forget all objects */
	for (i = 1; i < o_max; i++) {
		object_type *o_ptr = &o_list[i];

		/* Skip dead objects */
		if (!o_ptr->k_idx)
			continue;

		/* Skip held objects */
		if (o_ptr->held_m_idx)
			continue;

		/* Forget the object */
		o_ptr->marked = FALSE;
	}

	/* Fully update the visuals */
	p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redraw whole map, monster list */
	p_ptr->redraw |= (PR_MAP | PR_MONLIST | PR_ITEMLIST);
}



/**
 * Light or Darken the world
 */
void illuminate(void)
{
	int y, x, i;

	if (stage_map[p_ptr->stage][STAGE_TYPE] == CAVE)
		return;

	/* Apply light or darkness */
	for (y = 0; y < DUNGEON_HGT; y++) {
		for (x = 0; x < DUNGEON_WID; x++) {
			/* Grids outside town walls */
			if ((cave->feat[y][x] == FEAT_PERM_SOLID) && !p_ptr->depth) {

				/* Darken and forget the grid */
				sqinfo_off(cave->info[y][x], SQUARE_GLOW);
				sqinfo_off(cave->info[y][x], SQUARE_MARK);
			}

			/* Special case of shops */
			else if (cave->feat[y][x] == FEAT_PERM_EXTRA) {
				/* Illuminate the grid */
				sqinfo_on(cave->info[y][x], SQUARE_GLOW);

				/* Memorize the grid */
				sqinfo_on(cave->info[y][x], SQUARE_MARK);
			}

			/* Viewable grids (light) */
			else if (is_daylight) {
				/* Illuminate the grid */
				sqinfo_on(cave->info[y][x], SQUARE_GLOW);
			}

			/* Viewable grids (dark) */
			else {
				/* Darken the grid */
				sqinfo_off(cave->info[y][x], SQUARE_GLOW);
			}
		}
	}


	/* Handle shop doorways */
	for (y = 0; y < DUNGEON_HGT; y++) {
		for (x = 0; x < DUNGEON_WID; x++) {
			feature_type *f_ptr = &f_info[cave->feat[y][x]];
			/* Track shop doorways */
			if (tf_has(f_ptr->flags, TF_SHOP)) {
				/* Illuminate and memorise the grid */
				sqinfo_on(cave->info[y][x], SQUARE_GLOW);
				sqinfo_on(cave->info[y][x], SQUARE_MARK);

				for (i = 0; i < 8; i++) {
					int yy = y + ddy_ddd[i];
					int xx = x + ddx_ddd[i];

					/* Illuminate and memorise the grid */
					sqinfo_on(cave->info[yy][xx], SQUARE_GLOW);
					sqinfo_on(cave->info[yy][xx], SQUARE_MARK);
				}
			}
		}
	}


	/* Fully update the visuals */
	p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redraw map, monster list */
	p_ptr->redraw |= (PR_MAP | PR_MONLIST | PR_ITEMLIST);
}


/**
 * Change the "feat" flag for a grid, and notice/redraw the grid. 
 */
void cave_set_feat(int y, int x, int feat)
{
	int current_feat = cave->feat[y][x];

	/* Track changes */
	if (current_feat) cave->feat_count[current_feat]--;
	if (feat) cave->feat_count[feat]++;

	/* Change the feature */
	cave->feat[y][x] = feat;

	/* Notice/Redraw */
	if (character_dungeon) {
		/* Notice */
		note_spot(y, x);

		/* Redraw */
		light_spot(y, x);
	}
}

bool square_in_bounds(struct cave *c, int y, int x)
{
	return x >= 0 && x < c->width && y >= 0 && y < c->height;
}

bool square_in_bounds_fully(struct cave *c, int y, int x)
{
	return x > 0 && x < c->width - 1 && y > 0 && y < c->height - 1;
}



/**
 * Determine the path taken by a projection.
 *
 * The projection will always start from the grid (y1,x1), and will travel
 * towards the grid (y2,x2), touching one grid per unit of distance along
 * the major axis, and stopping when it enters the destination grid or a
 * wall grid, or has travelled the maximum legal distance of "range".
 *
 * Note that "distance" in this function (as in the update_view() code)
 * is defined as "MAX(dy,dx) + MIN(dy,dx)/2", which means that the player
 * actually has an "octagon of projection" not a "circle of projection".
 *
 * The path grids are saved into the grid array pointed to by "gp", and
 * there should be room for at least "range" grids in "gp".  Note that
 * due to the way in which distance is calculated, this function normally
 * uses fewer than "range" grids for the projection path, so the result
 * of this function should never be compared directly to "range".  Note
 * that the initial grid (y1,x1) is never saved into the grid array, not
 * even if the initial grid is also the final grid.  XXX XXX XXX
 *
 * The "flg" flags can be used to modify the behavior of this function.
 *
 * In particular, the "PROJECT_STOP" and "PROJECT_THRU" flags have the same
 * semantics as they do for the "project" function, namely, that the path
 * will stop as soon as it hits a monster, or that the path will continue
 * through the destination grid, respectively.
 *
 * The "PROJECT_JUMP" flag, which for the project() function means to
 * start at a special grid (which makes no sense in this function), means
 * that the path should be "angled" slightly if needed to avoid any wall
 * grids, allowing the player to "target" any grid which is in "view".
 * This flag is non-trivial and has not yet been implemented, but could
 * perhaps make use of the "vinfo" array (above).  XXX XXX XXX
 *
 * This function returns the number of grids (if any) in the path.  This
 * function will return zero if and only if (y1,x1) and (y2,x2) are equal.
 *
 * This algorithm is similar to, but slightly different from, the one used
 * by update_view_los(), and very different from the one used by los().
 */
int project_path(u16b * gp, int range, int y1, int x1, int y2, int x2,
				 int flg)
{
	int y, x;

	int n = 0;
	int k = 0;

	/* Absolute */
	int ay, ax;

	/* Offsets */
	int sy, sx;

	/* Fractions */
	int frac;

	/* Scale factors */
	int full, half;

	/* Slope */
	int m;

	bool blocked = FALSE;

	/* No path necessary (or allowed) */
	if ((x1 == x2) && (y1 == y2))
		return (0);


	/* Analyze "dy" */
	if (y2 < y1) {
		ay = (y1 - y2);
		sy = -1;
	} else {
		ay = (y2 - y1);
		sy = 1;
	}

	/* Analyze "dx" */
	if (x2 < x1) {
		ax = (x1 - x2);
		sx = -1;
	} else {
		ax = (x2 - x1);
		sx = 1;
	}


	/* Number of "units" in one "half" grid */
	half = (ay * ax);

	/* Number of "units" in one "full" grid */
	full = half << 1;


	/* Vertical */
	if (ay > ax) {
		/* Start at tile edge */
		frac = ax * ax;

		/* Let m = ((dx/dy) * full) = (dx * dx * 2) = (frac * 2) */
		m = frac << 1;

		/* Start */
		y = y1 + sy;
		x = x1;

		/* Create the projection path */
		while (1) {
			/* Save grid */
			gp[n++] = GRID(y, x);

			/* Hack -- Check maximum range */
			if ((n + (k >> 1)) >= range)
				break;

			/* Sometimes stop at destination grid */
			if (!(flg & (PROJECT_THRU))) {
				if ((x == x2) && (y == y2))
					break;
			}

			/* Always stop at non-initial wall grids */
			if ((n > 0) && !cave_project(y, x))
				break;

			/* Sometimes stop at non-initial monsters/players */
			if (flg & (PROJECT_STOP)) {
				if ((n > 0) && (cave->m_idx[y][x] != 0))
					break;
			}

			/* Sometimes notice non-initial monsters/players */
			if (flg & (PROJECT_CHCK)) {
				if ((n > 0) && (cave->m_idx[y][x] != 0))
					blocked = TRUE;
			}

			/* Slant */
			if (m) {
				/* Advance (X) part 1 */
				frac += m;

				/* Horizontal change */
				if (frac >= half) {
					/* Advance (X) part 2 */
					x += sx;

					/* Advance (X) part 3 */
					frac -= full;

					/* Track distance */
					k++;
				}
			}

			/* Advance (Y) */
			y += sy;
		}
	}

	/* Horizontal */
	else if (ax > ay) {
		/* Start at tile edge */
		frac = ay * ay;

		/* Let m = ((dy/dx) * full) = (dy * dy * 2) = (frac * 2) */
		m = frac << 1;

		/* Start */
		y = y1;
		x = x1 + sx;

		/* Create the projection path */
		while (1) {
			/* Save grid */
			gp[n++] = GRID(y, x);

			/* Hack -- Check maximum range */
			if ((n + (k >> 1)) >= range)
				break;

			/* Sometimes stop at destination grid */
			if (!(flg & (PROJECT_THRU))) {
				if ((x == x2) && (y == y2))
					break;
			}

			/* Always stop at non-initial wall grids */
			if ((n > 0) && !cave_project(y, x))
				break;

			/* Sometimes stop at non-initial monsters/players */
			if (flg & (PROJECT_STOP)) {
				if ((n > 0) && (cave->m_idx[y][x] != 0))
					break;
			}

			/* Sometimes notice non-initial monsters/players */
			if (flg & (PROJECT_CHCK)) {
				if ((n > 0) && (cave->m_idx[y][x] != 0))
					blocked = TRUE;
			}

			/* Slant */
			if (m) {
				/* Advance (Y) part 1 */
				frac += m;

				/* Vertical change */
				if (frac >= half) {
					/* Advance (Y) part 2 */
					y += sy;

					/* Advance (Y) part 3 */
					frac -= full;

					/* Track distance */
					k++;
				}
			}

			/* Advance (X) */
			x += sx;
		}
	}

	/* Diagonal */
	else {
		/* Start */
		y = y1 + sy;
		x = x1 + sx;

		/* Create the projection path */
		while (1) {
			/* Save grid */
			gp[n++] = GRID(y, x);

			/* Hack -- Check maximum range */
			if ((n + (n >> 1)) >= range)
				break;

			/* Sometimes stop at destination grid */
			if (!(flg & (PROJECT_THRU))) {
				if ((x == x2) && (y == y2))
					break;
			}

			/* Always stop at non-initial wall grids */
			if ((n > 0) && !cave_project(y, x))
				break;

			/* Sometimes stop at non-initial monsters/players */
			if (flg & (PROJECT_STOP)) {
				if ((n > 0) && (cave->m_idx[y][x] != 0))
					break;
			}

			/* Sometimes notice non-initial monsters/players */
			if (flg & (PROJECT_CHCK)) {
				if ((n > 0) && (cave->m_idx[y][x] != 0))
					blocked = TRUE;
			}

			/* Advance (Y) */
			y += sy;

			/* Advance (X) */
			x += sx;
		}
	}


	/* Length */
	if (blocked)
		return (-n);
	else
		return (n);
}


/**
 * Determine if a bolt spell cast from (y1,x1) to (y2,x2) will arrive
 * at the final destination, using the project_path() function to check 
 * the projection path.
 *
 * Accept projection flags, and pass them onto project_path().
 *
 * Note that no grid is ever projectable() from itself.
 *
 * This function is used to determine if the player can (easily) target
 * a given grid, if a monster can target the player, and if a clear shot 
 * exists from monster to player.
 */
byte projectable(int y1, int x1, int y2, int x2, int flg)
{
	int y, x;

	int grid_n = 0;
	u16b grid_g[512];

	/* Check the projection path */
	grid_n = project_path(grid_g, MAX_RANGE, y1, x1, y2, x2, flg);

	/* No grid is ever projectable from itself */
	if (!grid_n)
		return (FALSE);

	/* Final grid.  As grid_n may be negative, use absolute value.  */
	y = GRID_Y(grid_g[ABS(grid_n) - 1]);
	x = GRID_X(grid_g[ABS(grid_n) - 1]);

	/* May not end in an unrequested grid */
	if ((y != y2) || (x != x2))
		return (PROJECT_NO);

	/* Must end in a passable grid. */
	if (!square_ispassable(cave, y, x))
		return (PROJECT_NO);


	/* Promise a clear bolt shot if we have verified that there is one */
	if ((flg & (PROJECT_STOP)) || (flg & (PROJECT_CHCK))) {
		/* Positive value for grid_n mean no obstacle was found. */
		if (grid_n > 0)
			return (PROJECT_CLEAR);
	}

	/* Assume projectable, but make no promises about clear shots */
	return (PROJECT_NOT_CLEAR);
}

/**
 * Standard "find me a location" function
 *
 * Obtains a legal location within the given distance of the initial
 * location, and with los() from the source to destination location.
 *
 * This function is often called from inside a loop which searches for
 * locations while increasing the "d" distance.
 *
 * Currently the "m" parameter is unused.
 */
void scatter(int *yp, int *xp, int y, int x, int d, int m)
{
	int nx, ny;

	/* Unused */
	m = m;

	/* Pick a location */
	while (TRUE) {
		/* Pick a new location */
		ny = rand_spread(y, d);
		nx = rand_spread(x, d);

		/* Ignore annoying locations */
		if (!square_in_bounds_fully(cave, y, x))
			continue;

		/* Ignore "excessively distant" locations */
		if ((d > 1) && (distance(y, x, ny, nx) > d))
			continue;

		/* Require "line of sight" */
		if (los(y, x, ny, nx))
			break;
	}

	/* Save the location */
	(*yp) = ny;
	(*xp) = nx;
}






/**
 * Track a new monster
 */
void health_track(int m_idx)
{
	/* Track a new guy */
	p_ptr->health_who = m_idx;

	/* Redraw (later) */
	p_ptr->redraw |= (PR_HEALTH | PR_MON_MANA);
}



/**
 * Hack -- track the given monster race
 */
void monster_race_track(int r_idx)
{
	/* Save this monster ID */
	p_ptr->monster_race_idx = r_idx;

	/* Window stuff */
	p_ptr->redraw |= (PR_MONSTER);
}



/**
 * Hack -- track the given object kind
 */
void track_object(int item)
{
	p_ptr->object_idx = item;
	p_ptr->object_kind_idx = 0;
	p_ptr->redraw |= (PR_OBJECT);
}

void track_object_kind(int k_idx)
{
	p_ptr->object_idx = 0;
	p_ptr->object_kind_idx = k_idx;
	p_ptr->redraw |= (PR_OBJECT);
}



/**
 * Something has happened to disturb the player.
 *
 * The first arg indicates a major disturbance, which affects search.
 *
 * The second arg is currently unused, but could induce output flush.
 *
 * All disturbance cancels repeated commands, resting, and running.
 */
void disturb(int stop_search, int unused_flag)
{
	/* Unused parameter */
	(void) unused_flag;

	/* Cancel repeated commands */
	cmd_cancel_repeat();

	/* Cancel Resting */
	if (p_ptr->resting) {
		/* Cancel */
		p_ptr->resting = 0;

		/* Redraw the state (later) */
		p_ptr->redraw |= (PR_STATE);
	}

	/* Cancel running */
	if (p_ptr->running) {
		/* Cancel */
		p_ptr->running = 0;

		/* Recenter the panel when running stops */
		if (OPT(center_player))
			verify_panel();

		/* Calculate torch radius */
		p_ptr->update |= (PU_TORCH);
	}

	/* Cancel searching if requested */
	if (stop_search && p_ptr->searching) {
		/* Cancel */
		p_ptr->searching = FALSE;

		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);

		/* Redraw the state */
		p_ptr->redraw |= (PR_STATE);
	}

	/* Flush the input */
	flush();
}


struct cave *cave = NULL;

struct cave *cave_new(void) {
	int y, x;

	struct cave *c = mem_zalloc(sizeof *c);
	c->feat_count = mem_zalloc((z_info->f_max + 1) * sizeof(int));
	c->info = mem_zalloc(DUNGEON_HGT * sizeof(bitflag**));
	c->feat = mem_zalloc(DUNGEON_HGT * sizeof(byte*));
	c->cost = mem_zalloc(DUNGEON_HGT * sizeof(byte*));
	c->when = mem_zalloc(DUNGEON_HGT * sizeof(byte*));
	c->m_idx = mem_zalloc(DUNGEON_HGT * sizeof(s16b*));
	c->o_idx = mem_zalloc(DUNGEON_HGT * sizeof(s16b*));
	for (y = 0; y < DUNGEON_HGT; y++){
		c->info[y] = mem_zalloc(DUNGEON_WID * sizeof(bitflag*));
		for (x = 0; x < DUNGEON_WID; x++)
			c->info[y][x] = mem_zalloc(SQUARE_SIZE * sizeof(bitflag));
		c->feat[y] = mem_zalloc(DUNGEON_WID * sizeof(byte));
		c->cost[y] = mem_zalloc(DUNGEON_WID * sizeof(byte));
		c->when[y] = mem_zalloc(DUNGEON_WID * sizeof(byte));
		c->m_idx[y] = mem_zalloc(DUNGEON_WID * sizeof(s16b));
		c->o_idx[y] = mem_zalloc(DUNGEON_WID * sizeof(s16b));
	}

	c->monsters = mem_zalloc(z_info->m_max * sizeof(struct monster));
	c->mon_max = 1;

	c->traps = mem_zalloc(z_info->l_max * sizeof(struct trap_type));
	c->trap_max = 1;
	c->trap_cnt = 0;

	c->created_at = 1;
	return c;
}

void cave_free(struct cave *c) {
	int y, x;
	for (y = 0; y < DUNGEON_HGT; y++){
		for (x = 0; x < DUNGEON_WID; x++)
			mem_free(c->info[y][x]);
		mem_free(c->info[y]);
		mem_free(c->feat[y]);
		mem_free(c->cost[y]);
		mem_free(c->when[y]);
		mem_free(c->m_idx[y]);
		mem_free(c->o_idx[y]);
	}
	mem_free(c->feat_count);
	mem_free(c->info);
	mem_free(c->feat);
	mem_free(c->cost);
	mem_free(c->when);
	mem_free(c->m_idx);
	mem_free(c->o_idx);
	mem_free(c->monsters);
	mem_free(c->traps);
	mem_free(c);
}

/**
 * FEATURE PREDICATES
 *
 * These functions are used to figure out what kind of square something is,
 * via c->feat[y][x]. All direct testing of c->feat[y][x] should be rewritten
 * in terms of these functions.
 *
 * It's often better to use feature behavior predicates (written in terms of
 * these functions) instead of these functions directly. For instance,
 * square_isrock() will return false for a secret door, even though it will
 * behave like a rock wall until the player determines it's a door.
 *
 * Use functions like square_isdiggable, square_iswall, etc. in these cases.
 */

/**
 * True if the square is normal open floor.
 */
bool square_isfloor(struct cave *c, int y, int x) {
	return tf_has(f_info[c->feat[y][x]].flags, TF_FLOOR);
}

/**
 * True if the square is a normal granite rock wall.
 */
bool square_isrock(struct cave *c, int y, int x) {
	return (tf_has(f_info[c->feat[y][x]].flags, TF_GRANITE) &&
			!tf_has(f_info[c->feat[y][x]].flags, TF_DOOR_ANY));
}

/**
 * True if the square is a permanent wall.
 */
bool square_isperm(struct cave *c, int y, int x) {
	return (tf_has(f_info[c->feat[y][x]].flags, TF_PERMANENT) &&
			tf_has(f_info[c->feat[y][x]].flags, TF_ROCK));
}

/**
 * True if the square is a magma wall.
 */
bool feat_is_magma(int feat)
{
	return tf_has(f_info[feat].flags, TF_MAGMA);
}

/**
 * True if the square is a magma wall.
 */
bool square_ismagma(struct cave *c, int y, int x) {
	return feat_is_magma(c->feat[y][x]);
}

/**
 * True if the square is a quartz wall.
 */
bool feat_is_quartz(int feat)
{
	return tf_has(f_info[feat].flags, TF_QUARTZ);
}

/**
 * True if the square is a quartz wall.
 */
bool square_isquartz(struct cave *c, int y, int x) {
	return feat_is_quartz(c->feat[y][x]);
}

/**
 * True if the square is a mineral wall (magma/quartz).
 */
bool square_ismineral(struct cave *c, int y, int x) {
	return square_isrock(c, y, x) || square_ismagma(c, y, x) || square_isquartz(c, y, x);
}

/**
 * True if the square is a mineral wall with treasure (magma/quartz).
 */
bool feat_is_treasure(int feat) {
	return (tf_has(f_info[feat].flags, TF_GOLD) &&
			tf_has(f_info[feat].flags, TF_INTERESTING));
}

/**
 * True if the square is rubble.
 */
bool square_isrubble(struct cave *c, int y, int x) {
    return (!tf_has(f_info[c->feat[y][x]].flags, TF_WALL) &&
			tf_has(f_info[c->feat[y][x]].flags, TF_ROCK));
}

/**
 * True if the square is a hidden secret door.
 *
 * These squares appear as if they were granite--when detected a secret door
 * is replaced by a closed door.
 */
bool square_issecretdoor(struct cave *c, int y, int x) {
    return (tf_has(f_info[c->feat[y][x]].flags, TF_DOOR_ANY) &&
			tf_has(f_info[c->feat[y][x]].flags, TF_ROCK));
}

/**
 * True if the square is an open door.
 */
bool square_isopendoor(struct cave *c, int y, int x) {
    return (tf_has(f_info[c->feat[y][x]].flags, TF_CLOSABLE));
}

/**
 * True if the square is a closed door (possibly locked or jammed).
 */
bool square_iscloseddoor(struct cave *c, int y, int x) {
	int feat = c->feat[y][x];
	return tf_has(f_info[feat].flags, TF_DOOR_CLOSED);
}

/**
 * True if the square is a closed, locked door.
 */
bool square_islockeddoor(struct cave *c, int y, int x) {
	int feat = c->feat[y][x];
	return (tf_has(f_info[feat].flags, TF_DOOR_LOCKED) ||
			tf_has(f_info[feat].flags, TF_DOOR_JAMMED));
}

/**
 * True if the square is a door.
 *
 * This includes open, closed, and hidden doors.
 */
bool square_isdoor(struct cave *c, int y, int x) {
	int feat = c->feat[y][x];
	return tf_has(f_info[feat].flags, TF_DOOR_ANY);
}

/**
 * True if the square is an unknown trap (it will appear as a floor tile).
 */
bool square_issecrettrap(struct cave *c, int y, int x) {
    return square_invisible_trap(c, y, x);
}

/**
 * True is the feature is a solid wall (not rubble).
 */
bool feat_is_wall(int feat) {
	return tf_has(f_info[feat].flags, TF_WALL);
}

/**
 * True if the square is a known trap.
 */
bool square_isknowntrap(struct cave *c, int y, int x) {
	return square_visible_trap(c, y, x);
}

/**
 * True if the square contains a trap, known or unknown.
 */
bool square_istrap(struct cave *c, int y, int x) {
	return square_issecrettrap(cave, y, x) || square_isknowntrap(cave, y, x);
}

/**
 * True if the feature is a shop entrance.
 */
bool feature_isshop(int feat) {
	return tf_has(f_info[feat].flags, TF_SHOP);
}

/**
 * True if square is any stair
 */
bool square_isstairs(struct cave*c, int y, int x) {
	int feat = c->feat[y][x];
	return tf_has(f_info[feat].flags, TF_STAIR);
}

/**
 * True if square is an up stair.
 */
bool square_isupstairs(struct cave*c, int y, int x) {
	int feat = c->feat[y][x];
	return tf_has(f_info[feat].flags, TF_UPSTAIR);
}

/**
 * True if square is a down stair.
 */
bool square_isdownstairs(struct cave *c, int y, int x) {
	int feat = c->feat[y][x];
	return tf_has(f_info[feat].flags, TF_DOWNSTAIR);
}

/**
 * True if the square is a shop entrance.
 */
bool square_isshop(struct cave *c, int y, int x) {
	return feature_isshop(c->feat[y][x]);
}

int square_shopnum(struct cave *c, int y, int x) {
	if (square_isshop(c, y, x))
		return c->feat[y][x] - FEAT_SHOP_HEAD;
	return -1;
}

/**
 * SQUARE BEHAVIOR PREDICATES
 *
 * These functions define how a given square behaves, e.g. whether it is
 * passable by the player, whether it is diggable, contains items, etc.
 *
 * These functions use the FEATURE PREDICATES (as well as c->info) to make
 * the determination.
 */

/**
 * True if the square is open (a floor square not occupied by a monster).
 */
bool square_isopen(struct cave *c, int y, int x) {
	return square_isfloor(c, y, x) && !c->m_idx[y][x];
}

/**
 * True if the square is empty (an open square without any items).
 */
bool square_isempty(struct cave *c, int y, int x) {
	return square_isopen(c, y, x) && !c->o_idx[y][x];
}

/**
 * True if the square is a floor square without items.
 */
bool square_canputitem(struct cave *c, int y, int x) {
	return square_isfloor(c, y, x) && !c->o_idx[y][x];
}

/**
 * True if the square can be dug: this includes rubble and non-permanent walls.
 */
bool square_isdiggable(struct cave *c, int y, int x) {
	return (square_ismineral(c, y, x) ||
			square_issecretdoor(c, y, x) || 
			square_isrubble(c, y, x));
}

/**
 * True if a monster can walk through the feature.
 */
bool feat_is_monster_walkable(feature_type *feature)
{
	return tf_has(feature->flags, TF_PASSABLE);
}

/**
 * True if a monster can walk through the tile.
 *
 * This is needed for polymorphing. A monster may be on a feature that isn't
 * an empty space, causing problems when it is replaced with a new monster.
 */
bool square_is_monster_walkable(struct cave *c, int y, int x)
{
	assert(square_in_bounds(c, y, x));
	return feat_is_monster_walkable(&f_info[c->feat[y][x]]);
}

/**
 * True if the feature is passable by the player.
 */
bool feat_ispassable(feature_type *f_ptr) {
	return tf_has(f_ptr->flags, TF_PASSABLE);
}

/**
 * True if the square is passable by the player.
 */
bool square_ispassable(struct cave *c, int y, int x) {
	assert(square_in_bounds(c, y, x));
	return feat_ispassable(&f_info[c->feat[y][x]]);
}

/**
 * True if any projectable can pass through the feature.
 */
bool feat_isprojectable(feature_type *f_ptr) {
	return tf_has(f_ptr->flags, TF_PROJECT);
}

/**
 * True if any projectable can pass through the square.
 *
 * This function is the logical negation of square_iswall().
 */
bool square_isprojectable(struct cave *c, int y, int x) {
	assert(square_in_bounds(c, y, x));
	return feat_isprojectable(&f_info[c->feat[y][x]]);
}

/**
 * True if the square is a wall square (impedes the player).
 *
 * This function is the logical negation of square_isprojectable().
 */
bool square_iswall(struct cave *c, int y, int x) {
	assert(square_in_bounds(c, y, x));
	return !square_isprojectable(c, y, x);
}

/**
 * True if the square is a permanent wall or one of the "stronger" walls.
 *
 * The stronger walls are granite, magma and quartz. This excludes things like
 * secret doors and rubble.
 */
bool square_isstrongwall(struct cave *c, int y, int x) {
	assert(square_in_bounds(c, y, x));
	return square_ismineral(c, y, x) || square_isperm(c, y, x);
}

/**
 * True if the square is part of a vault.
 *
 * This doesn't say what kind of square it is, just that it is part of a vault.
 */
bool square_isvault(struct cave *c, int y, int x) {
	assert(square_in_bounds(c, y, x));
	return sqinfo_has(c->info[y][x], SQUARE_ICKY);
}

/**
 * True if the square is part of a room.
 */
bool square_isroom(struct cave *c, int y, int x) {
	assert(square_in_bounds(c, y, x));
	return sqinfo_has(c->info[y][x], SQUARE_ROOM);
}

/* True if the cave square is viewable */
bool square_isview(struct cave *c, int y, int x) {
	assert(square_in_bounds(c, y, x));
	return sqinfo_has(c->info[y][x], SQUARE_VIEW);
}

bool square_isseen(struct cave *c, int y, int x) {
	assert(square_in_bounds(c, y, x));
	return sqinfo_has(c->info[y][x], SQUARE_SEEN);
}

bool square_wasseen(struct cave *c, int y, int x) {
	assert(square_in_bounds(c, y, x));
	return sqinfo_has(c->info[y][x], SQUARE_TEMP);
}

bool square_isglow(struct cave *c, int y, int x) {
	assert(square_in_bounds(c, y, x));
	return sqinfo_has(c->info[y][x], SQUARE_GLOW);
}

/**
 * True if the feature is "boring".
 */
bool feat_isboring(feature_type *f_ptr) {
	return !tf_has(f_ptr->flags, TF_INTERESTING);
}

/**
 * True if the cave square is "boring".
 */
bool square_isboring(struct cave *c, int y, int x) {
	assert(square_in_bounds(c, y, x));
	return feat_isboring(&f_info[c->feat[y][x]]);
}

/**
 * Get a monster on the current level by its index.
 */
struct monster *cave_monster(struct cave *c, int idx) {
	return &c->monsters[idx];
}

/**
 * Get a monster on the current level by its position.
 */
struct monster *square_monster(struct cave *c, int y, int x) {
	if (c->m_idx[y][x] > 0) {
		struct monster *mon = cave_monster(c, c->m_idx[y][x]);
		return &r_info[mon->r_idx] ? mon : NULL;
	}

	return NULL;
}

/**
 * The maximum number of monsters allowed in the level.
 */
int cave_monster_max(struct cave *c) {
	return c->mon_max;
}

/**
 * The current number of monsters present on the level.
 */
int cave_monster_count(struct cave *c) {
	return c->mon_cnt;
}

/**
 * Add visible treasure to a mineral square.
 */
void upgrade_mineral(struct cave *c, int y, int x) {
	switch (c->feat[y][x]) {
	case FEAT_MAGMA: cave_set_feat(y, x, FEAT_MAGMA_K); break;
	case FEAT_QUARTZ: cave_set_feat(y, x, FEAT_QUARTZ_K); break;
	}
}

int square_door_power(struct cave *c, int y, int x) {
	return (c->feat[y][x] - FEAT_DOOR_HEAD) & 0x07;
}

void square_open_door(struct cave *c, int y, int x) {
	cave_set_feat(y, x, FEAT_OPEN);
}

void square_smash_door(struct cave *c, int y, int x) {
	cave_set_feat(y, x, FEAT_BROKEN);
}

void square_destroy_trap(struct cave *c, int y, int x) {
	square_remove_trap(cave, y, x, FALSE, -1);
}

void square_lock_door(struct cave *c, int y, int x, int power) {
	cave_set_feat(y, x, FEAT_DOOR_HEAD + power);
}

bool square_hasgoldvein(struct cave *c, int y, int x) {
	return tf_has(f_info[c->feat[y][x]].flags, TF_GOLD);
}

void square_tunnel_wall(struct cave *c, int y, int x) {
	cave_set_feat(y, x, FEAT_FLOOR);
}

void square_destroy_wall(struct cave *c, int y, int x) {
	cave_set_feat(y, x, FEAT_FLOOR);
}

void square_close_door(struct cave *c, int y, int x) {
	cave_set_feat(y, x, FEAT_DOOR_HEAD);
}

bool square_isbrokendoor(struct cave *c, int y, int x) {
	int feat = c->feat[y][x];
    return (tf_has(f_info[feat].flags, TF_DOOR_ANY) &&
			tf_has(f_info[feat].flags, TF_PASSABLE) &&
			!tf_has(f_info[feat].flags, TF_CLOSABLE));
}

void square_add_trap(struct cave *c, int y, int x) {
	place_trap(c, y, x, -1, c->depth);
}

bool square_iswarded(struct cave *c, int y, int x) {
	return square_trap_specific(c, y, x, RUNE_PROTECT);
}

void square_add_ward(struct cave *c, int y, int x) {
	place_trap(c, y, x, RUNE_PROTECT, 0);
}

void square_remove_ward(struct cave *c, int y, int x) {
	assert(square_iswarded(c, y, x));
	square_remove_trap_kind(c, y, x, TRUE, RUNE_PROTECT);
}

bool square_canward(struct cave *c, int y, int x) {
	return square_isfloor(c, y, x);
}

bool square_seemslikewall(struct cave *c, int y, int x) {
	return tf_has(f_info[c->feat[y][x]].flags, TF_ROCK);
}

bool square_isinteresting(struct cave *c, int y, int x) {
	int f = c->feat[y][x];
	return tf_has(f_info[f].flags, TF_INTERESTING);
}

void square_show_vein(struct cave *c, int y, int x) {
	if (c->feat[y][x] == FEAT_MAGMA_H)
		cave_set_feat(y, x, FEAT_MAGMA_K);
	else if (c->feat[y][x] == FEAT_QUARTZ_H)
		cave_set_feat(y, x, FEAT_QUARTZ_K);
}

//void square_add_stairs(struct cave *c, int y, int x, int depth) {
//	int down = randint0(100) < 50;
//	if (depth == 0)
//		down = 1;
//	else if (is_quest(depth) || depth >= MAX_DEPTH - 1)
//		down = 0;
//	cave_set_feat(y, x, down ? FEAT_MORE : FEAT_LESS);
//}

void square_destroy(struct cave *c, int y, int x) {
int feat = FEAT_FLOOR;
	int r = randint0(200);

	if (r < 20)
		feat = FEAT_WALL_EXTRA;
	else if (r < 70)
		feat = FEAT_QUARTZ;
	else if (r < 100)
		feat = FEAT_MAGMA;

	cave_set_feat(y, x, feat);
}

void square_earthquake(struct cave *c, int y, int x) {
	int t = randint0(100);
	int f;

	if (!square_ispassable(c, y, x)) {
		cave_set_feat(y, x, FEAT_FLOOR);
		return;
	}

	if (t < 20)
		f = FEAT_WALL_EXTRA;
	else if (t < 70)
		f = FEAT_QUARTZ;
	else
		f = FEAT_MAGMA;
	cave_set_feat(y, x, f);
}

bool square_hassecretvein(struct cave *c, int y, int x) {
	return (tf_has(f_info[c->feat[y][x]].flags, TF_GOLD) &&
			!tf_has(f_info[c->feat[y][x]].flags, TF_INTERESTING));
}

bool square_noticeable(struct cave *c, int y, int x) {
	return tf_has(f_info[c->feat[y][x]].flags, TF_INTERESTING);
}

const char *square_apparent_name(struct cave *c, struct player *p, int y, int x) {
	int f = f_info[c->feat[y][x]].mimic;

	if (!sqinfo_has(c->info[y][x], SQUARE_MARK) && !player_can_see_bold(y, x))
		f = FEAT_NONE;

	if (f == FEAT_NONE)
		return "unknown_grid";

	return f_info[f].name;
}

void square_unlock_door(struct cave *c, int y, int x) {
	assert(square_islockeddoor(c, y, x));
	cave_set_feat(y, x, FEAT_DOOR_HEAD);
}

void square_destroy_door(struct cave *c, int y, int x) {
	assert(square_isdoor(c, y, x));
	cave_set_feat(y, x, FEAT_FLOOR);
}

void square_destroy_rubble(struct cave *c, int y, int x) {
	assert(square_isrubble(c, y, x));
	cave_set_feat(y, x, FEAT_FLOOR);
}

void square_add_door(struct cave *c, int y, int x, bool closed) {
	cave_set_feat(y, x, closed ? FEAT_DOOR_HEAD : FEAT_OPEN);
}

void square_force_floor(struct cave *c, int y, int x) {
	cave_set_feat(y, x, FEAT_FLOOR);
}

/*
 * Return the number of doors/traps around (or under) the character.
 */
int count_feats(int *y, int *x, bool (*test)(struct cave *cave, int y, int x), bool under)
{
int d;
	int xx, yy;
	int count = 0; /* Count how many matches */

	/* Check around (and under) the character */
	for (d = 0; d < 9; d++)
	{
		/* if not searching under player continue */
		if ((d == 8) && !under) continue;

		/* Extract adjacent (legal) location */
		yy = p_ptr->py + ddy_ddd[d];
		xx = p_ptr->px + ddx_ddd[d];

		/* Paranoia */
		if (!square_in_bounds_fully(cave, yy, xx)) continue;

		/* Must have knowledge */
		if (!sqinfo_has(cave->info[yy][xx], SQUARE_MARK)) continue;

		/* Not looking for this feature */
		if (!((*test)(cave, yy, xx))) continue;

		/* Count it */
		++count;

		/* Remember the location of the last door found */
		*y = yy;
		*x = xx;
	}

	/* All done */
	return count;
}
