import sys
from astrodb import (
    Library, Database, Table, AstroDBError,
)

def vii110a_main(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 0, 1)

        print("Opening VII 110A s catalog")
        
        # In python binding Table(db, class, id, name) opens it cleanly.
        tbl = Table(db, "VII", "110A", "s")
        
        # Equivalent to the C script terminating gracefully
        tbl.close()
        db.close()
        lib.close()
    except AstroDBError as e:
        print(f"Failed: {e}")

def usage(name):
    print(f"Usage: {name} [library dir]")
    sys.exit(0)

if __name__ == '__main__':
    print(f"{sys.argv[0]} using python astrodb binding\n")
    if len(sys.argv) < 2:
        usage(sys.argv[0])
        
    vii110a_main(sys.argv[1])
