from .lib import (
    libadb,
    adb_object,
    adb_object_p,
    adb_pobject,
    ADB_OP_AND,
    ADB_OP_OR,
    ADB_COMP_LT,
    ADB_COMP_GT,
    ADB_COMP_EQ,
    ADB_COMP_NE,
    ADB_CONSTRAINT_MAG,
    ADB_CONSTRAINT_FOV,
    ADB_CONSTRAINT_RA,
    ADB_CONSTRAINT_DEC,
    ADB_CONSTRAINT_AREA,
    ADB_CONSTRAINT_JD,
    ADB_CONSTRAINT_POBJECTS,
    ADB_FIND_ALL,
    ADB_FIND_FIRST,
    ADB_FIND_PLANETS,
    ADB_FIND_ASTEROIDS,
    ADB_FIND_PROPER_MOTION,
    ADB_BOUND_TOP_RIGHT,
    ADB_BOUND_TOP_LEFT,
    ADB_BOUND_BOTTOM_RIGHT,
    ADB_BOUND_BOTTOM_LEFT,
    ADB_BOUND_CENTRE,
    ADB_CTYPE_INT,
    ADB_CTYPE_FLOAT,
    ADB_CTYPE_DOUBLE,
    ADB_CTYPE_STRING
)

import ctypes

class AstroDBError(Exception):
    """Base exception for astrodb wrapper errors."""
    pass

class Library:
    def __init__(self, host: str, remote: str, local: str):
        self._ptr = libadb.adb_open_library(
            host.encode('utf-8'),
            remote.encode('utf-8'),
            local.encode('utf-8')
        )
        if not self._ptr:
            raise AstroDBError("Failed to open library. Check paths and network.")

    @property
    def version(self) -> str:
        v = libadb.adb_get_version()
        return v.decode('utf-8') if v else "unknown"

    def close(self):
        if self._ptr:
            libadb.adb_close_library(self._ptr)
            self._ptr = None

    def __del__(self):
        self.close()

class Database:
    def __init__(self, lib: Library, depth: int, num_tables: int):
        self.lib = lib
        self._ptr = libadb.adb_create_db(lib._ptr, depth, num_tables)
        if not self._ptr:
            raise AstroDBError("Failed to create database context.")

    def close(self):
        if self._ptr:
            libadb.adb_db_free(self._ptr)
            self._ptr = None

    def __del__(self):
        self.close()

class AstroObject:
    def __init__(self, c_obj, table):
        self.c_obj = c_obj
        self.table = table
        
    @property
    def id(self): return getattr(self.c_obj.key, 'id', 0)
    
    @property
    def designation(self): return getattr(self.c_obj.key, 'designation', b'')
    
    @property
    def ra(self): return self.c_obj.ra
    
    @property
    def dec(self): return self.c_obj.dec
    
    @property
    def mag(self): return self.c_obj.mag
    
    @property
    def size(self): return getattr(self.c_obj, 'size', getattr(self.c_obj, 'mean', 0.0))

    def get_string(self, field: str) -> str:
        offset = self.table.get_field_offset(field)
        if offset < 0:
            return ""
        addr = ctypes.addressof(self.c_obj) + offset
        try:
            return ctypes.cast(addr, ctypes.c_char_p).value.decode('utf-8', 'ignore')
        except:
            return ""

    def get_int(self, field: str) -> int:
        offset = self.table.get_field_offset(field)
        if offset < 0:
            return 0
        addr = ctypes.addressof(self.c_obj) + offset
        return ctypes.cast(addr, ctypes.POINTER(ctypes.c_int)).contents.value

