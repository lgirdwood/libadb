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
 *  Copyright (C) 2008, 2012 Liam Girdwood
 */

#include <math.h>
#include <errno.h>
#include <string.h>

#include <libastrodb/db.h>
#include <libastrodb/table.h>
#include <libastrodb/htm.h>
#include <libastrodb/private.h>
#include <libastrodb/adbstdio.h>

/* vertex multiplication */
static inline float vertex_mult(struct htm_vertex *a, struct htm_vertex *b)
{
	return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

/* vertex cross product */
static inline void vertex_cross(struct htm_vertex *a, struct htm_vertex *b,
	struct htm_vertex *prod)
{
	prod->x = a->y * b->z - b->y * a->z;
	prod->y = a->z * b->x - b->z * a->x;
	prod->z = a->x * b->y - b->x * a->y;
}

/* vertex dot product */
static inline void vertex_dot(struct htm_vertex *a, struct htm_vertex *b,
	struct htm_vertex *prod)
{
	prod->x = a->x * b->x;
	prod->y = a->y * b->y;
	prod->z = a->z * b->z;
}

/* check vertex point is inside UP trixel */
static int vertex_is_inside_up(struct htm_trixel *t, struct htm_vertex *point)
{
	struct htm_vertex prod;
	float val;

	/*
	 * calculate the cross product of each edge in a clockwise a->b->c->a
	 * direction. If cross product multiplied by the point vertex is greater than
	 * INSIDE_UP_LIMIT for each edge then point is inside trixel.
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

/* check vertex point is inside DOWN trixel */
static int vertex_is_inside_down(struct htm_trixel *t, struct htm_vertex *point)
{
	struct htm_vertex prod;
	float val;

	/*
	 * calculate the cross product of each edge in a anti-clockwise a->c->b->a
	 * direction. If cross product multiplied by the point vertex is greater than
	 * INSIDE_UP_LIMIT for each edge then point is inside trixel.
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

/* recursively check each child trixel if it contains point */
static struct htm_trixel *trixel_is_container(struct htm *htm,
	struct htm_trixel *t, struct htm_vertex *point, int depth, int level)
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
	return t; 	/* return the parent, it's better than nothing */
}

/* search the HTM down to depth for points parent trixel */
struct htm_trixel *htm_get_home_trixel(struct htm *htm,
		struct htm_vertex *point, int depth)
{
	struct htm_trixel *t = NULL;
	int i;

	/* validate point */
	if (point->ra < 0.0 || point->ra >= 2.0 * M_PI ||
			point->dec < -M_PI_2 || point->dec > M_PI_2) {
		adb_htm_debug(htm, ADB_LOG_HTM_GET, "Invalid point %f:%f\n",
			point->ra * R2D, point->dec * R2D);
		return NULL;
	}

	/* convert RA,DEC to octohedron coords */
	htm_vertex_update_unit(point);

	if (depth > htm->depth)
		return NULL;

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
	adb_htm_error(htm, "No valid trixel for X %f Y %f Z %f\n",
			point->x, point->y, point->z);
	return NULL;
}

/* get all trixels associated with this vertex and copy to passed in list */
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

/* is trixel in buffer ? */
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

/* get trixels associated with this vertex at discrete depth */
int vertex_get_trixels_depth(struct htm *htm, struct htm_vertex *v,
	struct htm_trixel *origin, struct htm_trixel **trixel_buf,
	int buf_size, int depth, int pos)
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
			//adb_htm_debug(htm, ADB_LOG_HTM_GET, "add trixel at pos %d ", pos);
			//htm_dump_trixel(trixel_buf[pos]);
			pos++;
		} else
			empty++;
	}

	/* number of trixels added */
	adb_htm_vdebug(htm, ADB_LOG_HTM_GET,"added %d trixels at depth %d\n",
		num - empty, depth);
	return num - empty;
}

/* get trixel neighbour trixels,
 * i.e. trixels that share verticies with this trixel */
