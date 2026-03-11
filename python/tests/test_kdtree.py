import unittest
import os
import math
from astrodb import Library, Database, Table, ObjectSet, AstroDBError

D2R = 1.7453292519943295769e-2

class TestKDTree(unittest.TestCase):
    def setUp(self):
        self.local_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'tests')
        self.lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", self.local_dir)
        self.db = Database(self.lib, 7, 1)

    def tearDown(self):
        self.db.close()
        self.lib.close()

    def test_kdtree_neighbors(self):
        tbl = Table(self.db, "V", "109", "sky2kv4")
        tbl.hash_key("_DESIGNATION")

        oset = ObjectSet(tbl)
        try:
            oset.apply_constraints(0.0 * D2R, 0.0 * D2R, 360.0 * D2R, -90.0, 90.0)
        except AstroDBError:
            pass # C test ignores -EINVAL on 360 FOV constraint evaluation

        oset.populate()

        # Nearest to Pole tests M_PI_2 for DEC
        nearest_pos = oset.get_nearest_on_pos(0.0, math.pi / 2.0)
        self.assertIsNotNone(nearest_pos)

        oset.close()
        tbl.close()

if __name__ == '__main__':
    unittest.main()
