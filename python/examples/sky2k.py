import sys
import time
import math
from astrodb import (
    Library, Database, Table, ObjectSet, Search, Solver, Solution, AstroDBError,
    ADB_COMP_LT, ADB_COMP_GT, ADB_OP_AND, ADB_OP_OR, ADB_COMP_EQ,
    ADB_FIND_FIRST, ADB_CONSTRAINT_MAG, ADB_CONSTRAINT_FOV,
)
from astrodb.lib import (
    ADB_CTYPE_STRING, ADB_CTYPE_INT, ADB_CTYPE_DOUBLE_HMS_HRS,
    ADB_CTYPE_DOUBLE_HMS_MINS, ADB_CTYPE_DOUBLE_HMS_SECS,
    ADB_CTYPE_DOUBLE_DMS_DEGS, ADB_CTYPE_DOUBLE_DMS_MINS,
    ADB_CTYPE_DOUBLE_DMS_SECS, ADB_CTYPE_SIGN, ADB_CTYPE_FLOAT,
    ADB_CTYPE_DOUBLE, ADB_CTYPE_SHORT, adb_pobject,
)

D2R = 1.7453292519943295769e-2
R2D = 5.7295779513082320877e1

def print_objects(oset):
    for obj in oset:
        print(f"Obj: {obj.get_string('Name')} {obj.id} RA: {obj.ra * R2D:f} DEC: {obj.dec * R2D:f} Mag {obj.mag:f} Type {obj.get_string('Sp')} HD {obj.get_int('HD')}")

def search_print(search):
    for obj in search:
        print(f"Obj:{obj.get_string('Name')} {obj.id} RA: {obj.ra * R2D:f} DEC: {obj.dec * R2D:f} Mag {obj.mag:f} Type {obj.get_string('Sp')} HD {obj.get_int('HD')}")

def object_printf(obj):
    print(f"Obj: {obj.get_string('Name')} {obj.id} RA: {obj.ra * R2D:f} DEC: {obj.dec * R2D:f} Mag {obj.mag:f} Type {obj.get_string('Sp')} HD {obj.get_int('HD')}")

def sobject_printf(sobject):
    print(f"Plate object X {sobject.x()} Y {sobject.y()} ADU {sobject.adu()}")
    if sobject.has_object():
        obj = sobject.get_object()
        print(f" Obj: {obj.get_string('Name')} {obj.id} RA: {obj.ra * R2D:f} DEC: {obj.dec * R2D:f} Mag {obj.mag:f} Type {obj.get_string('Sp')} HD {obj.get_int('HD')}")
    print(f" Estimated plate RA: {sobject.ra() * R2D:f} DEC: {sobject.dec() * R2D:f} Mag: {sobject.mag():f}")

def get1(db, tbl, print_out=False):
    print("Get all objects")
    oset = ObjectSet(tbl)
    oset.apply_constraints(0.0, 0.0, 2.0 * math.pi, -2.0, 16.0)
    oset.populate()
    print(f" found {len(oset)} object list heads {len(oset)} objects\n")
    if print_out: print_objects(oset)
    oset.close()

def get2(db, tbl, print_out=False):
    print("Get all objects < mag 2")
    oset = ObjectSet(tbl)
    oset.apply_constraints(0.0, 0.0, 2.0 * math.pi, -2.0, 2.0)
    oset.populate()
    print(f" found {len(oset)} object list heads {len(oset)} objects\n")
    if print_out: print_objects(oset)
    oset.close()

def get3(db, tbl, print_out=False):
    print("Get all objects < mag 2, in radius 30 deg around 0,0")
    oset = ObjectSet(tbl)
    oset.apply_constraints(0.0, 0.0, 30.0 * D2R, -2.0, 2.0)
    oset.populate()
    print(f" found {len(oset)} object list heads {len(oset)} objects\n")
    if print_out: print_objects(oset)
    oset.close()

def get4(db, tbl, print_out=False):
    oset = ObjectSet(tbl)
    
    # Needs to be populated before lookups
    oset.populate()
    print("Get object HD 58977 ")
    found = tbl.get_object(58977, "HD")
    if found: object_printf(found)

    print("Get object 21alp Sc")
    found = tbl.get_object("21alp Sc", "Name")
    if found: object_printf(found)
    
    print("Get nearest object in db to 21alp Sc")
    if found:
        near = oset.get_nearest_on_object(found)
        if near: object_printf(near)
    
    print("Get nearest object in db to north pole")
    near = oset.get_nearest_on_pos(0.0, math.pi / 2.0)
    if near: object_printf(near)
    
    near = oset.get_nearest_on_pos(181.0 * D2R, 90.0 * D2R)
    if near: object_printf(near)
    
    oset.close()

