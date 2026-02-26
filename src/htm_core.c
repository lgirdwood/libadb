/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Copyright (C) 2010 - 2014 Liam Girdwood
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h> // IWYU pragma: keep
#include <math.h>
#include <stdio.h> // IWYU pragma: keep
#include <stdlib.h>
#include <string.h> // IWYU pragma: keep
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "debug.h"
#include "private.h" // IWYU pragma: keep
#include "table.h" // IWYU pragma: keep
#include "libastrodb/db.h"
#include "libastrodb/object.h" // IWYU pragma: keep

#if 1
#define vdebug(x...) printf(x)
#else
#define vdebug(x...)
#endif

#if 1
#define fdebug(x...) printf(x)
#else
#define fdebug(x...)
#endif

static void trixel_create_down(struct htm *htm, struct htm_trixel *parent,
							   int depth, int level, int hemisphere);
static void trixel_create_up(struct htm *htm, struct htm_trixel *parent,
							 int depth, int level, int hemisphere);

static const double htm_resolution[] = {
	M_PI_2,			M_PI_2 / 2.0,	M_PI_2 / 4.0,	 M_PI_2 / 8.0,
	M_PI_2 / 16.0,	M_PI_2 / 32.0,	M_PI_2 / 64.0,	 M_PI_2 / 128.0,
	M_PI_2 / 256.0, M_PI_2 / 512.0, M_PI_2 / 1024.0, M_PI_2 / 2048.0,
};

static struct htm_trixel *trixel_get_from_id_(struct htm *htm, unsigned int id,
											  struct htm_trixel *t, int depth)
{
	int pos;

	if (++depth > htm_trixel_depth(id))
		return t;

	if (depth >= HTM_MAX_DEPTH)
		return NULL;

	pos = htm_trixel_position(id, depth);

	return trixel_get_from_id_(htm, id, &t->child[pos], depth);
}

struct htm_trixel *htm_get_trixel(struct htm *htm, unsigned int id)
{
	int quad = htm_trixel_quadrant(id);

	if (htm_trixel_north(id))
		return trixel_get_from_id_(htm, id, &htm->N[quad], 0);
	else
		return trixel_get_from_id_(htm, id, &htm->S[quad], 0);
}

static int dec_strip_init(struct htm *htm)
{
	struct dec_strip *dec_strip;
	double dec_step_size;
	int num_ra_steps, i;

	/* calc domain size and DEC granularity */
	htm->dec_strip_count = (1 << (htm->depth + 1)) + 1;
	dec_step_size = 2.0 / (htm->dec_strip_count - 1);

	dec_strip = calloc(htm->dec_strip_count, sizeof(struct dec_strip));
	if (dec_strip == NULL)
		return -ENOMEM;
	htm->dec_step = dec_step_size;
	htm->dec = dec_strip;

	/* create DEC strip starting at -1 ... 0 ... 1 */
	for (i = 0; i < htm->dec_strip_count; i++) {
		if (i <= (htm->dec_strip_count >> 1)) {
			/* southern hemisphere <= 0 */
			num_ra_steps = i * 4;
			dec_strip->width = (i * dec_step_size);
		} else {
			/* norther hemisphere > 0 */
			num_ra_steps = (htm->dec_strip_count - i - 1) * 4;
			dec_strip->width = ((htm->dec_strip_count - i - 1) * dec_step_size);
		}

		/* we have 1 RA step at poles */
		if (num_ra_steps == 0)
			num_ra_steps = 1;

		/* domain RA granularity */
		dec_strip->half_size = num_ra_steps / 2;
		dec_strip->vertex_count = num_ra_steps;

		dec_strip->vertex = calloc(num_ra_steps, sizeof(struct htm_vertex));
		dec_strip++;
	}
	return 0;
}

/* free trixel and it's children */
static void free_trixel(struct htm_trixel *t)
{
	struct htm_trixel *child = t->child;

	if (child) {
		free_trixel(&child[0]);
		free_trixel(&child[1]);
		free_trixel(&child[2]);
		free_trixel(&child[3]);
	}
	free(child);
}

