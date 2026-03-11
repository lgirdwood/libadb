import unittest
import os
from astrodb import Library, Database, Table, Solver, AstroDBError

class TestSolver(unittest.TestCase):
    def setUp(self):
        self.local_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'tests')
        self.lib = Library("cdsarc.u-strasbg.fr", "/pub/cats", self.local_dir)
        self.db = Database(self.lib, 7, 1)

    def tearDown(self):
        self.db.close()
        self.lib.close()

    def test_solver_initialization(self):
        try:
            tbl = Table(self.db, "V", "109", "sky2kv4")
            solver = Solver(tbl)
            self.assertIsNotNone(solver._ptr)

            # Test basic parameter setting
            solver.set_magnitude_delta(1.0)
            solver.set_distance_delta(50.0)
            solver.set_pa_delta(2.0)

            # Test adding plate object
            solver.add_plate_object(100, 100, 500, 0)
            solver.add_plate_object(200, 200, 600, 0)

            solver.close()
            tbl.close()
        except AstroDBError as e:
            self.skipTest(f"Skipping solver test because dataset might be missing: {e}")

if __name__ == '__main__':
    unittest.main()
