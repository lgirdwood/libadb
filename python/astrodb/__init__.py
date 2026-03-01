from .lib import (
    libadb,
    adb_object,
    ADB_OP_AND,
    ADB_OP_OR,
    ADB_COMP_LT,
    ADB_COMP_GT,
    ADB_COMP_EQ,
    ADB_COMP_NE
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

class Table:
    def __init__(self, db: Database, cat_class: str, cat_id: str, table_name: str):
        self.db = db
        self.table_id = libadb.adb_table_open(
            db._ptr,
            cat_class.encode('utf-8'),
            cat_id.encode('utf-8'),
            table_name.encode('utf-8')
        )
        if self.table_id < 0:
            raise AstroDBError(f"Failed to open table {cat_class}/{cat_id} {table_name}")

    def __len__(self):
        count = libadb.adb_table_get_count(self.db._ptr, self.table_id)
        return count if count >= 0 else 0

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

    def __len__(self):
        count = libadb.adb_set_get_count(self._ptr)
        return count if count >= 0 else 0

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

    def execute(self, obj_set: ObjectSet):
        # We need a double pointer for results: const struct adb_object **objects[]
        # We allocate an array of pointers
        results_arr = ctypes.POINTER(ctypes.POINTER(adb_object))()
        
        hit_count = libadb.adb_search_get_results(self._ptr, obj_set._ptr, ctypes.byref(results_arr))
        
        if hit_count < 0:
            raise AstroDBError(f"Search execution failed with {hit_count}")
        
        # In a full wrapper, we would parse results_arr into python objects here
        # Return the raw hit count for now
        return hit_count

    @property
    def hits(self):
        return libadb.adb_search_get_hits(self._ptr)

    def close(self):
        if self._ptr:
            libadb.adb_search_free(self._ptr)
            self._ptr = None

    def __del__(self):
        self.close()
