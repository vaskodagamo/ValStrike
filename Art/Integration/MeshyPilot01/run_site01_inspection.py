"""INT-ART-01: run the existing, unmodified Site01 inspection once and exit.

Loads Art/Source/Maps/Site01/inspect_site01.py as-is (map contracts: spawns, site,
recovery, route floors, cover, sightlines; it never saves the map) and quits the
editor after it finishes. Its outputs land in docs/maps/Site01 (not INT-ART-01
owned); the operator copies them to docs/assets/meshy-pilot-integration/ and
restores the tracked originals.
"""

import importlib.util
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("inspect_site01", ROOT / "Art/Source/Maps/Site01/inspect_site01.py")
inspection = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspection)


def wait(_delta):
    if inspection.STATE.get("finished"):
        unreal.unregister_slate_post_tick_callback(HANDLE)
        unreal.SystemLibrary.quit_editor()


inspection.main()
HANDLE = unreal.register_slate_post_tick_callback(wait)
