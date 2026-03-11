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
        print(f"Obj: {obj.designation.decode('utf-8')} {obj.id} RA: {obj.ra * R2D:f} DEC: {obj.dec * R2D:f} Mag {obj.mag:f} size {obj.size:f} desc {obj.get_string('Desc')}")

def get_all(db, table_id, print_out=False):
    print("Get all objects")
    try:
        tbl = Table(db, "VII", "118", "ngc2000")
        oset = ObjectSet(tbl)
        oset.apply_constraints(0.0, 0.0, 2.0 * math.pi, 0.0, 16.0)
        oset.populate()
        
        print(f" found {len(oset)} object list heads {len(oset)} objects\n")
        if print_out:
            print_objects(oset)
            
        oset.close()
        tbl.close()
    except AstroDBError as e:
        print(f"Failed to get_all: {e}")

def ngc_query(lib_dir, print_out=False):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 5, 1)
        
        tbl = Table(db, "VII", "118", "ngc2000")
        get_all(db, tbl.table_id, print_out)
        
        tbl.close()
        db.close()
        lib.close()
    except AstroDBError as e:
        print(f"Query failed: {e}")

def ngc_import(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 5, 1)
        
        tbl = Table.import_new(db, "VII", "118", "ngc2000", "mag", 0.0, 18.0)
        
        # Add ngc schema layout equivalent
        tbl.import_field("Name", "Name", ADB_CTYPE_STRING, "", 0)
        tbl.import_field("Type", "Type", ADB_CTYPE_STRING, "", 0)
        tbl.import_field("RA Hours", "RAh", ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 1)
        tbl.import_field("RA Minutes", "RAm", ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 0)
        tbl.import_field("DEC Degrees", "DEd", ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 2)
        tbl.import_field("DEC Minutes", "DEm", ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 1)
        tbl.import_field("DEC sign", "DE-", ADB_CTYPE_SIGN, "", 0)
        tbl.import_field("Integrated Mag", "mag", ADB_CTYPE_FLOAT, "", 0)
        tbl.import_field("Description", "Desc", ADB_CTYPE_STRING, "", 0)
        tbl.import_field("Largest Dimension", "size", ADB_CTYPE_FLOAT, "arcmin", 0)
        
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
            ngc_import(sys.argv[i])
        elif arg == "-q":
            i += 1
            if i == len(sys.argv): usage(sys.argv[0])
            ngc_query(sys.argv[i], print_out)
        elif arg == "-s":
            i += 1
            if i == len(sys.argv): usage(sys.argv[0])
            print("Solve not implemented")
        i += 1
