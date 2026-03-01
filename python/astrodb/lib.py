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

# Define opaque pointer types to match C structures
class adb_library_p(ctypes.c_void_p): pass
class adb_db_p(ctypes.c_void_p): pass
class adb_search_p(ctypes.c_void_p): pass
class adb_object_set_p(ctypes.c_void_p): pass

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

class adb_object_p(ctypes.POINTER(adb_object)): pass


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

# int adb_search_get_results(struct adb_search *search, struct adb_object_set *set, const struct adb_object **objects[]);
libadb.adb_search_get_results.argtypes = [adb_search_p, adb_object_set_p, ctypes.POINTER(ctypes.POINTER(adb_object_p))]
libadb.adb_search_get_results.restype = ctypes.c_int

# int adb_search_get_hits(struct adb_search *search);
libadb.adb_search_get_hits.argtypes = [adb_search_p]
libadb.adb_search_get_hits.restype = ctypes.c_int

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

# int adb_table_import(struct adb_db *db, int table_id);
libadb.adb_table_import.argtypes = [adb_db_p, ctypes.c_int]
libadb.adb_table_import.restype = ctypes.c_int

# int adb_table_import_alt_dataset(struct adb_db *db, int table_id, const char *dataset, int num_objects);
libadb.adb_table_import_alt_dataset.argtypes = [adb_db_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
libadb.adb_table_import_alt_dataset.restype = ctypes.c_int

# void adb_set_msg_level(struct adb_db *db, enum adb_msg_level level);
libadb.adb_set_msg_level.argtypes = [adb_db_p, ctypes.c_int]
libadb.adb_set_msg_level.restype = None

# void adb_set_log_level(struct adb_db *db, unsigned int log);
libadb.adb_set_log_level.argtypes = [adb_db_p, ctypes.c_uint]
libadb.adb_set_log_level.restype = None