void htm_free(struct htm *htm)
{
	struct dec_strip *dec_strip = htm->dec;
	struct htm_vertex *v;
	int i, j;

	/* free verticies and trixel maps */
	for (i = 0; i < htm->dec_strip_count; i++) {
		for (j = 0; j < dec_strip->vertex_count; j++) {
			v = dec_strip->vertex + j;
			free(v->trixel);
		}
		free(dec_strip->vertex);
		dec_strip++;
	}

	/* free trixels */
	free_trixel(&htm->N[0]);
	free_trixel(&htm->N[1]);
	free_trixel(&htm->N[2]);
	free_trixel(&htm->N[3]);
	free_trixel(&htm->S[0]);
	free_trixel(&htm->S[1]);
	free_trixel(&htm->S[2]);
	free_trixel(&htm->S[3]);

	free(htm->dec);
	free(htm);
}

static void vertex_init_radec(struct htm_vertex *v)
{
	double cos_dec, x, y, z;

	/* convert (x,y,z) from poly to to spherical */
	if (v->x < -0)
		x = -sqrt(-v->x);
	else
		x = sqrt(v->x);
	if (v->y < -0)
		y = -sqrt(-v->y);
	else
		y = sqrt(v->y);
	if (v->z < -0)
		z = -sqrt(-v->z);
	else
		z = sqrt(v->z);

	/* calc Declination */
	v->dec = asin(y);
	cos_dec = cos(v->dec);

	/* calc RA */
	if (cos_dec > 1e-5 || cos_dec < -1e-5) {
		if (x > 1e-5 || x < -1e-5) {
			if (x < 0.0)
				v->ra = 2.0 * M_PI - acos(z / cos_dec);
			else
				v->ra = acos(z / cos_dec);
		} else {
			v->ra = (z < 0.0 ? M_PI : 0.0);
		}
	} else
		v->ra = 0.0;
}

/* lookup vertex based on position from dec domain */
struct htm_vertex *vertex_get(struct htm *htm, int level, double x, double y,
							  double z)
{
	struct dec_strip *dec_strip;
	struct htm_vertex *v;
	int y_pos, x_pos, num_trixels;

	/* calc Y position and get Y domain */
	y_pos = round((y + 1.0) / htm->dec_step);
	if (y_pos < 0 || y_pos >= htm->dec_strip_count)
		return NULL;
	dec_strip = htm->dec + y_pos;

	/* calc X position and whether we are Pole */
	if (y_pos == 0 || y_pos == htm->dec_strip_count - 1)
		x_pos = 0; /* pole */
	else {
		x_pos = round((x + dec_strip->width) / htm->dec_step);
		if (z < -1e-5)
			x_pos += dec_strip->half_size; /* neg Z values */
	}

	/* get vertex */
	v = dec_strip->vertex + x_pos;
	if (v->trixel)
		return v;

	/* vertex is new so set positional data */
	v->x = x;
	v->y = y;
	v->z = z;

	/* init vertex spherical RA and DEC */
	vertex_init_radec(v);

	/* allocate trixel pointers for associated trixels */
	num_trixels = TRIXELS_PER_VERTEX * ((htm->depth - level) + 1);
	v->trixel = calloc(num_trixels, sizeof(struct htm_trixel *));

	/* depth we are created at */
	v->depth = level;
	htm->vertex_count++;
	return v;
}

static inline void vertex_get_midpoint(struct htm_vertex *a,
									   struct htm_vertex *b, double *x,
									   double *y, double *z)
{
	*x = ((a->x + b->x) / 2.0);
	*y = ((a->y + b->y) / 2.0);
	*z = ((a->z + b->z) / 2.0);
}

/* calculate child verticies for up trixels */
static inline void trixel_init_child_verticies(struct htm *htm,
											   struct htm_trixel *parent,
											   struct htm_vertex **a,
											   struct htm_vertex **b,
											   struct htm_vertex **c, int level)
{
	double x, y, z;