def search1(db, tbl, print_out=False):
    print("Searching for high PM or RV objects")
    search = Search(tbl)
    oset = ObjectSet(tbl)
    
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
    
    if print_out: search_print(search)
        
    search.close()
    oset.close()

def search2(db, tbl, print_out=False):
    print("Searching for G5 class objects")
    search = Search(tbl)
    oset = ObjectSet(tbl)
    
    search.add_comparator("Sp", ADB_COMP_EQ, "G5*")
    search.add_operator(ADB_OP_OR)
    
    start_t = time.time()
    search.execute(oset)
    end_t = time.time()
    
    secs = end_t - start_t
    tests = search.tests
    if secs > 0:
        print(f"   Time {secs * 1000.0:3.1f} msecs @ {tests / secs:3.3e} objects per sec")
    else:
        print(f"   Time 0.0 msecs @ inf objects per sec")
        
    print(f"   Search found {search.hits} objects out of {search.tests} tests\n")
    if print_out: search_print(search)
    search.close()
    oset.close()

def search3(db, tbl, print_out=False):
    print("Searching for M1 class objects")
    search = Search(tbl)
    oset = ObjectSet(tbl)
    
    search.add_comparator("Sp", ADB_COMP_EQ, "M1")
    search.add_operator(ADB_OP_OR)
    
    start_t = time.time()
    search.execute(oset)
    end_t = time.time()
    
    secs = end_t - start_t
    tests = search.tests
    if secs > 0:
        print(f"   Time {secs * 1000.0:3.1f} msecs @ {tests / secs:3.3e} objects per sec")
    else:
        print(f"   Time 0.0 msecs @ inf objects per sec")
        
    print(f"   Found {search.hits} objects out of {search.tests} tests\n")
    if print_out: search_print(search)
    search.close()
    oset.close()

