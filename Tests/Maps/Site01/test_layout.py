"""Authored-layout regressions; these do not replace saved Unreal collision queries.

Run from the tactical-map checkout with:
    python3 -m unittest discover -s Tests/Maps/Site01 -p 'test_*.py' -v
"""

import importlib.util
import math
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "site01_design", ROOT / "Art/Source/Maps/Site01/design.py")
DESIGN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DESIGN)
RADIUS = 34.0
CLEARANCE = 10.0  # Additional authoring margin, not a CharacterMovement setting.
KIT_SIZES = {"Cover_200x120": (200, 100, 120), "Crate_100": (100, 100, 100)}


def obstacles():
    """Conservative full collision envelopes, including both stacked crates."""
    for label, center, size, _finish in DESIGN.BLOCKS:
        if center[2] + size[2] / 2 <= 0:
            continue  # The supporting floor is not a horizontal obstruction.
        yield label, (center[0] - size[0] / 2, center[1] - size[1] / 2,
                      center[0] + size[0] / 2, center[1] + size[1] / 2), center[2] + size[2] / 2
    for label, mesh, center, yaw, base in DESIGN.PROPS:
        sx, sy, sz = KIT_SIZES[mesh]
        if yaw % 180 == 90:
            sx, sy = sy, sx
        elif yaw % 180 != 0:
            raise ValueError("Test geometry requires axis-aligned prop rotations")
        yield label, (center[0] - sx / 2, center[1] - sy / 2,
                      center[0] + sx / 2, center[1] + sy / 2), base + sz


def point_segment_distance(point, start, end):
    dx, dy = end[0] - start[0], end[1] - start[1]
    length2 = dx * dx + dy * dy
    fraction = 0 if length2 == 0 else max(0, min(1,
        ((point[0] - start[0]) * dx + (point[1] - start[1]) * dy) / length2))
    return math.hypot(point[0] - start[0] - fraction * dx,
                      point[1] - start[1] - fraction * dy)


def segment_intersects_rectangle(start, end, rect):
    """Closed Liang–Barsky interval; touching an obstruction counts as contact."""
    low, high = 0.0, 1.0
    for axis in (0, 1):
        delta = end[axis] - start[axis]
        minimum, maximum = rect[axis], rect[axis + 2]
        if abs(delta) < 1e-12:
            if not minimum <= start[axis] <= maximum:
                return False
        else:
            enter, leave = sorted(((minimum - start[axis]) / delta,
                                   (maximum - start[axis]) / delta))
            low, high = max(low, enter), min(high, leave)
            if low > high:
                return False
    return True


def segment_rectangle_distance(start, end, rect):
    if segment_intersects_rectangle(start, end, rect):
        return 0.0
    corners = [(rect[x], rect[y]) for x in (0, 2) for y in (1, 3)]
    values = [point_segment_distance(p, start, end) for p in corners]
    for point in (start, end):
        values.append(math.hypot(max(rect[0] - point[0], 0, point[0] - rect[2]),
                                 max(rect[1] - point[1], 0, point[1] - rect[3])))
    return min(values)


class GeometryMathTests(unittest.TestCase):
    def test_segment_rectangle_distance_handles_crossing_parallel_and_corner(self):
        box = (0, 0, 100, 100)
        self.assertEqual(segment_rectangle_distance((-10, 50), (110, 50), box), 0)
        self.assertEqual(segment_rectangle_distance((-10, 140), (110, 140), box), 40)
        self.assertAlmostEqual(segment_rectangle_distance((-30, -40), (-30, -40), box), 50)
        self.assertEqual(segment_rectangle_distance((100, -10), (100, 110), box), 0)