	/* vertex a */
	vertex_get_midpoint(parent->c, parent->b, &x, &y, &z);
	*a = vertex_get(htm, level, x, y, z);

	/* vertex b */
	vertex_get_midpoint(parent->a, parent->b, &x, &y, &z);
	*b = vertex_get(htm, level, x, y, z);

	/* vertex c */
	vertex_get_midpoint(parent->c, parent->a, &x, &y, &z);
	*c = vertex_get(htm, level, x, y, z);
}

static inline void vertex_assoc_trixel_a(struct htm *htm, struct htm_trixel *t,
										 struct htm_vertex *v, int level)
{
	int group_index, i;

	group_index = (level - v->depth) * TRIXELS_PER_VERTEX;
	t->a = v;

	for (i = group_index; i < group_index + TRIXELS_PER_VERTEX; i++) {
		if (v->trixel[i] == NULL) {
			v->trixel[i] = t;
			return;
		}
	}
	adb_htm_error(htm, "vertex %f:%f at level %d overflow\n", v->ra * R2D,
				  v->dec * R2D, level);
	adb_htm_error(htm, "  x %f y %f z %f\n", v->x, v->y, v->z);
}

static inline void vertex_assoc_trixel_b(struct htm *htm, struct htm_trixel *t,
										 struct htm_vertex *v, int level)
{
	int group_index, i;

	group_index = (level - v->depth) * TRIXELS_PER_VERTEX;
	t->b = v;

	for (i = group_index; i < group_index + TRIXELS_PER_VERTEX; i++) {
		if (v->trixel[i] == NULL) {
			v->trixel[i] = t;
			return;
		}
	}
	adb_htm_error(htm, "vertex %f:%f at level %d overflow\n", v->ra * R2D,
				  v->dec * R2D, level);
	adb_htm_error(htm, "  x %f y %f z %f\n", v->x, v->y, v->z);
}

static inline void vertex_assoc_trixel_c(struct htm *htm, struct htm_trixel *t,
										 struct htm_vertex *v, int level)
{
	int group_index, i;

	group_index = (level - v->depth) * TRIXELS_PER_VERTEX;
	t->c = v;

	for (i = group_index; i < group_index + TRIXELS_PER_VERTEX; i++) {
		if (v->trixel[i] == NULL) {
			v->trixel[i] = t;
			return;
		}
	}
	adb_htm_error(htm, "vertex %f:%f at level %d overflow\n", v->ra * R2D,
				  v->dec * R2D, level);
	adb_htm_error(htm, "  x %f y %f z %f\n", v->x, v->y, v->z);
}

/* t0 is middle */
static inline void trixel_0_parent_up(struct htm *htm, struct htm_trixel *t0,
									  struct htm_vertex *a,
									  struct htm_vertex *b,
									  struct htm_vertex *c, int level)
{
	/* a is bottom middle B - C */
	vertex_assoc_trixel_a(htm, t0, a, level);

	/* b is top left B - A */
	vertex_assoc_trixel_b(htm, t0, b, level);

	/* c is top right C - A */
	vertex_assoc_trixel_c(htm, t0, c, level);
}

/* t1 is top */
static inline void
trixel_1_parent_up(struct htm *htm, struct htm_trixel *parent,
				   struct htm_trixel *t1, struct htm_vertex *a,
				   struct htm_vertex *b, struct htm_vertex *c, int level)
{
	/* a is parent A */
	vertex_assoc_trixel_a(htm, t1, parent->a, level);

	/* b is bottom left B - A */
	vertex_assoc_trixel_b(htm, t1, b, level);

	/* c is bottom right C - A */
	vertex_assoc_trixel_c(htm, t1, c, level);
}

/* t2 is bottom left */
static inline void
trixel_2_parent_up(struct htm *htm, struct htm_trixel *parent,
				   struct htm_trixel *t2, struct htm_vertex *a,
				   struct htm_vertex *b, struct htm_vertex *c, int level)
{
	/* a is top vertex b */
	vertex_assoc_trixel_a(htm, t2, b, level);

