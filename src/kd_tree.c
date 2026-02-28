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
 *  Copyright (C) 2013 - 2014 Liam Girdwood
 */

#include <ctype.h> // IWYU pragma: keep
#include <errno.h> // IWYU pragma: keep
#include <math.h>
#include <stdio.h> // IWYU pragma: keep
#include <stdlib.h>
#include <string.h> // IWYU pragma: keep
#include <unistd.h>

#include "debug.h"
#include "table.h"
#include "libastrodb/db.h"
#include "libastrodb/object.h"

/* debug KD tree search for nearest */
#define CHECK_KD_TREE 0

enum kd_pivot {
	KD_PIVOT_X = 0,
	KD_PIVOT_Y = 1,
	KD_PIVOT_Z = 2,
};

struct kd_elem;

/*! \defgroup kdtree KD-Tree
 *
 * \brief KD-Tree implementation for spatial indexing.
 *
 * Contains algorithms to build and query balanced k-dimensional trees 
 * for rapid spatial searches over astronomical datasets, supporting nearest 
 * neighbor lookups and bounding box exclusions.
 */

/*! \struct kd
 * \brief kd test
 * \ingroup kdtree
 */
struct kd {
	uint32_t parent; /*!< parent ID */
	uint32_t child[2]; /*!< child IDs */
};

/*! \struct kd_vertex
 * \brief Vertice for KD
 * \ingroup kdtree
 */
struct kd_vertex {
	double x; /*!< x coordinate */
	double y; /*!< y coordinate */
	double z; /*!< z coordinate */
};

/*! \struct kd_base_elem
 * \brief KD base elem
 * \ingroup kdtree
 */
struct kd_base_elem {
	struct kd kd; /*!< used by base */
	int bid; /*!< base ID */
	int used; /*!< is used */
	struct kd_vertex v; /*!< vertex */
	struct adb_object *object; /*!< object */
};

/*! \struct kd_elem
 * \brief KD element
 * \ingroup kdtree
 */
struct kd_elem {
	struct kd_base_elem *base; /*!< used by RA/DEC elems */
	int eid; /*!< elem ID */
};

/*! \struct kd_base
 * \brief KD base
 * \ingroup kdtree
 */
struct kd_base {
	struct adb_db *db; /*!< database */
	int count; /*!< count */
	int total; /*!< total */
	float percent; /*!< percentage */
	struct kd_base_elem *base; /*!< base array */
	struct kd_elem *x_base; /*!< X elements */
	struct kd_elem *y_base; /*!< Y elements */
	struct kd_elem *z_base; /*!< Z elements */
};

/*! \struct kd_init
 * \brief KD int
 * \ingroup kdtree
 */
struct kd_init {
	struct adb_db *db; /*!< database */
	struct adb_table *table; /*!< table */
	struct kd_base *kbase; /*!< kbase */
	int table_id; /*!< table ID */
	int iid; /*!< initial ID */
};

static struct kd_base_elem *kd_x_select_elem(struct kd_base *mbase, int x_start,
											 int x_end, int y_start, int y_end,
											 int z_start, int z_end);
static struct kd_base_elem *kd_y_select_elem(struct kd_base *mbase, int x_start,
											 int x_end, int y_start, int y_end,
											 int z_start, int z_end);
static struct kd_base_elem *kd_z_select_elem(struct kd_base *mbase, int x_start,
											 int x_end, int y_start, int y_end,
											 int z_start, int z_end);

/**
 * \brief Convert equatorial coordinates to a Cartesian 3D unit vector.
 *
 * Translates Right Ascension (RA) and Declination (DEC) into 3D Cartesian coordinates (X, Y, Z).
 *
 * \param ra Right Ascension coordinate in radians.
 * \param dec Declination coordinate in radians.
 * \param v The output vertex structure where the 3D coordinates will be written.
 */
static inline void equ_to_kd_vertex(double ra, double dec, struct kd_vertex *v)
{
	double cos_dec = cos(dec);

	v->x = cos_dec * sin(ra);
	v->y = sin(dec);
	v->z = cos_dec * cos(ra);
}

/**
 * \brief Convert a Cartesian 3D unit vector back to equatorial coordinates.
 *
 * Computes Right Ascension (RA) and Declination (DEC) from a given 3D vector.
 *
 * \param ra Pointer to store the computed Right Ascension in radians.
 * \param dec Pointer to store the computed Declination in radians.
 * \param v The input 3D vertex to convert.
 */
static inline void kd_vertex_to_equ(double *ra, double *dec,
									struct kd_vertex *v)
{
	*dec = asin(v->y);
	*ra = atan2(v->x, v->z);
	if (*ra < 0.0)
		*ra += 2.0 * M_PI;
}