class Table:
    def __init__(self, db: Database, cat_class: str, cat_id: str, table_name: str):
        self.db = db
        self.table_id = libadb.adb_table_open(
            db._ptr,
            cat_class.encode('utf-8'),
            cat_id.encode('utf-8'),
            table_name.encode('utf-8')
        )
        self._kept_strings = []
        if self.table_id < 0:
            raise AstroDBError(f"Failed to open table {cat_class}/{cat_id} {table_name}")

    def __len__(self):
        count = libadb.adb_table_get_count(self.db._ptr, self.table_id)
        return count if count >= 0 else 0

    @property
    def size(self) -> int:
        return libadb.adb_table_get_size(self.db._ptr, self.table_id)

    @property
    def object_size(self) -> int:
        return libadb.adb_table_get_object_size(self.db._ptr, self.table_id)

    def hash_key(self, key: str):
        bkey = key.encode('utf-8')
        self._kept_strings.append(bkey)
        res = libadb.adb_table_hash_key(self.db._ptr, self.table_id, bkey)
        if res < 0:
            raise AstroDBError(f"Failed to set hash key {key} for table.")

    def get_object(self, id_val, field: str):
        if isinstance(id_val, int):
            c_val = ctypes.c_int(id_val)
            ptr = ctypes.cast(ctypes.pointer(c_val), ctypes.c_void_p)
        elif isinstance(id_val, str):
            c_val = ctypes.create_string_buffer(id_val.encode('utf-8'))
            ptr = ctypes.cast(c_val, ctypes.c_void_p)
        else:
            raise AstroDBError("id_val must be int or str")
        obj_ptr = adb_object_p()
        res = libadb.adb_table_get_object(self.db._ptr, self.table_id, ptr, field.encode('utf-8'), ctypes.byref(obj_ptr))
        if res < 0:
            raise AstroDBError(f"Failed to get object with ID {id_val} on field {field}")
        return obj_ptr.contents if obj_ptr else None

    # Import functionalities
    def import_new(self, cat_class: str, cat_id: str, table_name: str, depth_field: str, min_limit: float, max_limit: float, otype: int):
        res = libadb.adb_table_import_new(self.db._ptr, cat_class.encode('utf-8'), cat_id.encode('utf-8'), table_name.encode('utf-8'), depth_field.encode('utf-8'), min_limit, max_limit, otype)
        if res < 0:
            raise AstroDBError("Failed to configure new table import.")

    def import_field(self, field: str, alt: str, flags: int):
        res = libadb.adb_table_import_field(self.db._ptr, self.table_id, field.encode('utf-8'), alt.encode('utf-8'), flags)
        if res < 0:
            raise AstroDBError(f"Failed to set alternative import field for {field}.")

    def import_alt_dataset(self, dataset: str, num_objects: int):
        res = libadb.adb_table_import_alt_dataset(self.db._ptr, self.table_id, dataset.encode('utf-8'), num_objects)
        if res < 0:
            raise AstroDBError("Failed to set alternative import dataset schema.")

    def run_import(self):
        res = libadb.adb_table_import(self.db._ptr, self.table_id)
        if res < 0:
            raise AstroDBError("Failed to run full database importing routine.")

    def get_field_type(self, field: str) -> int:
        return libadb.adb_table_get_field_type(self.db._ptr, self.table_id, field.encode('utf-8'))

    def get_field_offset(self, field: str) -> int:
        return libadb.adb_table_get_field_offset(self.db._ptr, self.table_id, field.encode('utf-8'))

    def close(self):
        if self.table_id >= 0 and self.db._ptr:
            libadb.adb_table_close(self.db._ptr, self.table_id)
            self.table_id = -1

    def __del__(self):
        self.close()

