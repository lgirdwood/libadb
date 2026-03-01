#!/usr/bin/env python3
import sys
import ctypes
import math

from astrodb import Library, Database, Table, AstroDBError, ObjectSet, Search
from astrodb.lib import (
    libadb, adb_object, adb_schema_field,
    ADB_CTYPE_FLOAT, ADB_CTYPE_DEGREES, ADB_CTYPE_STRING,
    ADB_IMPORT_INC,
    ADB_COMP_LT, ADB_COMP_GT, ADB_OP_AND, ADB_OP_OR
)

D2R = 1.7453292519943295769e-2
R2D = 5.7295779513082320877e1

class gsc_object(ctypes.Structure):
    _fields_ = [
        ("object", adb_object),
        ("pos_err", ctypes.c_float),
        ("pmag_err", ctypes.c_float),
    ]

def offsetof(cls, field): return getattr(cls, field).offset
def sizeof(cls, field): return getattr(cls, field).size

def adb_member(name, symbol, cls, field, type_enum, units, group_posn=0, group_offset=-1, cb=None):
    sf = adb_schema_field()
    sf.name = name.encode('utf-8')
    sf.symbol = symbol.encode('utf-8')
    sf.struct_offset = offsetof(cls, field)
    sf.struct_bytes = sizeof(cls, field)
    sf.group_offset = group_offset if group_offset >= 0 else -1
    sf.group_posn = group_posn
    sf.type = type_enum
    sf.units = units.encode('utf-8')
    if cb: sf.import_cb = cb
    return sf

gsc_fields_list = [
    adb_member("Designation", "GSC", gsc_object, "object", ADB_CTYPE_STRING, ""),
    adb_member("RA", "RAdeg", gsc_object, "object", ADB_CTYPE_DEGREES, "degrees", 1),
    adb_member("DEC", "DEdeg", gsc_object, "object", ADB_CTYPE_DEGREES, "degrees", 1),
    adb_member("Photographic Mag", "Pmag", gsc_object, "object", ADB_CTYPE_FLOAT, ""),
    adb_member("Mag error", "e_Pmag", gsc_object, "pmag_err", ADB_CTYPE_FLOAT, ""),
    adb_member("Pos error", "PosErr", gsc_object, "pos_err", ADB_CTYPE_FLOAT, ""),
]

gsc_fields_list[1].struct_offset = offsetof(gsc_object, "object") + offsetof(adb_object, "ra")
gsc_fields_list[2].struct_offset = offsetof(gsc_object, "object") + offsetof(adb_object, "dec")
gsc_fields_list[3].struct_offset = offsetof(gsc_object, "object") + offsetof(adb_object, "mag")

SchemaArrayType = adb_schema_field * len(gsc_fields_list)
gsc_fields = SchemaArrayType(*gsc_fields_list)


def search1(db, table_id, table):
    try:
        print("Searching objects")
        search = Search(table)
        obj_set = ObjectSet(table)
        
        search.add_comparator("DEdeg", ADB_COMP_LT, "58.434773")
        search.add_comparator("DEdeg", ADB_COMP_GT, "57.678541")
        search.add_operator(ADB_OP_AND)
        
        search.add_comparator("RAdeg", ADB_COMP_LT, "342.434232")
        search.add_comparator("RAdeg", ADB_COMP_GT, "341.339925")
        search.add_operator(ADB_OP_AND)
        
        search.add_operator(ADB_OP_AND)
        
        hits = search.execute(obj_set)
        
        print(f"   Search got {search.hits} objects")
        
        search.close()
        obj_set.close()
    except AstroDBError as e:
        print(f"Search failed: {e}")

def gsc_query(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 9, 1)
        
        table = Table(db, "I", "254", "gsc")
        search1(db, table.table_id, table)
        
        print("Get all objects")
        obj_set = ObjectSet(table)
        obj_set.apply_constraints(0.0, 0.0, 2.0 * math.pi, 0.0, 16.0)
        obj_set.populate()
        print(f" found {len(obj_set)} objects")
        
        obj_set.close()
        table.close()
        db.close()
        lib.close()
        return 0
    except Exception as e:
        print(f"Error: {e}")
        return -1


def gsc_import(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 9, 1)
        libadb.adb_set_msg_level(db._ptr, 3)
        libadb.adb_set_log_level(db._ptr, 0xFFFF)
        
        table_id = libadb.adb_table_import_new(
            db._ptr, b"I", b"254", b"out",
            b"Pmag", -2.0, 17.0, ADB_IMPORT_INC
        )
        if table_id < 0: return table_id
        
        libadb.adb_table_import_alt_dataset(db._ptr, table_id, b"gsc", 0)
            
        ret = libadb.adb_table_import_schema(
            db._ptr, table_id, gsc_fields, len(gsc_fields_list), ctypes.sizeof(gsc_object)
        )
        ret = libadb.adb_table_import(db._ptr, table_id)
        
        db.close()
        lib.close()
        return ret
    except Exception as e:
        print(f"Error: {e}")
        return -1

def usage(argv):
    print(f"Import: {argv} -i [import dir]")
    print(f"Query:  {argv} -q [library dir]")
    sys.exit(0)

if __name__ == '__main__':
    if len(sys.argv) < 3: usage(sys.argv[0])
    for i in range(1, len(sys.argv) - 1):
        if sys.argv[i] == '-i': gsc_import(sys.argv[i+1])
        elif sys.argv[i] == '-q': gsc_query(sys.argv[i+1])