static int trixel_get_neighbours(struct htm *htm, struct htm_trixel *t,
		int depth, struct astrodb_object_set *set)
{
	int neighbours;

	/* vertex A */
	neighbours = vertex_get_trixels_depth(htm, t->a, t,
		set->trixels, htm->trixel_count, depth, 0);

	/* vertex B */
	neighbours += vertex_get_trixels_depth(htm, t->b, t,
		set->trixels, htm->trixel_count, depth, neighbours);

	/* vertex C */
	neighbours += vertex_get_trixels_depth(htm, t->c, t,
		set->trixels, htm->trixel_count, depth, neighbours);

	adb_htm_debug(htm, ADB_LOG_HTM_GET, "found %d neighbours at depth %d\n", neighbours,
		depth);
	return neighbours;
}

static int trixel_add_parent(struct htm *htm,
	struct htm_trixel **trixel_buf,
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
			adb_htm_vdebug(htm, ADB_LOG_HTM_GET, "add trixel at pos %d ", i);
			//htm_dump_trixel(trixel_buf[i]);
			return 1;
		}
	}

	adb_htm_error(htm, "out of space");
	return 0;
}

static int trixel_get_parents(struct htm *htm, struct astrodb_object_set *set,
	int buf_size, int current_depth,
	int buffer_start, int buffer_end, int parents)
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
		new += trixel_add_parent(htm, set->trixels, i,
			buffer_end, buf_size);
	}

	/* add parents for next iteration at depth - 1 */
	return trixel_get_parents(htm, set, buf_size, current_depth - 1,
		buffer_end, buffer_end + new, parents + new);
}

static int trixel_get_children(struct htm *htm, struct astrodb_object_set *set,
	struct htm_trixel *parent, int buf_size, int current_depth,
	int buf_pos)
{
	/* finished ? */
	if (current_depth++ >= set->max_depth)
		return buf_pos;

	adb_htm_vdebug(htm, ADB_LOG_HTM_GET, 
		"Parent %x add children at depth %d at offset %x\n",
		htm_trixel_id(parent), current_depth, buf_pos);
	//htm_dump_trixel(parent);

	/* room for children ? */
	if (buf_size < 4) {
		adb_htm_error(htm, "no space for new trixels at depth %d "
			"size %d space %d\n", current_depth, buf_pos, buf_size);
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

int htm_clip(struct htm *htm, struct astrodb_object_set *set,
	float ra, float dec, float fov, float min_depth, float max_depth)
{
	struct htm_vertex vertex;
	struct astrodb_table *table = set->table;

	set->min_depth = table_get_object_depth_min(table, min_depth);
	set->max_depth = table_get_object_depth_max(table, max_depth);
	if (set->min_depth < 0 || set->max_depth < 0) {
		adb_error(table->db, "invalid clip depth min %d max %d\n",
			set->min_depth, set->max_depth);
		return -EINVAL;
	}

	set->fov_depth = htm_get_depth_from_resolution(fov);
	set->fov = fov;
	vertex.ra = ra;
	vertex.dec = dec;
//#err fov depth should not be used to gather trixels when fov depth > depth
	adb_htm_debug(htm, ADB_LOG_HTM_GET, "depth limits: %3.3f (htm depth %d) <--> %3.3f (htm depth %d)\n",
		min_depth, set->min_depth, max_depth, set->max_depth);
	adb_htm_debug(htm, ADB_LOG_HTM_GET, "fov %3.3f degrees (htm fov depth %d)\n",
		fov * R2D, set->fov_depth);
	adb_htm_debug(htm, ADB_LOG_HTM_GET, "centre RA %f DEC %f\n", ra * R2D, dec * R2D);

	set->centre = htm_get_home_trixel(htm, &vertex, set->fov_depth);
	if (set->centre == NULL) {
		adb_htm_error(htm, " invalid trixel at %3.3f:%3.3f\n",
					vertex.ra * R2D, vertex.dec * R2D);
		return -EINVAL;
	}
	adb_htm_debug(htm, ADB_LOG_HTM_GET, "origin trixel = ");
	htm_dump_trixel(htm, set->centre);

	set->valid_trixels = 0;
	return 0;
}

#if 0
int htm_unclip(struct htm *htm)
{
	struct htm_vertex vertex;

	set->min_depth = 0;
	set->max_depth = htm->depth;
	set->fov_depth = htm_get_depth_from_resolution(2.0 * M_PI);
	vertex.ra = 0.0;
	vertex.dec = 0.0;
	set->centre = htm_get_home_trixel(htm, &vertex, set->fov_depth);
	set->valid_trixels = 0;
	set->fov = 2.0 * M_PI;
	bzero(set->trixels, htm->trixel_count * sizeof(struct htm_trixel *));

	return 0;
}
#endif

int htm_get_trixels(struct htm *htm, struct astrodb_object_set *set)
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
		neighbours = trixel_get_neighbours(htm, set->centre,
			set->fov_depth, set);

		/* get parents for each trixel */
		parents = trixel_get_parents(htm, set, htm->trixel_count - neighbours,
			set->fov_depth, 0, neighbours, 0);

		trixels = neighbours + parents;
	}

	/* get children for each trixel neighbour */
	for (i = 0; i < neighbours; i++)
		trixels = trixel_get_children(htm, set, set->trixels[i],
			htm->trixel_count - trixels, set->fov_depth, trixels);

	adb_htm_debug(htm, ADB_LOG_HTM_GET, "trixels %d parents %d neighbours %d\n",
		trixels, parents, neighbours);

	set->trixels[trixels] = NULL;
	set->valid_trixels = trixels;
	return trixels;
}

