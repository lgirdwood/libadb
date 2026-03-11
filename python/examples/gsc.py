import sys
import time
import math
from astrodb import (
    Library, Database, Table, ObjectSet, Search, AstroDBError,
    ADB_COMP_LT, ADB_COMP_GT, ADB_OP_AND, ADB_OP_OR
)

D2R = 1.7453292519943295769e-2
R2D = 5.7295779513082320877e1

def print_objects(oset):
    for obj in oset:
        print(f"Obj: {obj.designation.decode('utf-8')} RA: {obj.ra * R2D:f} DEC: {obj.dec * R2D:f} Mag {obj.mag:3.2f}")

def search_print(search):
    for obj in search:
        print(f"Obj: {obj.designation.decode('utf-8')} RA: {obj.ra * R2D:f} DEC: {obj.dec * R2D:f} Mag {obj.mag:3.2f}")

def get_all(db, table_name, print_out=False):
    print("Get all objects")
    try:
        tbl = Table(db, "I", "254", table_name)
        oset = ObjectSet(tbl)
        oset.apply_constraints(0.0, 0.0, 2.0 * math.pi, 0.0, 16.0)
        oset.populate()
        
        print(f" found {len(oset)} list heads {len(oset)} objects\n")
        if print_out:
            print_objects(oset)
        
        oset.close()
        tbl.close()
    except AstroDBError as e:
        print(f"Failed to get_all: {e}")

def search1(db, table_name, print_out=False):
    print("Searching objects")
    try:
        tbl = Table(db, "I", "254", table_name)
        search = Search(tbl)
        oset = ObjectSet(tbl)
        
        search.add_comparator("DEdeg", ADB_COMP_LT, "58.434773")
        search.add_comparator("DEdeg", ADB_COMP_GT, "57.678541")
        search.add_operator(ADB_OP_AND)
        
        search.add_comparator("RAdeg", ADB_COMP_LT, "342.434232")
        search.add_comparator("RAdeg", ADB_COMP_GT, "341.339925")
        search.add_operator(ADB_OP_AND)
        
        search.add_operator(ADB_OP_AND)
        
        start_t = time.time()
        search.execute(oset)
        end_t = time.time()
        
        secs = end_t - start_t
        tests = search.tests
        if secs > 0:
            print(f"   Time {secs * 1000.0:3.1f} msecs @ {tests / secs:3.3e} objects per sec")
        else:
            print(f"   Time 0.0 msecs @ inf objects per sec")
        
        print(f"   Search got {search.hits} objects out of {search.tests} tests\n")
        
        if print_out:
            search_print(search)
            
        search.close()
        oset.close()
        tbl.close()
    except AstroDBError as e:
        print(f"Search failed: {e}")

def gsc_query(lib_dir, print_out=False):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 9, 1)
        
        search1(db, "gsc", print_out)
        get_all(db, "gsc", print_out)
        
        db.close()
        lib.close()
    except AstroDBError as e:
        print(f"Query failed: {e}")

def gsc_import(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 9, 1)
        
        tbl = Table.import_new(db, "I", "254", "out", "Pmag", -2.0, 17.0)
        tbl.import_alt_dataset("gsc", 0)
        
        # Add GSC schema layout equivalent
        tbl.import_field("Designation", "GSC", libadb.ADB_CTYPE_STRING, "", 0)
        tbl.import_field("RA", "RAdeg", libadb.ADB_CTYPE_DEGREES, "degrees", 1)
        tbl.import_field("DEC", "DEdeg", libadb.ADB_CTYPE_DEGREES, "degrees", 1)
        tbl.import_field("Photographic Mag", "Pmag", libadb.ADB_CTYPE_FLOAT, "", 0)
        tbl.import_field("Mag error", "e_Pmag", libadb.ADB_CTYPE_FLOAT, "", 0)
        tbl.import_field("Pos error", "PosErr", libadb.ADB_CTYPE_FLOAT, "", 0)
        
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
            gsc_import(sys.argv[i])
        elif arg == "-q":
            i += 1
            if i == len(sys.argv): usage(sys.argv[0])
            gsc_query(sys.argv[i], print_out)
        elif arg == "-s":
            i += 1
            if i == len(sys.argv): usage(sys.argv[0])
            print("Solve not implemented")
        i += 1