	/* b is bottom left - parent B */
	vertex_assoc_trixel_b(htm, t2, parent->b, level);

	/* c is bottom right - vertex a */
	vertex_assoc_trixel_c(htm, t2, a, level);
}

/* t3 is bottom right */
static inline void
trixel_3_parent_up(struct htm *htm, struct htm_trixel *parent,
				   struct htm_trixel *t3, struct htm_vertex *a,
				   struct htm_vertex *b, struct htm_vertex *c, int level)
{
	/* a is top vertex c */
	vertex_assoc_trixel_a(htm, t3, c, level);

	/* b is bottom left - vertex a */
	vertex_assoc_trixel_b(htm, t3, a, level);

	/* c is bottom right - parent C */
	vertex_assoc_trixel_c(htm, t3, parent->c, level);
}

static inline void trixel_0_parent_down(struct htm *htm, struct htm_trixel *t0,
										struct htm_vertex *a,
										struct htm_vertex *b,
										struct htm_vertex *c, int level)
{
	/* a is top middle B - C */
	vertex_assoc_trixel_a(htm, t0, a, level);

	/* b is bottom left B - A */
	vertex_assoc_trixel_b(htm, t0, b, level);

	/* c is bottom right C - A */
	vertex_assoc_trixel_c(htm, t0, c, level);
}

static inline void
trixel_1_parent_down(struct htm *htm, struct htm_trixel *parent,
					 struct htm_trixel *t1, struct htm_vertex *a,
					 struct htm_vertex *b, struct htm_vertex *c, int level)
{
	/* a is parent A */
	vertex_assoc_trixel_a(htm, t1, parent->a, level);

	/* b is bottom left B - A */
	vertex_assoc_trixel_b(htm, t1, b, level);

	/* c is bottom right C - A */
	vertex_assoc_trixel_c(htm, t1, c, level);
}

static inline void
trixel_2_parent_down(struct htm *htm, struct htm_trixel *parent,
					 struct htm_trixel *t2, struct htm_vertex *a,
					 struct htm_vertex *b, struct htm_vertex *c, int level)
{
	/* a is top vertex b */
	vertex_assoc_trixel_a(htm, t2, b, level);

	/* b is top left - parent B */
	vertex_assoc_trixel_b(htm, t2, parent->b, level);

	/* c is top right - vertex a */
	vertex_assoc_trixel_c(htm, t2, a, level);
}

static inline void
trixel_3_parent_down(struct htm *htm, struct htm_trixel *parent,
					 struct htm_trixel *t3, struct htm_vertex *a,
					 struct htm_vertex *b, struct htm_vertex *c, int level)
{
	/* a is top vertex c */
	vertex_assoc_trixel_a(htm, t3, c, level);

	/* b is top left - vertex a */
	vertex_assoc_trixel_b(htm, t3, a, level);

	/* c is top right - parent C */
	vertex_assoc_trixel_c(htm, t3, parent->c, level);
}