class Site01LayoutTests(unittest.TestCase):
    def test_version_one_contract_and_role_swap_safe_identity(self):
        contract = DESIGN.contract()
        self.assertEqual((contract["schema_version"], contract["units"], contract["map_asset"]),
                         (1, "cm", "/Game/Maps/Site01/VS_Site01"))
        self.assertEqual(len(DESIGN.SPAWNS), 4)
        self.assertEqual(sorted(s["role"] for s in DESIGN.SPAWNS),
                         ["attacker", "attacker", "defender", "defender"])
        self.assertEqual(len({s["id"] for s in DESIGN.SPAWNS}), 4)
        labels = [s["actor_label"] for s in DESIGN.SPAWNS]
        labels += [DESIGN.SITE["actor_label"], DESIGN.RECOVERY["actor_label"]]
        self.assertEqual(len(set(labels)), 6)
        self.assertEqual(DESIGN.SITE["id"], "A")
        for bounds in (DESIGN.SITE, DESIGN.BOUNDS):
            self.assertEqual(len(bounds["center_cm"]), 3)
            self.assertEqual(len(bounds["half_extents_cm"]), 3)
            self.assertTrue(all(math.isfinite(v) and v > 0 for v in bounds["half_extents_cm"]))
        for spawn in DESIGN.SPAWNS:
            self.assertEqual(len(spawn["location_cm"]), 3)
            self.assertTrue(all(math.isfinite(v) for v in spawn["location_cm"]))
            self.assertTrue(math.isfinite(spawn["yaw_degrees"]))
            self.assertGreaterEqual(spawn["location_cm"][2], 88)

    def test_every_route_has_continuous_clear_capsule_centerline(self):
        """Swept-circle footprint is exact for these axis-aligned tall solids."""
        center, extent = DESIGN.BOUNDS["center_cm"], DESIGN.BOUNDS["half_extents_cm"]
        for route in DESIGN.ROUTES:
            points = route["points_cm"]
            self.assertGreaterEqual(len(points), 2)
            for point in points:
                for axis in (0, 1):
                    self.assertLessEqual(abs(point[axis] - center[axis]) + RADIUS + CLEARANCE,
                                         extent[axis], (route["id"], point, "bounds"))
            for index, (start, end) in enumerate(zip(points, points[1:])):
                self.assertGreater(math.dist(start, end), 0)
                for label, rect, _height in obstacles():
                    distance = segment_rectangle_distance(start, end, rect)
                    self.assertGreaterEqual(distance + 1e-6, RADIUS + CLEARANCE,
                        f"{route['id']} segment {index} {start}->{end}: {label} "
                        f"centerline clearance {distance:.2f} cm; need {RADIUS+CLEARANCE:.0f}")

    def test_spawn_and_recovery_clearance_and_floor_support(self):
        floor = next(b for b in DESIGN.BLOCKS if b[0] == "Floor")
        floor_center, floor_size = floor[1:3]
        self.assertEqual(floor_center[2] + floor_size[2] / 2, 0)
        positions = [(s["id"], s["location_cm"]) for s in DESIGN.SPAWNS]
        positions.append(("recovery", DESIGN.RECOVERY["location_cm"]))
        for name, position in positions:
            for label, rect, _height in obstacles():
                self.assertGreaterEqual(segment_rectangle_distance(position, position, rect),
                                        RADIUS + CLEARANCE, (name, label))
            for axis in (0, 1):
                self.assertLess(abs(position[axis] - floor_center[axis]) + RADIUS,
                                floor_size[axis] / 2, name)
        for index, spawn in enumerate(DESIGN.SPAWNS):
            for other in DESIGN.SPAWNS[index + 1:]:
                self.assertGreaterEqual(math.dist(spawn["location_cm"], other["location_cm"]), 200)

    def test_opposing_spawns_have_no_direct_standing_or_crouched_sightline(self):
        attackers = [s for s in DESIGN.SPAWNS if s["role"] == "attacker"]
        defenders = [s for s in DESIGN.SPAWNS if s["role"] == "defender"]
        for attacker in attackers:
            for defender in defenders:
                for eye_height in (88, 152):
                    self.assertTrue(any(height > eye_height and segment_intersects_rectangle(
                        attacker["location_cm"], defender["location_cm"], rect)
                        for _label, rect, height in obstacles()),
                        (attacker["id"], defender["id"], eye_height))

    def test_two_approaches_flank_and_defender_access_connect_to_site(self):
        attackers = [r for r in DESIGN.ROUTES if r["role"] == "attacker"]
        self.assertEqual(sum(r["kind"] == "approach" for r in attackers), 2)
        self.assertEqual(sum(r["kind"] == "flank" for r in attackers), 1)
        self.assertEqual(len({r["id"] for r in DESIGN.ROUTES}), len(DESIGN.ROUTES))
        for route in DESIGN.ROUTES:
            self.assertIn(route["points_cm"][0],
                          [s["location_cm"][:2] for s in DESIGN.SPAWNS if s["role"] == route["role"]])
            self.assertEqual(route["points_cm"][-1], DESIGN.SITE["center_cm"][:2])
        self.assertEqual(DESIGN.RECOVERY["location_cm"][:2], DESIGN.SITE["center_cm"][:2])
        lengths = {r["id"]: sum(math.dist(a, b) for a, b in zip(r["points_cm"], r["points_cm"][1:]))
                   for r in DESIGN.ROUTES}
        self.assertGreater(lengths["north_flank"], 1.25 * max(lengths["north_main"], lengths["south_main"]))
        flank = next(r for r in DESIGN.ROUTES if r["kind"] == "flank")
        self.assertTrue(any(p[1] > 1500 for p in flank["points_cm"]))
        self.assertTrue(any(p[0] > 1800 for p in flank["points_cm"]))

    def test_route_diagram_marks_timings_as_estimates_and_uses_authored_distances(self):
        spec = importlib.util.spec_from_file_location(
            "site01_draw_plan", ROOT / "Art/Source/Maps/Site01/draw_plan.py")
        drawing = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(drawing)
        for route in DESIGN.ROUTES:
            values = drawing.route_metrics(route)
            expected = sum(math.dist(a, b) for a, b in zip(route["points_cm"], route["points_cm"][1:]))
            self.assertAlmostEqual(values["distance_cm"], expected)
            self.assertEqual(values["timing_kind"], "calculated_estimate")
            self.assertEqual(values["speed_cm_s"], 450)
            self.assertAlmostEqual(values["estimated_seconds"], expected / 450)
            self.assertIsNone(values["observed_seconds"])
        svg = drawing.render_svg()
        self.assertIn("Estimated at 450 cm/s", svg)
        self.assertIn("Diagram times are estimates; see runtime-report.json for observed PIE evidence.", svg)


if __name__ == "__main__":
    unittest.main()