class ObjectSet:
    def __init__(self, table: Table):
        self.table = table
        self._ptr = libadb.adb_table_set_new(table.db._ptr, table.table_id)
        self._kept_strings = [] # Keep referenced memory alive for C structs
        self._head_count = 0
        if not self._ptr:
            raise AstroDBError("Failed to allocate ObjectSet.")

    def apply_constraints(self, ra: float, dec: float, fov: float, min_z: float, max_z: float):
        res = libadb.adb_table_set_constraints(self._ptr, ra, dec, fov, min_z, max_z)
        if res < 0:
            raise AstroDBError(f"Constraints failed with error {res}")

    def populate(self):
        res = libadb.adb_set_get_objects(self._ptr)
        if res < 0:
            raise AstroDBError("Failed to populate object set.")
        self._head_count = res

    def __len__(self):
        count = libadb.adb_set_get_count(self._ptr)
        return count if count >= 0 else 0

    def hash_key(self, key: str):
        bkey = key.encode('utf-8')
        self._kept_strings.append(bkey)
        res = libadb.adb_set_hash_key(self._ptr, bkey)
        if res < 0:
            raise AstroDBError(f"Failed to set hash key {key} on ObjectSet.")

    def get_object(self, id_val, field: str):
        if isinstance(id_val, int):
            c_val = ctypes.c_int(id_val)
            ptr = ctypes.cast(ctypes.pointer(c_val), ctypes.c_void_p)
        elif isinstance(id_val, str):
            c_val = ctypes.create_string_buffer(id_val.encode('utf-8'))
            ptr = ctypes.cast(c_val, ctypes.c_void_p)
        else:
            raise AstroDBError("id_val must be int or str")
        obj_ptr = adb_object_p()
        res = libadb.adb_set_get_object(self._ptr, ptr, field.encode('utf-8'), ctypes.byref(obj_ptr))
        if res < 0:
            raise AstroDBError(f"Failed to set_get_object with ID {id_val} on field {field}, error: {res}")
        return AstroObject(obj_ptr.contents, self.table) if obj_ptr else None

    def get_nearest_on_pos(self, ra: float, dec: float):
        obj_ptr = libadb.adb_table_set_get_nearest_on_pos(self._ptr, ra, dec)
        if not bool(obj_ptr): # Check if pointer is NULL
            return None
        return AstroObject(obj_ptr.contents, self.table)

    # Omit adb_table_set_get_nearest_on_object for now, requires C adb_object reference lookup.

    @property
    def head(self) -> dict:
        head_ptr = libadb.adb_set_get_head(self._ptr)
        if not head_ptr:
            return None
        return {"objects_ptr": head_ptr.contents.objects, "count": head_ptr.contents.count}

    def __iter__(self):
        # adb_set_get_head returns an array of struct adb_object_head
        if self._head_count <= 0:
            return
            
        head_arr_ptr = libadb.adb_set_get_head(self._ptr)
        if not bool(head_arr_ptr):
            return
            
        head_arr = ctypes.cast(head_arr_ptr, ctypes.POINTER(lib.adb_object_head))
        # Important: the layout in memory for custom dataset items relies on `table.object_size` dynamically computed by DB at open.
        # But we don't have python subclasses. We can cast generically to byte arrays or adb_object wrappers.
        object_bytes_size = self.table.size
        
        for i in range(self._head_count):
            head_obj = head_arr[i]
            count = head_obj.count
            
            # Using raw pointers due to varying size structs
            base_ptr = ctypes.cast(head_obj.objects, ctypes.c_void_p).value
            
            for j in range(count):
                addr = base_ptr + j * object_bytes_size
                obj_ref = ctypes.cast(addr, adb_object_p).contents
                yield AstroObject(obj_ref, self.table)

    def close(self):
        if self._ptr:
            libadb.adb_table_set_free(self._ptr)
            self._ptr = None

    def __del__(self):
        self.close()

class Search:
    def __init__(self, table: Table):
        self.table = table
        self._ptr = libadb.adb_search_new(table.db._ptr, table.table_id)
        if not self._ptr:
            raise AstroDBError("Failed to create search context.")

    def add_operator(self, op: int):
        res = libadb.adb_search_add_operator(self._ptr, op)
        if res < 0:
            raise AstroDBError("Failed to add operator.")

    def add_comparator(self, field: str, comp: int, value: str):
        res = libadb.adb_search_add_comparator(
            self._ptr,
            field.encode('utf-8'),
            comp,
            value.encode('utf-8')
        )
        if res < 0:
            raise AstroDBError("Failed to add comparator.")

    def add_custom_comparator(self, callback: callable):
        from .lib import adb_custom_comparator
        self._c_callback = adb_custom_comparator(callback)
        res = libadb.adb_search_add_custom_comparator(self._ptr, self._c_callback)
        if res < 0:
            raise AstroDBError("Failed to add custom comparator.")

    def execute(self, obj_set: ObjectSet):
        # We need a double pointer for results: const struct adb_object **objects[]
        # We allocate an array of pointers
        self._results_arr = ctypes.POINTER(adb_object_p)()
        
        hit_count = libadb.adb_search_get_results(self._ptr, obj_set._ptr, ctypes.cast(ctypes.byref(self._results_arr), ctypes.c_void_p))
        
        if hit_count < 0:
            raise AstroDBError(f"Search execution failed with {hit_count}")
        
        self._hit_count = hit_count
        return hit_count

    def __iter__(self):
        if not getattr(self, '_hit_count', 0) or not getattr(self, '_results_arr', None):
            return
        for i in range(self._hit_count):
            obj_ptr = self._results_arr[i]
            if obj_ptr:
                yield AstroObject(obj_ptr.contents, self.table)

    @property
    def hits(self):
        return libadb.adb_search_get_hits(self._ptr)

    @property
    def tests(self):
        return libadb.adb_search_get_tests(self._ptr)

    def close(self):
        if self._ptr:
            libadb.adb_search_free(self._ptr)
            self._ptr = None

    def __del__(self):
        self.close()

