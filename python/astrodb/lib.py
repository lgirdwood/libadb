import ctypes
import ctypes.util
import os
import sys

# Attempt to load libastrodb.so
_lib_name = "astrodb"
_lib_path = ctypes.util.find_library(_lib_name)

# If not found via standard LD_LIBRARY_PATH, check if running from source tree build directory
if not _lib_path:
    _src_build_path = os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'src', 'libastrodb.so')
    if os.path.exists(_src_build_path):
        _lib_path = _src_build_path

if not _lib_path:
    # Final fallback, try straight filename and hope OS resolves it
    _lib_path = "libastrodb.so"

try:
    libadb = ctypes.CDLL(_lib_path)
except OSError as e:
    print(f"Error loading libastrodb.so: {e}", file=sys.stderr)
    print("Ensure libastrodb.so is installed in your LD_LIBRARY_PATH or built in ../build/src/", file=sys.stderr)
    raise

ADB_CTYPE_INT = 0
ADB_CTYPE_SHORT = 1
ADB_CTYPE_DOUBLE = 2
ADB_CTYPE_DEGREES = 3
ADB_CTYPE_FLOAT = 4
ADB_CTYPE_STRING = 5

# Define opaque pointer types to match C structures
class adb_library_p(ctypes.c_void_p): pass
class adb_db_p(ctypes.c_void_p): pass
class adb_search_p(ctypes.c_void_p): pass
class adb_object_set_p(ctypes.c_void_p): pass
class adb_solve_p(ctypes.c_void_p): pass
class adb_solve_solution_p(ctypes.c_void_p): pass

# adb_object (opaque representation for results)
class adb_object_key_union(ctypes.Union):
    _fields_ = [
        ("id", ctypes.c_ulong),
        ("designation", ctypes.c_char * 16)
    ]

class adb_object(ctypes.Structure):
    _anonymous_ = ("key",)
    _fields_ = [
        ("key", adb_object_key_union),
        ("ra", ctypes.c_double),
        ("dec", ctypes.c_double),
        ("mag", ctypes.c_float),
        ("size", ctypes.c_float),
        ("pad", ctypes.c_byte * 16) # For the internal kd tree union
    ]

adb_object_p = ctypes.POINTER(adb_object)

class adb_object_head(ctypes.Structure):
    _fields_ = [
        ("objects", ctypes.c_void_p),
        ("count", ctypes.c_uint)
    ]
adb_object_head_p = ctypes.POINTER(adb_object_head)

class adb_pobject(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_int),
        ("y", ctypes.c_int),
        ("adu", ctypes.c_uint),
        ("extended", ctypes.c_uint)
    ]
adb_pobject_p = ctypes.POINTER(adb_pobject)

class adb_solve_object(ctypes.Structure):
    _fields_ = [
        ("object", adb_object_p),
        ("pobject", adb_pobject),
        ("ra", ctypes.c_double),
        ("dec", ctypes.c_double),
        ("mag", ctypes.c_float),
        ("mean", ctypes.c_float),
        ("sigma", ctypes.c_float)
    ]
adb_solve_object_p = ctypes.POINTER(adb_solve_object)


### Library Bindings ###

# struct adb_library *adb_open_library(const char *host, const char *remote, const char *local);
libadb.adb_open_library.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
libadb.adb_open_library.restype = adb_library_p

# void adb_close_library(struct adb_library *lib);
libadb.adb_close_library.argtypes = [adb_library_p]
libadb.adb_close_library.restype = None

# const char *adb_get_version(void);
libadb.adb_get_version.argtypes = []
libadb.adb_get_version.restype = ctypes.c_char_p


### Database Bindings ###

# struct adb_db *adb_create_db(struct adb_library *lib, int depth, int tables);
libadb.adb_create_db.argtypes = [adb_library_p, ctypes.c_int, ctypes.c_int]
libadb.adb_create_db.restype = adb_db_p

# void adb_db_free(struct adb_db *db);
libadb.adb_db_free.argtypes = [adb_db_p]
libadb.adb_db_free.restype = None


