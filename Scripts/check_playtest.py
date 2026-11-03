#!/usr/bin/env python3
"""SET-05 source-checkout preflight; no packaging or LAN acceptance is implied.

Default CLI checks files, Git revision and pinned tools without starting Unreal.
--native starts one bounded fresh Editor commandlet under a caller-held UE slot.
The same file is the commandlet script; its child-only request environment binds
each result to a nonce, project, map, mode and current compiled-module hashes.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import signal
import subprocess
import sys
import time
import traceback
import uuid


ROOT = Path(__file__).resolve().parents[1]
MAPS = {"graybox": "/Game/Maps/VS_Graybox", "site01": "/Game/Maps/Site01/VS_Site01"}
MODES = {"practice": "/Script/ValStrike.VSGameMode", "match": "/Script/ValStrike.VSMatchGameMode"}
MAP_DEPENDENCIES = {
    "graybox": tuple(["Content/Maps/VS_Graybox.umap"] + [
        f"Content/Materials/Bootstrap/M_{name}.uasset" for name in ("Cover", "Floor", "Spawn", "Wall")]),
    "site01": tuple(["Content/Maps/Site01/VS_Site01.umap"] + [
        f"Content/Maps/Site01/Materials/M_Site01_{name}.uasset" for name in ("Paving", "Lane", "Site")]
        + [f"Content/Art/Environment/Materials/M_VS_{name}.uasset"
           for name in ("Plaster", "PaintedMetal", "Trim", "Accent")]
        + [f"Content/Art/Environment/Meshes/SM_VS_{name}.uasset" for name in
           ("Wall_400x300", "Pillar_40x300", "Crate_100", "Cover_200x120", "Vent_120x80")]),
}
NATIVE_SUFFIXES = {".umap", ".uasset"}
PACKAGE_MAGIC = (b"\xc1\x83\x2a\x9e", b"\x9e\x2a\x83\xc1")
LFS_VERSION = "version https://git-lfs.github.com/spec/v1"
PROBE_ENV = "VALSTRIKE_PLAYTEST_PROBE_REQUEST"
PROBE_TIMEOUT_SECONDS = 90


class PreflightError(Exception):
    """Actionable failure; callers must not launch a server after this error."""


def require(condition, message):
    if not condition:
        raise PreflightError(message)


def selected(map_key, mode=None):
    require(map_key in MAPS, f"Unknown map {map_key!r}; choose graybox or site01.")
    if mode is not None:
        require(mode in MODES, f"Unknown mode {mode!r}; choose practice or match.")


def sha256(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def inside(root, relative):
    root = Path(root).resolve()
    path = root / relative
    try:
        path.resolve().relative_to(root)
    except ValueError as error:
        raise PreflightError(f"Path escapes checkout: {relative}") from error
    require(not path.is_symlink(), f"Native input must be a regular checkout file, not a symlink: {relative}")
    return path


def native_file(root, relative):
    path = inside(root, relative)
    require(path.is_file(), f"Missing native asset: {relative}. Run git lfs pull in this checkout.")
    size = path.stat().st_size
    with path.open("rb") as stream:
        prefix = stream.read(1024)
    text_prefix = prefix.removeprefix(b"\xef\xbb\xbf").lstrip()
    require(not text_prefix.startswith(LFS_VERSION.encode("ascii")),
            f"Unresolved Git LFS pointer: {relative}. Run git lfs install --local and git lfs pull.")
    require(size >= 64 and prefix[:4] in PACKAGE_MAGIC,
            f"Invalid or truncated Unreal package header: {relative} ({size} bytes). Retrieve the committed LFS asset.")
    return {"path": relative, "size_bytes": size}


def basic_map_assets(root, map_key):
    """Pure filesystem check for selected known dependencies; safe in dry runs.

    Header checks identify pointer/truncated inputs. They do not establish Unreal
    package validity; --native adds real package/dependency loads.
    """
    selected(map_key)
    try:
        assets = [native_file(Path(root), path) for path in MAP_DEPENDENCIES[map_key]]
        return {"passed": True, "map_key": map_key, "map_asset": MAPS[map_key], "assets": assets}
    except OSError as error:
        raise PreflightError(f"Cannot read selected map assets: {error}") from error


def git(root, *arguments):
    try:
        result = subprocess.run(["git", *arguments], cwd=root, capture_output=True,
                                check=False, timeout=20)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise PreflightError(f"Git check failed: {error}") from error
    if result.returncode:
        detail = result.stderr.decode("utf-8", "replace").strip()
        raise PreflightError(f"git {' '.join(arguments[:2])} failed: {detail}")
    return result.stdout


def parse_pointer(data, relative):
    try:
        lines = data.decode("ascii").splitlines()
    except UnicodeError as error:
        raise PreflightError(f"Committed native asset is not an LFS pointer: {relative}") from error
    require(len(lines) == 3 and lines[0] == LFS_VERSION,
            f"Malformed or non-LFS committed native asset: {relative}")
    oid = re.fullmatch(r"oid sha256:([0-9a-f]{64})", lines[1])
    size = re.fullmatch(r"size ([0-9]+)", lines[2])
    require(oid is not None and size is not None and int(size.group(1)) >= 64,
            f"Malformed LFS oid/size for committed native asset: {relative}")
    return oid.group(1), int(size.group(1))


def validate_assets(root, map_key):
    """Validate every HEAD-tracked Content .uasset/.umap against its LFS body.

    Git inventory detects missing directories/files that a disk glob would miss.
    This intentionally rejects native edits not yet committed to this revision.
    No fetch, checkout, LFS mutation, repair or network operation is performed.
    """
    selected(map_key)
    root = Path(root).resolve()
    basic = basic_map_assets(root, map_key)
    head = git(root, "rev-parse", "--verify", "HEAD").decode("ascii").strip()
    records = git(root, "ls-tree", "-r", "-z", "--full-tree", head, "--", "Content").split(b"\0")
    assets = []
    try:
        for record in records:
            if not record:
                continue
            metadata, raw_path = record.split(b"\t", 1)
            mode, kind, blob = metadata.decode("ascii").split()
            relative = raw_path.decode("utf-8", "surrogateescape")
            if Path(relative).suffix.lower() not in NATIVE_SUFFIXES:
                continue
            require(mode == "100644" and kind == "blob", f"Unexpected native Git entry: {relative} ({mode}/{kind})")
            blob_size = int(git(root, "cat-file", "-s", blob).strip())
            require(blob_size <= 4096, f"Committed native asset is not a small LFS pointer: {relative}")
            oid, size = parse_pointer(git(root, "cat-file", "blob", blob), relative)
            info = native_file(root, relative)
            require(info["size_bytes"] == size,
                    f"LFS size mismatch: {relative}: expected {size}, found {info['size_bytes']}. Commit intentional edits or retrieve this revision.")
            observed = sha256(inside(root, relative))
            require(observed == oid, f"LFS SHA256 mismatch: {relative}. Commit intentional edits or retrieve this revision.")
            assets.append({**info, "oid_sha256": oid})
    except (OSError, ValueError) as error:
        raise PreflightError(f"Cannot validate committed native assets: {error}") from error
    require(assets, "HEAD contains no tracked Content native assets; use the complete source checkout.")
    committed = {a["path"] for a in assets}
    missing = sorted(set(MAP_DEPENDENCIES[map_key]) - committed)
    require(not missing, f"Selected map dependencies are not committed in HEAD: {', '.join(missing)}")
    require(git(root, "rev-parse", "--verify", "HEAD").decode("ascii").strip() == head,
            "Checkout revision changed during asset validation; rerun after Git work finishes.")
    return {"passed": True, "map_key": map_key, "map_asset": MAPS[map_key],
            "committed_native_count": len(assets), "assets": assets,
            "checked_head": head,
            "scope": "all HEAD-tracked Content .uasset/.umap files; selected known map dependencies",
            "selected_dependency_count": len(basic["assets"])}


def checkout_revision(root):
    head = git(root, "rev-parse", "--verify", "HEAD").decode("ascii").strip()
    branch = git(root, "rev-parse", "--abbrev-ref", "HEAD").decode("utf-8", "replace").strip()
    items = git(root, "status", "--porcelain=v1", "-z", "--untracked-files=all").split(b"\0")
    changes, index = [], 0
    while index < len(items):
        entry = items[index]
        index += 1
        if not entry:
            continue
        require(len(entry) >= 4 and entry[2:3] == b" ", "Malformed Git status output")
        status = entry[:2].decode("ascii")
        item = {"status": status, "path": entry[3:].decode("utf-8", "surrogateescape")}
        if "R" in status or "C" in status:
            require(index < len(items) and items[index], "Incomplete Git rename status")
            item["original_path"] = items[index].decode("utf-8", "surrogateescape")
            index += 1
        changes.append(item)
    return {"head": head, "branch": branch, "dirty": bool(changes), "changes": changes}


def module_identity(root):
    """Fingerprint actual module files; this alone is not a build receipt."""
    root = Path(root).resolve()
    manifest = inside(root, "Binaries/Mac/UnrealEditor.modules")
    require(manifest.is_file(), "Compiled Editor module manifest is missing. Build ValStrikeEditor in this checkout first.")
    try:
        data = json.loads(manifest.read_text(encoding="utf-8"))
        name = data["Modules"]["ValStrike"]
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise PreflightError(f"Invalid compiled Editor module manifest: {manifest}") from error
    require(isinstance(name, str) and Path(name).name == name and name.endswith(".dylib"),
            "Unsafe or unexpected ValStrike module filename in Editor manifest")
    module = inside(root, "Binaries/Mac/" + name)
    require(module.is_file() and module.stat().st_size > 0,
            "Compiled ValStrike Editor module is missing or empty. Build this checkout first.")
    return {"module_sha256": sha256(module), "module_file": str(module.relative_to(root)),
            "module_manifest_sha256": sha256(manifest)}


def build_inputs(root, engine):
    """Snapshot exact source/config/project inputs before the helper's build."""
    root, engine = Path(root).resolve(), Path(engine).resolve()
    try:
        if not (engine / "Build/Build.version").is_file():
            engine = engine / "Engine"
        version_path = engine / "Build/Build.version"
        version = json.loads(version_path.read_text())
        require(tuple(version[k] for k in ("MajorVersion", "MinorVersion", "PatchVersion")) == (5, 8, 4),
                "Editor build receipt requires pinned Unreal 5.8.4")
        paths = []
        for folder in ("Source", "Config"):
            base = root / folder
            require(base.is_dir(), f"Missing {folder} directory for build input snapshot")
            found = [p for p in base.rglob("*") if p.is_file()]
            require(found, f"No files in {folder} for build input snapshot")
            paths.extend(found)
        paths.append(inside(root, "ValStrike.uproject"))
        files = {str(p.relative_to(root)): sha256(inside(root, str(p.relative_to(root))))
                 for p in sorted(paths)}
        fingerprint = hashlib.sha256(json.dumps(files, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
        return {"files": files, "source_fingerprint": fingerprint, "engine_root": str(engine),
                "engine_build_version_sha256": sha256(version_path), "engine_build_version": version}
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise PreflightError(f"Cannot snapshot Editor build inputs: {error}") from error


def record_editor_build(root, engine, expected_inputs):
    """Call only after Scripts/ue.py reports a successful Mac Editor build.

    The caller supplies its pre-build snapshot. Changed inputs or missing output
    artifacts reject the receipt; no compilation is performed by this function.
    """
    root = Path(root).resolve()
    try:
        actual = build_inputs(root, engine)
        require(actual == expected_inputs,
                "Source/config/project/engine inputs changed during Editor build. Rebuild after edits finish; no current receipt recorded.")
        module = module_identity(root)
        receipt = {"schema_version": 1, "project_root": str(root), "inputs": actual, "module": module,
                   "recorded_by": "Scripts/ue.py successful Mac Editor build",
                   "recorded_at_unix_seconds": time.time()}
        folder = root / "Saved/Playtest"
        folder.mkdir(parents=True, exist_ok=True)
        pending = folder / f".editor-build-{uuid.uuid4().hex}.json"
        pending.write_text(json.dumps(receipt, indent=2) + "\n")
        pending.replace(folder / "editor-build.json")
        return receipt
    except OSError as error:
        raise PreflightError(f"Cannot record successful Editor build: {error}") from error


def native_identity(root, map_key):
    selected(map_key)
    root = Path(root).resolve()
    advice = "Rebuild this checkout with Scripts/ue.py build --target Editor --platform Mac under with_ue_slot.py."
    try:
        receipt_path = root / "Saved/Playtest/editor-build.json"
        require(receipt_path.is_file(), f"Current Editor build receipt is missing. {advice}")
        receipt = json.loads(receipt_path.read_text())
        require(isinstance(receipt, dict) and receipt.get("schema_version") == 1
                and receipt.get("project_root") == str(root), f"Wrong or malformed Editor build receipt. {advice}")
        inputs = receipt["inputs"]
        require(build_inputs(root, Path(inputs["engine_root"])) == inputs,
                f"Editor build receipt is stale for source/config/project/engine inputs. {advice}")
        module = module_identity(root)
        require(module == receipt["module"], f"Editor module or manifest differs from its build receipt. {advice}")
        return {**module, "map_sha256": sha256(inside(root, MAP_DEPENDENCIES[map_key][0])),
                "probe_script_sha256": sha256(Path(__file__).resolve()),
                "project_sha256": inputs["files"]["ValStrike.uproject"],
                "source_fingerprint": inputs["source_fingerprint"], "engine_root": inputs["engine_root"],
                "engine_build_version_sha256": inputs["engine_build_version_sha256"]}
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise PreflightError(f"Cannot fingerprint compiled native inputs: {error}") from error


def stop_owned_process(process):
    """The commandlet owns a fresh session; stop only that process group."""
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait(timeout=1)


def verify_probe_result(result, request):
    require(isinstance(result, dict), "Native preflight produced a malformed result")
    for key in ("nonce", "project_root", "map_key", "map_asset", "mode", "game_mode_class", "identity"):
        require(result.get(key) == request[key], f"Native preflight result has stale or mismatched {key}")
    require(result.get("passed") is True,
            f"Native preflight failed: {result.get('error', 'no successful reflection/dependency result')}")
    require(isinstance(result.get("dependencies"), list) and request["map_asset"] in result["dependencies"],
            "Native preflight omitted selected map dependency evidence")
    require(str(result.get("engine_version", "")).startswith("5.8.4-"), "Native preflight used a different Unreal release")
    return result


def probe_native(engine, root, map_key, mode):
    """Load actual compiled reflected class + native map in a fresh commandlet.

    Caller must hold Scripts/with_ue_slot.py for this whole operation. This does
    not acquire a nested slot, run a build, or start a gameplay server.
    """
    selected(map_key, mode)
    root, engine = Path(root).resolve(), Path(engine).resolve()
    basic_map_assets(root, map_key)
    process = None
    previous_handlers = {}
    requested_signal = None

    def request_stop(signum, _frame):
        nonlocal requested_signal
        requested_signal = signum

    try:
        if not (engine / "Build/Build.version").is_file():
            engine = engine / "Engine"
        version = json.loads((engine / "Build/Build.version").read_text())
        require(tuple(version[k] for k in ("MajorVersion", "MinorVersion", "PatchVersion")) == (5, 8, 4),
                "Native preflight requires Unreal 5.8.4")
        editor = engine / "Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
        require(editor.is_file(), f"Editor executable missing: {editor}")
        identity = native_identity(root, map_key)
        require(identity["engine_root"] == str(engine), "Selected engine differs from the verified Editor build receipt; rebuild with the selected engine.")
        nonce = uuid.uuid4().hex
        folder = root / "Saved/Playtest/native" / nonce
        folder.mkdir(parents=True, exist_ok=False)
        request_path, result_path = folder / "request.json", folder / "result.json"
        log_path = folder / "Unreal.log"
        request = {"nonce": nonce, "project_root": str(root), "map_key": map_key, "map_asset": MAPS[map_key],
                   "mode": mode, "game_mode_class": MODES[mode], "identity": identity,
                   "result_path": str(result_path)}
        request_path.write_text(json.dumps(request, indent=2) + "\n")
        argv = [str(editor), str(root / "ValStrike.uproject"), "-run=pythonscript",
                f"-script={Path(__file__).resolve()}", "-unattended", "-nullrhi", "-nosound",
                "-DisablePlugins=AndroidFileServer", f"-abslog={log_path}"]
        request["argv"] = argv
        request_path.write_text(json.dumps(request, indent=2) + "\n")
        environment = os.environ.copy()
        environment[PROBE_ENV] = str(request_path)
        # The outer slot wrapper gives this caller ten seconds to clean up.
        # Record cancellation here even if a session runner installed a handler
        # that only sets its own flag; never orphan our separate commandlet group.
        for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            previous_handlers[sig] = signal.signal(sig, request_stop)
        with (folder / "stdout.log").open("wb") as output:
            process = subprocess.Popen(argv, cwd=root, env=environment, stdout=output,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            deadline = time.monotonic() + PROBE_TIMEOUT_SECONDS
            while True:
                require(requested_signal is None, f"Native preflight cancelled by signal {requested_signal}; no server was started.")
                remaining = deadline - time.monotonic()
                require(remaining > 0, f"Native preflight timed out after {PROBE_TIMEOUT_SECONDS}s. See {log_path}")
                try:
                    status = process.wait(timeout=min(1, remaining))
                    break
                except subprocess.TimeoutExpired:
                    pass
        require(requested_signal is None, f"Native preflight cancelled by signal {requested_signal}; no server was started.")
        require(result_path.is_file(), f"Native preflight produced no nonce-bound result (exit {status}). See {log_path}")
        result = json.loads(result_path.read_text())
        verify_probe_result(result, request)
        require(status == 0, f"Native preflight exited {status} despite its result. See {log_path}")
        require(native_identity(root, map_key) == identity, "Map, project, module or probe changed during native preflight; rerun after edits/builds finish.")
        return {**result, "report_path": str(result_path), "log_path": str(log_path)}
    except PreflightError:
        raise
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        raise PreflightError(f"Native preflight could not complete: {error}") from error
    finally:
        try:
            if process is not None:
                stop_owned_process(process)
        finally:
            for sig, handler in previous_handlers.items():
                signal.signal(sig, handler)


def native_probe_entry():
    """Executed only inside Unreal's fresh Python commandlet, never in unit tests."""
    import unreal
    request, result_path = None, None
    try:
        root = Path(unreal.Paths.project_dir()).resolve()
        request_path = Path(os.environ[PROBE_ENV]).resolve()
        request_path.relative_to(root / "Saved/Playtest/native")
        request = json.loads(request_path.read_text())
        selected(request["map_key"], request["mode"])
        require(re.fullmatch(r"[0-9a-f]{32}", request["nonce"]) is not None, "Invalid probe nonce")
        require(request_path == root / "Saved/Playtest/native" / request["nonce"] / "request.json", "Wrong probe request path")
        require(request["project_root"] == str(root), "Native probe opened the wrong project checkout")
        require(request["map_asset"] == MAPS[request["map_key"]] and request["game_mode_class"] == MODES[request["mode"]],
                "Native probe request contains a non-allowlisted map/class")
        result_path = request_path.with_name("result.json")
        require(str(result_path) == request["result_path"], "Wrong probe result path")
        require(native_identity(root, request["map_key"]) == request["identity"], "Native inputs changed before probe started")
        require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Wrong Unreal engine release")
        # Loading the exact reflected class tests the compiled artifact. Source
        # strings, manifest presence and Unreal's URL fallback are insufficient.
        cls = unreal.load_class(None, request["game_mode_class"])
        require(cls is not None and cls.get_path_name() == request["game_mode_class"],
                f"Compiled game mode {request['game_mode_class']} is unavailable. Integrate its implementation and build this checkout; no fallback server was started.")
        require(isinstance(unreal.get_default_object(cls), unreal.GameModeBase), "Requested reflected class is not a GameModeBase")
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        registry.search_all_assets(True)
        options = unreal.AssetRegistryDependencyOptions()
        options.set_editor_property("include_hard_package_references", True)
        options.set_editor_property("include_soft_package_references", False)
        options.set_editor_property("include_searchable_names", False)
        options.set_editor_property("include_soft_management_references", False)
        options.set_editor_property("include_hard_management_references", False)
        pending, seen = [request["map_asset"]], set()
        while pending:
            package = pending.pop()
            if package in seen:
                continue
            seen.add(package)
            assets = registry.get_assets_by_package_name(package, True)
            require(assets, f"Missing hard native dependency package: {package}")
            require(all(asset.get_asset() is not None for asset in assets), f"Cannot load hard native dependency: {package}")
            for dependency in registry.get_dependencies(package, options) or []:
                name = str(dependency)
                # Script packages are supplied by native modules. Engine/plugin
                # packages are included too so missing installed content fails.
                if not name.startswith("/Script/") and name not in seen:
                    pending.append(name)
        world = unreal.EditorLoadingAndSavingUtils.load_map(request["map_asset"])
        require(world is not None, f"Unreal could not load selected map {request['map_asset']}")
        require(world.get_path_name().split(".", 1)[0] == request["map_asset"], "Unreal loaded a different map")
        require(native_identity(root, request["map_key"]) == request["identity"], "Native inputs changed during inspection")
        result = {**request, "passed": True, "engine_version": unreal.SystemLibrary.get_engine_version(),
                  "dependencies": sorted(seen), "loaded_world": world.get_path_name(),
                  "scope": "compiled-class reflection and hard on-disk package loads; no gameplay, soft dynamic references, multiplayer or LAN acceptance"}
        result_path.write_text(json.dumps(result, indent=2) + "\n")
        unreal.log("VS_PLAYTEST_NATIVE_SUCCESS " + request["nonce"])
    except Exception:
        error = traceback.format_exc()
        if request is not None and result_path is not None:
            result_path.write_text(json.dumps({**request, "passed": False, "error": error}, indent=2) + "\n")
        unreal.log_error("VS_PLAYTEST_NATIVE_FAILED " + error)
        raise


def main(argv=None, root=ROOT):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", help="Pinned Unreal installation or Engine directory; defaults to UE_ROOT")
    parser.add_argument("--map", choices=tuple(MAPS), default="graybox", dest="map_key")
    parser.add_argument("--mode", choices=tuple(MODES), default="practice")
    parser.add_argument("--native", action="store_true", help="Load native dependencies/reflected class under caller-held UE slot")
    parser.add_argument("--report", type=Path, help="Optional JSON path inside this checkout's Saved/Playtest")
    args = parser.parse_args(argv)
    root = Path(root).resolve()
    report_path = None
    result = {"passed": False, "map_key": args.map_key, "mode": args.mode,
              "native_requested": args.native, "lan_verified": False, "packaged_verified": False}
    try:
        if args.report is not None:
            candidate = args.report if args.report.is_absolute() else root / args.report
            candidate = candidate.resolve()
            require(candidate.is_relative_to(root / "Saved/Playtest"), "Report path must stay inside this checkout's Saved/Playtest")
            report_path = candidate
        # Late import prevents check_playtest <-> ue import cycles.
        import ue
        require(ue.host_platform() == "Mac", "This playtest preflight targets native macOS")
        engine = ue.engine_path(args.engine)
        result["toolchain"] = ue.check_xcode()
        result["architecture"] = platform.machine()
        require(result["architecture"] == "arm64", "This source playtest route requires the agreed Apple-silicon arm64 Mac")
        result["macos_sdk_version"] = ue.probe(["xcrun", "--sdk", "macosx", "--show-sdk-version"])
        result["engine"] = str(engine)
        result["editor"] = str(ue.editor_binary(engine, "Mac", True))
        result["git_lfs"] = ue.probe(["git", "lfs", "version"])
        result["assets"] = validate_assets(root, args.map_key)
        result["revision"] = checkout_revision(root)
        if args.native:
            result["native"] = probe_native(engine, root, args.map_key, args.mode)
        else:
            result["native"] = {"checked": False, "reason": "Use --native under with_ue_slot.py after building to verify reflected game mode and dependency loads."}
        result["passed"] = True
        print(json.dumps(result, indent=2))
        return 0
    except Exception as error:
        # SetupError is owned by late-imported ue; normalize CLI errors without
        # allowing a traceback or a stale report to masquerade as a pass.
        result["error"] = str(error)
        print(f"Playtest preflight failed: {error}", file=sys.stderr)
        return 1
    finally:
        if report_path is not None:
            try:
                report_path.parent.mkdir(parents=True, exist_ok=True)
                report_path.write_text(json.dumps(result, indent=2) + "\n")
            except OSError as error:
                print(f"Cannot write playtest report: {error}", file=sys.stderr)
                return 1


if __name__ == "__main__":
    if os.environ.get(PROBE_ENV):
        native_probe_entry()
    else:
        sys.exit(main())
