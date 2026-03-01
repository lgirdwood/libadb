#!/usr/bin/env python3
import sys
import ctypes
import math

from astrodb import Library, Database, Table, AstroDBError, ObjectSet
from astrodb.lib import (
    libadb, adb_object, adb_object_p, adb_schema_field,
    ADB_CTYPE_INT, ADB_CTYPE_STRING,
    ADB_CTYPE_DOUBLE_HMS_HRS, ADB_CTYPE_DOUBLE_HMS_MINS, ADB_CTYPE_DOUBLE_HMS_SECS,
    ADB_CTYPE_DOUBLE_DMS_DEGS, ADB_CTYPE_DOUBLE_DMS_MINS, ADB_CTYPE_DOUBLE_DMS_SECS,
    ADB_CTYPE_SIGN, ADB_CTYPE_FLOAT, ADB_IMPORT_DEC,
    adb_field_import1
)

D2R = 1.7453292519943295769e-2
R2D = 5.7295779513082320877e1

class hyperleda_object(ctypes.Structure):
    _fields_ = [
        ("d", adb_object),
        ("position_angle", ctypes.c_float),
        ("axis_ratio", ctypes.c_float),
        ("MType", ctypes.c_char * 4),
        ("OType", ctypes.c_char * 1),
        ("other_name", ctypes.c_char * 15),
    ]

# Callbacks
@adb_field_import1
def pa_insert(obj_ptr, offset, src):
    try:
        val = float(src.decode('utf-8'))
        if val == 999.0: val = float('nan')
    except ValueError:
        val = float('nan')
    dest_ptr = ctypes.cast(ctypes.addressof(obj_ptr.contents) + offset, ctypes.POINTER(ctypes.c_float))
    dest_ptr[0] = val
    return 0

@adb_field_import1
def size_insert(obj_ptr, offset, src):
    try:
        val = float(src.decode('utf-8'))
        if val == 9.99: val = float('nan')
    except ValueError:
        val = float('nan')
    dest_ptr = ctypes.cast(ctypes.addressof(obj_ptr.contents) + offset, ctypes.POINTER(ctypes.c_float))
    dest_ptr[0] = val
    return 0

@adb_field_import1
def otype_insert(obj_ptr, offset, src):
    s = src.decode('utf-8')
    val = b'G'
    if s and s[0] == 'M': val = b'M'
    elif s and s[0] == 'G':
        if len(s)>1 and s[1] == 'M': val = b'X'
        else: val = b'G'
    dest_ptr = ctypes.cast(ctypes.addressof(obj_ptr.contents) + offset, ctypes.POINTER(ctypes.c_char))
    dest_ptr[0] = val
    return 0

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

def adb_gmember(name, symbol, cls, field, type_enum, units, group_posn):
    return adb_member(name, symbol, cls, field, type_enum, units, group_posn, offsetof(cls, field))

hyperleda_fields_list = [
    adb_member("Name", "ANames", hyperleda_object, "other_name", ADB_CTYPE_STRING, ""),
    adb_member("ID", "PGC", hyperleda_object, "d", ADB_CTYPE_INT, ""), # d.id offset is the same as d offset
    adb_gmember("RA Hours", "RAh", hyperleda_object, "d", ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 2),
    adb_gmember("RA Minutes", "RAm", hyperleda_object, "d", ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 1),
    adb_gmember("RA Seconds", "RAs", hyperleda_object, "d", ADB_CTYPE_DOUBLE_HMS_SECS, "seconds", 0),
    adb_gmember("DEC Degrees", "DEd", hyperleda_object, "d", ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 3),
    adb_gmember("DEC Minutes", "DEm", hyperleda_object, "d", ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 2),
    adb_gmember("DEC Seconds", "DEs", hyperleda_object, "d", ADB_CTYPE_DOUBLE_DMS_SECS, "seconds", 1),
    adb_gmember("DEC sign", "DE-", hyperleda_object, "d", ADB_CTYPE_SIGN, "", 0),
    adb_member("Type", "MType", hyperleda_object, "MType", ADB_CTYPE_STRING, ""),
    adb_member("OType", "OType", hyperleda_object, "OType", ADB_CTYPE_STRING, "", cb=otype_insert),
    adb_member("Diameter", "logD25", hyperleda_object, "d", ADB_CTYPE_FLOAT, "0.1amin", cb=size_insert), # Maps to mag visually
    adb_member("Axis Ratio", "logR25", hyperleda_object, "axis_ratio", ADB_CTYPE_FLOAT, "0.1amin", cb=size_insert),
    adb_member("Position Angle", "PA", hyperleda_object, "position_angle", ADB_CTYPE_FLOAT, "deg", cb=pa_insert),
]

# Adjust "ID" and "Diameter" offset manually since they map to fields inside d 
hyperleda_fields_list[1].struct_offset = offsetof(hyperleda_object, "d") + offsetof(adb_object, "id")
hyperleda_fields_list[11].struct_offset = offsetof(hyperleda_object, "d") + offsetof(adb_object, "mag")

SchemaArrayType = adb_schema_field * len(hyperleda_fields_list)
hyperleda_fields = SchemaArrayType(*hyperleda_fields_list)

def hyperleda_import(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        libadb.adb_set_msg_level(db._ptr, 3) 
        libadb.adb_set_log_level(db._ptr, 0xFFFF)
        
        table_id = libadb.adb_table_import_new(
            db._ptr, b"VII", b"237", b"pgc",
            b"logD25", 0.0, 2.0, ADB_IMPORT_DEC
        )
        if table_id < 0:
            print("failed to create import table")
            return table_id
            
        ret = libadb.adb_table_import_schema(
            db._ptr, table_id, hyperleda_fields, len(hyperleda_fields_list), ctypes.sizeof(hyperleda_object)
        )
        ret = libadb.adb_table_import(db._ptr, table_id)
        db.close()
        lib.close()
        return ret
    except Exception as e:
        print(f"Error: {e}")
        return -1

def hyperleda_query(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        libadb.adb_set_msg_level(db._ptr, 3)
        libadb.adb_set_log_level(db._ptr, 0xFFFF)
        
        table = Table(db, "VII", "237", "pgc")
        
        print("Get all objects")
        obj_set = ObjectSet(table)
        obj_set.apply_constraints(0.0, 0.0, 2.0 * math.pi, -2.0, 16.0)
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

def usage(argv):
    print(f"Import: {argv} -i [import dir]")
    print(f"Query:  {argv} -q [library dir]")
    sys.exit(0)

if __name__ == '__main__':
    if len(sys.argv) < 3: usage(sys.argv[0])
    for i in range(1, len(sys.argv) - 1):
        if sys.argv[i] == '-i': hyperleda_import(sys.argv[i+1])
        elif sys.argv[i] == '-q': hyperleda_query(sys.argv[i+1])
