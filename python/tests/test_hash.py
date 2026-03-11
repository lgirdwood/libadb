import unittest
import os
from astrodb import Library, Database, Table, AstroDBError

class TestHash(unittest.TestCase):
    def setUp(self):
        self.local_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'tests')
        self.lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", self.local_dir)
        self.db = Database(self.lib, 7, 1)

    def tearDown(self):
        self.db.close()
        self.lib.close()

    def test_hash_build(self):
        try:
            tbl = Table(self.db, "V", "109", "sky2kv4")
            # Build a hash map for "Name" similar to the C unit test
            tbl.hash_key("Name")
            
            # Ensure the table is still healthy and sizes are intact
            self.assertTrue(tbl.size > 0)
            self.assertTrue(len(tbl) > 0)
            
            tbl.close()
        except AstroDBError as e:
            self.skipTest(f"Table open failed, dataset might be missing locally: {e}")

if __name__ == '__main__':
    unittest.main()
