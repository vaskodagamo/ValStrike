#!/usr/bin/env python3
"""Draw the original Site01 measured authoring plan with only the Python stdlib.

This is a route diagram, not an Unreal screenshot or a movement test result.
Run: python3 Art/Source/Maps/Site01/draw_plan.py
"""

from html import escape
import importlib.util
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SPEC = importlib.util.spec_from_file_location("site01_plan_design", Path(__file__).with_name("design.py"))
DESIGN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DESIGN)
OUTPUT = ROOT / "docs/maps/site01/route-plan.svg"
COLORS = {"north_main": "#55d4df", "south_main": "#ff9b71", "north_flank": "#b69bff",
          "defender_north": "#f2d381", "defender_south": "#f2d381"}
# Designed sightline segments, measured geometrically. Native Visibility results
# belong in editor-report.json; drawing a segment does not claim a passing query.
SIGHTLINES = (
    ("S1", "Gallery", (-1050, 850), (800, 850), (535, 421)),
    ("S2", "South, after transformer", (-150, -1450), (1000, -650), (770, 810)),
    ("S3", "Service", (-1000, 1750), (1900, 1750), (900, 203)),
)


def route_metrics(route):
    distance = sum(math.dist(a, b) for a, b in zip(route["points_cm"], route["points_cm"][1:]))
    return {"distance_cm": distance, "speed_cm_s": 450,
            "estimated_seconds": distance / 450, "timing_kind": "calculated_estimate",
            "observed_seconds": None}


def xy(point):
    return 600 + point[0] * .2, 570 - point[1] * .2


