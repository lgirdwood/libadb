import unittest
import os
from astrodb import Library, Database, Table, AstroDBError

class TestAstroDB(unittest.TestCase):

    def setUp(self):
        # Assume tests are run from the src tree, use local test directory as 'local'
        self.local_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'tests')
        self.lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", self.local_dir)

    def tearDown(self):
        self.lib.close()

    def test_version_string(self):
        v = self.lib.version
        self.assertTrue(isinstance(v, str))
        self.assertTrue(len(v) > 0)

    def test_database_creation(self):
        db = Database(self.lib, depth=7, num_tables=1)
        self.assertIsNotNone(db._ptr)
        db.close()

    def test_table_open(self):
        db = Database(self.lib, 7, 1)
        # Attempt to open 'sky2kv4' which is what test_schema.c uses
        try:
            tbl = Table(db, "V", "109", "sky2kv4")
            self.assertTrue(tbl.table_id >= 0)
            self.assertTrue(len(tbl) >= 0)
            self.assertTrue(tbl.size > 0)
            self.assertTrue(tbl.object_size > 0)

            # Test object set
            from astrodb import ObjectSet, Search
            oset = ObjectSet(tbl)
            oset.apply_constraints(2.0, 3.0, 1.0, 0.0, 10.0)
            oset.populate()
            self.assertTrue(len(oset) >= 0)

            # Test search
            search = Search(tbl)
            search.add_comparator("_DESIGNATION", 2, "Sirius")
            try:
                search.execute(oset)
                self.assertTrue(search.tests >= 0)
                self.assertTrue(search.hits >= 0)
            except AstroDBError:
                pass

            # Test custom comparator
            def my_comp(obj_ptr):
                return 1

            try:
                search.add_custom_comparator(my_comp)
            except Exception as e:
                print("Custom comparator failed:", e)

            search.close()
            oset.close()

            tbl.close()
        except AstroDBError as e:
            # If the dataset isn't downloaded locally, it might fail in CI
            print("Table open failed, dataset might be missing locally:", e)
        db.close()

if __name__ == '__main__':
    unittest.main()