/**
 * \brief Extract the X coordinate value from a KD tree element array.
 *
 * \param x_base Base array of KD tree elements sorted or indexed.
 * \param x_idx The index of the element to retrieve.
 * \return The X coordinate of the element at the specified index.
 */
static inline double kd_get_x(struct kd_elem *x_base, int x_idx)
{
	return x_base[x_idx].base->v.x;
}

/**
 * \brief Extract the Y coordinate value from a KD tree element array.
 *
 * \param y_base Base array of KD tree elements sorted or indexed.
 * \param y_idx The index of the element to retrieve.
 * \return The Y coordinate of the element at the specified index.
 */
static inline double kd_get_y(struct kd_elem *y_base, int y_idx)
{
	return y_base[y_idx].base->v.y;
}

/**
 * \brief Extract the Z coordinate value from a KD tree element array.
 *
 * \param z_base Base array of KD tree elements sorted or indexed.
 * \param z_idx The index of the element to retrieve.
 * \return The Z coordinate of the element at the specified index.
 */
static inline double kd_get_z(struct kd_elem *z_base, int z_idx)
{
	return z_base[z_idx].base->v.z;
}

/**
 * \brief Comparator function to sort KD tree elements by their X coordinate.
 *
 * \param o1 First KD tree element pointer.
 * \param o2 Second KD tree element pointer.
 * \return -1 if o1 < o2, 1 if o1 > o2, 0 if equal.
 */
static int elem_x_cmp(const void *o1, const void *o2)
{
	const struct kd_elem *elem1 = o1, *elem2 = o2;

	if (elem1->base->v.x < elem2->base->v.x)
		return -1;
	else if (elem1->base->v.x > elem2->base->v.x)
		return 1;
	else
		return 0;
}

/**
 * \brief Comparator function to sort KD tree elements by their Y coordinate.
 *
 * \param o1 First KD tree element pointer.
 * \param o2 Second KD tree element pointer.
 * \return -1 if o1 < o2, 1 if o1 > o2, 0 if equal.
 */
static int elem_y_cmp(const void *o1, const void *o2)
{
	const struct kd_elem *elem1 = o1, *elem2 = o2;

	if (elem1->base->v.y < elem2->base->v.y)
		return -1;
	else if (elem1->base->v.y > elem2->base->v.y)
		return 1;
	else
		return 0;
}

/**
 * \brief Comparator function to sort KD tree elements by their Z coordinate.
 *
 * \param o1 First KD tree element pointer.
 * \param o2 Second KD tree element pointer.
 * \return -1 if o1 < o2, 1 if o1 > o2, 0 if equal.
 */
static int elem_z_cmp(const void *o1, const void *o2)
{
	const struct kd_elem *elem1 = o1, *elem2 = o2;

	if (elem1->base->v.z < elem2->base->v.z)
		return -1;
	else if (elem1->base->v.z > elem2->base->v.z)
		return 1;
	else
		return 0;
}

/**
 * \brief Check if a KD tree element falls within specified X coordinate bounds.
 *
 * \param elem The KD tree element to validate.
 * \param min The lower bound for the X coordinate.
 * \param max The upper bound for the X coordinate.
 * \return 1 if valid and unused, 0 otherwise.
 */
static inline int valid_x_elem(struct kd_elem *elem, double min, double max)
{
	if (elem->base->used)
		return 0;

	if (elem->base->v.x < min || elem->base->v.x > max)
		return 0;

	return 1;
}

/**
 * \brief Check if a KD tree element falls within specified Y coordinate bounds.
 *
 * \param elem The KD tree element to validate.
 * \param min The lower bound for the Y coordinate.
 * \param max The upper bound for the Y coordinate.
 * \return 1 if valid and unused, 0 otherwise.
 */
static inline int valid_y_elem(struct kd_elem *elem, double min, double max)
{
	if (elem->base->used)
		return 0;

	if (elem->base->v.y < min || elem->base->v.y > max)
		return 0;

	return 1;
}

/**
 * \brief Check if a KD tree element falls within specified Z coordinate bounds.
 *
 * \param elem The KD tree element to validate.
 * \param min The lower bound for the Z coordinate.
 * \param max The upper bound for the Z coordinate.
 * \return 1 if valid and unused, 0 otherwise.
 */
static inline int valid_z_elem(struct kd_elem *elem, double min, double max)
{
	if (elem->base->used)
		return 0;

	if (elem->base->v.z < min || elem->base->v.z > max)
		return 0;

	return 1;
}

