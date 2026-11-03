#!/usr/bin/env python3
"""Local Unreal helpers. Server/client launches use the editor, not packaged builds."""

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ENGINE_VERSION = (5, 8, 4)
XCODE_VERSION = "26.1.1"
MAP = "/Game/Maps/VS_Graybox"
GENERATOR_VERSION = 2


class SetupError(Exception):
    """An actionable local prerequisite failure."""


def require_file(path, advice):
    if not path.is_file():
        raise SetupError(f"Missing {path}. {advice}")
    return path


def engine_path(value=None):
    value = value or os.environ.get("UE_ROOT")
    if not value:
        raise SetupError("Set UE_ROOT or pass --engine PATH to an Unreal 5.8.4 installation.")
    root = Path(value).expanduser().resolve()
    engine = root if (root / "Build/Build.version").is_file() else root / "Engine"
    version_file = require_file(engine / "Build/Build.version", "Check the engine installation path.")
    try:
        data = json.loads(version_file.read_text())
        version = tuple(data[key] for key in ("MajorVersion", "MinorVersion", "PatchVersion"))
    except (ValueError, KeyError, OSError) as error:
        raise SetupError(f"Cannot read engine version from {version_file}: {error}") from error
    if version != ENGINE_VERSION:
        raise SetupError(f"Engine version {version} differs from pinned {ENGINE_VERSION}; select Unreal 5.8.4.")
    return engine


def host_platform():
    supported = {"darwin": "Mac", "win32": "Win64", "linux": "Linux"}
    if sys.platform not in supported:
        raise SetupError(f"Unsupported build host: {sys.platform}")
    return supported[sys.platform]


def project_file(root):
    return require_file(root / "ValStrike.uproject", "Run this helper from the ValStrike checkout.")


def map_file(root):
    return require_file(root / "Content/Maps/VS_Graybox.umap", "Build the Editor target, then run generate-map.")


def generation_marker(root):
    return root / "Saved/Bootstrap/graybox-success.json"


def verify_generation(root):
    map_file(root)
    marker = require_file(generation_marker(root), "Map generation did not finish; inspect the Unreal log before using the partial map.")
    try:
        result = json.loads(marker.read_text())
    except (ValueError, OSError) as error:
        raise SetupError(f"Cannot verify map-generation result: {error}") from error
    if (not isinstance(result, dict) or result.get("map") != MAP
            or result.get("generator_version") != GENERATOR_VERSION
            or type(result.get("actor_count")) is not int or result["actor_count"] <= 0):
        raise SetupError("Invalid map-generation success marker; inspect the Unreal log and generator output.")


def editor_binary(engine, host, command_line=False):
    relative = {
        "Mac": "Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor",
        "Win64": "Win64/UnrealEditor-Cmd.exe" if command_line else "Win64/UnrealEditor.exe",
        "Linux": "Linux/UnrealEditor",
    }[host]
    return require_file(engine / "Binaries" / relative, "Install or build the editor for this host.")


def command(args, engine, root, host):
    project = project_file(root)
    if args.action == "build":
        target_platform = args.platform or host
        if target_platform != host:
            raise SetupError(f"This helper only builds native targets: {host} host cannot build {target_platform}. Use that platform's build machine.")
        if args.target in ("Client", "Server") and (engine / "Build/InstalledBuild.txt").exists():
            raise SetupError("Client/Server targets require the verified source-engine build path; this is an installed engine. Editor/Game targets remain available.")
        wrapper = engine / "Build/BatchFiles" / ("Build.bat" if host == "Win64" else f"{host}/Build.sh")
        require_file(wrapper, "Check that the engine build tools are installed.")
        if host == "Win64" and any(re.search(r'[&|<>^%!"\r\n]', str(path)) for path in (wrapper, project)):
            raise SetupError("Windows batch build paths cannot contain shell metacharacters; use a simple checkout/engine path (spaces are supported).")
        target = "ValStrike" + ("" if args.target == "Game" else args.target)
        prefix = [str(wrapper)] if host == "Win64" else ["/bin/bash", str(wrapper)]
        return prefix + [target, target_platform, "Development", f"-Project={project}", "-WaitMutex", "-NoHotReloadFromIDE"]
    executable = editor_binary(engine, host, args.action == "generate-map")
    argv = [str(executable), str(project)]
    if args.action == "generate-map":
        script = require_file(root / "Scripts/create_graybox.py", "Restore the map-generation script from this checkout.")
        if (root / "Content/Maps/VS_Graybox.umap").exists():
            raise SetupError("VS_Graybox already exists; generate-map will not overwrite an edited map.")
        return argv + ["/Engine/Maps/Entry", f"-ExecutePythonScript={script}", "-unattended", "-log"]
    map_file(root)
    if args.action == "editor":
        return argv + [MAP, "-log"]
    if args.action == "server":
        log = root / "Saved/Logs" / f"VS_Server_{args.port}.log"
        return argv + [MAP, "-server", "-nullrhi", "-nosound", "-unattended",
                       f"-port={args.port}", f"-abslog={log}"]
    log = root / "Saved/Logs" / f"VS_Client_{os.getpid()}.log"
    return argv + [args.address, "-game", "-windowed", "-ResX=1280", "-ResY=720", f"-abslog={log}"]