/* create up trixel children */
static void trixel_create_up(struct htm *htm, struct htm_trixel *parent,
							 int depth, int level, int hemisphere)
{
	struct htm_trixel *child;
	struct htm_vertex *a, *b, *c;

	/* do we need to recurse into children */
	if (level++ >= depth)
		return;

	parent->child = calloc(NUM_CHILD_TRIXELS, sizeof(struct htm_trixel));
	assert(parent->child);
	child = parent->child;
	htm->trixel_count += NUM_CHILD_TRIXELS;

	/* calculate mid triangle vertex */
	trixel_init_child_verticies(htm, parent, &a, &b, &c, level);

	/* create child triangles */
	trixel_0_parent_up(htm, &child[0], a, b, c, level);
	trixel_1_parent_up(htm, parent, &child[1], a, b, c, level);
	trixel_2_parent_up(htm, parent, &child[2], a, b, c, level);
	trixel_3_parent_up(htm, parent, &child[3], a, b, c, level);

	/* assign parent */
	child[0].parent = parent;
	child[1].parent = parent;
	child[2].parent = parent;
	child[3].parent = parent;

	/* child 0 is down */
	child[0].orientation = TRIXEL_DOWN;
	child[0].hemisphere = hemisphere;
	child[0].quadrant = parent->quadrant;
	child[0].depth = level;
	child[0].position = parent->position;
	trixel_create_down(htm, &child[0], depth, level, hemisphere);

	/* children 1 - 3 are up */
	child[1].orientation = TRIXEL_UP;
	child[1].hemisphere = hemisphere;
	child[1].quadrant = parent->quadrant;
	child[1].position = parent->position | (1 << (level << 1));
	child[1].depth = level;
	trixel_create_up(htm, &child[1], depth, level, hemisphere);

	child[2].orientation = TRIXEL_UP;
	child[2].hemisphere = hemisphere;
	child[2].quadrant = parent->quadrant;
	child[2].position = parent->position | (2 << (level << 1));
	child[2].depth = level;
	trixel_create_up(htm, &child[2], depth, level, hemisphere);

	child[3].orientation = TRIXEL_UP;
	child[3].hemisphere = hemisphere;
	child[3].quadrant = parent->quadrant;
	child[3].position = parent->position | (3 << (level << 1));
	child[3].depth = level;
	trixel_create_up(htm, &child[3], depth, level, hemisphere);
}

/* create down trixel children */
static void trixel_create_down(struct htm *htm, struct htm_trixel *parent,
							   int depth, int level, int hemisphere)
{
	struct htm_trixel *child;
	struct htm_vertex *a, *b, *c;

	/* do we need to recurse into children */
	if (level++ >= depth)
		return;

	parent->child = calloc(NUM_CHILD_TRIXELS, sizeof(struct htm_trixel));
	assert(parent->child);
	child = parent->child;
	htm->trixel_count += NUM_CHILD_TRIXELS;

	/* calculate mid triangle vertex */
	trixel_init_child_verticies(htm, parent, &a, &b, &c, level);

	/* create child triangles */
	trixel_0_parent_down(htm, &child[0], a, b, c, level);
	trixel_1_parent_down(htm, parent, &child[1], a, b, c, level);
	trixel_2_parent_down(htm, parent, &child[2], a, b, c, level);
	trixel_3_parent_down(htm, parent, &child[3], a, b, c, level);

	/* assign parent */
	child[0].parent = parent;
	child[1].parent = parent;
	child[2].parent = parent;
	child[3].parent = parent;

	/* child 0 is up */
	child[0].orientation = TRIXEL_UP;
	child[0].hemisphere = hemisphere;
	child[0].quadrant = parent->quadrant;
	child[0].depth = level;
	child[0].position = parent->position;
	trixel_create_up(htm, &child[0], depth, level, hemisphere);

	/* children 1 - 3 are down */
	child[1].orientation = TRIXEL_DOWN;
	child[1].hemisphere = hemisphere;
	child[1].quadrant = parent->quadrant;
	child[1].depth = level;
	child[1].position = parent->position | (1 << (level << 1));
	trixel_create_down(htm, &child[1], depth, level, hemisphere);

	child[2].orientation = TRIXEL_DOWN;
	child[2].hemisphere = hemisphere;
	child[2].quadrant = parent->quadrant;
	child[2].depth = level;
	child[2].position = parent->position | (2 << (level << 1));
	trixel_create_down(htm, &child[2], depth, level, hemisphere);

	child[3].orientation = TRIXEL_DOWN;
	child[3].hemisphere = hemisphere;
	child[3].quadrant = parent->quadrant;
	child[3].depth = level;
	child[3].position = parent->position | (3 << (level << 1));
	trixel_create_down(htm, &child[3], depth, level, hemisphere);
}

/*! \struct polyhedron
 * \brief polyhedron
 * \ingroup htm
 */
struct polyhedron {
	double x; /*!< x coordinate */
	double y; /*!< y coordinate */
	double z; /*!< z coordinate */
};