/**
 * \brief Find the median element along the X axis within a given bounding box.
 *
 * Selects an unused element close to the median index in the X-sorted array that also falls within the supplied Y and Z spatial ranges.
 *
 * \param x_base Base array of elements sorted by X coordinate.
 * \param x_start Starting index for the search.
 * \param x_end Ending index for the search.
 * \param y_min Minimum Y coordinate boundary.
 * \param y_max Maximum Y coordinate boundary.
 * \param z_min Minimum Z coordinate boundary.
 * \param z_max Maximum Z coordinate boundary.
 * \return Pointer to the matched median element, or NULL if none fits the constraints.
 */
static struct kd_elem *elem_get_median_x_elem(struct kd_elem *x_base,
											  int x_start, int x_end,
											  double y_min, double y_max,
											  double z_min, double z_max)
{
	int size = x_end - x_start;
	int count = size >> 1;
	int index = count + x_start, i = 1;

	if (valid_y_elem(&x_base[index], y_min, y_max) &&
		valid_z_elem(&x_base[index], z_min, z_max))
		return &x_base[index];

	while (i <= count) {
		int start = index - i, end = index + i;

		if (valid_y_elem(&x_base[start], y_min, y_max) &&
			valid_z_elem(&x_base[start], z_min, z_max))
			return &x_base[start];

		if (valid_y_elem(&x_base[end], y_min, y_max) &&
			valid_z_elem(&x_base[end], z_min, z_max))
			return &x_base[end];

		i++;
	}

	/* ra end is not checked if gap is even */
	if ((size & 0x1) != 0 && valid_y_elem(&x_base[x_end], y_min, y_max) &&
		valid_z_elem(&x_base[x_end], z_min, z_max))
		return &x_base[x_end];

	return NULL;
}

/**
 * \brief Find the median element along the Y axis within a given bounding box.
 *
 * Selects an unused element close to the median index in the Y-sorted array that also falls within the supplied X and Z spatial ranges.
 *
 * \param y_base Base array of elements sorted by Y coordinate.
 * \param y_start Starting index for the search.
 * \param y_end Ending index for the search.
 * \param x_min Minimum X coordinate boundary.
 * \param x_max Maximum X coordinate boundary.
 * \param z_min Minimum Z coordinate boundary.
 * \param z_max Maximum Z coordinate boundary.
 * \return Pointer to the matched median element, or NULL if none fits the constraints.
 */
static struct kd_elem *elem_get_median_y_elem(struct kd_elem *y_base,
											  int y_start, int y_end,
											  double x_min, double x_max,
											  double z_min, double z_max)
{
	int size = y_end - y_start;
	int count = size >> 1;
	int index = count + y_start, i = 1;

	if (valid_x_elem(&y_base[index], x_min, x_max) &&
		valid_z_elem(&y_base[index], z_min, z_max))
		return &y_base[index];

	while (i <= count) {
		int start = index - i, end = index + i;

		if (valid_x_elem(&y_base[start], x_min, x_max) &&
			valid_z_elem(&y_base[start], z_min, z_max))
			return &y_base[start];

		if (valid_x_elem(&y_base[end], x_min, x_max) &&
			valid_z_elem(&y_base[end], z_min, z_max))
			return &y_base[end];

		i++;
	}

	/* ra end is not checked if gap is even */
	if ((size & 0x1) != 0 && valid_x_elem(&y_base[y_end], x_min, x_max) &&
		valid_z_elem(&y_base[y_end], z_min, z_max))
		return &y_base[y_end];

	return NULL;
}

/**
 * \brief Find the median element along the Z axis within a given bounding box.
 *
 * Selects an unused element close to the median index in the Z-sorted array that also falls within the supplied X and Y spatial ranges.
 *
 * \param z_base Base array of elements sorted by Z coordinate.
 * \param z_start Starting index for the search.
 * \param z_end Ending index for the search.
 * \param x_min Minimum X coordinate boundary.
 * \param x_max Maximum X coordinate boundary.
 * \param y_min Minimum Y coordinate boundary.
 * \param y_max Maximum Y coordinate boundary.
 * \return Pointer to the matched median element, or NULL if none fits the constraints.
 */
static struct kd_elem *elem_get_median_z_elem(struct kd_elem *z_base,
											  int z_start, int z_end,
											  double x_min, double x_max,
											  double y_min, double y_max)
{
	int size = z_end - z_start;
	int count = size >> 1;
	int index = count + z_start, i = 1;

	if (valid_x_elem(&z_base[index], x_min, x_max) &&
		valid_y_elem(&z_base[index], y_min, y_max))
		return &z_base[index];

	while (i <= count) {
		int start = index - i, end = index + i;

		if (valid_x_elem(&z_base[start], x_min, x_max) &&
			valid_y_elem(&z_base[start], y_min, y_max))
			return &z_base[start];

		if (valid_x_elem(&z_base[end], x_min, x_max) &&
			valid_y_elem(&z_base[end], y_min, y_max))
			return &z_base[end];

		i++;
	}

	/* ra end is not checked if gap is even */
	if ((size & 0x1) != 0 && valid_x_elem(&z_base[z_end], x_min, x_max) &&
		valid_y_elem(&z_base[z_end], y_min, y_max))
		return &z_base[z_end];

	return NULL;
}

