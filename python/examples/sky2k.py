#!/usr/bin/env python3
import sys
import ctypes
import math

from astrodb import Library, Database, Table, AstroDBError, ObjectSet, Search
from astrodb.lib import (
    libadb, adb_object, adb_schema_field,
    ADB_CTYPE_INT, ADB_CTYPE_FLOAT, ADB_CTYPE_DOUBLE,
    ADB_CTYPE_DEGREES, ADB_CTYPE_STRING,
    ADB_CTYPE_DOUBLE_HMS_HRS, ADB_CTYPE_DOUBLE_HMS_MINS, ADB_CTYPE_DOUBLE_HMS_SECS,
    ADB_CTYPE_DOUBLE_DMS_DEGS, ADB_CTYPE_DOUBLE_DMS_MINS, ADB_CTYPE_DOUBLE_DMS_SECS,
    ADB_CTYPE_SIGN, ADB_CTYPE_SHORT, ADB_IMPORT_INC,
    ADB_COMP_LT, ADB_COMP_GT, ADB_COMP_EQ, ADB_COMP_NE,
    ADB_OP_AND, ADB_OP_OR,
    adb_field_import1
)

D2R = 1.7453292519943295769e-2
R2D = 5.7295779513082320877e1

class sky2kv4_object(ctypes.Structure):
    _fields_ = [
        ("object", adb_object),
        ("type", ctypes.c_ubyte),
        ("name", ctypes.c_char * 11),
        ("sp", ctypes.c_char * 4),
        ("HD", ctypes.c_int),
        ("SAO", ctypes.c_int),
        ("PPM", ctypes.c_int),
        ("pmRA", ctypes.c_double),
        ("pmDEC", ctypes.c_double),
        ("RV", ctypes.c_double),
        ("sep", ctypes.c_double),
        ("Dmag", ctypes.c_float),
        ("orbPer", ctypes.c_double),
        ("PA", ctypes.c_short),
        ("date", ctypes.c_double),
        ("ID_A", ctypes.c_int),
        ("ID_B", ctypes.c_int),
        ("ID_C", ctypes.c_int),
        ("magMax", ctypes.c_float),
        ("magMin", ctypes.c_float),
        ("varAmp", ctypes.c_float),
        ("varPer", ctypes.c_double),
        ("varEpo", ctypes.c_double),
        ("varType", ctypes.c_short),
    ]

# Simplified sp assignment hook for python examples
@adb_field_import1
def sky2kv4_sp_insert(obj_ptr, offset, src):
    val = src.decode('utf-8').strip()
    ctypes.memmove(ctypes.addressof(obj_ptr.contents) + offset, val.encode('utf-8'), len(val) + 1)
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
    sf.type = type_enum
    sf.units = units.encode('utf-8')
    if cb: sf.import_cb = cb
    return sf

def adb_gmember(name, symbol, cls, field, type_enum, units, group_posn):
    return adb_member(name, symbol, cls, field, type_enum, units, group_posn, group_offset=offsetof(cls, field))

sky2kv4_fields_list = [
    adb_member("Name", "Name", sky2kv4_object, "name", ADB_CTYPE_STRING, ""),
    adb_member("ID", "ID", sky2kv4_object, "object", ADB_CTYPE_INT, ""),
    adb_gmember("RA Hours", "RAh", sky2kv4_object, "object", ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 2),
    adb_gmember("RA Minutes", "RAm", sky2kv4_object, "object", ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 1),
    adb_gmember("RA Seconds", "RAs", sky2kv4_object, "object", ADB_CTYPE_DOUBLE_HMS_SECS, "seconds", 0),
    adb_gmember("DEC Degrees", "DEd", sky2kv4_object, "object", ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 3),
    adb_gmember("DEC Minutes", "DEm", sky2kv4_object, "object", ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 2),
    adb_gmember("DEC Seconds", "DEs", sky2kv4_object, "object", ADB_CTYPE_DOUBLE_DMS_SECS, "seconds", 1),
    adb_gmember("DEC sign", "DE-", sky2kv4_object, "object", ADB_CTYPE_SIGN, "", 0),
    adb_member("Visual Mag", "Vmag", sky2kv4_object, "object", ADB_CTYPE_FLOAT, ""),
    adb_member("sp", "Sp", sky2kv4_object, "sp", ADB_CTYPE_STRING, "", cb=sky2kv4_sp_insert),
    adb_member("HD", "HD", sky2kv4_object, "HD", ADB_CTYPE_INT, ""),
    adb_member("SAO", "SAO", sky2kv4_object, "SAO", ADB_CTYPE_INT, ""),
    adb_member("PPM", "PPM", sky2kv4_object, "PPM", ADB_CTYPE_INT, ""),
    adb_member("pmRA", "pmRA", sky2kv4_object, "pmRA", ADB_CTYPE_DOUBLE, ""),
    adb_member("pmDEC", "pmDEC", sky2kv4_object, "pmDEC", ADB_CTYPE_DOUBLE, ""),
    adb_member("Radial Vel", "RV", sky2kv4_object, "RV", ADB_CTYPE_DOUBLE, ""),
    adb_member("Binary sep", "sep", sky2kv4_object, "sep", ADB_CTYPE_DOUBLE, ""),
    adb_member("Dmag", "Dmag", sky2kv4_object, "Dmag", ADB_CTYPE_FLOAT, ""),
    adb_member("Orb Per", "orbPer", sky2kv4_object, "orbPer", ADB_CTYPE_DOUBLE, ""),
    adb_member("Pos Angle", "PA", sky2kv4_object, "PA", ADB_CTYPE_SHORT, ""),
    adb_member("Obs Date", "date", sky2kv4_object, "date", ADB_CTYPE_DOUBLE, ""),
    adb_member("ID_A", "ID_A", sky2kv4_object, "ID_A", ADB_CTYPE_INT, ""),
    adb_member("ID_B", "ID_B", sky2kv4_object, "ID_B", ADB_CTYPE_INT, ""),
    adb_member("ID_C", "ID_C", sky2kv4_object, "ID_C", ADB_CTYPE_INT, ""),
    adb_member("Var max mag", "magMax", sky2kv4_object, "magMax", ADB_CTYPE_FLOAT, ""),
    adb_member("Var min mag", "magMin", sky2kv4_object, "magMin", ADB_CTYPE_FLOAT, ""),
    adb_member("Var Amp", "varAmp", sky2kv4_object, "varAmp", ADB_CTYPE_FLOAT, ""),
    adb_member("Var Period", "varPer", sky2kv4_object, "varPer", ADB_CTYPE_DOUBLE, ""),
    adb_member("Var Epoch", "varEpo", sky2kv4_object, "varEpo", ADB_CTYPE_DOUBLE, ""),
    adb_member("Var Type", "varType", sky2kv4_object, "varType", ADB_CTYPE_SHORT, ""),
]

