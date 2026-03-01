#!/usr/bin/env python3
import sys
import ctypes
import math

from astrodb import Library, Database, Table, AstroDBError, ObjectSet
from astrodb.lib import (
    libadb, adb_object, adb_object_p, adb_schema_field,
    ADB_CTYPE_INT, ADB_CTYPE_FLOAT, ADB_CTYPE_DEGREES,
    ADB_IMPORT_DEC, adb_field_import1
)

D2R = 1.7453292519943295769e-2
R2D = 5.7295779513082320877e1

class tycho_object(ctypes.Structure):
    _fields_ = [
        ("object", adb_object),
        ("pmRA", ctypes.c_float),
        ("pmDEC", ctypes.c_float),
        ("plx", ctypes.c_float),
        ("HD", ctypes.c_uint),
        ("HIP", ctypes.c_uint),
    ]

@adb_field_import1
def tycid1_sp_insert(obj_ptr, offset, src):
    val = src.decode('utf-8').strip()
    dest_ptr = ctypes.cast(ctypes.addressof(obj_ptr.contents) + offset, ctypes.c_char_p)
    # the offset goes directly to object.designation array
    ctypes.memmove(ctypes.addressof(obj_ptr.contents) + offset, val.encode('utf-8'), len(val) + 1)
    return 0

@adb_field_import1
def tycid2_sp_insert(obj_ptr, offset, src):
    val = src.decode('utf-8').strip()
    # Read existing designation
    exist_bytes = (ctypes.c_char * 16).from_address(ctypes.addressof(obj_ptr.contents) + offset).value
    exist = exist_bytes.decode('utf-8')
    new_val = f"{exist}-{val}"
    
    if len(new_val) < 16:
        ctypes.memmove(ctypes.addressof(obj_ptr.contents) + offset, new_val.encode('utf-8'), len(new_val) + 1)
        return 0
    return -1

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
    sf.text_size = 0
    sf.text_offset = 0
    sf.type = type_enum
    sf.units = units.encode('utf-8')
    if cb: sf.import_cb = cb
    return sf

tycho_fields_list = [
    adb_member("Name", "TYCID1", tycho_object, "object", ADB_CTYPE_INT, "", cb=tycid1_sp_insert),
    adb_member("Name", "TYCID2", tycho_object, "object", ADB_CTYPE_INT, "", cb=tycid2_sp_insert),
    adb_member("Name", "TYCID3", tycho_object, "object", ADB_CTYPE_INT, "", cb=tycid2_sp_insert),
    adb_member("RA", "RAdeg", tycho_object, "object", ADB_CTYPE_DEGREES, "degrees"),
    adb_member("DEC", "DEdeg", tycho_object, "object", ADB_CTYPE_DEGREES, "degrees"),
    adb_member("Mag", "VT", tycho_object, "object", ADB_CTYPE_FLOAT, ""),
    adb_member("pmRA", "pmRA", tycho_object, "pmRA", ADB_CTYPE_FLOAT, "mas/a"),
    adb_member("pmDEC", "pmDEC", tycho_object, "pmDEC", ADB_CTYPE_FLOAT, "mas/a"),
    adb_member("Parallax", "Plx", tycho_object, "plx", ADB_CTYPE_FLOAT, "mas"),
    adb_member("HD Number", "HD", tycho_object, "HD", ADB_CTYPE_INT, ""),
    adb_member("HIP Number", "HIP", tycho_object, "HIP", ADB_CTYPE_INT, ""),
]

# Adjust offsets to map closely to internal variables
tycho_fields_list[3].struct_offset = offsetof(tycho_object, "object") + offsetof(adb_object, "ra")
tycho_fields_list[4].struct_offset = offsetof(tycho_object, "object") + offsetof(adb_object, "dec")
tycho_fields_list[5].struct_offset = offsetof(tycho_object, "object") + offsetof(adb_object, "mag")

SchemaArrayType = adb_schema_field * len(tycho_fields_list)
tycho_fields = SchemaArrayType(*tycho_fields_list)

def tycho_import(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        libadb.adb_set_msg_level(db._ptr, 3) 
        libadb.adb_set_log_level(db._ptr, 0xFFFF)
        
        table_id = libadb.adb_table_import_new(
            db._ptr, b"I", b"250", b"catalog",
            b"VT", 0.0, 16.0, ADB_IMPORT_DEC
        )
        if table_id < 0: return table_id
            
        ret = libadb.adb_table_import_schema(
            db._ptr, table_id, tycho_fields, len(tycho_fields_list), ctypes.sizeof(tycho_object)
        )
        ret = libadb.adb_table_import(db._ptr, table_id)
        db.close()
        lib.close()
        return ret
    except Exception as e:
        print(f"Error: {e}")
        return -1

def tycho_query(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        
        table = Table(db, "I", "250", "catalog")
        
        print("Get all objects")
        obj_set = ObjectSet(table)
        obj_set.apply_constraints(0.0, 0.0, 2.0 * math.pi, 0.0, 16.0)
        obj_set.populate()
        
        print(f" found {len(obj_set)} objects")
        obj_set.close()
        
        print("Get all objects around 1 deg FoV RA 341.0, DEC 58.0")
        obj_set_small = ObjectSet(table)
        obj_set_small.apply_constraints(341.0 * D2R, 58.0 * D2R, 1.0 * D2R, -2.0, 16.0)
        obj_set_small.populate()
        
        print(f" found {len(obj_set_small)} objects")
        obj_set_small.close()
        table.close()
        db.close()
        lib.close()
        return 0
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
        if sys.argv[i] == '-i': tycho_import(sys.argv[i+1])
        elif sys.argv[i] == '-q': tycho_query(sys.argv[i+1])