/**
 * \brief Increment progress counter during KD tree generation.
 *
 * Updates progress and logs status when completion percentage milestones are hit.
 *
 * \param mbase The shared tree base state variable tracking limits and elements.
 */
static inline void mbase_inc(struct kd_base *mbase)
{
	mbase->count++;
	if (mbase->count >= mbase->total) {
		mbase->percent += 0.1;
		adb_info(mbase->db, ADB_LOG_CDS_KDTREE, "\r Building %3.1f percent ",
				 mbase->percent);
		mbase->count = 0;
	}
}

/**
 * \brief Partition tree logic over the X coordinate axis, returning a parent node.
 *
 * Recursively selects a pivot element near the median of the X coordinate bounds, and creates left and right child node sub-trees using alternating spatial axes.
 *
 * \param mbase The main context capturing tree structure states.
 * \param x_start X index partition start.
 * \param x_end X index partition end.
 * \param y_start Y index partition start.
 * \param y_end Y index partition end.
 * \param z_start Z index partition start.
 * \param z_end Z index partition end.
 * \return Parent node element structuring left and right sub-trees for this spatial division.
 */
static struct kd_base_elem *kd_x_select_elem(struct kd_base *mbase, int x_start,
											 int x_end, int y_start, int y_end,
											 int z_start, int z_end)
{
	struct kd_elem *x_base, *x_elem, *y_base, *z_base;
	struct kd_base_elem *child, *parent;
	double y_min, y_max, z_min, z_max;
	int x_id;

	x_base = mbase->x_base;
	y_base = mbase->y_base;
	z_base = mbase->z_base;

	/* get X median Elem in area */
	y_min = kd_get_y(y_base, y_start);
	y_max = kd_get_y(y_base, y_end);
	z_min = kd_get_z(z_base, z_start);
	z_max = kd_get_z(z_base, z_end);

	x_elem = elem_get_median_x_elem(x_base, x_start, x_end, y_min, y_max, z_min,
									z_max);
	if (x_elem == NULL)
		return NULL;

	mbase_inc(mbase);
	x_id = x_elem->eid;
	parent = x_elem->base;
	parent->used = 1;

	/* get LHS child */
	child =
		kd_y_select_elem(mbase, x_start, x_id, y_start, y_end, z_start, z_end);
	if (child) {
		child->kd.parent = parent->bid;
		parent->kd.child[0] = child->bid;
	} else
		parent->kd.child[0] = -1;

	/* get RHS child */
	if (x_id == x_end)
		child = kd_y_select_elem(mbase, x_id, x_end, y_start, y_end, z_start,
								 z_end);
	else
		child = kd_y_select_elem(mbase, x_id + 1, x_end, y_start, y_end,
								 z_start, z_end);
	if (child) {
		child->kd.parent = parent->bid;
		parent->kd.child[1] = child->bid;
	} else
		parent->kd.child[1] = -1;

	return parent;
}

/**
 * \brief Partition tree logic over the Y coordinate axis, returning a parent node.
 *
 * Recursively selects a pivot element near the median of the Y coordinate bounds, and creates left and right child node sub-trees using alternating spatial axes.
 *
 * \param mbase The main context capturing tree structure states.
 * \param x_start X index partition start.
 * \param x_end X index partition end.
 * \param y_start Y index partition start.
 * \param y_end Y index partition end.
 * \param z_start Z index partition start.
 * \param z_end Z index partition end.
 * \return Parent node element structuring left and right sub-trees for this spatial division.
 */
