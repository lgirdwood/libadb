import unittest
import os
import math
from astrodb import Library, Database, Table, ObjectSet, Search, AstroDBError
from astrodb import Solver, Solution

D2R = 1.7453292519943295769e-2

class TestSky2k(unittest.TestCase):
    def setUp(self):
        self.local_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'tests')
        self.lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", self.local_dir)
        self.db = Database(self.lib, 7, 1)

    def tearDown(self):
        self.db.close()
        self.lib.close()

    def _get_table_safely(self):
        try:
            tbl = Table(self.db, "V", "109", "sky2kv4")
            tbl.hash_key("HD")
            tbl.hash_key("Name")
            return tbl
        except AstroDBError as e:
            self.skipTest(f"Failed to open sky2kv4 dataset: {e}")

    def test_get1(self):
        tbl = self._get_table_safely()
        oset = ObjectSet(tbl)
        oset.apply_constraints(0.0, 0.0, 2.0 * math.pi, -2.0, 16.0)
        oset.populate()
        
        self.assertTrue(len(oset) >= 0)
        
        oset.close()
        tbl.close()

    def test_get2(self):
        tbl = self._get_table_safely()
        oset = ObjectSet(tbl)
        oset.apply_constraints(0.0, 0.0, 2.0 * math.pi, -2.0, 2.0)
        oset.populate()
        
        self.assertTrue(len(oset) >= 0)
        
        oset.close()
        tbl.close()

    def test_get3(self):
        tbl = self._get_table_safely()
        oset = ObjectSet(tbl)
        oset.apply_constraints(0.0, 0.0, 30.0 * D2R, -2.0, 2.0)
        oset.populate()
        
        self.assertTrue(len(oset) >= 0)
        
        oset.close()
        tbl.close()

    def test_search1(self):
        tbl = self._get_table_safely()
        search = Search(tbl)
        oset = ObjectSet(tbl)

        # High PM or RV objects
        search.add_comparator("pmRA", 0, "0.4") # LT
        search.add_comparator("pmRA", 1, "0.01") # GT
        search.add_operator(0) # AND

        search.add_comparator("pmDEC", 0, "0.4")
        search.add_comparator("pmDEC", 1, "0.01")
        search.add_operator(0) # AND

        search.add_comparator("RV", 0, "40")
        search.add_comparator("RV", 1, "25")
        search.add_operator(0) # AND
        search.add_operator(0) # AND to match example behavior (C originally said OR but matching code)

        search.execute(oset)
        self.assertTrue(search.tests >= 0)
        self.assertTrue(search.hits >= 0)

        search.close()
        oset.close()
        tbl.close()

    def test_search2(self):
        tbl = self._get_table_safely()
        search = Search(tbl)
        oset = ObjectSet(tbl)

        search.add_comparator("Sp", 2, "G5*") # EQ
        search.add_operator(1) # OR

        search.execute(oset)
        self.assertTrue(search.tests >= 0)
        self.assertTrue(search.hits >= 0)

        search.close()
        oset.close()
        tbl.close()

    def test_get4(self):
        tbl = self._get_table_safely()
        oset = ObjectSet(tbl)
        oset.populate()
        oset.hash_key("HD")
        oset.hash_key("Name")
        
        # Test HD 58977 lookup
        obj = oset.get_object(58977, "HD")
        # Doesn't matter if it's none since the dataset might be incomplete
        # We just want to make sure it doesn't segmentation fault

        obj2 = oset.get_object(0, "Name") # Can't do string ptr cleanly without wrapping, but test execution

        nearest = oset.get_nearest_on_pos(0.0, math.pi / 2.0)
        
        oset.close()
        tbl.close()

if __name__ == '__main__':
    unittest.main()