static struct polyhedron poly[] = {
	{ 0.0, 1.0, 0.0 },	{ 0.0, 0.0, 1.0 },	{ 1.0, 0.0, 0.0 },
	{ 0.0, 0.0, -1.0 }, { -1.0, 0.0, 0.0 }, { 0.0, -1.0, 0.0 },
};

/* create and init new HTM */
struct htm *htm_new(int depth, int tables)
{
	struct htm *htm;
	struct htm_trixel *t;

	htm = calloc(1, sizeof(struct htm));
	if (htm == NULL)
		return NULL;
	htm->depth = depth;
	htm->trixel_count = 8;

	/* create and init DEC domain */
	dec_strip_init(htm);

	/* create northern hemisphere trixels and verticies */
	t = &htm->N[0];
	t->a = vertex_get(htm, 0, poly[0].x, poly[0].y, poly[0].z);
	t->b = vertex_get(htm, 0, poly[1].x, poly[1].y, poly[1].z);
	t->c = vertex_get(htm, 0, poly[2].x, poly[2].y, poly[2].z);
	t->orientation = TRIXEL_UP;
	t->hemisphere = HEMI_NORTH;
	t->quadrant = 0;
	t->position = 0;
	t->depth = 0;
	trixel_0_parent_up(htm, t, t->a, t->b, t->c, 0);
	trixel_create_up(htm, &htm->N[0], depth, 0, HEMI_NORTH);

	t = &htm->N[1];
	t->a = vertex_get(htm, 0, poly[0].x, poly[0].y, poly[0].z);
	t->b = vertex_get(htm, 0, poly[2].x, poly[2].y, poly[2].z);
	t->c = vertex_get(htm, 0, poly[3].x, poly[3].y, poly[3].z);
	t->orientation = TRIXEL_UP;
	t->hemisphere = HEMI_NORTH;
	t->quadrant = 1;
	t->position = 0;
	t->depth = 0;
	trixel_0_parent_up(htm, t, t->a, t->b, t->c, 0);
	trixel_create_up(htm, &htm->N[1], depth, 0, HEMI_NORTH);

	t = &htm->N[2];
	t->a = vertex_get(htm, 0, poly[0].x, poly[0].y, poly[0].z);
	t->b = vertex_get(htm, 0, poly[3].x, poly[3].y, poly[3].z);
	t->c = vertex_get(htm, 0, poly[4].x, poly[4].y, poly[4].z);
	t->orientation = TRIXEL_UP;
	t->hemisphere = HEMI_NORTH;
	t->quadrant = 2;
	t->position = 0;
	t->depth = 0;
	trixel_0_parent_up(htm, t, t->a, t->b, t->c, 0);
	trixel_create_up(htm, &htm->N[2], depth, 0, HEMI_NORTH);

	t = &htm->N[3];
	t->a = vertex_get(htm, 0, poly[0].x, poly[0].y, poly[0].z);
	t->b = vertex_get(htm, 0, poly[4].x, poly[4].y, poly[4].z);
	t->c = vertex_get(htm, 0, poly[1].x, poly[1].y, poly[1].z);
	t->orientation = TRIXEL_UP;
	t->hemisphere = HEMI_NORTH;
	t->quadrant = 3;
	t->position = 0;
	t->depth = 0;
	trixel_0_parent_up(htm, t, t->a, t->b, t->c, 0);
	trixel_create_up(htm, &htm->N[3], depth, 0, HEMI_NORTH);

	/* create southern hemisphere trixels and verticies */
	t = &htm->S[0];
	t->a = vertex_get(htm, 0, poly[5].x, poly[5].y, poly[5].z);
	t->b = vertex_get(htm, 0, poly[1].x, poly[1].y, poly[1].z);
	t->c = vertex_get(htm, 0, poly[2].x, poly[2].y, poly[2].z);
	t->orientation = TRIXEL_DOWN;
	t->hemisphere = HEMI_SOUTH;
	t->quadrant = 0;
	t->position = 0;
	t->depth = 0;
	trixel_0_parent_down(htm, t, t->a, t->b, t->c, 0);
	trixel_create_down(htm, &htm->S[0], depth, 0, HEMI_SOUTH);

