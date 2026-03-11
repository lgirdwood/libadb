import sys
import ctypes
from astrodb import (
    Library, Database, Table, AstroDBError,
)
from astrodb.lib import (
    ADB_CTYPE_STRING, ADB_CTYPE_INT, ADB_CTYPE_FLOAT,
    ADB_CTYPE_DOUBLE_HMS_HRS, ADB_CTYPE_DOUBLE_HMS_MINS,
    ADB_CTYPE_DOUBLE_HMS_SECS, ADB_CTYPE_DOUBLE_DMS_DEGS,
    ADB_CTYPE_DOUBLE_DMS_MINS, ADB_CTYPE_DOUBLE_DMS_SECS,
    ADB_CTYPE_SIGN, adb_object_p, adb_object
)

D2R = 1.7453292519943295769e-2
R2D = 5.7295779513082320877e1

def v84_merge(lib_dir):
    try:
        lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", lib_dir)
        db = Database(lib, 0, 1)

        main_tbl = Table.import_new(db, "V", "84", "main", "Vmag", -2.0, 8.0)
        main_tbl.import_field("Name", "Name", ADB_CTYPE_STRING, "", 0)
        main_tbl.import_field("ID", "PNG", ADB_CTYPE_STRING, "", 0)
        main_tbl.import_field("RA Hours", "RAh", ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 2)
        main_tbl.import_field("RA Minutes", "RAm", ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 1)
        main_tbl.import_field("RA Seconds", "RAs", ADB_CTYPE_DOUBLE_HMS_SECS, "seconds", 0)
        main_tbl.import_field("DEC Degrees", "DEd", ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 3)
        main_tbl.import_field("DEC Minutes", "DEm", ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 2)
        main_tbl.import_field("DEC Seconds", "DEs", ADB_CTYPE_DOUBLE_DMS_SECS, "seconds", 1)
        main_tbl.import_field("DEC sign", "DE-", ADB_CTYPE_SIGN, "", 0)
        main_tbl.hash_key("PNG")
        main_tbl.run_import()

        diam_tbl = Table.import_new(db, "V", "84", "diam", "Vmag", -2.0, 8.0)
        diam_tbl.import_field("ID", "PNG", ADB_CTYPE_STRING, "", 0)
        diam_tbl.import_field("Opt Diam", "oDiam", ADB_CTYPE_FLOAT, "arcsec", 0)
        diam_tbl.import_field("Rad Diam", "rDiam", ADB_CTYPE_FLOAT, "arcsec", 0)
        diam_tbl.hash_key("PNG")
        diam_tbl.run_import()

        dist_tbl = Table.import_new(db, "V", "84", "dist", "Vmag", -2.0, 8.0)
        dist_tbl.import_field("ID", "PNG", ADB_CTYPE_STRING, "", 0)
        dist_tbl.import_field("Distance", "Dist", ADB_CTYPE_FLOAT, "kpc", 0)
        dist_tbl.hash_key("PNG")
        dist_tbl.run_import()

        vel_tbl = Table.import_new(db, "V", "84", "vel", "Vmag", -2.0, 8.0)
        vel_tbl.import_field("ID", "PNG", ADB_CTYPE_STRING, "", 0)
        vel_tbl.import_field("Radial Vel", "RVel", ADB_CTYPE_FLOAT, "kms", 0)
        vel_tbl.hash_key("PNG")
        vel_tbl.run_import()

        cstar_tbl = Table.import_new(db, "V", "84", "cstar", "Vmag", -2.0, 8.0)
        cstar_tbl.import_field("ID", "PNG", ADB_CTYPE_STRING, "", 0)
        cstar_tbl.import_field("Visual Mag", "Vmag", ADB_CTYPE_FLOAT, "", 0)
        cstar_tbl.hash_key("PNG")
        cstar_tbl.run_import()

        png_tbl = Table.import_new(db, "V", "84", "png", "Vmag", -2.0, 8.0)
        png_tbl.import_field("Name", "Name", ADB_CTYPE_STRING, "", 0)
        png_tbl.import_field("ID", "PNG", ADB_CTYPE_STRING, "", 0)
        png_tbl.import_field("RA Hours", "RAh", ADB_CTYPE_DOUBLE_HMS_HRS, "hours", 2)
        png_tbl.import_field("RA Minutes", "RAm", ADB_CTYPE_DOUBLE_HMS_MINS, "minutes", 1)
        png_tbl.import_field("RA Seconds", "RAs", ADB_CTYPE_DOUBLE_HMS_SECS, "seconds", 0)
        png_tbl.import_field("DEC Degrees", "DEd", ADB_CTYPE_DOUBLE_DMS_DEGS, "degrees", 3)
        png_tbl.import_field("DEC Minutes", "DEm", ADB_CTYPE_DOUBLE_DMS_MINS, "minutes", 2)
        png_tbl.import_field("DEC Seconds", "DEs", ADB_CTYPE_DOUBLE_DMS_SECS, "seconds", 1)
        png_tbl.import_field("DEC sign", "DE-", ADB_CTYPE_SIGN, "", 0)
        png_tbl.import_field("Visual Mag", "Vmag", ADB_CTYPE_FLOAT, "", 0)
        png_tbl.import_field("Opt Diam", "oDiam", ADB_CTYPE_FLOAT, "arcsec", 0)
        png_tbl.import_field("Rad Diam", "rDiam", ADB_CTYPE_FLOAT, "arcsec", 0)
        png_tbl.import_field("Distance", "Dist", ADB_CTYPE_FLOAT, "kpc", 0)
        png_tbl.import_field("Radial Vel", "Rvel", ADB_CTYPE_FLOAT, "kms", 0)
        png_tbl.hash_key("PNG")
        
        # We don't have python inserts implemented, the python API is query-only for `Table`.
        # So we skip the insert loop here as it's impossible without binding adb_table_insert_object.
        print("Data merge logic would execute here using adb_table_insert_object!")
        
        png_tbl.close()
        main_tbl.close()
        diam_tbl.close()
        dist_tbl.close()
        vel_tbl.close()
        cstar_tbl.close()
        db.close()
        lib.close()
    except AstroDBError as e:
        print(f"ETL failed: {e}")

def usage(name):
    print(f"Merge ETL: {name} -i [import dir]")
    sys.exit(0)

if __name__ == '__main__':
    print(f"{sys.argv[0]} using python astrodb binding\n")
    if len(sys.argv) < 3:
        usage(sys.argv[0])
        
    i = 1
    while i < len(sys.argv):
        if sys.argv[i] == "-i":
            i += 1
            if i >= len(sys.argv): usage(sys.argv[0])
            v84_merge(sys.argv[i])
        i += 1