static struct kd_base_elem *kd_y_select_elem(struct kd_base *mbase, int x_start,
											 int x_end, int y_start, int y_end,
											 int z_start, int z_end)
{
	struct kd_elem *y_base, *y_elem, *x_base, *z_base;
	struct kd_base_elem *child, *parent;
	double x_min, x_max, z_min, z_max;
	int y_id;

	x_base = mbase->x_base;
	y_base = mbase->y_base;
	z_base = mbase->z_base;

	/* get DEC median Elem in area */
	x_min = kd_get_x(x_base, x_start);
	x_max = kd_get_x(x_base, x_end);
	z_min = kd_get_z(z_base, z_start);
	z_max = kd_get_z(z_base, z_end);

	y_elem = elem_get_median_y_elem(y_base, y_start, y_end, x_min, x_max, z_min,
									z_max);
	if (y_elem == NULL)
		return NULL;

	mbase_inc(mbase);
	y_id = y_elem->eid;
	parent = y_elem->base;
	parent->used = 1;

	/* get LHS child */
	child =
		kd_z_select_elem(mbase, x_start, x_end, y_start, y_id, z_start, z_end);
	if (child) {
		child->kd.parent = parent->bid;
		parent->kd.child[0] = child->bid;
	} else
		parent->kd.child[0] = -1;

	/* get RHS child */
	if (y_id == y_end)
		child = kd_z_select_elem(mbase, x_start, x_end, y_id, y_end, z_start,
								 z_end);
	else
		child = kd_z_select_elem(mbase, x_start, x_end, y_id + 1, y_end,
								 z_start, z_end);
	if (child) {
		child->kd.parent = parent->bid;
		parent->kd.child[1] = child->bid;
	} else
		parent->kd.child[1] = -1;

	return parent;
}

/**
 * \brief Partition tree logic over the Z coordinate axis, returning a parent node.
 *
 * Recursively selects a pivot element near the median of the Z coordinate bounds, and creates left and right child node sub-trees using alternating spatial axes.
 *
 * \param mbase The main context capturing tree structure states.
 * \param x_start X index partition start.
 * \param x_end X index partition end.
 * \param y_start Y index partition start.
 * \param y_end Y index partition end.
 * \param z_start Z index partition start.
 * \param z_end Z index partition end.
 * \return Parent node element structuring left and right sub-trees for this spatial division.
 */
static struct kd_base_elem *kd_z_select_elem(struct kd_base *mbase, int x_start,
											 int x_end, int y_start, int y_end,
											 int z_start, int z_end)
{
	struct kd_elem *z_base, *z_elem, *x_base, *y_base;
	struct kd_base_elem *child, *parent;
	double x_min, x_max, y_min, y_max;
	int z_id;

	x_base = mbase->x_base;
	y_base = mbase->y_base;
	z_base = mbase->z_base;

	/* get DEC median Elem in area */
	x_min = kd_get_x(x_base, x_start);
	x_max = kd_get_x(x_base, x_end);
	y_min = kd_get_y(y_base, y_start);
	y_max = kd_get_y(y_base, y_end);

	z_elem = elem_get_median_z_elem(z_base, z_start, z_end, x_min, x_max, y_min,
									y_max);
	if (z_elem == NULL)
		return NULL;

	mbase_inc(mbase);
	z_id = z_elem->eid;
	parent = z_elem->base;
	parent->used = 1;

	/* get LHS child */
	child =
		kd_x_select_elem(mbase, x_start, x_end, y_start, y_end, z_start, z_id);
	if (child) {
		child->kd.parent = parent->bid;
		parent->kd.child[0] = child->bid;
	} else
		parent->kd.child[0] = -1;

	/* get RHS child */
	if (z_id == z_end)
		child = kd_x_select_elem(mbase, x_start, x_end, y_start, y_end, z_id,
								 z_end);
	else
		child = kd_x_select_elem(mbase, x_start, x_end, y_start, y_end,
								 z_id + 1, z_end);
	if (child) {
		child->kd.parent = parent->bid;
		parent->kd.child[1] = child->bid;
	} else
		parent->kd.child[1] = -1;

	return parent;
}

/**
 * \brief Rotate dynamically the pivoting logic across X, Y, and Z axes.
 *
 * Used sequentially during search routines to iterate through KD tree bounds checking dynamically.
 *
 * \param pivot The identifier of the active KD tree sorting axis.
 * \return The identifier of the next axis to utilize logically.
 */
static enum kd_pivot pivot_next(enum kd_pivot pivot)
{
	switch (pivot) {
	case KD_PIVOT_X:
		return KD_PIVOT_Y;
	case KD_PIVOT_Y:
		return KD_PIVOT_Z;
	case KD_PIVOT_Z:
		return KD_PIVOT_X;
	default:
		return 0;
	}
}

/* copy base elems into radec arrays with a pointer back to their base elem */
/**
 * \brief Allocate Cartesian indices from equatorial mappings within tree elements.
 *
 * Populates structural sub-indices mapping (X, Y, Z arrays) with their identical corresponding tree elements.
 *
 * \param base The raw unstructured array containing KD base elements.
 * \param table Reference to the active dataset populated within the table context.
 */
static void elem_create_xyz(struct kd_base_elem *base, struct adb_table *table)
{
	struct kd_elem *x_base, *y_base, *z_base;
	int i;

	x_base = (struct kd_elem *)(base + table->object.count);
	y_base = x_base + table->object.count;
	z_base = y_base + table->object.count;

	for (i = 0; i < table->object.count; i++) {
		equ_to_kd_vertex(base[i].object->ra, base[i].object->dec, &base[i].v);
		x_base[i].base = &base[i];
		y_base[i].base = &base[i];
		z_base[i].base = &base[i];
	}
}

