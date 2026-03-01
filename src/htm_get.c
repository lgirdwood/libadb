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
 *  Copyright (C) 2008 - 2012 Liam Girdwood
 */

#include <errno.h> // IWYU pragma: keep
#include <math.h>
#include <string.h>

#include "debug.h"
#include "htm.h"
#include "private.h"
#include "table.h"
#include "libastrodb/db.h"
#include "libastrodb/object.h"

/**
 * \brief Compute the dot product (scalar multiplication) of two 3D spatial vertices.
 *
 * \param a First vertex coordinate.
 * \param b Second vertex coordinate.
 * \return Resulting floating point scalar value.
 */
static inline double vertex_mult(struct htm_vertex *a, struct htm_vertex *b)
{
	return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

/**
 * \brief Compute the cross product of two 3D vectors.
 *
 * \param a Target vertex A.
 * \param b Target vertex B.
 * \param prod Output vertex storing the orthogonal vector result.
 */
static inline void vertex_cross(struct htm_vertex *a, struct htm_vertex *b,
								struct htm_vertex *prod)
{
	prod->x = a->y * b->z - b->y * a->z;
	prod->y = a->z * b->x - b->z * a->x;
	prod->z = a->x * b->y - b->x * a->y;
}

/**
 * \brief Check if a spatial point is contained inside an UP-oriented trixel.
 *
 * Employs edge cross products multiplied against the target point to determine
 * bounded inclusion within the parent shape boundaries.
 *
 * \param t The UP trixel defining the boundary region.
 * \param point The target point to test for containment.
 * \return 1 if point is inside, 0 otherwise.
 */
static int vertex_is_inside_up(struct htm_trixel *t, struct htm_vertex *point)
{
	struct htm_vertex prod;
	double val;

	/*
   * calculate the cross product of each edge in a clockwise a->b->c->a
   * direction. If cross product multiplied by the point vertex is greater
   * than INSIDE_UP_LIMIT for each edge then point is inside trixel.
   */

	/* edge a -> b */
	vertex_cross(t->a, t->b, &prod);
	val = vertex_mult(&prod, point);
	if (val < INSIDE_UP_LIMIT)
		return 0;

	/* edge b -> c */
	vertex_cross(t->b, t->c, &prod);
	val = vertex_mult(&prod, point);
	if (val < INSIDE_UP_LIMIT)
		return 0;

	/* edge c -> a */
	vertex_cross(t->c, t->a, &prod);
	val = vertex_mult(&prod, point);
	if (val < INSIDE_UP_LIMIT)
		return 0;

	/* inside trixel */
	return 1;
}

/**
 * \brief Check if a spatial point is contained inside a DOWN-oriented trixel.
 *
 * Evaluates inclusion logic using an anti-clockwise edge sequence traversal.
 *
 * \param t The DOWN trixel defining the boundary region.
 * \param point The target test point.
 * \return 1 if point is inside, 0 otherwise.
 */
static int vertex_is_inside_down(struct htm_trixel *t, struct htm_vertex *point)
{
	struct htm_vertex prod;
	double val;

	/*
   * calculate the cross product of each edge in a anti-clockwise a->c->b->a
   * direction. If cross product multiplied by the point vertex is greater
   * than INSIDE_UP_LIMIT for each edge then point is inside trixel.
   */

	/* edge a -> c */
	vertex_cross(t->a, t->c, &prod);
	val = vertex_mult(&prod, point);
	if (val < INSIDE_UP_LIMIT)
		return 0;

	/* edge c -> b */
	vertex_cross(t->c, t->b, &prod);
	val = vertex_mult(&prod, point);
	if (val < INSIDE_UP_LIMIT)
		return 0;

	/* edge b -> a */
	vertex_cross(t->b, t->a, &prod);
	val = vertex_mult(&prod, point);
	if (val < INSIDE_UP_LIMIT)
		return 0;

	/* inside trixel */
	return 1;
}

/**
 * \brief Recursively check each child trixel to find which one contains the target point.
 *
 * Traverses down to the specified depth, determining point inclusion at each level.
 *
 * \param htm Parent spatial engine.
 * \param t The current parent trixel evaluating its bounds.
 * \param point Target coordinate to locate.
 * \param depth Maximum recursion depth limit.
 * \param level Current target iteration level.
 * \return Pointer to the matched bounding trixel, or NULL if outside bounds.
 */
static struct htm_trixel *trixel_is_container(struct htm *htm,
											  struct htm_trixel *t,
											  struct htm_vertex *point,
											  int depth, int level)
{
	struct htm_trixel *child;

	/* is point inside this trixel ? */
	if (t->orientation == TRIXEL_UP)
		t->visible = vertex_is_inside_up(t, point);
	else
		t->visible = vertex_is_inside_down(t, point);

	/* return NULL if point is not in this trixel */
	if (!t->visible)
		return NULL;

	/* return trixel if we are visible and at correct depth */
	if (level++ == depth)
		return t;

	/* check child trixels if point is contained with parent */
	child = trixel_is_container(htm, &t->child[0], point, depth, level);
	if (child)
		return child;

	child = trixel_is_container(htm, &t->child[1], point, depth, level);
	if (child)
		return child;

	child = trixel_is_container(htm, &t->child[2], point, depth, level);
	if (child)
		return child;

	child = trixel_is_container(htm, &t->child[3], point, depth, level);
	if (child)
		return child;

	/* we should never get here as one child will contain point */
	adb_htm_debug(htm, ADB_LOG_HTM_GET, "No valid child for X %f Y %f Z %f\n",
				  point->x, point->y, point->z);
	printf("\n!!!!! No valid child for X %f Y %f Z %f\n", point->x, point->y,
		   point->z);
	return t; /* return the parent, it's better than nothing */
}

/**
 * \brief Search the HTM down to depth for a point's bounding parent trixel.
 *
 * Initiates a top-level search over all 8 spherical root sectors to locate
 * the finest bounding partition containing the specified coordinate.
 *
 * \param htm Spatial indexing instance.
 * \param point Target RA/Dec vertex matching position.
 * \param depth Target recursive block dimension limit to stop at.
 * \return The bounding leaf trixel at depth, or NULL.
 */
struct htm_trixel *htm_get_home_trixel(struct htm *htm,
									   struct htm_vertex *point, int depth)
{
	struct htm_trixel *t = NULL;
	int i;

	/* validate point */
	if (point->ra < 0.0 || point->ra >= 2.0 * M_PI || point->dec < -M_PI_2 ||
		point->dec > M_PI_2) {
		adb_htm_debug(htm, ADB_LOG_HTM_GET, "Invalid point %f:%f\n",
					  point->ra * R2D, point->dec * R2D);
		return NULL;
	}

	/* convert RA,DEC to octohedron coords */
	htm_vertex_update_unit(point);

	if (depth > htm->depth)
		depth = htm->depth - 1;

	/* northern hemisphere quad trixels */
	for (i = 0; i < 4; i++) {
		t = trixel_is_container(htm, &htm->N[i], point, depth, 0);
		if (t)
			return t;
	}
	/* southern hemisphere quad trixels */
	for (i = 0; i < 4; i++) {
		t = trixel_is_container(htm, &htm->S[i], point, depth, 0);
		if (t)
			return t;
	}

	/* we should never get here as one trixel will contain point */
	adb_htm_error(htm, "No valid trixel for X %f Y %f Z %f\n", point->x,
				  point->y, point->z);
	return NULL;
}

/**
 * \brief Copy all trixels associated with a specific vertex into an array.
 *
 * \param htm Spatial engine.
 * \param v Bounding point vertex mapping trixels.
 * \param trixel_buf Output buffer to collect resulting pointers.
 * \param buf_size Sizing limit of output buffer.
 * \return Total stored valid elements added, or required deficit size if buffer is too small.
 */
int vertex_get_all_trixels(struct htm *htm, struct htm_vertex *v,
						   struct htm_trixel **trixel_buf, int buf_size)
{
	int num = (htm->depth - v->depth) * TRIXELS_PER_VERTEX, i, empty = 0;

	/* not enough space for complete list of trixels */
	if (buf_size < num)
		return (buf_size - num);

	/* make sure we only add valid trixels */
	for (i = 0; i < num; i++) {
		if (v->trixel[i])
			trixel_buf[i] = v->trixel[i];
		else
			empty++;
	}

	/* number of trixels added */
	return num - empty;
}

/**
 * \brief Query if a specified trixel already exists inside a buffer array.
 *
 * \param t The trixel sought block reference.
 * \param trixel_buf Lookuptarget array.
 * \param pos Total indexed boundary limits.
 * \return 1 if found, 0 otherwise.
 */
static inline int trixel_in_buffer(struct htm_trixel *t,
								   struct htm_trixel **trixel_buf, int pos)
{
	int i;

	for (i = 0; i < pos; i++) {
		if (trixel_buf[i] == t)
			return 1;
	}
	return 0;
}

/**
 * \brief Fetch all trixels sharing a specific vertex at a targeted depth.
 *
 * Scans a vertex's bound trixel list fetching depth-matching neighbors, optionally filtering
 * out a caller origin trixel to strictly find neighboring entities.
 *
 * \param htm Engine instance.
 * \param v The shared connection vertex.
 * \param origin The originating trixel looking for its neighbors (ignored).
 * \param trixel_buf Output buffer to collect valid neighbor pointers.
 * \param buf_size Sizing limit of the output buffer.
 * \param depth Target spatial depth level.
 * \param pos Current append index within the output buffer.
 * \return The integer count of newly added valid neighboring trixels.
 */
int vertex_get_trixels_depth(struct htm *htm, struct htm_vertex *v,
							 struct htm_trixel *origin,
							 struct htm_trixel **trixel_buf, int buf_size,
							 int depth, int pos)
{
	int num = TRIXELS_PER_VERTEX, i, offset, empty = 0;

	/* depth is before trixel was created */
	if (depth < v->depth || depth > htm->depth)
		return 0;

	/* not enough space for complete list of trixels */
	if ((buf_size - pos) < num) {
		adb_htm_error(htm, "%d free, need %d for new trixels at depth\n",
					  buf_size - pos, num, depth);
		return 0;
	}

	/* calc trixel assoc offset */
	offset = (depth - v->depth) * TRIXELS_PER_VERTEX;

	/* make sure we only add valid trixels */
	for (i = 0; i < num; i++) {
		if (v->trixel[i + offset] &&
			!trixel_in_buffer(v->trixel[i + offset], trixel_buf, pos)) {
			trixel_buf[pos] = v->trixel[i + offset];
			pos++;
		} else
			empty++;
	}

	/* number of trixels added */
	adb_htm_vdebug(htm, ADB_LOG_HTM_GET, "added %d trixels at depth %d\n",
				   num - empty, depth);
	return num - empty;
}

/**
 * \brief Retrieve all immediate neighboring trixels sharing any vertices with a target block.
 *
 * \param htm Spatial engine.
 * \param t The center trixel to discover neighbors for.
 * \param depth The specified depth tier to search within.
 * \param set Target bounded collection managing output buffers.
 * \return Total collected neighboring elements.
 */
static int trixel_get_neighbours(struct htm *htm, struct htm_trixel *t,
								 int depth, struct adb_object_set *set)
{
	int neighbours;

	/* vertex A */
	neighbours = vertex_get_trixels_depth(htm, t->a, t, set->trixels,
										  htm->trixel_count, depth, 0);

	/* vertex B */
	neighbours += vertex_get_trixels_depth(
		htm, t->b, t, set->trixels, htm->trixel_count, depth, neighbours);

	/* vertex C */
	neighbours += vertex_get_trixels_depth(
		htm, t->c, t, set->trixels, htm->trixel_count, depth, neighbours);

	adb_htm_debug(htm, ADB_LOG_HTM_GET, "found %d neighbours at depth %d\n",
				  neighbours, depth);
	return neighbours;
}

/**
 * \brief Register the parent of a given trixel into a unique collection buffer.
 *
 * Extracts a trixel's top-level parent and ensures it's uniquely inserted
 * into the buffer tracking array exactly once.
 *
 * \param htm Spatial engine.
 * \param trixel_buf Pointer to the bounding collection buffer.
 * \param offset Index identifying the child block whose parent is being stored.
 * \param buffer_start Search window start for duplicate checking.
 * \param buffer_end Search window limit for duplicate checking.
 * \return 1 if successfully added, 0 on failure or pre-existing duplicate.
 */
static int trixel_add_parent(struct htm *htm, struct htm_trixel **trixel_buf,
							 int offset, int buffer_start, int buffer_end)
{
	struct htm_trixel *t = trixel_buf[offset]->parent;
	int i;

	/* top level */
	if (!t)
		return 0;

	for (i = buffer_start; i < buffer_end; i++) {
		/* already in buffer ? */
		if (trixel_buf[i] == t)
			return 0;

		/* not in buffer, so add */
		if (trixel_buf[i] == NULL) {
			trixel_buf[i] = t;
			adb_htm_vdebug(htm, ADB_LOG_HTM_GET, "add trixel at pos %d\n", i);
			htm_dump_trixel(htm, t);
			return 1;
		}
	}

	adb_htm_error(htm, "out of space");
	return 0;
}

/**
 * \brief Recursively extract all cascading parent structures originating from an initial trixel list.
 *
 * Reverses upward through the hierarchy grabbing parents until reaching a defined minimum
 * depth ceiling. Used for constructing wide, high-level structural bounds above detailed areas.
 *
 * \param htm Spatial engine.
 * \param set Managed subset state bounds object.
 * \param buf_size Limits parameter blocking overflows.
 * \param current_depth Deepest originating starting depth constraint limit.
 * \param buffer_start Origin slice for checking.
 * \param buffer_end Trailing tail slice point.
 * \param parents Accumulated valid parent sum tracking.
 * \return Final running count of all unified mapped parents.
 */
static int trixel_get_parents(struct htm *htm, struct adb_object_set *set,
							  int buf_size, int current_depth, int buffer_start,
							  int buffer_end, int parents)
{
	int i, new = 0;

	/* finished if we are at min depth */
	if (current_depth == set->min_depth) {
		adb_htm_debug(htm, ADB_LOG_HTM_GET, "found %d parents to depth %d\n",
					  parents, set->min_depth);
		return parents;
	}

	/* add parents from last iteration */
	for (i = buffer_start; i < buffer_end; i++) {
		if (i == buf_size) {
			adb_htm_error(htm, "no space for new trixels at depth\n",
						  current_depth);
			return parents;
		}
		new += trixel_add_parent(htm, set->trixels, i, buffer_end, buf_size);
	}

	/* add parents for next iteration at depth - 1 */
	return trixel_get_parents(htm, set, buf_size, current_depth - 1, buffer_end,
							  buffer_end + new, parents + new);
}

/**
 * \brief Recursively navigate downward returning all subdivided children from a parent context.
 *
 * \param htm Spatial engine.
 * \param set Container accumulating discovered block boundaries.
 * \param parent Origin point root node.
 * \param buf_size Limits variable blocking overflows.
 * \param current_depth Recursion state monitoring maximum constraints.
 * \param buf_pos Active write positioning marker.
 * \return Position cursor marker indicating buffer write boundary.
 */
static int trixel_get_children(struct htm *htm, struct adb_object_set *set,
							   struct htm_trixel *parent, int buf_size,
							   int current_depth, int buf_pos)
{
	/* finished ? */
	if (current_depth++ >= set->max_depth)
		return buf_pos;

	adb_htm_vdebug(htm, ADB_LOG_HTM_GET,
				   "Parent %x add children at depth %d at offset %x\n",
				   htm_trixel_id(parent), current_depth, buf_pos);

	/* room for children ? */
	if (buf_size < 4) {
		adb_htm_error(htm,
					  "no space for new trixels at depth %d "
					  "size %d space %d\n",
					  current_depth, buf_pos, buf_size);
		htm_dump_trixel(htm, parent);
		return buf_pos;
	}

	/* add children */
	set->trixels[buf_pos++] = &parent->child[0];
	set->trixels[buf_pos++] = &parent->child[1];
	set->trixels[buf_pos++] = &parent->child[2];
	set->trixels[buf_pos++] = &parent->child[3];
	buf_size -= 4;

	/* add grand children */
	buf_pos = trixel_get_children(htm, set, &parent->child[0], buf_size,
								  current_depth, buf_pos);
	buf_pos = trixel_get_children(htm, set, &parent->child[1], buf_size,
								  current_depth, buf_pos);
	buf_pos = trixel_get_children(htm, set, &parent->child[2], buf_size,
								  current_depth, buf_pos);
	buf_pos = trixel_get_children(htm, set, &parent->child[3], buf_size,
								  current_depth, buf_pos);

	return buf_pos;
}

/**
 * \brief Create a spatial bounding perimeter using FOV degrees and center coordinates.
 *
 * Defines the spatial dimensions of an initialized `adb_object_set` subset container.
 *
 * \param htm Spatial engine.
 * \param set Target clipping area configuration manager.
 * \param ra Center celestial Right Ascension marking the field middle.
 * \param dec Center celestial Declination marking the field middle.
 * \param fov Diameter Field Of View bounds.
 * \param min_depth Hard limit minimal recursive block depth tier mapping limit.
 * \param max_depth Hard limit maximal recursive block depth tier mapping limit.
 * \return 0 on successful validation, or -EINVAL on out-of-bounds inputs.
 */
int htm_clip(struct htm *htm, struct adb_object_set *set, double ra, double dec,
			 double fov, double min_depth, double max_depth)
{
	struct htm_vertex vertex;
	struct adb_table *table = set->table;

	bzero(set->trixels, set->valid_trixels * sizeof(struct htm_trixel *));

	set->min_depth = table_get_object_depth_min(table, min_depth);
	set->max_depth = table_get_object_depth_max(table, max_depth);
	if (set->min_depth < 0 || set->max_depth < 0) {
		adb_error(table->db, "invalid clip depth min %d max %d\n",
				  set->min_depth, set->max_depth);
		return -EINVAL;
	}

	set->fov_depth = htm_get_depth_from_resolution(fov);
	if (set->fov_depth > htm->depth)
		set->fov_depth = htm->depth;

	set->fov = fov;
	vertex.ra = ra;
	vertex.dec = dec;

	/* fov depth should not be used to gather trixels when fov depth > depth */
	adb_htm_debug(
		htm, ADB_LOG_HTM_GET,
		"depth limits: %3.3f (htm depth %d) <--> %3.3f (htm depth %d)\n",
		min_depth, set->min_depth, max_depth, set->max_depth);
	adb_htm_debug(htm, ADB_LOG_HTM_GET,
				  "fov %3.3f degrees (htm fov depth %d)\n", fov * R2D,
				  set->fov_depth);
	adb_htm_debug(htm, ADB_LOG_HTM_GET, "centre RA %f DEC %f\n", ra * R2D,
				  dec * R2D);

	set->centre = htm_get_home_trixel(htm, &vertex, set->fov_depth);
	if (set->centre == NULL) {
		adb_htm_error(htm, " invalid trixel at %3.3f:%3.3f\n", vertex.ra * R2D,
					  vertex.dec * R2D);
		return -EINVAL;
	}
	adb_htm_debug(htm, ADB_LOG_HTM_GET, "origin trixel = ");
	htm_dump_trixel(htm, set->centre);

	set->valid_trixels = 0;
	return 0;
}

/**
 * \brief Extract bounding trixels contained inside an initialized object set subset FOV constraint.
 *
 * Traverses spatial bounds radiating outwards from the center subset position
 * returning arrays mapping valid contained areas.
 *
 * \param htm Engine configuration reference.
 * \param set Target boundaries structure requesting the map.
 * \return Total aggregated element counts placed successfully into the subset.
 */
int htm_get_trixels(struct htm *htm, struct adb_object_set *set)
{
	int trixels, parents, neighbours, i;

	if (set->fov >= M_PI) {
		/* get all trixels at depth 0 */
		adb_htm_debug(htm, ADB_LOG_HTM_GET, "fov is > M_PI depth %d\n",
					  set->fov_depth);

		trixels = 0;
		neighbours = 8;
		parents = 0;

		set->trixels[trixels++] = &htm->N[0];
		set->trixels[trixels++] = &htm->N[1];
		set->trixels[trixels++] = &htm->N[2];
		set->trixels[trixels++] = &htm->N[3];
		set->trixels[trixels++] = &htm->S[0];
		set->trixels[trixels++] = &htm->S[1];
		set->trixels[trixels++] = &htm->S[2];
		set->trixels[trixels++] = &htm->S[3];
	} else {
		adb_htm_debug(htm, ADB_LOG_HTM_GET, "fov < M_PI depth %d\n",
					  set->fov_depth);

		/* get neighbours for each trixel */
		neighbours =
			trixel_get_neighbours(htm, set->centre, set->fov_depth, set);

		/* get parents for each trixel */
		parents = trixel_get_parents(htm, set, htm->trixel_count - neighbours,
									 set->fov_depth, 0, neighbours, 0);

		trixels = neighbours + parents;
	}

	/* get children for each trixel neighbour */
	for (i = 0; i < neighbours; i++)
		trixels = trixel_get_children(htm, set, set->trixels[i],
									  htm->trixel_count - trixels,
									  set->fov_depth, trixels);

	adb_htm_debug(htm, ADB_LOG_HTM_GET, "trixels %d parents %d neighbours %d\n",
				  trixels, parents, neighbours);

	set->trixels[trixels] = NULL;
	set->valid_trixels = trixels;
	return trixels;
}

/**
 * \brief Compile heads referencing objects constrained inside a subset.
 *
 * Analyzes previously clipped trixels within an `adb_object_set` to build consolidated
 * lists of objects populating those constrained blocks.
 *
 * \param set Bounded configuration instance.
 * \return Valid count of objects bound by the constraints.
 */
int htm_get_clipped_objects(struct adb_object_set *set)
{
	struct htm *htm = set->db->htm;
	int trixel_count = 0, populated_trixels = 0;
	int i, object_count = 0, head_count = 0;

	if (!set->valid_trixels)
		trixel_count = htm_get_trixels(htm, set);

	if (trixel_count < 0) {
		adb_htm_error(htm, "invalid trixel count %d\n", trixel_count);
		adb_htm_debug(htm, ADB_LOG_HTM_GET,
					  "htm clip depths: min %d max %d fov %d\n", set->min_depth,
					  set->max_depth, set->fov_depth);
		adb_htm_debug(htm, ADB_LOG_HTM_GET, "clip fov %3.3f at ",
					  set->fov * R2D);
		htm_dump_trixel(htm, set->centre);
		return -EINVAL;
	}

	adb_htm_debug(htm, ADB_LOG_HTM_GET, "got %d potential clipped trixels\n",
				  trixel_count);

	/* get object head for every clipped trixel */
	for (i = 0; i < set->valid_trixels; i++) {
		adb_htm_vdebug(
			htm, ADB_LOG_HTM_GET,
			"trixels got %d count %d pos %x objects %d parent pos %x\n",
			trixel_count, i, set->trixels[i]->position,
			set->trixels[i]->data[set->table_id].num_objects,
			set->trixels[i]->parent ? set->trixels[i]->parent->position : 0);
		adb_htm_vdebug(
			htm, ADB_LOG_HTM_GET,
			" RA %3.3f DEC %3.3f RA %3.3f DEC %3.3f RA %3.3f DEC %3.3f\n",
			set->trixels[i]->a->ra * R2D, set->trixels[i]->a->dec * R2D,
			set->trixels[i]->b->ra * R2D, set->trixels[i]->b->dec * R2D,
			set->trixels[i]->c->ra * R2D, set->trixels[i]->c->dec * R2D);
		//htm_dump_trixel_objects(htm, set->trixels[i], 0);

		if (set->trixels[i]->depth < set->min_depth ||
			set->trixels[i]->depth > set->max_depth)
			continue;

		if (set->trixels[i]->data[set->table_id].num_objects == 0)
			continue;

		set->object_heads[populated_trixels].objects =
			set->trixels[i]->data[set->table_id].objects;
		set->object_heads[populated_trixels++].count =
			set->trixels[i]->data[set->table_id].num_objects;
		object_count += set->trixels[i]->data[set->table_id].num_objects;
		head_count++;
	}

	adb_htm_debug(htm, ADB_LOG_HTM_GET,
				  "got %d populated trixels with %d objects\n",
				  populated_trixels, object_count);
	adb_htm_debug(htm, ADB_LOG_HTM_GET,
				  "htm clip depths: min %d max %d fov depth %d\n",
				  set->min_depth, set->max_depth, set->fov_depth);
	adb_htm_debug(htm, ADB_LOG_HTM_GET, "clip fov %3.3f\n", set->fov * R2D);

	set->count = object_count;
	set->head_count = head_count;

	return populated_trixels;
}

/**
 * \brief Create a new dataset object subset (clipping area).
 * \ingroup htm
 *
 * Allocates and initializes an `adb_object_set` bounded area. By default, the
 * field of view is set to 2*PI (the entire table) and is linked to the
 * underlying dataset's HTM depth limits.
 *
 * \param db Database catalog to query
 * \param table_id Target ID of the table
 * \return A pointer to a newly allocated `adb_object_set`, or NULL on failure
 */
struct adb_object_set *adb_table_set_new(struct adb_db *db, int table_id)
{
	struct adb_table *table;
	struct adb_object_set *set;

	if (table_id < 0 || table_id >= ADB_MAX_TABLES)
		return NULL;
	table = &db->table[table_id];

	set = calloc(1, sizeof(*set));
	if (set == NULL)
		return NULL;

	/* alloc clipped trixels */
	set->trixels =
		calloc(1, (db->htm->trixel_count + 1) * sizeof(struct htm_trixel *));
	if (set->trixels == NULL) {
		free(set);
		return NULL;
	}

	set->object_heads =
		calloc(1, db->htm->trixel_count * sizeof(struct adb_object_head));
	if (set->object_heads == NULL) {
		free(set->trixels);
		free(set);
		return NULL;
	}

	set->db = db;
	set->table = table;
	set->table_id = table_id;
	set->centre_ra = 0.0;
	set->centre_dec = 0.0;
	set->fov = 2.0 * M_PI;

	htm_clip(set->db->htm, set, set->centre_ra, set->centre_dec, set->fov,
			 table->depth_map[0].min_value,
			 table->depth_map[db->htm->depth].max_value);

	return set;
}

int adb_table_set_constraints(struct adb_object_set *set, double ra, double dec,
							  double fov, double start, double end)
{
	set->centre_ra = ra;
	set->centre_dec = dec;
	set->fov = fov;

	return htm_clip(set->db->htm, set, set->centre_ra, set->centre_dec,
					set->fov, start, end);
}

/**
 * \brief Free a dataset object subset allocation.
 * \ingroup htm
 *
 * Releases memory resources for the designated subset's bounded trixels,
 * object headers, and the set object itself.
 *
 * \param set Dataset object set to free
 */
void adb_table_set_free(struct adb_object_set *set)
{
	if (set == NULL)
		return;

	free(set->object_heads);
	free(set->trixels);
	free(set);
}

/**
 * \brief Retrieve objects from a dataset based on clipping boundaries.
 * \ingroup htm
 *
 * Reads or utilizes previously clipped HTM trixels within the dataset
 * set's declared spatial bounds, returning the number of valid constrained
 * trixels mapped for examination.
 *
 * \param set Configured dataset set object boundary
 * \return The number of populated object head trixels, or a negative error code
 */
int adb_set_get_objects(struct adb_object_set *set)
{
	/* check for previous "get" since trixels will still be valid */
	if (set->valid_trixels) {
		adb_debug(set->db, ADB_LOG_HTM_GET, "using existing clipped trixels\n");
		return set->valid_trixels;
	}

	/* get trixels from HTM */
	adb_debug(set->db, ADB_LOG_HTM_GET,
			  "no existing clipped trixels exist, so clipping...\n");
	return htm_get_clipped_objects(set);
}

struct adb_object_head *adb_set_get_head(struct adb_object_set *set)
{
	return set->object_heads;
}

int adb_set_get_count(struct adb_object_set *set)
{
	return set->count;
}

/**
 * \brief Retrieve a key-matching index offset mapping to the hash definition arrays.
 *
 * \param set The subset collection being queried.
 * \param key Desired property mapping symbolic string.
 * \return Discovered mapped integer layout identifier, or -EINVAL on miss.
 */
static int set_get_hashmap(struct adb_object_set *set, const char *key)
{
	int i;

	for (i = 0; i < set->hash.num; i++) {
		if (!strcmp(key, set->hash.map[i].key))
			return i;
	}

	return -EINVAL;
}

/**
 * \brief Look up a data entry by identifying value traversing rapid hash tables.
 *
 * \param hash Core structural framework storing precompiled hash table metadata.
 * \param id Identifying input argument (as string, integer, etc).
 * \param map Target active array definitions layout ID.
 * \param offset Object field value index jump offset position.
 * \param ctype Target object underlying value layout datatype format.
 * \param count Total limit iterating objects.
 * \param object Output address receiving the generated database query element.
 * \return 1 on successful retrieval hit, 0 on missing record, and -EINVAL on bad type.
 */
static int hash_get_object(struct table_hash *hash, const void *id, int map,
						   int offset, adb_ctype ctype, int count,
						   const struct adb_object **object)
{
	int i, index;

	switch (ctype) {
	case ADB_CTYPE_STRING:
		index = hash_string((const char *)id, strlen((const char *)id), count);

		/* any objects at hash index ? */
		if (!hash->map[map].index[index])
			return 0;

		/* search through hashes for exact match */
		for (i = 0; i < hash->map[map].index[index]->count; i++) {
			const struct adb_object *o = hash->map[map].index[index]->object[i];
			char *_id = ((char *)o) + offset;

			if (!strstr(_id, id))
				continue;

			*object = o;
			return 1;
		}
		break;
	case ADB_CTYPE_SHORT:
	case ADB_CTYPE_INT:
		index = hash_int(*((int *)id), count);

		/* any objects at hash index ? */
		if (!hash->map[map].index[index])
			return 0;

		/* search through hashes for exact match */
		for (i = 0; i < hash->map[map].index[index]->count; i++) {
			const void *o = hash->map[map].index[index]->object[i];
			int *_id = ((int *)(o + offset));

			if (*_id != *((int *)id))
				continue;

			*object = o;
			return 1;
		}
		break;
	case ADB_CTYPE_DOUBLE_MPC:
	case ADB_CTYPE_SIGN:
	case ADB_CTYPE_NULL:
	case ADB_CTYPE_FLOAT:
	case ADB_CTYPE_DOUBLE:
	case ADB_CTYPE_DEGREES:
	case ADB_CTYPE_DOUBLE_DMS_DEGS:
	case ADB_CTYPE_DOUBLE_DMS_MINS:
	case ADB_CTYPE_DOUBLE_DMS_SECS:
	case ADB_CTYPE_DOUBLE_HMS_HRS:
	case ADB_CTYPE_DOUBLE_HMS_MINS:
	case ADB_CTYPE_DOUBLE_HMS_SECS:
	default:
		return -EINVAL;
	}

	return -EINVAL;
}

/**
 * \brief Look up a specific object within a filtered bounding subset by value field key.
 *
 * \param set The subset area limiting valid candidates.
 * \param id Matcher evaluating value payload bounds.
 * \param field Target property dimension string.
 * \param object Final output node pointer to map hit to.
 * \return Discovery state status code or negative error bound failure.
 */
int adb_set_get_object(struct adb_object_set *set, const void *id,
					   const char *field, const struct adb_object **object)
{
	struct adb_table *table = set->table;
	struct adb_db *db = table->db;
	adb_ctype ctype;
	int map, offset;

	*object = NULL;

	/* get map based on key */
	map = set_get_hashmap(set, field);
	if (map < 0)
		return map;

	offset = adb_table_get_field_offset(db, table->id, field);
	if (offset < 0)
		return offset;

	/* get hash index */
	ctype = adb_table_get_field_type(db, table->id, field);

	return hash_get_object(&set->hash, id, map, offset, ctype, set->count,
						   object);
}

/**
 * \brief Globally look up a specific object within an entire table by value field key.
 *
 * \param db Initialized active catalog connection.
 * \param table_id Target structural layout offset to query against.
 * \param id Matcher identifying parameter evaluating target payload layout bounds.
 * \param field String parameter naming definition bounds.
 * \param object Final output pointer linking generated hit reference data memory.
 * \return Discovery state status code or negative error bound failure.
 */
int adb_table_get_object(struct adb_db *db, int table_id, const void *id,
						 const char *field, const struct adb_object **object)
{
	struct adb_table *table = &db->table[table_id];
	adb_ctype ctype;
	int map, offset;

	*object = NULL;

	/* get map based on key */
	map = table_get_hashmap(db, table->id, field);
	if (map < 0)
		return map;

	offset = adb_table_get_field_offset(db, table->id, field);
	if (offset < 0)
		return offset;

	/* get hash index */
	ctype = adb_table_get_field_type(db, table->id, field);

	return hash_get_object(&table->hash, id, map, offset, ctype,
						   table->object.count, object);
}

/**
 * \brief Trigger generation caching of internal fast indexing tables over the bounding bounds.
 *
 * Iterates through available hashes reconstructing and inserting keys matching the newly clipped area.
 *
 * \param set Bound active working container subset bounds array.
 * \return Zero on operational completion or negative initialization block failure sequence bound.
 */
int adb_table_set_hash_objects(struct adb_object_set *set)
{
	struct adb_table *table = set->table;
	int i, ret;

	for (i = 0; i < table->hash.num; i++) {
		ret = hash_build_set(set, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}
