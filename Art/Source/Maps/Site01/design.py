"""Original Switchyard layout, centimeters. Editable authoring data, no Unreal dependency."""
MAP = "/Game/Maps/Site01/VS_Site01"
SPAWNS = [
    {"id": "ATK_1", "role": "attacker", "actor_label": "VS_ATK_1", "location_cm": [-2250, -250, 100], "yaw_degrees": 90},
    {"id": "ATK_2", "role": "attacker", "actor_label": "VS_ATK_2", "location_cm": [-2250, 250, 100], "yaw_degrees": -90},
    {"id": "DEF_1", "role": "defender", "actor_label": "VS_DEF_1", "location_cm": [2200, -250, 100], "yaw_degrees": -90},
    {"id": "DEF_2", "role": "defender", "actor_label": "VS_DEF_2", "location_cm": [2200, 250, 100], "yaw_degrees": 90},
]
SITE = {"id": "A", "actor_label": "VS_Site_A", "center_cm": [1050, 0, 100], "half_extents_cm": [400, 500, 100]}
RECOVERY = {"actor_label": "VS_DeviceRecovery", "location_cm": [1050, 0, 20]}
BOUNDS = {"center_cm": [0, 0, 300], "half_extents_cm": [2550, 2050, 300]}

# Bounds of actual collision geometry (native kit placements are centered by their imported bounds).
# label, center XYZ, dimensions XYZ, finish. Architectural blocks supplement the unmodified kit.
BLOCKS = [
    ("Floor", (0, 0, -50), (5200, 4200, 100), "Paving"),
    ("BoundaryWest", (-2600, 0, 250), (100, 4300, 500), "Plaster"),
    ("BoundaryEast", (2600, 0, 250), (100, 4300, 500), "Plaster"),
    ("BoundaryNorth", (0, 2100, 250), (5200, 100, 500), "Plaster"),
    ("BoundarySouth", (0, -2100, 250), (5200, 100, 500), "Plaster"),
    ("RelayHouse", (-500, 0, 210), (1600, 1400, 420), "Plaster"),
    ("AttackerScreen", (-1800, 0, 180), (100, 1400, 360), "Plaster"),
    ("DefenderScreen", (1800, 0, 180), (100, 1200, 360), "Plaster"),
    ("NorthService", (600, 1375, 190), (2000, 250, 380), "Plaster"),
    ("SouthTransformer", (-500, -1200, 130), (400, 400, 260), "PaintedMetal"),
]
# label, native kit mesh suffix, center XY, yaw, base Z. No shared asset is modified.
PROPS = [
    ("NorthHalfCover", "Cover_200x120", (-700, 1070), 0, 0),
    ("SouthHalfCover", "Cover_200x120", (150, -1600), 0, 0),
    ("SiteWestCover", "Cover_200x120", (600, 320), 90, 0),
    ("SiteEastCrate", "Crate_100", (1460, -300), 0, 0),
    ("SiteEastCrateTop", "Crate_100", (1460, -300), 0, 100),
    ("NorthCrate", "Crate_100", (1150, 950), 0, 0),
    ("SouthCrate", "Crate_100", (-1450, -1700), 0, 0),
]
ROUTES = [
    {"id": "north_main", "name": "01 / Gallery", "role": "attacker", "kind": "approach", "points_cm": [[-2250,250],[-2250,1000],[-1550,1000],[-1100,850],[350,850],[800,650],[1050,350],[1050,0]]},
    {"id": "south_main", "name": "02 / Transformer", "role": "attacker", "kind": "approach", "points_cm": [[-2250,-250],[-2250,-1100],[-1100,-1100],[-1000,-1600],[-200,-1600],[500,-1050],[1050,-650],[1050,0]]},
    {"id": "north_flank", "name": "03 / Service flank", "role": "attacker", "kind": "flank", "points_cm": [[-2250,250],[-2250,1000],[-1550,1000],[-1000,1750],[1900,1750],[2200,1000],[2200,850],[1550,850],[1050,500],[1050,0]]},
    {"id": "defender_north", "name": "Defender / Gallery", "role": "defender", "kind": "rotation", "points_cm": [[2200,250],[2200,850],[1500,850],[1050,500],[1050,0]]},
    {"id": "defender_south", "name": "Defender / Transformer", "role": "defender", "kind": "rotation", "points_cm": [[2200,-250],[2200,-850],[1550,-850],[1050,-650],[1050,0]]},
]
VIEWS = [
    {"id": "01_attacker_exit", "location": [-2250, 650, 152], "target": [-1500, 1000, 152], "caption": "Attacker north exit; screen protects the spawn pocket."},
    {"id": "02_gallery_contact", "location": [-1050, 850, 152], "target": [800, 850, 152], "caption": "Gallery first-contact lane; half cover leaves standing heads exposed."},
    {"id": "03_transformer_contact", "location": [-1000, -1550, 152], "target": [800, -750, 152], "caption": "Transformer approach; full-height island breaks the long southern lane."},
    {"id": "04_site_a", "location": [1550, -900, 152], "target": [900, 100, 140], "caption": "Site A amber boundary, west half cover and east stacked crate."},
    {"id": "05_service_flank", "location": [50, 1750, 152], "target": [1900, 1750, 152], "caption": "Long service flank rejoins behind the defender north exit."},
    {"id": "06_defender_exit", "location": [2200, -650, 152], "target": [1300, -850, 152], "caption": "Defender south exit; full-height screen prevents direct spawn contact."},
    {"id": "07_overview", "location": [4700, -5300, 6600], "target": [0, 0, 0], "caption": "Actual Unreal map overview; perspective, not a measured plan."},
]

def contract():
    return {"schema_version": 1, "map_asset": MAP, "units": "cm", "spawns": SPAWNS,
            "site": SITE, "device_recovery": RECOVERY, "playable_bounds": BOUNDS,
            "bounds_limitations": "Outer AABB includes solid buildings/screens/cover. Later recovery must also validate floor support and collision/reachability; bounds alone are insufficient.",
            "integration_status": "Authored metadata only. Baseline GameMode ignores role tags. No objective gameplay implemented here."}