class Solution:
    def __init__(self, solver: 'Solver', index: int):
        self.solver = solver
        self._ptr = libadb.adb_solve_get_solution(solver._ptr, index)
        if not self._ptr:
            raise AstroDBError(f"No solution found at index {index}")

        # Ensure objects are loaded
        libadb.adb_solution_get_objects(self._ptr)

    @property
    def divergence(self) -> float:
        return libadb.adb_solution_get_divergence(self._ptr) if hasattr(libadb, 'adb_solution_get_divergence') else libadb.adb_solution_divergence(self._ptr)

    @property
    def pixel_size(self) -> float:
        return libadb.adb_solution_get_pixel_size(self._ptr)

    def get_equ_bounds(self, bounds: int) -> tuple[float, float]:
        ra = ctypes.c_double()
        dec = ctypes.c_double()
        libadb.adb_solution_get_plate_equ_bounds(self._ptr, bounds, ctypes.byref(ra), ctypes.byref(dec))
        return ra.value, dec.value

    def plate_to_equ(self, x: int, y: int) -> tuple[float, float]:
        ra = ctypes.c_double()
        dec = ctypes.c_double()
        libadb.adb_solution_plate_to_equ_position(self._ptr, x, y, ctypes.byref(ra), ctypes.byref(dec))
        return ra.value, dec.value

    def equ_to_plate(self, ra: float, dec: float) -> tuple[float, float]:
        x = ctypes.c_double()
        y = ctypes.c_double()
        libadb.adb_solution_equ_to_plate_position(self._ptr, ra, dec, ctypes.byref(x), ctypes.byref(y))
        return x.value, y.value

class Solver:
    def __init__(self, table: Table):
        self.table = table
        self._ptr = libadb.adb_solve_new(table.db._ptr, table.table_id)
        if not self._ptr:
            raise AstroDBError("Failed to create solver context.")

    def set_magnitude_delta(self, delta: float):
        libadb.adb_solve_set_magnitude_delta(self._ptr, delta)

    def set_distance_delta(self, delta: float):
        libadb.adb_solve_set_distance_delta(self._ptr, delta)

    def set_pa_delta(self, delta: float):
        libadb.adb_solve_set_pa_delta(self._ptr, delta)

    def add_plate_object(self, x: int, y: int, adu: int = 1, extended: int = 0):
        pobj = adb_pobject(x=x, y=y, adu=adu, extended=extended)
        res = libadb.adb_solve_add_plate_object(self._ptr, ctypes.byref(pobj))
        if res < 0:
            raise AstroDBError("Failed to add plate object.")

    def add_constraint(self, constraint_type: int, min_val: float, max_val: float):
        res = libadb.adb_solve_constraint(self._ptr, constraint_type, min_val, max_val)
        if res < 0:
            raise AstroDBError("Failed to set constraint.")

    def execute(self, obj_set: ObjectSet = None, find_flags: int = ADB_FIND_ALL):
        set_ptr = obj_set._ptr if obj_set else None
        res = libadb.adb_solve(self._ptr, set_ptr, find_flags)
        if res < 0:
            raise AstroDBError(f"Solve execution failed with code {res}")
        return res

    def get_solution(self, index: int = 0) -> Solution:
        return Solution(self, index)

    def close(self):
        if self._ptr:
            libadb.adb_solve_free(self._ptr)
            self._ptr = None

    def __del__(self):
        self.close()