/**
 * \brief Helper to incrementally recursively insert single astronomical layout objects dynamically.
 *
 * Maps active objects belonging to an HTM structure branch directly into chronological tree indexes.
 *
 * \param init Operational initialization configuration tree index parameters.
 * \param trixel Working structure branch node handling nested identifiers natively.
 */
static void insert_elem_object(struct kd_init *init, struct htm_trixel *trixel)
{
	struct adb_object *object;
	struct kd_base_elem *base = &init->kbase->base[init->iid];

	if (!trixel)
		return;

	/* insert children if this trixel has no objects */
	if (!trixel->data[init->table_id].num_objects)
		goto children;

	/* insert objects into elems */
	object = trixel->data[init->table_id].objects;

	while (object) {
		base->object = object;
		base->bid = init->iid++;
		base++;
		object = object->import.next;
	}

children:
	if (!trixel->child)
		return;

	insert_elem_object(init, &trixel->child[0]);
	insert_elem_object(init, &trixel->child[1]);
	insert_elem_object(init, &trixel->child[2]);
	insert_elem_object(init, &trixel->child[3]);
}

/**
 * \brief Copy sequential object layout arrays linearly from HTM node configurations into tree contexts.
 *
 * \param db Primary catalog mapping structure database correctly variables strings cleanly arrays smoothly fields.
 * \param table Safely populated objects metrics bounds defining safely cleanly definitions arrays boundaries sizes strings structs string structs sizes dynamically comfortably strings smoothly.
 * \param mbase Extracted base allocation limits limits mapping securely variables elegantly cleanly mapping structures tracking safely stably strings neatly fields elegantly correctly securely parameters successfully.
 */
static void insert_elem_objects(struct adb_db *db, struct adb_table *table,
								struct kd_base *mbase)
{
	struct htm *htm = db->htm;
	struct kd_init init;

	/* create array or elems for HTM object order */
	init.kbase = mbase;
	init.db = db;
	init.table = table;
	init.table_id = table->id;
	init.iid = 0;

	insert_elem_object(&init, &htm->N[0]);
	insert_elem_object(&init, &htm->N[1]);
	insert_elem_object(&init, &htm->N[2]);
	insert_elem_object(&init, &htm->N[3]);
	insert_elem_object(&init, &htm->S[0]);
	insert_elem_object(&init, &htm->S[1]);
	insert_elem_object(&init, &htm->S[2]);
	insert_elem_object(&init, &htm->S[3]);
}

/**
 * \brief Synchronize finalized KD tree pointers back into target adb_objects.
 *
 * Assures objects retain their location and path within the finalized tree structure for rapid future reference.
 *
 * \param base Pointer to the array of KD base elements.
 * \param table Pointer to the table holding the objects to be updated.
 * \param root Pointer to the root element of the built KD tree.
 */
static void update_objects(struct kd_base_elem *base, struct adb_table *table,
						   struct kd_base_elem *root)
{
	int i;

	for (i = 0; i < table->object.count; i++) {
		base[i].object->import.kd->child[0] = base[i].kd.child[0];
		base[i].object->import.kd->child[1] = base[i].kd.child[1];
		base[i].object->import.kd->parent = base[i].kd.parent;
		base[i].object->import.kd->index = i;
	}

	table->import.kd_root = root->bid;
}

/**
 * \brief Build a fully structured KD-tree encompassing all objects within a dataset.
 *
 * \param db The active database context.
 * \param table The table structure whose objects will be indexed into the tree.
 * \return 0 on success, or a negative error code on failure.
 */