### Table Bindings ###

# int adb_table_open(struct adb_db *db, const char *cat_class, const char *cat_id, const char *table_name);
libadb.adb_table_open.argtypes = [adb_db_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
libadb.adb_table_open.restype = ctypes.c_int

# int adb_table_close(struct adb_db *db, int table_id);
libadb.adb_table_close.argtypes = [adb_db_p, ctypes.c_int]
libadb.adb_table_close.restype = ctypes.c_int

# int adb_table_get_count(struct adb_db *db, int table_id);
libadb.adb_table_get_count.argtypes = [adb_db_p, ctypes.c_int]
libadb.adb_table_get_count.restype = ctypes.c_int

# int adb_table_hash_key(struct adb_db *db, int table_id, const char *key);
libadb.adb_table_hash_key.argtypes = [adb_db_p, ctypes.c_int, ctypes.c_char_p]
libadb.adb_table_hash_key.restype = ctypes.c_int

# int adb_table_get_size(struct adb_db *db, int table_id);
libadb.adb_table_get_size.argtypes = [adb_db_p, ctypes.c_int]
libadb.adb_table_get_size.restype = ctypes.c_int

# int adb_table_get_object_size(struct adb_db *db, int table_id);
libadb.adb_table_get_object_size.argtypes = [adb_db_p, ctypes.c_int]
libadb.adb_table_get_object_size.restype = ctypes.c_int


### Dataset Constraining (Object Sets) ###

# struct adb_object_set *adb_table_set_new(struct adb_db *db, int table_id);
libadb.adb_table_set_new.argtypes = [adb_db_p, ctypes.c_int]
libadb.adb_table_set_new.restype = adb_object_set_p

# int adb_table_set_constraints(struct adb_object_set *set, double ra, double dec, double fov, double min_Z, double max_Z);
libadb.adb_table_set_constraints.argtypes = [adb_object_set_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]
libadb.adb_table_set_constraints.restype = ctypes.c_int

# void adb_table_set_free(struct adb_object_set *set);
libadb.adb_table_set_free.argtypes = [adb_object_set_p]
libadb.adb_table_set_free.restype = None

# int adb_set_get_objects(struct adb_object_set *set);
libadb.adb_set_get_objects.argtypes = [adb_object_set_p]
libadb.adb_set_get_objects.restype = ctypes.c_int

# int adb_set_get_count(struct adb_object_set *set);
libadb.adb_set_get_count.argtypes = [adb_object_set_p]
libadb.adb_set_get_count.restype = ctypes.c_int

# int adb_set_hash_key(struct adb_object_set *set, const char *key);
libadb.adb_set_hash_key.argtypes = [adb_object_set_p, ctypes.c_char_p]
libadb.adb_set_hash_key.restype = ctypes.c_int

# int adb_set_get_object(struct adb_object_set *set, const void *id, const char *field, const struct adb_object **object);
libadb.adb_set_get_object.argtypes = [adb_object_set_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(adb_object_p)]
libadb.adb_set_get_object.restype = ctypes.c_int

# int adb_table_get_object(struct adb_db *db, int table_id, const void *id, const char *field, const struct adb_object **object);
libadb.adb_table_get_object.argtypes = [adb_db_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(adb_object_p)]
libadb.adb_table_get_object.restype = ctypes.c_int

# const struct adb_object *adb_table_set_get_nearest_on_object(struct adb_object_set *set, const struct adb_object *object);
libadb.adb_table_set_get_nearest_on_object.argtypes = [adb_object_set_p, adb_object_p]
libadb.adb_table_set_get_nearest_on_object.restype = adb_object_p

# const struct adb_object *adb_table_set_get_nearest_on_pos(struct adb_object_set *set, double ra, double dec);
libadb.adb_table_set_get_nearest_on_pos.argtypes = [adb_object_set_p, ctypes.c_double, ctypes.c_double]
libadb.adb_table_set_get_nearest_on_pos.restype = adb_object_p

# struct adb_object_head *adb_set_get_head(struct adb_object_set *set);
libadb.adb_set_get_head.argtypes = [adb_object_set_p]
libadb.adb_set_get_head.restype = adb_object_head_p


### Search Bindings ###

# enum constants
ADB_OP_AND = 0
ADB_OP_OR = 1

ADB_COMP_LT = 0
ADB_COMP_GT = 1
ADB_COMP_EQ = 2
ADB_COMP_NE = 3

# struct adb_search *adb_search_new(struct adb_db *db, int table_id);
libadb.adb_search_new.argtypes = [adb_db_p, ctypes.c_int]
libadb.adb_search_new.restype = adb_search_p

# void adb_search_free(struct adb_search *search);
libadb.adb_search_free.argtypes = [adb_search_p]
libadb.adb_search_free.restype = None

# int adb_search_add_operator(struct adb_search *search, enum adb_operator op);
libadb.adb_search_add_operator.argtypes = [adb_search_p, ctypes.c_int]
libadb.adb_search_add_operator.restype = ctypes.c_int

# int adb_search_add_comparator(struct adb_search *search, const char *field, enum adb_comparator comp, const char *value);
libadb.adb_search_add_comparator.argtypes = [adb_search_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p]
libadb.adb_search_add_comparator.restype = ctypes.c_int

adb_custom_comparator = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p)