def sky2k_solve(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        tbl = Table(db, "V", "109", "sky2kv4")
        
        oset = ObjectSet(tbl)
        oset.apply_constraints(0.0 * D2R, 0.0 * D2R, 360.0 * D2R, -90.0, 90.0)
        
        solve = Solver(tbl)
        solve.constraint(ADB_CONSTRAINT_MAG, 6.0, -2.0)
        solve.constraint(ADB_CONSTRAINT_FOV, 0.1 * D2R, 5.0 * D2R)
        
        # Pleiades M45 plate objects 
        pobject = [
            adb_pobject(513, 434, 408725),
            adb_pobject(141, 545, 123643),
            adb_pobject(1049, 197, 128424),
            adb_pobject(956, 517, 106906),
            adb_pobject(682, 180, 98841),
            adb_pobject(173, 623, 37537),
        ]
        
        for p in pobject[:5]:
            solve.add_plate_object(p)
            
        solve.set_magnitude_delta(0.5)
        solve.set_distance_delta(5.0)
        solve.set_pa_delta(2.0 * D2R)
        
        start_t = time.time()
        found = solve.execute(oset, ADB_FIND_FIRST)
        secs = time.time() - start_t
        
        if secs > 0:
            print(f"   Time {secs * 1000.0:3.1f} msecs @ {found / secs:3.3e} objects per sec")
        else:
            print(f"   Time 0.0 msecs @ inf objects per sec")
            
        print(f"found {found} solutions")
        
        if found > 0:
            for i in range(found):
                solution = solve.get_solution(i)
                solution.set_search_limits(2.0, 8.0, tbl.table_id)
                print(f"Solution {i} score {solution.divergence():f}")
                
                solution.add_pobjects(pobject)
                solution.get_objects()
                
                for j in range(6):
                    sobject_printf(solution.get_object(j))
                    
        solve.close()
        oset.close()
        tbl.close()
        db.close()
        lib.close()
    except AstroDBError as e:
        print(f"Solve failed: {e}")

def sky2k_query(lib_dir, print_out=False):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        
        tbl = Table(db, "V", "109", "sky2kv4")
        tbl.hash_key("HD")
        tbl.hash_key("Name")
        
        get1(db, tbl, print_out)
        get2(db, tbl, print_out)
        get3(db, tbl, print_out)
        search1(db, tbl, print_out)
        search2(db, tbl, print_out)
        search3(db, tbl, print_out)
        get4(db, tbl, print_out)
        
        tbl.close()
        db.close()
        lib.close()
    except AstroDBError as e:
        print(f"Query failed: {e}")

def sky2k_import(lib_dir, mag_limit):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 7, 1)
        
        tbl = Table.import_new(db, "V", "109", "sky2kv4", "Vmag", -2.0, mag_limit)
        
        # Add sky2k schema layout equivalent
        tbl.import_field("Name", "Name", ADB_CTYPE_STRING, "", 0)
        tbl.import_field("ID", "ID", ADB_CTYPE_INT, "", 0)
        tbl.import_field("RA Hours", "RAh", ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 2)
        tbl.import_field("RA Minutes", "RAm", ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 1)
        tbl.import_field("RA Seconds", "RAs", ADB_CTYPE_DOUBLE_HMS_SECS, "seconds", 0)
        tbl.import_field("DEC Degrees", "DEd", ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 3)
        tbl.import_field("DEC Minutes", "DEm", ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 2)
        tbl.import_field("DEC Seconds", "DEs", ADB_CTYPE_DOUBLE_DMS_SECS, "seconds", 1)
        tbl.import_field("DEC sign", "DE-", ADB_CTYPE_SIGN, "", 0)
        tbl.import_field("Visual Mag", "Vmag", ADB_CTYPE_FLOAT, "", 0)
        tbl.import_field("sp", "Sp", ADB_CTYPE_STRING, "", 0)
        tbl.import_field("HD", "HD", ADB_CTYPE_INT, "", 0)
        tbl.import_field("SAO", "SAO", ADB_CTYPE_INT, "", 0)
        tbl.import_field("PPM", "PPM", ADB_CTYPE_INT, "", 0)
        tbl.import_field("pmRA", "pmRA", ADB_CTYPE_DOUBLE, "", 0)
        tbl.import_field("pmDEC", "pmDEC", ADB_CTYPE_DOUBLE, "", 0)
        tbl.import_field("Radial Vel", "RV", ADB_CTYPE_DOUBLE, "", 0)
        tbl.import_field("Binary sep", "sep", ADB_CTYPE_DOUBLE, "", 0)
        tbl.import_field("Dmag", "Dmag", ADB_CTYPE_FLOAT, "", 0)
        tbl.import_field("Orb Per", "orbPer", ADB_CTYPE_DOUBLE, "", 0)
        tbl.import_field("Pos Angle", "PA", ADB_CTYPE_SHORT, "", 0)
        tbl.import_field("Obs Date", "date", ADB_CTYPE_DOUBLE, "", 0)
        tbl.import_field("ID_A", "ID_A", ADB_CTYPE_INT, "", 0)
        tbl.import_field("ID_B", "ID_B", ADB_CTYPE_INT, "", 0)
        tbl.import_field("ID_C", "ID_C", ADB_CTYPE_INT, "", 0)
        tbl.import_field("Var max mag", "magMax", ADB_CTYPE_FLOAT, "", 0)
        tbl.import_field("Var min mag", "magMin", ADB_CTYPE_FLOAT, "", 0)
        tbl.import_field("Var Amp", "varAmp", ADB_CTYPE_FLOAT, "", 0)
        tbl.import_field("Var Period", "varPer", ADB_CTYPE_DOUBLE, "", 0)
        tbl.import_field("Var Epoch", "varEpo", ADB_CTYPE_DOUBLE, "", 0)
        tbl.import_field("Var Type", "varType", ADB_CTYPE_SHORT, "", 0)
        
        tbl.import_field("Vmag", "Vder", 0, "", 0)
        
        tbl.run_import()
        tbl.close()
        
        db.close()
        lib.close()
    except AstroDBError as e:
        print(f"Import failed: {e}")

def usage(name):
    print(f"Import: {name} -i [import dir] [-m max_mag]")
    print(f"Query: {name} -q [library dir]")
    print(f"Solve: {name} -s [library dir]")
    sys.exit(0)

if __name__ == '__main__':
    print(f"{sys.argv[0]} using python astrodb binding\n")
    
    if len(sys.argv) < 3:
        usage(sys.argv[0])
        
    print_out = "-p" in sys.argv
    mag_limit = 12.0
    
    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg == "-m":
            i += 1
            if i >= len(sys.argv): usage(sys.argv[0])
            mag_limit = float(sys.argv[i])
        elif arg == "-i":
            i += 1
            if i >= len(sys.argv): usage(sys.argv[0])
            sky2k_import(sys.argv[i], mag_limit)
        elif arg == "-q":
            i += 1
            if i >= len(sys.argv): usage(sys.argv[0])
            sky2k_query(sys.argv[i], print_out)
        elif arg == "-s":
            i += 1
            if i >= len(sys.argv): usage(sys.argv[0])
            sky2k_solve(sys.argv[i])
        elif arg == "-p":
            pass # handled above
        i += 1