int import_build_kdtree(struct adb_db *db, struct adb_table *table)
{
	struct kd_base mbase;
	struct kd_base_elem *base, *root;
	int i, ret = 0;

	if (table->object.count <= 0) {
		adb_error(db, "error: table %s is empty\n", table->cds.name);
		return -EINVAL;
	}

	/* allocate idx elem, ra elem  and dec elem arrays */
	base = calloc(1, (sizeof(struct kd_base_elem) * table->object.count) +
						 (sizeof(struct kd_elem) * table->object.count * 3));
	if (base == NULL)
		return -ENOMEM;

	mbase.db = db;
	mbase.base = base;
	mbase.count = 0;
	mbase.percent = 0.0;
	mbase.total = table->object.count / 1000;
	mbase.x_base = (struct kd_elem *)(base + table->object.count);
	mbase.y_base = mbase.x_base + table->object.count;
	mbase.z_base = mbase.y_base + table->object.count;

	adb_info(db, ADB_LOG_CDS_KDTREE,
			 "Preparing KD Tree for %s with %d objects\n", table->cds.name,
			 table->object.count);

	/* Get objects from HTM */
	insert_elem_objects(db, table, &mbase);

	/* create RA and DEC elems */
	elem_create_xyz(base, table);

	adb_info(db, ADB_LOG_CDS_KDTREE, "Sorting KD Tree source objects...\n");

	/* sort RA and DEC elems in order */
	qsort(mbase.x_base, table->object.count, sizeof(struct kd_elem),
		  elem_x_cmp);
	qsort(mbase.y_base, table->object.count, sizeof(struct kd_elem),
		  elem_y_cmp);
	qsort(mbase.z_base, table->object.count, sizeof(struct kd_elem),
		  elem_z_cmp);

	/* give XYZ elems index numbers */
	for (i = 0; i < table->object.count; i++) {
		mbase.x_base[i].eid = i;
		mbase.y_base[i].eid = i;
		mbase.z_base[i].eid = i;
	}

	/* build the KD tree starting on X */
	adb_info(db, ADB_LOG_CDS_KDTREE, "\r Building 0.0 percent ");
	root = kd_x_select_elem(&mbase, 0, table->object.count - 1, 0,
							table->object.count - 1, 0,
							table->object.count - 1);
	if (root == NULL) {
		adb_error(db, "cant get root elem in KD tree\n");
		ret = -EINVAL;
		goto out;
	}
	adb_info(db, ADB_LOG_CDS_KDTREE, "\r Building 100.0 percent\n");

	/* update the objects with KD data */
	update_objects(base, table, root);

	adb_info(db, ADB_LOG_CDS_KDTREE,
			 "Completed KD Tree for %s with %d objects at root %d\n",
			 table->cds.name, table->object.count, root->bid);

out:
	free(base);
	return ret;
}

/**
 * \brief Calculate spherical angular separation between equatorial coordinates and a Cartesian vector.
 *
 * \param ra1 Right ascension of the first point in radians.
 * \param dec1 Declination of the first point in radians.
 * \param v Pointer to the KD vertex indicating the 3D position of the second point.
 * \return The angular distance between the points in radians.
 */
static double get_distance(double ra1, double dec1, struct kd_vertex *v)
{
	double ra2, dec2;
	double x, y, z;

	kd_vertex_to_equ(&ra2, &dec2, v);

	x = (cos(dec1) * sin(dec2)) - (sin(dec1) * cos(dec2) * cos(ra2 - ra1));
	y = cos(dec2) * sin(ra2 - ra1);
	z = (sin(dec1) * sin(dec2)) + (cos(dec1) * cos(dec2) * cos(ra2 - ra1));

	x = x * x;
	y = y * y;
	return atan2(sqrt(x + y), z);
}

/*! \struct kd_get_data
 * \brief get kd data
 * \ingroup kdtree
 */
struct kd_get_data {
	struct adb_table *table; /*!< table */
	const struct adb_object *closest; /*!< closest object */
	const struct adb_object *exclude; /*!< excluded object */
	double distance; /*!< distance */
	struct kd_vertex target; /*!< target */
	double ra; /*!< Right Ascension */
	double dec; /*!< Declination */
};

/**
 * \brief Recursively traverse the tree to fetch the nearest matching object.
 *
 * Navigates nodes spatially to locate the closest astronomical object to a target coordinate.
 *
 * \param kd Search configuration variables, boundaries, and current closest tracker structure.
 * \param node The current node ID under evaluation.
 * \param pivot The active spatial axis being tracked for this depth.
 * \return 0 if traversal should continue up the stack, 1 if leaf node end reached.
 */