# int adb_search_add_custom_comparator(struct adb_search *search, adb_custom_comparator comp);
libadb.adb_search_add_custom_comparator.argtypes = [adb_search_p, adb_custom_comparator]
libadb.adb_search_add_custom_comparator.restype = ctypes.c_int

# int adb_search_get_results(struct adb_search *search, struct adb_object_set *set, const struct adb_object **objects[]);
libadb.adb_search_get_results.argtypes = [adb_search_p, adb_object_set_p, ctypes.c_void_p]
libadb.adb_search_get_results.restype = ctypes.c_int

# int adb_search_get_hits(struct adb_search *search);
libadb.adb_search_get_hits.argtypes = [adb_search_p]
libadb.adb_search_get_hits.restype = ctypes.c_int

# int adb_search_get_tests(struct adb_search *search);
libadb.adb_search_get_tests.argtypes = [adb_search_p]
libadb.adb_search_get_tests.restype = ctypes.c_int


### Import Bindings ###

# Constants
ADB_SCHEMA_NAME_SIZE = 32
ADB_SCHEMA_SYMBOL_SIZE = 8
ADB_SCHEMA_UNITS_SIZE = 8

ADB_CTYPE_INT = 0
ADB_CTYPE_SHORT = 1
ADB_CTYPE_DOUBLE = 2
ADB_CTYPE_DEGREES = 3
ADB_CTYPE_FLOAT = 4
ADB_CTYPE_STRING = 5
ADB_CTYPE_SIGN = 6
ADB_CTYPE_DOUBLE_HMS_HRS = 7
ADB_CTYPE_DOUBLE_HMS_MINS = 8
ADB_CTYPE_DOUBLE_HMS_SECS = 9
ADB_CTYPE_DOUBLE_DMS_DEGS = 10
ADB_CTYPE_DOUBLE_DMS_MINS = 11
ADB_CTYPE_DOUBLE_DMS_SECS = 12
ADB_CTYPE_DOUBLE_MPC = 13
ADB_CTYPE_NULL = 14

ADB_IMPORT_INC = 0
ADB_IMPORT_DEC = 1

adb_field_import1 = ctypes.CFUNCTYPE(ctypes.c_int, adb_object_p, ctypes.c_int, ctypes.c_char_p)

class adb_schema_field(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * ADB_SCHEMA_NAME_SIZE),
        ("symbol", ctypes.c_char * ADB_SCHEMA_SYMBOL_SIZE),
        ("struct_offset", ctypes.c_int32),
        ("struct_bytes", ctypes.c_int32),
        ("group_offset", ctypes.c_int32),
        ("group_posn", ctypes.c_int32),
        ("text_size", ctypes.c_int32),
        ("text_offset", ctypes.c_int32),
        ("type", ctypes.c_int), # enum adb_ctype
        ("units", ctypes.c_char * ADB_SCHEMA_UNITS_SIZE),
        ("import_cb", adb_field_import1),
    ]

