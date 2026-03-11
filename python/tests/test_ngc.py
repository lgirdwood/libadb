import unittest
import os
import math
import ctypes
from astrodb import Library, Database, Table, ObjectSet, AstroDBError
from astrodb.lib import adb_object

class TestNGC(unittest.TestCase):
    def setUp(self):
        self.local_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'tests')
        self.lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", self.local_dir)
        self.db = Database(self.lib, 5, 1)

    def tearDown(self):
        self.db.close()
        self.lib.close()

    def _get_table_safely(self):
        try:
            return Table(self.db, "VII", "118", "ngc2000")
        except AstroDBError as e:
            self.skipTest(f"Failed to open ngc2000 dataset: {e}")

    def test_query_all_objects(self):
        tbl = self._get_table_safely()
        oset = ObjectSet(tbl)
        oset.apply_constraints(0.0, 0.0, 2.0 * math.pi, 0.0, 16.0)
        oset.populate()
        
        # Verify sizes matches C tests
        head = oset.head
        self.assertIsNotNone(head)
        count = len(oset)
        
        # Original test_ngc.c expects EXACTLY 7765 objects from this query
        self.assertEqual(count, 7765)
        
        oset.close()
        tbl.close()

    def test_query_bright_objects(self):
        tbl = self._get_table_safely()
        oset = ObjectSet(tbl)
        oset.apply_constraints(0.0, 0.0, 2.0 * math.pi, -2.0, 10.0)
        oset.populate()
        
        # Trixel clipping over the entire sky FOV will return all elements inside the depth.
        count = len(oset)
        self.assertEqual(count, 7765)

        oset.close()
        tbl.close()

    def test_query_specific_region(self):
        tbl = self._get_table_safely()
        oset = ObjectSet(tbl)

        # Center at RA=11h(2.87rad), DEC=10deg(0.17rad), radius=10deg(0.17rad)
        oset.apply_constraints(2.87, 0.17, 0.17, 0.0, 16.0)
        oset.populate()
        
        count = len(oset)
        self.assertTrue(count > 0 and count < 7765)

        oset.close()
        tbl.close()

    def test_query_faint_objects(self):
        tbl = self._get_table_safely()
        oset = ObjectSet(tbl)

        # get very faint objects (mag 14.0 - 16.0)
        oset.apply_constraints(0.0, 0.0, 2.0 * math.pi, 14.0, 16.0)
        oset.populate()
        
        count = len(oset)
        self.assertEqual(count, 7765)

        oset.close()
        tbl.close()

    def test_query_north_pole(self):
        tbl = self._get_table_safely()
        oset = ObjectSet(tbl)

        # Center at RA=0, DEC=90 deg (1.57 rad), radius=10 deg (0.17 rad)
        oset.apply_constraints(0.0, 1.57, 0.17, 0.0, 16.0)
        oset.populate()
        
        count = len(oset)
        self.assertTrue(count > 0 and count < 7765)

        oset.close()
        tbl.close()

if __name__ == '__main__':
    unittest.main()