int get_nearest(struct kd_get_data *kd, int node, enum kd_pivot pivot)
{
	struct adb_table *table = kd->table;
	const struct adb_object *current =
		(const void *)table->objects + node * table->object.bytes;
	struct kd_vertex plane_vertex, current_vertex;
	double d;
	int end;

	/* are we at leaf end ? */
	if (node < 0)
		return 1;

	/* get xyz for current */
	equ_to_kd_vertex(current->ra, current->dec, &current_vertex);

	/* get the new node */
	switch (pivot) {
	case KD_PIVOT_X:
		if (kd->target.x < current_vertex.x)
			node = current->kd.child[0];
		else
			node = current->kd.child[1];
		break;
	case KD_PIVOT_Y:
		if (kd->target.y < current_vertex.y)
			node = current->kd.child[0];
		else
			node = current->kd.child[1];
		break;
	case KD_PIVOT_Z:
		if (kd->target.z < current_vertex.z)
			node = current->kd.child[0];
		else
			node = current->kd.child[1];
		break;
	}

	/* get to leaf */
	end = get_nearest(kd, node, pivot_next(pivot));
	if (end && kd->closest == NULL) {
		/* at leaf, so unwind and this will be initial closest */
		kd->closest = current;
		kd->distance = get_distance(kd->ra, kd->dec, &current_vertex);
		return 0;
	}

	/* unwinding, check object for closest */
	d = get_distance(kd->ra, kd->dec, &current_vertex);

	/* update, since object is closest */
	if (d < kd->distance && current != kd->exclude) {
		kd->closest = current;
		kd->distance = d;
	}

	/* calc distance to ther pivot plane */
	switch (pivot) {
	case KD_PIVOT_X:
		plane_vertex.x = current_vertex.x;
		plane_vertex.y = kd->target.y;
		plane_vertex.z = kd->target.z;
		break;
	case KD_PIVOT_Y:
		plane_vertex.x = kd->target.x;
		plane_vertex.y = current_vertex.y;
		plane_vertex.z = kd->target.z;
		break;
	case KD_PIVOT_Z:
		plane_vertex.x = kd->target.x;
		plane_vertex.y = kd->target.y;
		plane_vertex.z = current_vertex.z;
		break;
	}

	d = get_distance(kd->ra, kd->dec, &plane_vertex);

	/* pivot plane closer so check */
	if (d < kd->distance) {
		if (node == current->kd.child[0])
			node = current->kd.child[1];
		else
			node = current->kd.child[0];

		end = get_nearest(kd, node, pivot_next(pivot));
	}

	return 0;
}

#if CHECK_KD_TREE
static void check_search(struct adb_table *table, double ra, double dec,
						 double distance, const struct adb_object *object)
{
	const struct adb_object *current;
	struct kd_vertex current_v, pos_v;
	double d;
	int i;

	/* get xyz for pos */
	equ_to_kd_vertex(ra, dec, &pos_v);

	for (i = 0; i < table->object.count; i++) {
		current = (const void *)table->objects + i * table->object.bytes;

		/* get xyz for current */
		equ_to_kd_vertex(current->ra, current->dec, &current_v);

		d = get_distance(current->ra, current->dec, &pos_v);

		if (d < distance && current != object) {
			printf("closer new %9.9f old %9.9f id %ld ra %f dec"
				   "%f X %f Y %f Z %f\n",
				   d, distance, current->id, adb_object_ra(current) * R2D,
				   adb_object_dec(current) * R2D, current_v.x, current_v.y,
				   current_v.z);
			distance = d;
		}
	}
}
#endif

/**
 * \brief Retrieve the closest object to a supplied spatial coordinate pair.
 *
 * \param set The active object set specifying table and constraints.
 * \param ra Right Ascension coordinate limit target in radians.
 * \param dec Declination angle coordinate target in radians.
 * \return Pointer to the matched nearest object, or NULL if none evaluated.
 */
const struct adb_object *
adb_table_set_get_nearest_on_pos(struct adb_object_set *set, double ra,
								 double dec)
{
	struct adb_table *table = set->table;
	struct kd_get_data kd;

	/* get xyz for object */
	equ_to_kd_vertex(ra, dec, &kd.target);

	kd.closest = NULL;
	kd.distance = 10e9;
	kd.table = table;
	kd.exclude = NULL;
	kd.ra = ra;
	kd.dec = dec;

	get_nearest(&kd, table->kd_root, KD_PIVOT_X);
#if CHECK_KD_TREE
	check_search(table, ra, dec, kd.distance, kd.closest);
#endif
	return kd.closest;
}

/**
 * \brief Retrieve the nearest object to a given reference object.
 *
 * Functions similarly to coordinate-based neighbor checking, but explicitly excludes the input object itself from the results so that it evaluates its closest distinct neighbor.
 *
 * \param set Active object scope set parameters.
 * \param object Target object serving as search origin coordinate.
 * \return Pointer to the matched distinct nearest neighbor object, or NULL.
 */
const struct adb_object *
adb_table_set_get_nearest_on_object(struct adb_object_set *set,
									const struct adb_object *object)
{
	struct adb_table *table = set->table;
	struct kd_get_data kd;

	/* get xyz for object */
	equ_to_kd_vertex(adb_object_ra(object), adb_object_dec(object), &kd.target);

	kd.closest = NULL;
	kd.distance = 10e9;
	kd.exclude = object;
	kd.ra = adb_object_ra(object);
	kd.dec = adb_object_dec(object);
	kd.table = table;

	get_nearest(&kd, table->kd_root, KD_PIVOT_X);

#if CHECK_KD_TREE
	check_search(table, adb_object_ra(object), adb_object_dec(object),
				 kd.distance, object);
#endif
	return kd.closest;
}