int htm_get_clipped_objects(struct astrodb_object_set *set)
{
	struct htm *htm = set->db->htm;
	int trixel_count = 0, populated_trixels = 0;
	int i, object_count = 0;

	if (!set->valid_trixels)
		trixel_count = htm_get_trixels(htm, set);

	if (trixel_count < 0) {
		adb_htm_error(htm, "invalid trixel count %d\n", trixel_count);
		adb_htm_debug(htm, ADB_LOG_HTM_GET, "htm clip depths: min %d max %d fov %d\n",
				set->min_depth, set->max_depth, set->fov_depth);
		adb_htm_debug(htm, ADB_LOG_HTM_GET, "clip fov %3.3f at ", set->fov * R2D);
		htm_dump_trixel(htm, set->centre);
		return -EINVAL;
	}

	adb_htm_debug(htm, ADB_LOG_HTM_GET, "got %d potential clipped trixels\n",
		trixel_count);

	/* get object head for every clipped trixel */
	for (i = 0; i < set->valid_trixels; i++) {

		adb_htm_vdebug(htm, ADB_LOG_HTM_GET, "trixels got %d count %d pos %x obs %d\n",
			trixel_count, i, set->trixels[i]->position,
			set->trixels[i]->data[set->table_id].num_objects);

		if (set->trixels[i]->depth < set->min_depth ||
			set->trixels[i]->depth > set->max_depth)
			continue;

		if (set->trixels[i]->data[set->table_id].num_objects == 0)
			continue;

		set->object_heads[populated_trixels].objects =
			set->trixels[i]->data[set->table_id].objects;
		set->object_heads[populated_trixels++].count =
			set->trixels[i]->data[set->table_id].num_objects;
		object_count +=
			set->trixels[i]->data[set->table_id].num_objects;

	}

	adb_htm_debug(htm, ADB_LOG_HTM_GET, "got %d populated trixels with %d objects\n",
		populated_trixels, object_count);
	adb_htm_debug(htm, ADB_LOG_HTM_GET, "htm clip depths: min %d max %d fov %d\n",
				set->min_depth, set->max_depth, set->fov_depth);
		adb_htm_debug(htm, ADB_LOG_HTM_GET, "clip fov %3.3f at ", set->fov * R2D);

	set->count = object_count;

	return populated_trixels;
}


/*! \fn void astrodb_table_clip.on_fov (astrodb_table* table, double ra, double dec, double radius,
 double faint_mag, double bright_mag);
 * \brief Set dataset clipping area based on field of view
 * \ingroup dataset
 */
struct astrodb_object_set *astrodb_table_set_new(struct astrodb_db *db,
	int table_id)
{
	struct astrodb_table *table;
	struct astrodb_object_set *set;

	if (table_id < 0 || table_id >= db->table_count)
		return NULL;
	table = &db->table[table_id];

	set = calloc(1, sizeof(*set));
	if (set == NULL)
		return NULL;

	/* alloc clipped trixels */
	set->trixels =
		calloc(1, (db->htm->trixel_count + 1) * sizeof(struct htm_trixel*));
	if (set->trixels == NULL) {
		free(set);
		return NULL;
	}

	set->object_heads =
		calloc(1, db->htm->trixel_count * sizeof(struct astrodb_object_head));
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
	set->fov = 360.0 * D2R;

	htm_clip(set->db->htm, set, set->centre_ra,
		set->centre_dec, set->fov, table->depth_map[0].min_value,
		table->depth_map[db->htm->depth].max_value);

	return set;
}