# ID mappings to underlying object components
sky2kv4_fields_list[1].struct_offset = offsetof(sky2kv4_object, "object") + offsetof(adb_object, "id")
sky2kv4_fields_list[2].struct_offset = offsetof(sky2kv4_object, "object") + offsetof(adb_object, "ra")
sky2kv4_fields_list[5].struct_offset = offsetof(sky2kv4_object, "object") + offsetof(adb_object, "dec")
sky2kv4_fields_list[9].struct_offset = offsetof(sky2kv4_object, "object") + offsetof(adb_object, "mag")

SchemaArrayType = adb_schema_field * len(sky2kv4_fields_list)
star_fields = SchemaArrayType(*sky2kv4_fields_list)


def search1(db, table_id, table):
    try:
        print("Searching for high PM or RV objects")
        search = Search(table)
        obj_set = ObjectSet(table)
        
        search.add_comparator("pmRA", ADB_COMP_LT, "0.4")
        search.add_comparator("pmRA", ADB_COMP_GT, "0.01")
        search.add_operator(ADB_OP_AND)
        
        search.add_comparator("pmDEC", ADB_COMP_LT, "0.4")
        search.add_comparator("pmDEC", ADB_COMP_GT, "0.01")
        search.add_operator(ADB_OP_AND)
        
        search.add_comparator("RV", ADB_COMP_LT, "40")
        search.add_comparator("RV", ADB_COMP_GT, "25")
        search.add_operator(ADB_OP_AND)
        search.add_operator(ADB_OP_AND)
        
        hits = search.execute(obj_set)
        print(f"   Search got {search.hits} objects")
        search.close()
        obj_set.close()
    except AstroDBError as e:
        print(f"Search failed: {e}")

def sky2k_query(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        libadb.adb_set_msg_level(db._ptr, 3) 
        
        table = Table(db, "V", "109", "sky2kv4")
        
        print("Get all objects")
        obj_set = ObjectSet(table)
        obj_set.apply_constraints(0.0, 0.0, 2.0 * math.pi, -2.0, 16.0)
        obj_set.populate()
        
        print(f" found {len(obj_set)} objects")
        obj_set.close()
        
        search1(db, table.table_id, table)
        
        table.close()
        db.close()
        lib.close()
        return 0
    except Exception as e:
        print(f"Error: {e}")
        return -1


def sky2k_import(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        libadb.adb_set_msg_level(db._ptr, 3)
        libadb.adb_set_log_level(db._ptr, 0xFFFF)
        
        table_id = libadb.adb_table_import_new(
            db._ptr, b"V", b"109", b"sky2kv4",
            b"Vmag", -2.0, 16.0, ADB_IMPORT_INC
        )
        if table_id < 0: return table_id
            
        ret = libadb.adb_table_import_schema(
            db._ptr, table_id, star_fields, len(sky2kv4_fields_list), ctypes.sizeof(sky2kv4_object)
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
        if sys.argv[i] == '-i': sky2k_import(sys.argv[i+1])
        elif sys.argv[i] == '-q': sky2k_query(sys.argv[i+1])

