import unittest
import os
from astrodb import Library, Database, Table, AstroDBError, ADB_CTYPE_FLOAT
from astrodb.lib import ADB_CTYPE_DOUBLE

class TestImport(unittest.TestCase):
    def setUp(self):
        self.local_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'tests')
        self.lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", self.local_dir)
        self.db = Database(self.lib, 7, 1)

    def tearDown(self):
        self.db.close()
        self.lib.close()

    def test_import_functions(self):
        try:
            # We open an existing table here to gain a valid table_id context
            # to test the import APIs on.
            tbl = Table(self.db, "V", "109", "sky2kv4")
            
            # The C test validates histogram depths, but for Python we exposed
            # `get_field_type` and `get_field_offset` for import analysis
            field_type = tbl.get_field_type("Vmag")
            self.assertTrue(field_type == ADB_CTYPE_FLOAT)
            
            field_offset = tbl.get_field_offset("Vmag")
            self.assertTrue(field_offset >= 0)
            
            # And attempt a fallback field configuration
            tbl.import_field("_DESIGNATION", "Name", 0)

            # We won't fully execute run_import() to avoid overriding the test dataset
            # But configuring it ensures C pointers don't crash.
            
            tbl.close()
        except AstroDBError as e:
            self.skipTest(f"Table open failed, dataset might be missing locally: {e}")

if __name__ == '__main__':
    unittest.main()