int astrodb_table_set_constraints(struct astrodb_object_set *set,
				double ra, double dec, 
				double fov, double start,
				double end)
{
	set->centre_ra = ra * D2R;
	set->centre_dec = dec * D2R;
	set->fov = fov * D2R;

	return htm_clip(set->db->htm, set, set->centre_ra,
		set->centre_dec, set->fov, start, end);
}

/*! \fn void astrodb_table_unclip (astrodb_table* table)
 * \param table dataset
 *
 * Unclip the dataset to it's full boundaries
 */
void astrodb_table_set_free(struct astrodb_object_set *set)
{
	free(set->object_heads);
	free(set->trixels);
	free(set);
}

/*! int astrodb_table_get_objects (astrodb_table* table, astrodb_progress progress, astrodb_slist **result, unsigned int src)
 * \param table dataset
 * \param progress Progress callback
 * \param results dataset objects
 * \param src Get objects flags
 * \return number of objects, or negative for failed
 *
 * Get objects from dataset based on clipping area. Results must be released
 * with astrodb_table_put_objects() after use.
 */
int astrodb_table_set_get_objects(struct astrodb_object_set *set)
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

struct astrodb_object_head *adb_set_get_head(struct astrodb_object_set *set)
{
	return set->object_heads;
}

int adb_set_get_count(struct astrodb_object_set *set)
{
	return set->count;
}

/*! \fn void* astrodb_table_get_object (astrodb_table* table, char* id, char* field);
 * \param table dataset
 * \param id object id
 * \param field dataset field
 * \return object or NULL if not found
 *
 * Get an object based on it' ID
 */
int astrodb_table_set_get_object(struct astrodb_object_set *set,
	const void *id, const char *field, const struct astrodb_object **object)
{
	struct astrodb_table *table = set->table;
	struct astrodb_db *db = set->db;
	astrodb_ctype ctype;
	int map, index, i, offset;

	*object = NULL;

	/* get map based on key */
	map = table_get_hashmap(db, table->id, field);
	if (map < 0)
		return map;

	offset = astrodb_table_get_field_offset(db, table->id, field);
	if (offset < 0)
		return offset;

	/* get hash index */
	ctype = astrodb_table_get_field_type(db, table->id, field);
	switch (ctype) {
	case ADB_CTYPE_STRING:
		index = hash_string((const char*)id, strlen((const char *)id), table->object.count);

		/* any objects at hash index ? */
		if (!table->hash.map[map].index[index])
			return 0;

		/* search through hashes for exact match */
		for (i = 0; i < table->hash.map[map].index[index]->count; i++) {
			const struct astrodb_object *o = table->hash.map[map].index[index]->object[i];
			char *_id = ((char *)o) + offset;

			if (!strstr(_id, id))
				continue;

			*object = o;
			goto out;
		}
		break;
	case ADB_CTYPE_SHORT:
	case ADB_CTYPE_INT:
		index = hash_int(*((int*)id), table->object.count);

		/* any objects at hash index ? */
		if (!table->hash.map[map].index[index])
			return 0;

		/* search through hashes for exact match */
		for (i = 0; i < table->hash.map[map].index[index]->count; i++) {
			const void *o = table->hash.map[map].index[index]->object[i];
			int *_id = ((int *)(o + offset));

			if (*_id != *((int*)id))
				continue;

			*object = o;
			goto out;
		}
		break;
	case ADB_CTYPE_DOUBLE_MPC:
	case ADB_CTYPE_SIGN:
	case ADB_CTYPE_NULL:
	case ADB_CTYPE_FLOAT:
	case ADB_CTYPE_DOUBLE:
	case ADB_CTYPE_DOUBLE_DMS_DEGS:
	case ADB_CTYPE_DOUBLE_DMS_MINS:
	case ADB_CTYPE_DOUBLE_DMS_SECS:
	case ADB_CTYPE_DOUBLE_HMS_HRS:
	case ADB_CTYPE_DOUBLE_HMS_MINS:
	case ADB_CTYPE_DOUBLE_HMS_SECS:
		adb_error(db, "ctype %d not implemented\n", ctype);
		break;
	}
	return 0;

out:
	return 1;
}

