import sys
import math
import ctypes
from astrodb import (
    Library, Database, Table, ObjectSet, AstroDBError,
)
from astrodb.lib import (
    ADB_CTYPE_STRING, ADB_CTYPE_INT, ADB_CTYPE_FLOAT,
    ADB_CTYPE_DEGREES
)

D2R = 1.7453292519943295769e-2
R2D = 5.7295779513082320877e1

def print_objects(oset):
    for obj in oset:
        print(f"Obj: {obj.designation.decode('utf-8')} {obj.id} RA: {obj.ra * R2D:f} DEC: {obj.dec * R2D:f} Mag {obj.mag:f}")

def get_all(db, tbl, print_out=False):
    print("Get all objects")
    oset = ObjectSet(tbl)
    oset.apply_constraints(0.0, 0.0, 2.0 * math.pi, 0.0, 16.0)
    oset.populate()
    
    print(f" found {len(oset)} object list heads {len(oset)} objects\n")
    if print_out:
        print_objects(oset)

    oset.hash_key("Designation")
    dnames = [
        "3992-746-1", "3992-942-1", "3992-193-1", "3996-1436-1",
        "3992-648-1", "3992-645-1", "3992-349-1", "3992-882-1",
    ]
    for name in dnames:
        found = oset.get_object(name, "Designation")
        if not found:
            print(f"can't find {name}", file=sys.stderr)
            
    oset.close()

def get2(db, tbl, print_out=False):
    print("Get all objects around 1 deg FoV RA 341.0, DEC 58.0")
    oset = ObjectSet(tbl)
    oset.apply_constraints(341.0 * D2R, 58.0 * D2R, 1.0 * D2R, -2.0, 16.0)
    oset.populate()
    
    print(f" found {len(oset)} object list heads {len(oset)} objects\n")
    if print_out:
        print_objects(oset)
        
    oset.close()

def tycho_query(lib_dir, print_out=False):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        
        tbl = Table(db, "I", "250", "catalog")
        tbl.hash_key("HD")
        tbl.hash_key("HIP")
        
        get_all(db, tbl, print_out)
        get2(db, tbl, print_out)
        
        tbl.close()
        db.close()
        lib.close()
    except AstroDBError as e:
        print(f"Query failed: {e}")

def tycho_import(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        
        # ADB_IMPORT_DEC = 1
        tbl = Table.import_new(db, "I", "250", "catalog", "VT", 0.0, 16.0, 1)
        
        # Add schema equivalent
        tbl.import_field("Name", "TYCID1", ADB_CTYPE_INT, "", 0)
        tbl.import_field("Name", "TYCID2", ADB_CTYPE_INT, "", 0)
        tbl.import_field("Name", "TYCID3", ADB_CTYPE_INT, "", 0)
        tbl.import_field("RA", "RAdeg", ADB_CTYPE_DEGREES, "degrees", 0)
        tbl.import_field("DEC", "DEdeg", ADB_CTYPE_DEGREES, "degrees", 0)
        tbl.import_field("Mag", "VT", ADB_CTYPE_FLOAT, "", 0)
        tbl.import_field("pmRA", "pmRA", ADB_CTYPE_FLOAT, "mas/a", 0)
        tbl.import_field("pmDEC", "pmDEC", ADB_CTYPE_FLOAT, "mas/a", 0)
        tbl.import_field("Parallax", "Plx", ADB_CTYPE_FLOAT, "mas", 0)
        tbl.import_field("HD Number", "HD", ADB_CTYPE_INT, "", 0)
        tbl.import_field("HIP Number", "HIP", ADB_CTYPE_INT, "", 0)
        
        tbl.run_import()
        tbl.close()
        db.close()
        lib.close()
    except AstroDBError as e:
        print(f"Import failed: {e}")

def tycho_solve(lib_dir):
    pass

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
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg == "-i":
            i += 1
            if i >= len(sys.argv): usage(sys.argv[0])
            tycho_import(sys.argv[i])
        elif arg == "-q":
            i += 1
            if i >= len(sys.argv): usage(sys.argv[0])
            tycho_query(sys.argv[i], print_out)
        elif arg == "-s":
            i += 1
            if i >= len(sys.argv): usage(sys.argv[0])
            tycho_solve(sys.argv[i])
        i += 1
