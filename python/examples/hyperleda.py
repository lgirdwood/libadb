import sys
import math
from astrodb import (
    Library, Database, Table, ObjectSet, AstroDBError,
    ADB_COMP_LT, ADB_COMP_GT, ADB_OP_AND, ADB_OP_OR
)
from astrodb.lib import (
    ADB_CTYPE_STRING, ADB_CTYPE_INT, ADB_CTYPE_DOUBLE_HMS_HRS,
    ADB_CTYPE_DOUBLE_HMS_MINS, ADB_CTYPE_DOUBLE_HMS_SECS,
    ADB_CTYPE_DOUBLE_DMS_DEGS, ADB_CTYPE_DOUBLE_DMS_MINS,
    ADB_CTYPE_DOUBLE_DMS_SECS, ADB_CTYPE_SIGN, ADB_CTYPE_FLOAT
)

D2R = 1.7453292519943295769e-2
R2D = 5.7295779513082320877e1

def print_objects(oset):
    for obj in oset:
        print(f"Obj: {obj.get_string('ANames')} {obj.id} RA: {obj.ra * R2D:f} DEC: {obj.dec * R2D:f} Size {obj.mag:f}")

def get1(db, table_id, print_out=False):
    print("Get all objects")
    try:
        tbl = Table(db, "VII", "237", "pgc")
        oset = ObjectSet(tbl)
        oset.apply_constraints(0.0, 0.0, 2.0 * math.pi, -2.0, 16.0)
        oset.populate()
        
        print(f" found {len(oset)} object list heads {len(oset)} objects\n")
        if print_out:
            print_objects(oset)
            
        oset.close()
        tbl.close()
    except AstroDBError as e:
        print(f"Failed to get_all: {e}")

def hyperleda_query(lib_dir, print_out=False):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        
        tbl = Table(db, "VII", "237", "pgc")
        tbl.hash_key("PGC")
        
        get1(db, tbl.table_id, print_out)
        
        tbl.close()
        db.close()
        lib.close()
    except AstroDBError as e:
        print(f"Query failed: {e}")

def hyperleda_import(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        
        tbl = Table.import_new(db, "VII", "237", "pgc", "logD25", 0.0, 2.0)
        
        # Add hyperleda schema layout equivalent
        tbl.import_field("Name", "ANames", ADB_CTYPE_STRING, "", 0)
        tbl.import_field("ID", "PGC", ADB_CTYPE_INT, "", 0)
        tbl.import_field("RA Hours", "RAh", ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 2)
        tbl.import_field("RA Minutes", "RAm", ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 1)
        tbl.import_field("RA Seconds", "RAs", ADB_CTYPE_DOUBLE_HMS_SECS, "seconds", 0)
        tbl.import_field("DEC Degrees", "DEd", ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 3)
        tbl.import_field("DEC Minutes", "DEm", ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 2)
        tbl.import_field("DEC Seconds", "DEs", ADB_CTYPE_DOUBLE_DMS_SECS, "seconds", 1)
        tbl.import_field("DEC sign", "DE-", ADB_CTYPE_SIGN, "", 0)
        tbl.import_field("Type", "MType", ADB_CTYPE_STRING, "", 0)
        tbl.import_field("OType", "OType", ADB_CTYPE_STRING, "", 0)
        tbl.import_field("Diameter", "logD25", ADB_CTYPE_FLOAT, "0.1amin", 0)
        tbl.import_field("Axis Ratio", "logR25", ADB_CTYPE_FLOAT, "0.1amin", 0)
        tbl.import_field("Position Angle", "PA", ADB_CTYPE_FLOAT, "deg", 0)
        
        tbl.run_import()
        tbl.close()
        
        db.close()
        lib.close()
    except AstroDBError as e:
        print(f"Import failed: {e}")

def usage(name):
    print(f"Import: {name} -i [import dir]")
    print(f"Query: {name} -q [library dir]")
    print(f"Solve: {name} -s [library dir]")
    sys.exit(0)

if __name__ == '__main__':
    print(f"{sys.argv[0]} using python astrodb binding\n")
    
    if len(sys.argv) < 3:
        usage(sys.argv[0])
        
    print_out = "-p" in sys.argv
    i = 1
    while i < len(sys.argv) - 1:
        arg = sys.argv[i]
        if arg == "-i":
            i += 1
            if i == len(sys.argv): usage(sys.argv[0])
            hyperleda_import(sys.argv[i])
        elif arg == "-q":
            i += 1
            if i == len(sys.argv): usage(sys.argv[0])
            hyperleda_query(sys.argv[i], print_out)
        elif arg == "-s":
            i += 1
            if i == len(sys.argv): usage(sys.argv[0])
            print("Solve not implemented")
        i += 1