def probe(argv):
    try:
        result = subprocess.run(argv, capture_output=True, text=True, check=False, timeout=15)
    except subprocess.TimeoutExpired as error:
        raise SetupError(f"{shlex.join(argv)} timed out; verify that tool directly.") from error
    if result.returncode:
        detail = (result.stderr or result.stdout).strip()
        raise SetupError(f"{shlex.join(argv)} failed ({result.returncode}): {detail}")
    return result.stdout.strip()


def check_xcode():
    selected = probe(["xcode-select", "-p"])
    if ".app/Contents/Developer" not in selected:
        raise SetupError(f"Full Xcode {XCODE_VERSION} must be selected; active directory is {selected}.")
    version = probe(["xcodebuild", "-version"])
    if not re.search(rf"^Xcode {re.escape(XCODE_VERSION)}$", version, re.MULTILINE):
        raise SetupError(f"Expected Xcode {XCODE_VERSION}; found {version}. Select the pinned toolchain.")
    metal = probe(["xcrun", "--sdk", "macosx", "--find", "metal"])
    require_file(Path(metal), "Install the Metal toolchain for the selected Xcode.")
    try:
        probe(["xcrun", "--sdk", "macosx", "metal", "--version"])
    except SetupError as error:
        raise SetupError(f"{error} Install the Metal Toolchain component for the selected Xcode; a metal stub is not a working compiler.") from error
    return f"Xcode {XCODE_VERSION}; Metal compiler runs"


def doctor(args, root, host):
    checks = [
        ("Engine", lambda: engine_path(args.engine)),
        ("Project", lambda: project_file(root)),
        ("Editor", lambda: editor_binary(engine_path(args.engine), host)),
        ("Graybox map", lambda: map_file(root)),
    ]
    failures = 0
    for label, check in checks:
        try:
            print(f"OK {label}: {check()}")
        except (SetupError, OSError) as error:
            print(f"FAIL {label}: {error}", file=sys.stderr)
            failures += 1
    probes = [["git", "lfs", "version"]]
    if host == "Mac":
        probes += [["xcode-select", "-p"], ["xcodebuild", "-version"],
                   ["xcrun", "--sdk", "macosx", "--find", "metal"],
                   ["xcrun", "--sdk", "macosx", "metal", "--version"]]
    if args.dry_run:
        for argv in probes:
            print("DRY RUN probe:", shlex.join(argv))
        print("Tool probes were not executed; this is not a readiness result.")
    else:
        checks = [("Git LFS", lambda: probe(probes[0]))]
        if host == "Mac":
            checks.append(("Mac toolchain", check_xcode))
        for label, check in checks:
            try:
                print(f"OK {label}: {check()}")
            except (SetupError, OSError) as error:
                print(f"FAIL {label}: {error}", file=sys.stderr)
                failures += 1
        if host != "Mac":
            print("Compiler/SDK readiness on this host must still be verified by a native build.")
    return 1 if failures else 0


def port(value):
    number = int(value)
    if not 1 <= number <= 65535:
        raise argparse.ArgumentTypeError("port must be between 1 and 65535")
    return number


def address(value):
    # Keep this a host:port, not an Unreal URL with injected travel options.
    if not re.fullmatch(r"[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?:[0-9]+", value):
        raise argparse.ArgumentTypeError("use hostname:port or IPv4:port (for example 127.0.0.1:7777)")
    port(value.rsplit(":", 1)[1])
    return value


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--engine", help="Unreal installation root or Engine directory; defaults to UE_ROOT")
    result.add_argument("--dry-run", action="store_true", help="validate paths and print argv without executing tools")
    actions = result.add_subparsers(dest="action", required=True)
    actions.add_parser("doctor", help="check prerequisites; missing generated map is reported as incomplete setup")
    build = actions.add_parser("build", help="compile only; does not cook or package")
    build.add_argument("--target", choices=("Editor", "Game", "Client", "Server"), default="Editor")
    build.add_argument("--platform", choices=("Mac", "Win64", "Linux"), help="defaults to native host; cross-compilation is not supported")
    actions.add_parser("generate-map", help="create the initial graybox using Unreal's Python editor plugin")
    actions.add_parser("editor", help="open the generated graybox")
    server = actions.add_parser("server", help="editor development server; NOT a packaged dedicated server")
    server.add_argument("--port", type=port, default=7777)
    client = actions.add_parser("client", help="editor development client; NOT a packaged client")
    client.add_argument("--address", type=address, default="127.0.0.1:7777")
    return result


def main(argv=None, root=ROOT):
    args = parser().parse_args(argv)
    try:
        host = host_platform()
        if args.action == "doctor":
            return doctor(args, root, host)
        invocation = command(args, engine_path(args.engine), root, host)
        print(("DRY RUN: " if args.dry_run else "RUN: ") + shlex.join(invocation), flush=True)
        if args.dry_run:
            return 0
        if args.action in ("server", "client"):
            print("Development editor process only; this does not verify packaged client/server builds.", flush=True)
            (root / "Saved/Logs").mkdir(parents=True, exist_ok=True)
        if args.action == "generate-map":
            generation_marker(root).unlink(missing_ok=True)
        result = subprocess.run(invocation, cwd=root, check=False)
        if result.returncode:
            return result.returncode if result.returncode > 0 else 1
        if args.action == "generate-map":
            verify_generation(root)  # Python failure can leave a partial map despite editor exit 0.
        return 0
    except (SetupError, OSError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
