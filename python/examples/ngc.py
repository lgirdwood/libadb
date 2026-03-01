#!/usr/bin/env python3
import sys
import ctypes
import math

from astrodb import Library, Database, Table, AstroDBError, ObjectSet
from astrodb.lib import (
    libadb,
    adb_object,
    adb_schema_field,
    ADB_CTYPE_STRING,
    ADB_CTYPE_DOUBLE_HMS_HRS,
    ADB_CTYPE_DOUBLE_HMS_MINS,
    ADB_CTYPE_DOUBLE_DMS_DEGS,
    ADB_CTYPE_DOUBLE_DMS_MINS,
    ADB_CTYPE_SIGN,
    ADB_CTYPE_FLOAT,
    ADB_IMPORT_INC,
    ADB_SCHEMA_NAME_SIZE,
    ADB_SCHEMA_SYMBOL_SIZE,
    ADB_SCHEMA_UNITS_SIZE
)

D2R = 1.7453292519943295769e-2
R2D = 5.7295779513082320877e1

# Define C struct for memory mapping
class ngc_object(ctypes.Structure):
    _fields_ = [
        ("object", adb_object),
        ("type", ctypes.c_char * 4),
        ("desc", ctypes.c_char * 51),
    ]

# Helper to calculate offset
def offsetof(cls, field):
    return getattr(cls, field).offset

def sizeof(cls, field):
    return getattr(cls, field).size

# Python equivalent of adb_member macro
def adb_member(name, symbol, cls, field, type_enum, units, group_posn=0, group_offset=-1):
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
    # sf.import_cb is NULL natively
    return sf

def adb_gmember(name, symbol, cls, field, type_enum, units, group_posn):
    return adb_member(name, symbol, cls, field, type_enum, units, group_posn, group_offset=offsetof(cls, field))

# Build Schema Array
ngc_fields_list = [
    adb_member("Name", "Name", ngc_object, "object", ADB_CTYPE_STRING, ""), # Need to target designation
    adb_member("Type", "Type", ngc_object, "type", ADB_CTYPE_STRING, ""),
    adb_gmember("RA Hours", "RAh", ngc_object, "object",  ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 1),
    adb_gmember("RA Minutes", "RAm", ngc_object, "object", ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 0),
    adb_gmember("DEC Degrees", "DEd", ngc_object, "object", ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 2),
    adb_gmember("DEC Minutes", "DEm", ngc_object, "object", ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 1),
    adb_gmember("DEC sign", "DE-", ngc_object, "object", ADB_CTYPE_SIGN, "", 0),
    adb_member("Integrated Mag", "mag", ngc_object, "object", ADB_CTYPE_FLOAT, ""),
    adb_member("Description", "Desc", ngc_object, "desc", ADB_CTYPE_STRING, ""),
    adb_member("Largest Dimension", "size", ngc_object, "object", ADB_CTYPE_FLOAT, "arcmin"),
]

SchemaArrayType = adb_schema_field * len(ngc_fields_list)
ngc_fields = SchemaArrayType(*ngc_fields_list)


def ngc_import(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 5, 1)
        
        libadb.adb_set_msg_level(db._ptr, 3) # DEBUG
        libadb.adb_set_log_level(db._ptr, 0xFFFF) # ALL
        
        table_id = libadb.adb_table_import_new(
            db._ptr,
            b"VII", b"118", b"ngc2000",
            b"mag", 0.0, 18.0, ADB_IMPORT_INC
        )
        
        if table_id < 0:
            print("failed to create import table")
            return table_id
            
        ret = libadb.adb_table_import_schema(
            db._ptr, table_id, ngc_fields, len(ngc_fields_list), ctypes.sizeof(ngc_object)
        )
        if ret < 0:
            print("failed to register object type")
            return ret
            
        ret = libadb.adb_table_import(db._ptr, table_id)
        if ret < 0:
            print("failed to import")
            
        db.close()
        lib.close()
        return ret
    except Exception as e:
        print(f"Error: {e}")
        return -1


def ngc_query(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 5, 1)
        
        libadb.adb_set_msg_level(db._ptr, 3) # DEBUG
        libadb.adb_set_log_level(db._ptr, 0xFFFF) # ALL
        
        table = Table(db, "VII", "118", "ngc2000")
        
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

def usage(argv):
    print(f"Import: {argv} -i [import dir]")
    print(f"Query:  {argv} -q [library dir]")
    sys.exit(0)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        usage(sys.argv[0])
        
    for i in range(1, len(sys.argv) - 1):
        if sys.argv[i] == '-i':
            ngc_import(sys.argv[i+1])
        elif sys.argv[i] == '-q':
            ngc_query(sys.argv[i+1])
        elif sys.argv[i] == '-p':
            print_all = True # Mock hook