# int adb_table_import_new(struct adb_db *db, const char *cat_class, const char *cat_id, const char *table_name, const char *depth_field, float min_limit, float max_limit, adb_import_type otype);
libadb.adb_table_import_new.argtypes = [adb_db_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_float, ctypes.c_float, ctypes.c_int]
libadb.adb_table_import_new.restype = ctypes.c_int

# int adb_table_import_schema(struct adb_db *db, int table_id, struct adb_schema_field *schema, int num_schema_fields, int object_size);
libadb.adb_table_import_schema.argtypes = [adb_db_p, ctypes.c_int, ctypes.POINTER(adb_schema_field), ctypes.c_int, ctypes.c_int]
libadb.adb_table_import_schema.restype = ctypes.c_int

# int adb_table_import_field(struct adb_db *db, int table_id, const char *field, const char *alt, int flags);
libadb.adb_table_import_field.argtypes = [adb_db_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
libadb.adb_table_import_field.restype = ctypes.c_int

# int adb_table_import(struct adb_db *db, int table_id);
libadb.adb_table_import.argtypes = [adb_db_p, ctypes.c_int]
libadb.adb_table_import.restype = ctypes.c_int

# adb_ctype adb_table_get_field_type(struct adb_db *db, int table_id, const char *field);
libadb.adb_table_get_field_type.argtypes = [adb_db_p, ctypes.c_int, ctypes.c_char_p]
libadb.adb_table_get_field_type.restype = ctypes.c_int

# int adb_table_get_field_offset(struct adb_db *db, int table_id, const char *field);
libadb.adb_table_get_field_offset.argtypes = [adb_db_p, ctypes.c_int, ctypes.c_char_p]
libadb.adb_table_get_field_offset.restype = ctypes.c_int

# int adb_table_import_alt_dataset(struct adb_db *db, int table_id, const char *dataset, int num_objects);
libadb.adb_table_import_alt_dataset.argtypes = [adb_db_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
libadb.adb_table_import_alt_dataset.restype = ctypes.c_int

# void adb_set_msg_level(struct adb_db *db, enum adb_msg_level level);
libadb.adb_set_msg_level.argtypes = [adb_db_p, ctypes.c_int]
libadb.adb_set_msg_level.restype = None

# void adb_set_log_level(struct adb_db *db, unsigned int log);
libadb.adb_set_log_level.argtypes = [adb_db_p, ctypes.c_uint]
libadb.adb_set_log_level.restype = None


### Solver Bindings ###

# Constants
ADB_CONSTRAINT_MAG = 0
ADB_CONSTRAINT_FOV = 1
ADB_CONSTRAINT_RA = 2
ADB_CONSTRAINT_DEC = 3
ADB_CONSTRAINT_AREA = 4
ADB_CONSTRAINT_JD = 5
ADB_CONSTRAINT_POBJECTS = 6

ADB_FIND_ALL = 1 << 0
ADB_FIND_FIRST = 1 << 1
ADB_FIND_PLANETS = 1 << 2
ADB_FIND_ASTEROIDS = 1 << 3
ADB_FIND_PROPER_MOTION = 1 << 4

ADB_BOUND_TOP_RIGHT = 0
ADB_BOUND_TOP_LEFT = 1
ADB_BOUND_BOTTOM_RIGHT = 2
ADB_BOUND_BOTTOM_LEFT = 3
ADB_BOUND_CENTRE = 4

# struct adb_solve *adb_solve_new(struct adb_db *db, int table_id);
libadb.adb_solve_new.argtypes = [adb_db_p, ctypes.c_int]
libadb.adb_solve_new.restype = adb_solve_p

# void adb_solve_free(struct adb_solve *solve);
libadb.adb_solve_free.argtypes = [adb_solve_p]
libadb.adb_solve_free.restype = None

# int adb_solve_set_magnitude_delta(struct adb_solve *solve, double delta_mag);
libadb.adb_solve_set_magnitude_delta.argtypes = [adb_solve_p, ctypes.c_double]
libadb.adb_solve_set_magnitude_delta.restype = ctypes.c_int

# int adb_solve_set_distance_delta(struct adb_solve *solve, double delta_pixels);
libadb.adb_solve_set_distance_delta.argtypes = [adb_solve_p, ctypes.c_double]
libadb.adb_solve_set_distance_delta.restype = ctypes.c_int

# int adb_solve_set_pa_delta(struct adb_solve *solve, double delta_degrees);
libadb.adb_solve_set_pa_delta.argtypes = [adb_solve_p, ctypes.c_double]
libadb.adb_solve_set_pa_delta.restype = ctypes.c_int

# int adb_solve_add_plate_object(struct adb_solve *solve, struct adb_pobject *pobject);
libadb.adb_solve_add_plate_object.argtypes = [adb_solve_p, ctypes.POINTER(adb_pobject)]
libadb.adb_solve_add_plate_object.restype = ctypes.c_int

# int adb_solve_constraint(struct adb_solve *solve, enum adb_constraint type, double min, double max);
libadb.adb_solve_constraint.argtypes = [adb_solve_p, ctypes.c_int, ctypes.c_double, ctypes.c_double]
libadb.adb_solve_constraint.restype = ctypes.c_int

# int adb_solve(struct adb_solve *solve, struct adb_object_set *set, enum adb_find find);
libadb.adb_solve.argtypes = [adb_solve_p, adb_object_set_p, ctypes.c_int]
libadb.adb_solve.restype = ctypes.c_int

# struct adb_solve_solution *adb_solve_get_solution(struct adb_solve *solve, unsigned int solution);
libadb.adb_solve_get_solution.argtypes = [adb_solve_p, ctypes.c_uint]
libadb.adb_solve_get_solution.restype = adb_solve_solution_p

# int adb_solution_get_objects(struct adb_solve_solution *solution);
libadb.adb_solution_get_objects.argtypes = [adb_solve_solution_p]
libadb.adb_solution_get_objects.restype = ctypes.c_int

# double adb_solution_divergence(struct adb_solve_solution *solution);
libadb.adb_solution_divergence.argtypes = [adb_solve_solution_p]
libadb.adb_solution_divergence.restype = ctypes.c_double

# struct adb_solve_object *adb_solution_get_object(struct adb_solve_solution *solution, int index);
libadb.adb_solution_get_object.argtypes = [adb_solve_solution_p, ctypes.c_int]
libadb.adb_solution_get_object.restype = adb_solve_object_p

# double adb_solution_get_pixel_size(struct adb_solve_solution *solution);
libadb.adb_solution_get_pixel_size.argtypes = [adb_solve_solution_p]
libadb.adb_solution_get_pixel_size.restype = ctypes.c_double

# void adb_solution_get_plate_equ_bounds(struct adb_solve_solution *solution, enum adb_plate_bounds bounds, double *ra, double *dec);
libadb.adb_solution_get_plate_equ_bounds.argtypes = [adb_solve_solution_p, ctypes.c_int, ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double)]
libadb.adb_solution_get_plate_equ_bounds.restype = None

# void adb_solution_plate_to_equ_position(struct adb_solve_solution *solution, int x, int y, double *ra, double *dec);
libadb.adb_solution_plate_to_equ_position.argtypes = [adb_solve_solution_p, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double)]
libadb.adb_solution_plate_to_equ_position.restype = None

# void adb_solution_equ_to_plate_position(struct adb_solve_solution *solution, double ra, double dec, double *x, double *y);
libadb.adb_solution_equ_to_plate_position.argtypes = [adb_solve_solution_p, ctypes.c_double, ctypes.c_double, ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double)]
libadb.adb_solution_equ_to_plate_position.restype = None