	t = &htm->S[1];
	t->a = vertex_get(htm, 0, poly[5].x, poly[5].y, poly[5].z);
	t->b = vertex_get(htm, 0, poly[2].x, poly[2].y, poly[2].z);
	t->c = vertex_get(htm, 0, poly[3].x, poly[3].y, poly[3].z);
	t->orientation = TRIXEL_DOWN;
	t->hemisphere = HEMI_SOUTH;
	t->quadrant = 1;
	t->position = 0;
	t->depth = 0;
	trixel_0_parent_down(htm, t, t->a, t->b, t->c, 0);
	trixel_create_down(htm, &htm->S[1], depth, 0, HEMI_SOUTH);

	t = &htm->S[2];
	t->a = vertex_get(htm, 0, poly[5].x, poly[5].y, poly[5].z);
	t->b = vertex_get(htm, 0, poly[3].x, poly[3].y, poly[3].z);
	t->c = vertex_get(htm, 0, poly[4].x, poly[4].y, poly[4].z);
	t->orientation = TRIXEL_DOWN;
	t->hemisphere = HEMI_SOUTH;
	t->quadrant = 2;
	t->position = 0;
	t->depth = 0;
	trixel_0_parent_down(htm, t, t->a, t->b, t->c, 0);
	trixel_create_down(htm, &htm->S[2], depth, 0, HEMI_SOUTH);

	t = &htm->S[3];
	t->a = vertex_get(htm, 0, poly[5].x, poly[5].y, poly[5].z);
	t->b = vertex_get(htm, 0, poly[4].x, poly[4].y, poly[4].z);
	t->c = vertex_get(htm, 0, poly[1].x, poly[1].y, poly[1].z);
	t->orientation = TRIXEL_DOWN;
	t->hemisphere = HEMI_SOUTH;
	t->quadrant = 3;
	t->position = 0;
	t->depth = 0;
	trixel_0_parent_down(htm, t, t->a, t->b, t->c, 0);
	trixel_create_down(htm, &htm->S[3], depth, 0, HEMI_SOUTH);

	adb_htm_info(htm, ADB_LOG_HTM_CORE, "HTM: depth %d\n", htm->depth);
	adb_htm_info(htm, ADB_LOG_HTM_CORE,
				 "HTM: %d trixels of %lu bytes total"
				 " %luk @ resolution %f degrees\n",
				 htm->trixel_count, sizeof(struct htm_trixel),
				 htm->trixel_count * sizeof(struct htm_trixel) / 1024,
				 90.0 / (powf(2, depth)));
	adb_htm_info(htm, ADB_LOG_HTM_CORE,
				 "HTM: %d domains of %lu bytes total %luk\n",
				 htm->dec_strip_count, sizeof(struct dec_strip),
				 htm->dec_strip_count * sizeof(struct dec_strip) / 1024);
	adb_htm_info(htm, ADB_LOG_HTM_CORE,
				 "HTM: %d vertices of %lu bytes total %luk\n",
				 htm->vertex_count, sizeof(struct htm_vertex),
				 htm->vertex_count * sizeof(struct htm_vertex) / 1024);
	adb_htm_info(htm, ADB_LOG_HTM_CORE, "HTM: Footprint %luk\n",
				 htm->dec_strip_count * sizeof(struct dec_strip) +
					 htm->vertex_count,
				 htm->vertex_count * sizeof(struct htm_vertex) +
					 htm->trixel_count * sizeof(struct htm_trixel) / 1024);
	return htm;
}

int htm_get_depth_from_resolution(double resolution)
{
	int depth;

	for (depth = 0; depth < HTM_MAX_DEPTH; depth++) {
		if (resolution >= htm_resolution[depth])
			return depth;
	}

	return HTM_MAX_DEPTH - 1;
}