def render_svg():
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="1080" viewBox="0 0 1600 1080">',
             '<title>Switchyard — Site01 authored route plan</title>',
             '<desc>Original 2v2 tactical map with two approaches, a service flank, four spawns, one site and recovery point. Distances follow authored route polylines. White dashed segments are designed sightlines with measured lengths, not claimed native query results. Diagram times are estimates; see runtime-report.json for observed PIE evidence.</desc>',
             '<defs><pattern id="siteHatch" width="12" height="12" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><line x1="0" y1="0" x2="0" y2="12" stroke="#f2d381" stroke-width="2" opacity=".2"/></pattern></defs>',
             '<rect width="1600" height="1080" fill="#101d26"/>',
             '<style>text{font-family:Arial,Helvetica,sans-serif} .small{font-size:14px;fill:#a9bec6} .body{font-size:17px;fill:#dce7ea} .label{font-size:15px;fill:#e6eef0;font-weight:600} .building{font-size:14px;fill:#38454a;font-weight:700;letter-spacing:1px}</style>']

    def text(x, y, value, cls="body", extra=""):
        parts.append(f'<text x="{x:.1f}" y="{y:.1f}" class="{cls}" {extra}>{escape(value)}</text>')

    def line(x1, y1, x2, y2, stroke, width=1, extra=""):
        parts.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{stroke}" stroke-width="{width}" {extra}/>')

    def rectangle(center, size, fill, stroke="#5d777e", extra=""):
        x, y = xy((center[0] - size[0] / 2, center[1] + size[1] / 2))
        parts.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{size[0]*.2:.1f}" height="{size[1]*.2:.1f}" fill="{fill}" stroke="{stroke}" {extra}/>')

    text(44, 45, "VALSTRIKE  /  PLAY-01", "small", 'letter-spacing="3"')
    text(44, 91, "SWITCHYARD", "label", 'style="font-size:38px;letter-spacing:2px"')
    text(355, 88, "Site01 · original 2v2 prototype", "body")
    text(1557, 45, "AUTHORING PLAN  /  V1", "small", 'text-anchor="end" letter-spacing="2"')
    text(1557, 82, "52 × 42 m floor · flat ground", "body", 'text-anchor="end"')
    parts.append('<rect x="40" y="126" width="1120" height="888" rx="12" fill="#20343d" stroke="#3c525c"/>')
    parts.append('<rect x="1182" y="126" width="378" height="888" rx="12" fill="#182b35" stroke="#3c525c"/>')

    rectangle((0, 0), (5200, 4200), "#293f46", "#6a8187")
    for value in range(-2500, 2501, 500):
        x, _y = xy((value, 0))
        line(x, 150, x, 990, "#496169", 1, 'opacity=".35"')
    for value in range(-2000, 2001, 500):
        _x, y = xy((0, value))
        line(80, y, 1120, y, "#496169", 1, 'opacity=".35"')

    for label, center, size, _finish in DESIGN.BLOCKS:
        if label == "Floor":
            continue
        rectangle(center, size, "#aeb4ac" if "Transformer" not in label else "#5f8c8c", "#d4d5c7", 'stroke-width="1.3"')
    for label, mesh, center, yaw, _base in DESIGN.PROPS:
        size = (200, 100) if mesh == "Cover_200x120" else (100, 100)
        if yaw % 180 == 90:
            size = tuple(reversed(size))
        rectangle(center, size, "#5e9699", "#b4d4d3", 'stroke-width="1.5"')
        if "Top" in label:
            x, y = xy(center)
            text(x, y + 4, "2", "label", 'text-anchor="middle" style="font-size:12px"')

    site = DESIGN.SITE
    size = [v * 2 for v in site["half_extents_cm"][:2]]
    rectangle(site["center_cm"], size, "#887846", "#f2d381", 'fill-opacity=".16" stroke-width="2"')
    rectangle(site["center_cm"], size, "url(#siteHatch)", "none")

    # Distinct line styles remain distinguishable without color.
    for route in DESIGN.ROUTES:
        points = " ".join(f"{x:.1f},{y:.1f}" for x, y in map(xy, route["points_cm"]))
        dash = 'stroke-dasharray="12 8"' if route["kind"] == "flank" else (
            'stroke-dasharray="3 7"' if route["role"] == "defender" else "")
        parts.append(f'<polyline points="{points}" fill="none" stroke="#10212a" stroke-width="8" opacity=".7" stroke-linejoin="round"/>')
        parts.append(f'<polyline points="{points}" fill="none" stroke="{COLORS[route["id"]]}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round" {dash}/>')

    for identifier, name, start, end, label_position in SIGHTLINES:
        x1, y1 = xy(start)
        x2, y2 = xy(end)
        distance_m = math.dist(start, end) / 100
        parts.append(f'<g><title>{escape(name)} designed sightline: {start} to {end} cm; {distance_m:.3f} m. Native result is separate.</title>')
        line(x1, y1, x2, y2, "#f0f4e9", 1.8, 'stroke-dasharray="8 6"')
        length = math.hypot(x2 - x1, y2 - y1)
        cap_x, cap_y = (y2 - y1) / length * 4, -(x2 - x1) / length * 4
        for x, y in ((x1, y1), (x2, y2)):
            line(x - cap_x, y - cap_y, x + cap_x, y + cap_y, "#f0f4e9", 1.5)
        lx, ly = label_position
        parts.append(f'<rect x="{lx-48}" y="{ly-14}" width="96" height="19" rx="4" fill="#20343d" opacity=".95"/>')
        text(lx, ly, f"{identifier} / {distance_m:.1f} m", "label", 'text-anchor="middle" style="font-size:12px;fill:#f0f4e9"')
        parts.append('</g>')

    text(*xy((-500, 100)), "RELAY HOUSE", "building", 'text-anchor="middle"')
    text(*xy((-500, -50)), "full-height route divider", "building", 'text-anchor="middle" style="font-size:12px;font-weight:400;letter-spacing:0"')
    text(*xy((600, 1355)), "SERVICE WALL", "building", 'text-anchor="middle"')
    text(*xy((-500, -1200)), "TX", "building", 'text-anchor="middle"')
    text(*xy((-1800, 0)), "SPAWN SCREEN", "building", 'text-anchor="middle" transform="rotate(-90 240 570)" style="font-size:11px"')
    text(*xy((1800, 0)), "SPAWN SCREEN", "building", 'text-anchor="middle" transform="rotate(-90 960 570)" style="font-size:11px"')

    for spawn in DESIGN.SPAWNS:
        x, y = xy(spawn["location_cm"])
        color = "#6ce2e0" if spawn["role"] == "attacker" else "#f2d381"
        parts.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="12" fill="#152b36" stroke="{color}" stroke-width="2.5"/>')
        text(x, y + 4, spawn["id"][-1], "label", f'text-anchor="middle" style="fill:{color};font-size:12px"')
        angle = math.radians(spawn["yaw_degrees"])
        dx, dy = 24 * math.cos(angle), -24 * math.sin(angle)
        line(x, y + (12 if dy > 0 else -12), x + dx, y + dy, color, 2)
        parts.append(f'<path d="M {x+dx-4:.1f} {y+dy+(5 if dy<0 else -5):.1f} L {x+dx:.1f} {y+dy:.1f} L {x+dx+4:.1f} {y+dy+(5 if dy<0 else -5):.1f}" fill="none" stroke="{color}" stroke-width="2"/>')
    text(*xy((-2250, 80)), "ATK", "label", 'text-anchor="middle" style="fill:#6ce2e0"')
    text(*xy((2200, 80)), "DEF", "label", 'text-anchor="middle" style="fill:#f2d381"')

    # Contact labels refer to intended engagement lanes; no player encounter is claimed.
    for number, point, offset, label in (
        ("C1", (-1050, 850), (-265, -60), "GALLERY CONTACT"),
        ("C2", (-150, -1450), (-215, 75), "AFTER TRANSFORMER / C2"),
    ):
        x, y = xy(point)
        dx, dy = offset
        line(x, y, x + dx + 72, y + dy - 6, "#edf2ec", 1.1)
        parts.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="12" fill="#172b35" stroke="#edf2ec" stroke-width="1.5"/>')
        text(x, y + 4, number, "label", 'text-anchor="middle" style="font-size:11px"')
        text(x + dx, y + dy, label, "label", 'style="font-size:12px"')

    sx, sy = xy(site["center_cm"])
    text(sx + 22, sy - 18, "A", "label", 'style="font-size:32px;fill:#ffe9a7"')
    text(sx + 22, sy + 2, "8 × 10 m", "small", 'style="fill:#ffe9a7"')
    text(sx + 22, sy + 20, "plant bounds", "small", 'style="font-size:12px;fill:#ffe9a7"')
    parts.append(f'<path d="M {sx} {sy-7} L {sx+7} {sy} L {sx} {sy+7} L {sx-7} {sy} Z" fill="#fff2bb" stroke="#182b35" stroke-width="1.5"/>')
    text(sx - 13, sy + 22, "R", "label", 'style="font-size:12px;fill:#fff2bb"')

    # Route numbers placed in open floor, away from the geometry labels.
    for point, number, color in (((100, 940), "01", COLORS["north_main"]),
                                 ((380, -1260), "02", COLORS["south_main"]),
                                 ((600, 1850), "03", COLORS["north_flank"])):
        x, y = xy(point)
        text(x, y, number, "label", f'style="font-size:25px;fill:{color}"')
    text(*xy((1750, 1060)), "REAR JOIN", "small", 'text-anchor="middle" style="font-size:12px"')

    # Axis triad and calibrated scale bar.
    ax, ay = 130, 940
    line(ax, ay, ax, ay - 53, "#d6e5e9", 2)
    line(ax, ay, ax + 53, ay, "#d6e5e9", 2)
    parts.append(f'<path d="M {ax-4} {ay-45} L {ax} {ay-53} L {ax+4} {ay-45} M {ax+45} {ay-4} L {ax+53} {ay} L {ax+45} {ay+4}" fill="none" stroke="#d6e5e9" stroke-width="2"/>')
    text(ax - 8, ay - 64, "+Y / N", "small")
    text(ax + 62, ay + 5, "+X / E", "small")
    for i in range(2):
        parts.append(f'<rect x="{750+i*100}" y="926" width="100" height="7" fill="{"#d6e5e9" if i==0 else "#536c76"}"/>')
    for x, label in ((750, "0"), (850, "5"), (950, "10 m")):
        line(x, 922, x, 938, "#d6e5e9", 1)
        text(x, 955, label, "small", 'text-anchor="middle"')

    text(1206, 164, "ROUTES & TIMING", "label", 'letter-spacing="1.5"')
    text(1206, 188, "Estimated at 450 cm/s", "small")
    y = 235
    for route in DESIGN.ROUTES:
        metrics = route_metrics(route)
        color = COLORS[route["id"]]
        line(1206, y - 6, 1234, y - 6, color, 4,
             'stroke-dasharray="5 4"' if route["kind"] != "approach" else "")
        text(1246, y, route["name"], "label", 'style="font-size:16px"')
        text(1246, y + 26, f"{metrics['distance_cm']/100:.1f} m  /  {metrics['estimated_seconds']:.1f} s estimated", "body", f'style="fill:{color};font-size:16px"')
        y += 77
    line(1206, 611, 1536, 611, "#3c525c")
    text(1206, 643, "HOW TO READ", "label", 'letter-spacing="1.5"')
    legend = ["Solid blocks: full-height barriers", "Teal boxes: kit cover / crates", "C1 / C2: intended first-contact lanes", "A: authored plant area · R: recovery", "Circles 1 / 2: role spawn centers"]
    for index, value in enumerate(legend):
        text(1206, 674 + index * 24, value, "small")
    line(1206, 793, 1234, 793, "#f0f4e9", 1.8, 'stroke-dasharray="8 6"')
    text(1246, 798, "S1–S3: designed sightline segments", "small", 'style="font-size:13px"')
    text(1206, 821, "Labels give measured segment lengths.", "small", 'style="font-size:13px"')
    line(1206, 837, 1536, 837, "#3c525c")
    for index, value in enumerate(("Route distances follow drawn polylines.",
                                   "Times exclude acceleration, turns,", "peeking, stops and combat.",
                                   "Diagram times are estimates; see",
                                   "runtime-report.json for observed PIE evidence.",
                                   "No human balance playtest claimed.",
                                   "Role tags need gameplay integration.")):
        text(1206, 864 + index * 20, value, "small", 'style="font-size:13px"')
    text(44, 1044, "Source: Art/Source/Maps/Site01/ · design.py + draw_plan.py · authored cm coordinates · original diagram, not a runtime capture", "small")
    text(1557, 1044, "VS_SITE01", "small", 'text-anchor="end" letter-spacing="2"')
    parts.append("</svg>")
    return "\n".join(parts) + "\n"


if __name__ == "__main__":
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(render_svg(), encoding="utf-8")
    print(OUTPUT)
