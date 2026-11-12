#!/usr/bin/env python3
"""Create and verify release manifests without platform-specific path parsing.

The manifest grammar is deliberately narrower than ``sha256sum``: every line is
``lowercase-sha256<two spaces>safe/relative/path<LF>``.  Paths use POSIX
separators and the manifest must describe the complete regular-file set below
the supplied root.  Symlinks and other special files are rejected.
"""

from __future__ import annotations

import argparse
import binascii
from datetime import datetime, timezone
import hashlib
import hmac
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import subprocess
import sys
from typing import Iterable
import zipfile
import zlib


HASH_RE = re.compile(r"[0-9a-f]{64}")
ENTRY_RE = re.compile(r"([0-9a-f]{64})  ([^\r\n]+)")
COMMIT_RE = re.compile(r"[0-9a-f]{40}")
TAG_RE = re.compile(r"v0\.7\.51")
CAPTURE_SCHEMA = "epoch2dwalk-native-capture-v2"
PROMOTION_SCHEMA = "epoch2dwalk-promotion-receipt-v2"
PROMOTION_SECRET_ENV = "RUNNER_PROTECTED_RELEASE_APPROVAL"
PROMOTION_WORKFLOW = ".github/workflows/release.yml"
PROMOTION_ROOT_FIELDS = frozenset(
    {
        "schema",
        "product",
        "source_commit",
        "release_tag",
        "event_name",
        "run_sha",
        "run_ref",
        "workflow_sha",
        "workflow_ref",
        "build_run_id",
        "build_run_attempt",
        "promotion_run_id",
        "promotion_run_attempt",
        "repository",
        "workflow",
        "publication_target",
        "github_status",
        "site_handoff_readiness",
        "object_count",
        "build_artifact",
        "promotion_inputs_artifact",
        "native_evidence_artifact",
        "build_receipt",
        "linux_abi",
        "native_capture_bundle",
        "assets",
    }
)
PROMOTION_FILE_FIELDS = frozenset({"filename", "bytes", "sha256"})
PROMOTION_CAPTURE_FIELDS = frozenset({"filename", "url", "bytes", "sha256"})
PROMOTION_ARTIFACT_FIELDS = frozenset({"id", "name", "sha256"})
CAPTURE_ROOT_FIELDS = frozenset(
    {
        "schema",
        "source_commit",
        "release_tag",
        "authority",
        "accepted",
        "accepted_by",
        "accepted_utc",
        "host",
        "runtime_archive",
        "captures",
    }
)
CAPTURE_HOST_FIELDS = frozenset({"platform", "vulkan_device"})
CAPTURE_RUNTIME_FIELDS = frozenset({"filename", "bytes", "sha256"})
CAPTURE_FIELDS = frozenset(
    {
        "scenario",
        "group",
        "subject",
        "phase",
        "filename",
        "viewport_class",
        "logical_width",
        "logical_height",
        "drawable_width",
        "drawable_height",
        "dpi",
        "bytes",
        "sha256",
    }
)

# Native evidence records the client coordinate space separately from the Vulkan
# drawable.  These are the exact reviewed client sizes; adding another size is a
# contract change rather than a way to bypass a class boundary.
VIEWPORT_LOGICAL_SIZES = {
    "compact": frozenset({(1368, 768), (1368, 799)}),
    "default": frozenset({(2048, 1152), (2560, 1392)}),
    "ultrawide": frozenset({(3440, 1440), (3840, 1600)}),
}
ALLOWED_DPI = frozenset({96, 120, 144, 168, 192})


def _scenario(
    group: str, subject: str, phase: str, viewport_class: str = "default"
) -> tuple[str, str, str, str]:
    return group, subject, phase, viewport_class


# The order is intentional: it also proves approach/entry/exit and
# plant/aim/fire/recovery are separately captured instead of relabeling one
# bitmap.  Each scenario's semantic fields are checked against this table.
CAPTURE_SCENARIO_MATRIX = {
    "live-compact": _scenario("live", "workspace", "overview", "compact"),
    "live-mission-compact-top": _scenario("live-page", "mission", "top", "compact"),
    "live-mission-compact-bottom": _scenario(
        "live-page", "mission", "bottom", "compact"
    ),
    "live-view-compact-top": _scenario("live-page", "view", "top", "compact"),
    "live-view-compact-bottom": _scenario("live-page", "view", "bottom", "compact"),
    "live-details-compact-top": _scenario(
        "live-page", "details", "top", "compact"
    ),
    "live-details-compact-bottom": _scenario(
        "live-page", "details", "bottom", "compact"
    ),
    "live-default": _scenario("live", "workspace", "overview"),
    "live-ultrawide": _scenario("live", "workspace", "overview", "ultrawide"),
    "live-ultrawide-pip": _scenario("live", "pip", "visible", "ultrawide"),
    "course-overview": _scenario("course", "world", "overview"),
    "lake-approach": _scenario("lake", "human", "approach"),
    "lake-immersion": _scenario("lake", "human", "immersion"),
    "lake-exit": _scenario("lake", "human", "exit"),
    "human-heel-strike": _scenario("gait", "human", "heel-strike"),
    "human-flat-support": _scenario("gait", "human", "flat-support"),
    "human-toe-off": _scenario("gait", "human", "toe-off"),
    "chicken-contact-loaded": _scenario("species-contact", "chicken", "loaded"),
    "chicken-contact-transfer": _scenario("species-contact", "chicken", "transfer"),
    "chicken-lake-entry": _scenario("species-lake", "chicken", "entry"),
    "chicken-lake-exit": _scenario("species-lake", "chicken", "exit"),
    "dog-contact-loaded": _scenario("species-contact", "dog", "loaded"),
    "dog-contact-transfer": _scenario("species-contact", "dog", "transfer"),
    "dog-lake-entry": _scenario("species-lake", "dog", "entry"),
    "dog-lake-exit": _scenario("species-lake", "dog", "exit"),
    "hexapod-contact-loaded": _scenario("species-contact", "hexapod", "loaded"),
    "hexapod-contact-transfer": _scenario("species-contact", "hexapod", "transfer"),
    "hexapod-lake-entry": _scenario("species-lake", "hexapod", "entry"),
    "hexapod-lake-exit": _scenario("species-lake", "hexapod", "exit"),
    "equipment-plant": _scenario("equipment", "human-carbine", "plant"),
    "equipment-aim": _scenario("equipment", "human-carbine", "aim"),
    "equipment-fire": _scenario("equipment", "human-carbine", "fire"),
    "equipment-recovery": _scenario("equipment", "human-carbine", "recovery"),
    "riglab-presets-compact-top": _scenario(
        "rig-lab-page", "presets", "top", "compact"
    ),
    "riglab-presets-compact-bottom": _scenario(
        "rig-lab-page", "presets", "bottom", "compact"
    ),
    "riglab-structure-compact-top": _scenario(
        "rig-lab-page", "structure", "top", "compact"
    ),
    "riglab-structure-compact-bottom": _scenario(
        "rig-lab-page", "structure", "bottom", "compact"
    ),
    "riglab-motors-compact-top": _scenario(
        "rig-lab-page", "motors", "top", "compact"
    ),
    "riglab-motors-compact-bottom": _scenario(
        "rig-lab-page", "motors", "bottom", "compact"
    ),
    "riglab-art-compact-top": _scenario("rig-lab-page", "art", "top", "compact"),
    "riglab-art-compact-bottom": _scenario(
        "rig-lab-page", "art", "bottom", "compact"
    ),
    "riglab-test-compact-top": _scenario("rig-lab-page", "test", "top", "compact"),
    "riglab-test-compact-bottom": _scenario(
        "rig-lab-page", "test", "bottom", "compact"
    ),
    "riglab-presets-normal": _scenario("rig-lab", "presets", "normal"),
    "riglab-presets-zoom": _scenario("rig-lab", "presets", "zoom"),
    "riglab-structure-normal": _scenario("rig-lab", "structure", "normal"),
    "riglab-structure-zoom": _scenario("rig-lab", "structure", "zoom"),
    "riglab-motors-normal": _scenario("rig-lab", "motors", "normal"),
    "riglab-motors-zoom": _scenario("rig-lab", "motors", "zoom"),
    "riglab-test-normal": _scenario("rig-lab", "test", "normal"),
    "riglab-test-zoom": _scenario("rig-lab", "test", "zoom"),
    "riglab-art-human-normal": _scenario("rig-lab-art", "human", "normal"),
    "riglab-art-human-zoom": _scenario("rig-lab-art", "human", "zoom"),
    "riglab-art-chicken-normal": _scenario("rig-lab-art", "chicken", "normal"),
    "riglab-art-chicken-zoom": _scenario("rig-lab-art", "chicken", "zoom"),
    "riglab-art-dog-normal": _scenario("rig-lab-art", "dog", "normal"),
    "riglab-art-dog-zoom": _scenario("rig-lab-art", "dog", "zoom"),
    "riglab-art-hexapod-normal": _scenario("rig-lab-art", "hexapod", "normal"),
    "riglab-art-hexapod-zoom": _scenario("rig-lab-art", "hexapod", "zoom"),
    "terminal-state": _scenario("terminal", "active-rig", "frozen"),
}
CAPTURE_SCENARIOS = tuple(CAPTURE_SCENARIO_MATRIX)


class ManifestError(RuntimeError):
    pass


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def safe_relative_path(raw: str) -> str:
    if not raw or "\\" in raw or "\x00" in raw or ":" in raw:
        raise ManifestError(f"unsafe manifest path: {raw!r}")
    candidate = PurePosixPath(raw)
    if candidate.is_absolute() or raw.startswith("/"):
        raise ManifestError(f"absolute manifest path: {raw!r}")
    if any(part in ("", ".", "..") for part in candidate.parts):
        raise ManifestError(f"non-normalized manifest path: {raw!r}")
    normalized = candidate.as_posix()
    if normalized != raw:
        raise ManifestError(f"non-canonical manifest path: {raw!r}")
    return normalized


def sorted_paths(paths: Iterable[str]) -> list[str]:
    return sorted(paths, key=lambda value: value.encode("utf-8"))


def inventory(root: Path) -> list[str]:
    root = root.resolve(strict=True)
    if not root.is_dir():
        raise ManifestError(f"manifest root is not a directory: {root}")
    paths: list[str] = []
    for current, directories, files in os.walk(root, followlinks=False):
        current_path = Path(current)
        for name in directories:
            path = current_path / name
            if path.is_symlink():
                raise ManifestError(f"symlinked directory is forbidden: {path}")
        for name in files:
            path = current_path / name
            if path.is_symlink() or not path.is_file():
                raise ManifestError(f"non-regular file is forbidden: {path}")
            relative = path.relative_to(root).as_posix()
            paths.append(safe_relative_path(relative))
    return sorted_paths(paths)


def directory_inventory(root: Path) -> list[str]:
    """Return every directory below *root*, including otherwise invisible empties."""
    root = root.resolve(strict=True)
    if not root.is_dir():
        raise ManifestError(f"inventory root is not a directory: {root}")
    paths: list[str] = []
    for current, directories, _ in os.walk(root, followlinks=False):
        current_path = Path(current)
        for name in directories:
            path = current_path / name
            if path.is_symlink() or not path.is_dir():
                raise ManifestError(f"non-directory handoff entry is forbidden: {path}")
            paths.append(safe_relative_path(path.relative_to(root).as_posix()))
    return sorted_paths(paths)


def parse_manifest(path: Path) -> list[tuple[str, str]]:
    payload = path.read_bytes()
    if not payload or not payload.endswith(b"\n"):
        raise ManifestError("manifest must be non-empty and LF-terminated")
    if payload.startswith(b"\xef\xbb\xbf"):
        raise ManifestError("manifest must not contain a UTF-8 byte-order mark")
    if b"\r" in payload:
        raise ManifestError("manifest must use LF-only line endings")
    try:
        text = payload.decode("utf-8", errors="strict")
    except UnicodeDecodeError as error:
        raise ManifestError("manifest is not strict UTF-8") from error
    raw_lines = text[:-1].split("\n")
    if any(not line for line in raw_lines):
        raise ManifestError("manifest contains an empty line")
    entries: list[tuple[str, str]] = []
    for line in raw_lines:
        match = ENTRY_RE.fullmatch(line)
        if match is None:
            raise ManifestError(f"invalid manifest entry: {line!r}")
        entries.append((match.group(1), safe_relative_path(match.group(2))))
    entry_paths = [relative for _, relative in entries]
    if entry_paths != sorted_paths(entry_paths):
        raise ManifestError("manifest paths are not bytewise sorted")
    if len(entry_paths) != len(set(entry_paths)):
        raise ManifestError("manifest contains duplicate paths")
    return entries


def write_manifest(root: Path, manifest: Path) -> None:
    resolved_root = root.resolve(strict=True)
    paths = inventory(resolved_root)
    if not paths:
        raise ManifestError("refusing to write an empty manifest")
    lines = [f"{sha256_file(resolved_root / relative)}  {relative}\n" for relative in paths]
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_bytes("".join(lines).encode("utf-8"))


def verify_manifest(root: Path, manifest: Path) -> None:
    resolved_root = root.resolve(strict=True)
    entries = parse_manifest(manifest)
    expected_paths = [relative for _, relative in entries]
    actual_paths = inventory(resolved_root)
    if expected_paths != actual_paths:
        missing = sorted(set(expected_paths) - set(actual_paths))
        extra = sorted(set(actual_paths) - set(expected_paths))
        raise ManifestError(
            f"manifest file set mismatch; missing={missing!r} extra={extra!r}"
        )
    for expected_hash, relative in entries:
        actual_hash = sha256_file(resolved_root / relative)
        if actual_hash != expected_hash:
            raise ManifestError(f"SHA-256 mismatch: {relative}")


def verify_checksum(archive: Path, sidecar: Path) -> None:
    payload = sidecar.read_bytes()
    expected = f"{sha256_file(archive)}  {archive.name}\n".encode("utf-8")
    if payload != expected:
        raise ManifestError(
            "checksum must be exact lowercase SHA-256, two spaces, archive basename, and LF"
        )


def bytewise_equal(left: Path, right: Path) -> bool:
    with left.open("rb") as left_stream, right.open("rb") as right_stream:
        while True:
            left_block = left_stream.read(1024 * 1024)
            right_block = right_stream.read(1024 * 1024)
            if left_block != right_block:
                return False
            if not left_block:
                return True


def verify_publication(asset_root: Path, public_root: Path, tag: str) -> None:
    resolved_assets = asset_root.resolve(strict=True)
    resolved_public = public_root.resolve(strict=True)
    if not resolved_assets.is_dir() or not resolved_public.is_dir():
        raise ManifestError("publication audit roots must be directories")
    names = release_asset_names(tag)
    expected_public_inventory = sorted_paths(names)
    actual_public_inventory = inventory(resolved_public)
    if actual_public_inventory != expected_public_inventory:
        missing = sorted(set(expected_public_inventory) - set(actual_public_inventory))
        extra = sorted(set(actual_public_inventory) - set(expected_public_inventory))
        raise ManifestError(
            "public release file set differs from the exact 15-object allowlist; "
            f"missing={missing!r} extra={extra!r}"
        )
    if directory_inventory(resolved_public):
        raise ManifestError("public release download contains unexpected directories")
    for name in names:
        source = resolved_assets / name
        public = resolved_public / name
        source_identity = regular_file_identity(source, name)
        public_identity = regular_file_identity(public, name)
        if source_identity != public_identity or not bytewise_equal(source, public):
            raise ManifestError(
                f"public release object is not byte-identical to the audited handoff: {name}"
            )


def git_tree(repository: Path, treeish: str) -> dict[str, str]:
    command = [
        "git",
        "-C",
        str(repository),
        "ls-tree",
        "-rz",
        "--full-tree",
        treeish,
    ]
    result = subprocess.run(command, check=True, stdout=subprocess.PIPE)
    entries: dict[str, str] = {}
    for record in result.stdout.split(b"\0"):
        if not record:
            continue
        try:
            metadata, raw_path = record.split(b"\t", 1)
            mode, object_type, object_id = metadata.decode("ascii").split(" ", 2)
            path = os.fsdecode(raw_path)
        except (ValueError, UnicodeDecodeError) as error:
            raise ManifestError("could not parse NUL-delimited git tree") from error
        if object_type != "blob" or mode not in ("100644", "100755"):
            raise ManifestError(f"unsupported source-archive entry: {path!r}")
        safe = safe_relative_path(path.replace(os.sep, "/"))
        if safe in entries:
            raise ManifestError(f"duplicate git tree path: {safe!r}")
        entries[safe] = object_id
    return entries


def verify_git_tree(
    repository: Path, treeish: str, root: Path, excluded_prefixes: list[str]
) -> None:
    expected = git_tree(repository, treeish)
    normalized_prefixes = []
    for prefix in excluded_prefixes:
        normalized = safe_relative_path(prefix.rstrip("/")) + "/"
        normalized_prefixes.append(normalized)
    expected = {
        path: object_id
        for path, object_id in expected.items()
        if not any(path.startswith(prefix) for prefix in normalized_prefixes)
    }
    actual_paths = inventory(root)
    expected_paths = sorted_paths(expected)
    if actual_paths != expected_paths:
        missing = sorted(set(expected_paths) - set(actual_paths))
        extra = sorted(set(actual_paths) - set(expected_paths))
        raise ManifestError(
            f"exact source file set mismatch; missing={missing!r} extra={extra!r}"
        )
    for relative in expected_paths:
        result = subprocess.run(
            ["git", "hash-object", "--no-filters", "--", str(root / relative)],
            check=True,
            stdout=subprocess.PIPE,
            text=True,
        )
        if result.stdout.strip() != expected[relative]:
            raise ManifestError(f"exact-commit source byte mismatch: {relative}")


def require_json_integer(value: object, label: str, minimum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise ManifestError(f"{label} must be an integer >= {minimum}")
    return value


def require_exact_fields(value: dict[str, object], expected: frozenset[str], label: str) -> None:
    actual = frozenset(value)
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        raise ManifestError(f"{label} fields differ; missing={missing!r} extra={extra!r}")


def require_json_string(value: object, label: str) -> str:
    if (not isinstance(value, str) or not value or value != value.strip()
            or any(ord(character) < 0x20 for character in value)):
        raise ManifestError(f"{label} must be a non-empty trimmed string")
    return value


def reject_json_constant(value: str) -> None:
    raise ManifestError(f"capture manifest contains a non-finite JSON number: {value}")


def unique_json_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ManifestError(f"capture manifest contains duplicate JSON key: {key!r}")
        result[key] = value
    return result


def release_asset_names(tag: str) -> tuple[str, ...]:
    if TAG_RE.fullmatch(tag) is None:
        raise ManifestError("promotion receipt requires release tag v0.7.51")
    prefix = f"Epoch2DWalkEngine-{tag}"
    bases = (
        f"{prefix}-runner-windows-x64.zip",
        f"{prefix}-runner-linux-x86_64.tar.gz",
        f"{prefix}-dev-windows-x64.zip",
        f"{prefix}-dev-linux-x86_64.tar.gz",
        f"{prefix}-source.zip",
    )
    result: list[str] = []
    for base in bases:
        manifest_stem = base[:-4] if base.endswith(".zip") else base[:-7]
        result.extend((base, f"{base}.sha256", f"{manifest_stem}.manifest.sha256"))
    return tuple(result)


def canonical_json_bytes(value: object) -> bytes:
    return (
        json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        + "\n"
    ).encode("utf-8")


def regular_file_identity(path: Path, expected_name: str) -> dict[str, object]:
    if path.name != expected_name:
        raise ManifestError(f"promotion input filename is not canonical: {path.name!r}")
    if path.is_symlink() or not path.is_file():
        raise ManifestError(f"promotion input is missing or non-regular: {expected_name}")
    size = path.stat().st_size
    if size < 1:
        raise ManifestError(f"promotion input is empty: {expected_name}")
    return {"filename": expected_name, "bytes": size, "sha256": sha256_file(path)}


def workflow_artifact_identity(
    artifact_id: str,
    name: str,
    digest: str,
    expected_name: str,
    label: str,
) -> dict[str, object]:
    if re.fullmatch(r"[1-9][0-9]*", artifact_id) is None:
        raise ManifestError(f"{label} artifact ID is invalid")
    if name != expected_name:
        raise ManifestError(f"{label} artifact name is not exact")
    if HASH_RE.fullmatch(digest) is None:
        raise ManifestError(f"{label} artifact digest is invalid")
    return {"id": int(artifact_id), "name": name, "sha256": digest}


def read_promotion_secret() -> bytes:
    raw = os.environ.get(PROMOTION_SECRET_ENV)
    if raw is None:
        raise ManifestError(f"{PROMOTION_SECRET_ENV} is not set")
    secret = raw.encode("utf-8")
    if len(secret) < 32:
        raise ManifestError(
            f"{PROMOTION_SECRET_ENV} must contain at least 32 UTF-8 bytes"
        )
    return secret


def strict_text_lines(path: Path, label: str) -> list[str]:
    if path.is_symlink() or not path.is_file():
        raise ManifestError(f"{label} is missing or non-regular")
    payload = path.read_bytes()
    if (not payload or not payload.endswith(b"\n") or b"\r" in payload
            or payload.startswith(b"\xef\xbb\xbf")):
        raise ManifestError(
            f"{label} must be strict UTF-8 with LF termination and no BOM/CR"
        )
    try:
        text = payload.decode("utf-8", errors="strict")
    except UnicodeDecodeError as error:
        raise ManifestError(f"{label} is not strict UTF-8") from error
    lines = text[:-1].split("\n")
    if any(not line for line in lines):
        raise ManifestError(f"{label} contains an empty line")
    return lines


def phase_a_identities(
    asset_root: Path,
    build_receipt: Path,
    linux_abi: Path,
    commit: str,
    tag: str,
    build_run_id: str,
    build_run_attempt: str,
) -> tuple[dict[str, object], dict[str, object], list[dict[str, object]]]:
    resolved_root = asset_root.resolve(strict=True)
    if not resolved_root.is_dir():
        raise ManifestError("Phase-A asset root is not a directory")
    if COMMIT_RE.fullmatch(commit) is None:
        raise ManifestError("promotion receipt requires a full lowercase source commit")
    if re.fullmatch(r"[1-9][0-9]*", build_run_id) is None:
        raise ManifestError("promotion receipt build run ID is invalid")
    if re.fullmatch(r"[1-9][0-9]*", build_run_attempt) is None:
        raise ManifestError("promotion receipt build run attempt is invalid")
    names = release_asset_names(tag)
    expected_abi_name = f"Epoch2DWalkEngine-{tag}-linux-abi.txt"
    if build_receipt.resolve(strict=True).parent != resolved_root:
        raise ManifestError("BUILD-RECEIPT.txt is outside the Phase-A asset root")
    if linux_abi.resolve(strict=True).parent != resolved_root:
        raise ManifestError("Linux ABI receipt is outside the Phase-A asset root")
    build_identity = regular_file_identity(build_receipt, "BUILD-RECEIPT.txt")
    abi_identity = regular_file_identity(linux_abi, expected_abi_name)
    expected_inventory = sorted_paths([*names, "BUILD-RECEIPT.txt", expected_abi_name])
    if inventory(resolved_root) != expected_inventory:
        raise ManifestError(
            "Phase-A artifact file set is not the exact 15-object set plus its two receipts"
        )
    assets = [regular_file_identity(resolved_root / name, name) for name in names]
    lines = strict_text_lines(build_receipt, "BUILD-RECEIPT.txt")
    expected_header = [
        "product=Epoch2DWalkEngine",
        "phase=build",
        f"build_run_id={build_run_id}",
        f"build_run_attempt={build_run_attempt}",
        f"source_commit={commit}",
        f"release_tag={tag}",
        "object_count=15",
    ]
    expected_asset_lines = [
        f"asset={item['filename']} bytes={item['bytes']} sha256={item['sha256']}"
        for item in assets
    ]
    if lines != [*expected_header, *expected_asset_lines]:
        raise ManifestError(
            "BUILD-RECEIPT.txt does not exactly bind the ordered 15-object Phase-A inventory"
        )
    return build_identity, abi_identity, assets


def validate_handoff_inventory(
    *,
    handoff_root: Path,
    capture_root: Path,
    identities: list[dict[str, object]],
    control_names: tuple[str, ...],
) -> None:
    resolved_handoff = handoff_root.resolve(strict=True)
    resolved_capture = capture_root.resolve(strict=True)
    if not resolved_handoff.is_dir():
        raise ManifestError("Site handoff root is not a directory")
    if not resolved_capture.is_dir():
        raise ManifestError("verified native capture root is not a directory")
    capture_files = inventory(resolved_capture)
    capture_directories = directory_inventory(resolved_capture)
    if "native-capture.json" not in capture_files:
        raise ManifestError("verified capture tree lacks canonical native-capture.json")

    base_names = [str(identity["filename"]) for identity in identities]
    expected_files = sorted_paths(
        [
            *base_names,
            *control_names,
            *(f"native-captures/{relative}" for relative in capture_files),
        ]
    )
    actual_files = inventory(resolved_handoff)
    if actual_files != expected_files:
        missing = sorted(set(expected_files) - set(actual_files))
        extra = sorted(set(actual_files) - set(expected_files))
        raise ManifestError(
            "Site handoff recursive file set differs from the exact allowlist; "
            f"missing={missing!r} extra={extra!r}"
        )
    expected_directories = sorted_paths(
        ["native-captures", *(f"native-captures/{path}" for path in capture_directories)]
    )
    actual_directories = directory_inventory(resolved_handoff)
    if actual_directories != expected_directories:
        missing = sorted(set(expected_directories) - set(actual_directories))
        extra = sorted(set(actual_directories) - set(expected_directories))
        raise ManifestError(
            "Site handoff recursive directory set differs from the exact allowlist; "
            f"missing={missing!r} extra={extra!r}"
        )

    for identity in identities:
        copied = regular_file_identity(
            resolved_handoff / str(identity["filename"]), str(identity["filename"])
        )
        if copied != identity:
            raise ManifestError(
                f"Site handoff copy does not match its audited input: {identity['filename']}"
            )
    for relative in capture_files:
        source = resolved_capture / relative
        copied = resolved_handoff / "native-captures" / relative
        if source.stat().st_size != copied.stat().st_size or not hmac.compare_digest(
            sha256_file(source), sha256_file(copied)
        ):
            raise ManifestError(
                f"Site handoff extracted capture differs from verified evidence: {relative}"
            )


def promotion_receipt_data(
    *,
    asset_root: Path,
    handoff_root: Path,
    capture_root: Path,
    build_receipt: Path,
    linux_abi: Path,
    capture_bundle: Path,
    capture_url: str,
    capture_sha256: str,
    commit: str,
    tag: str,
    build_run_id: str,
    build_run_attempt: str,
    promotion_run_id: str,
    promotion_run_attempt: str,
    event_name: str,
    run_sha: str,
    run_ref: str,
    workflow_sha: str,
    workflow_ref: str,
    repository: str,
    workflow: str,
    publication_target: str,
    github_status: str,
    build_artifact_id: str,
    build_artifact_name: str,
    build_artifact_sha256: str,
    promotion_inputs_artifact_id: str,
    promotion_inputs_artifact_name: str,
    promotion_inputs_artifact_sha256: str,
    native_evidence_artifact_id: str,
    native_evidence_artifact_name: str,
    native_evidence_artifact_sha256: str,
) -> dict[str, object]:
    if re.fullmatch(r"[1-9][0-9]*", promotion_run_id) is None:
        raise ManifestError("promotion run ID is invalid")
    if re.fullmatch(r"[1-9][0-9]*", promotion_run_attempt) is None:
        raise ManifestError("promotion run attempt is invalid")
    if event_name != "workflow_dispatch":
        raise ManifestError("promotion event identity is invalid")
    if run_sha != commit:
        raise ManifestError("executing run SHA is not the promoted candidate")
    if run_ref != "refs/heads/main":
        raise ManifestError("executing run ref is not refs/heads/main")
    if not repository or repository != repository.strip() or "/" not in repository:
        raise ManifestError("promotion repository identity is invalid")
    if workflow != PROMOTION_WORKFLOW:
        raise ManifestError("promotion workflow identity is invalid")
    if workflow_sha != commit:
        raise ManifestError("executing workflow SHA is not the promoted candidate")
    expected_workflow_ref = f"{repository}/{workflow}@{run_ref}"
    if workflow_ref != expected_workflow_ref:
        raise ManifestError("executing workflow ref is not the exact main-branch workflow")
    if publication_target not in ("site", "github"):
        raise ManifestError("promotion publication target is invalid")
    if github_status not in (
        "unavailable-http-403", "not-probed", "probe-required"
    ):
        raise ManifestError("promotion GitHub status is invalid")
    if publication_target == "github" and github_status != "probe-required":
        raise ManifestError("GitHub publication requires probe-required status")
    if publication_target == "site" and github_status == "probe-required":
        raise ManifestError("Site publication cannot claim a pending GitHub probe")
    expected_capture_url = (
        "https://epoch.adamrushford.chatgpt.site/downloads/candidates/"
        f"{tag}/{commit}/Epoch2DWalkEngine-{tag}-native-captures.zip"
    )
    if capture_url != expected_capture_url:
        raise ManifestError("native capture bundle URL is not the exact candidate endpoint")
    if HASH_RE.fullmatch(capture_sha256) is None:
        raise ManifestError("native capture bundle SHA-256 is invalid")
    build_identity, abi_identity, assets = phase_a_identities(
        asset_root,
        build_receipt,
        linux_abi,
        commit,
        tag,
        build_run_id,
        build_run_attempt,
    )
    capture_identity = regular_file_identity(capture_bundle, "native-captures.zip")
    if capture_identity["sha256"] != capture_sha256:
        raise ManifestError("native capture bundle SHA-256 does not match")
    capture_identity = {**capture_identity, "url": capture_url}
    build_artifact = workflow_artifact_identity(
        build_artifact_id,
        build_artifact_name,
        build_artifact_sha256,
        f"epoch2dwalk-v0751-build-candidate-{build_run_attempt}",
        "build candidate",
    )
    promotion_inputs_artifact = workflow_artifact_identity(
        promotion_inputs_artifact_id,
        promotion_inputs_artifact_name,
        promotion_inputs_artifact_sha256,
        f"epoch2dwalk-v0751-promotion-inputs-{promotion_run_attempt}",
        "promotion inputs",
    )
    native_evidence_artifact = workflow_artifact_identity(
        native_evidence_artifact_id,
        native_evidence_artifact_name,
        native_evidence_artifact_sha256,
        f"epoch2dwalk-v0751-native-visual-evidence-{promotion_run_attempt}",
        "native visual evidence",
    )
    if build_run_id == promotion_run_id:
        raise ManifestError("build and promotion run IDs must be distinct")
    return {
        "schema": PROMOTION_SCHEMA,
        "product": "Epoch2DWalkEngine",
        "source_commit": commit,
        "release_tag": tag,
        "event_name": event_name,
        "run_sha": run_sha,
        "run_ref": run_ref,
        "workflow_sha": workflow_sha,
        "workflow_ref": workflow_ref,
        "build_run_id": build_run_id,
        "build_run_attempt": build_run_attempt,
        "promotion_run_id": promotion_run_id,
        "promotion_run_attempt": promotion_run_attempt,
        "repository": repository,
        "workflow": workflow,
        "publication_target": publication_target,
        "github_status": github_status,
        "site_handoff_readiness": "strict-context-bound-15-object-handoff-ready",
        "object_count": 15,
        "build_artifact": build_artifact,
        "promotion_inputs_artifact": promotion_inputs_artifact,
        "native_evidence_artifact": native_evidence_artifact,
        "build_receipt": build_identity,
        "linux_abi": abi_identity,
        "native_capture_bundle": capture_identity,
        "assets": assets,
    }


def promotion_handoff_identities(data: dict[str, object]) -> list[dict[str, object]]:
    capture = data["native_capture_bundle"]
    if not isinstance(capture, dict):
        raise ManifestError("promotion capture identity is not an object")
    assets = data["assets"]
    if not isinstance(assets, list) or not all(isinstance(item, dict) for item in assets):
        raise ManifestError("promotion asset identities are invalid")
    build_receipt = data["build_receipt"]
    linux_abi = data["linux_abi"]
    if not isinstance(build_receipt, dict) or not isinstance(linux_abi, dict):
        raise ManifestError("promotion receipt identities are invalid")
    return [
        build_receipt,
        *assets,
        linux_abi,
        {key: capture[key] for key in PROMOTION_FILE_FIELDS},
    ]


def site_handoff_summary_bytes(
    data: dict[str, object],
    receipt: Path,
    signature: Path,
    linux_abi: Path,
) -> bytes:
    def artifact_lines(prefix: str, identity: object) -> list[str]:
        if not isinstance(identity, dict):
            raise ManifestError(f"{prefix} identity is not an object")
        return [
            f"{prefix}_id={identity['id']}",
            f"{prefix}_name={identity['name']}",
            f"{prefix}_sha256={identity['sha256']}",
        ]

    capture = data["native_capture_bundle"]
    if not isinstance(capture, dict):
        raise ManifestError("native capture identity is not an object")
    lines = [
        "product=Epoch2DWalkEngine",
        f"release_tag={data['release_tag']}",
        f"source_commit={data['source_commit']}",
        f"event_name={data['event_name']}",
        f"run_sha={data['run_sha']}",
        f"run_ref={data['run_ref']}",
        f"workflow_sha={data['workflow_sha']}",
        f"workflow_ref={data['workflow_ref']}",
        f"build_run_id={data['build_run_id']}",
        f"build_run_attempt={data['build_run_attempt']}",
        *artifact_lines("build_artifact", data["build_artifact"]),
        f"promotion_run_id={data['promotion_run_id']}",
        f"promotion_run_attempt={data['promotion_run_attempt']}",
        *artifact_lines("promotion_inputs_artifact", data["promotion_inputs_artifact"]),
        *artifact_lines("native_evidence_artifact", data["native_evidence_artifact"]),
        "artifact_policy=reused-exact-build-candidate-no-rebuild",
        "context_binding=stateless-exact-run-workflow-and-artifact-binding",
        "replay_policy=not-enforced-by-receipt-use-external-site-one-use-gate",
        f"object_count={data['object_count']}",
        f"publication_target={data['publication_target']}",
        "site_publication=pending-external-site-one-use-gate",
        "tag_creation=pending-publication",
        "github_publication=not-attempted",
        f"github_status={data['github_status']}",
        f"native_capture_bundle_url={capture['url']}",
        f"native_capture_bundle_sha256={capture['sha256']}",
        f"promotion_receipt={receipt.name}",
        f"promotion_receipt_sha256={sha256_file(receipt)}",
        f"promotion_signature={signature.name}",
        f"promotion_signature_sha256={sha256_file(signature)}",
        f"site_handoff_readiness={data['site_handoff_readiness']}",
        *strict_text_lines(linux_abi, "Linux ABI receipt"),
    ]
    assets = data["assets"]
    if not isinstance(assets, list):
        raise ManifestError("promotion assets are not a list")
    for identity in assets:
        if not isinstance(identity, dict):
            raise ManifestError("promotion asset identity is not an object")
        lines.append(
            f"asset={identity['filename']} bytes={identity['bytes']} "
            f"sha256={identity['sha256']}"
        )
    return ("\n".join(lines) + "\n").encode("utf-8")


def write_promotion_receipt(
    receipt: Path, signature: Path, summary: Path, **values: object
) -> None:
    expected_receipt_name = f"Epoch2DWalkEngine-{values['tag']}-PROMOTION-RECEIPT.json"
    expected_summary_name = f"Epoch2DWalkEngine-{values['tag']}-SITE-HANDOFF.txt"
    if receipt.name != expected_receipt_name:
        raise ManifestError("promotion receipt filename is not canonical")
    if signature.name != f"{expected_receipt_name}.hmac-sha256":
        raise ManifestError("promotion signature filename is not canonical")
    if summary.name != expected_summary_name:
        raise ManifestError("Site handoff summary filename is not canonical")
    control_parents = {
        receipt.resolve(strict=False).parent,
        signature.resolve(strict=False).parent,
        summary.resolve(strict=False).parent,
    }
    if len(control_parents) != 1:
        raise ManifestError("promotion receipt, signature, and summary must share a directory")
    if (receipt.resolve(strict=False).parent
            != values["handoff_root"].resolve(strict=True)):
        raise ManifestError(
            "promotion control files must be written into the audited Site handoff"
        )
    secret = read_promotion_secret()
    data = promotion_receipt_data(**values)
    identities = promotion_handoff_identities(data)
    validate_handoff_inventory(
        handoff_root=values["handoff_root"],
        capture_root=values["capture_root"],
        identities=identities,
        control_names=(),
    )
    payload = canonical_json_bytes(data)
    receipt.write_bytes(payload)
    digest = hmac.new(secret, payload, hashlib.sha256).hexdigest()
    signature.write_bytes(f"{digest}  {receipt.name}\n".encode("utf-8"))
    summary.write_bytes(
        site_handoff_summary_bytes(data, receipt, signature, values["linux_abi"])
    )
    validate_handoff_inventory(
        handoff_root=values["handoff_root"],
        capture_root=values["capture_root"],
        identities=identities,
        control_names=(receipt.name, signature.name, summary.name),
    )


def strict_json_document(path: Path, label: str) -> tuple[dict[str, object], bytes]:
    if path.is_symlink() or not path.is_file():
        raise ManifestError(f"{label} is missing or non-regular")
    payload = path.read_bytes()
    if (not payload.endswith(b"\n") or b"\r" in payload
            or payload.startswith(b"\xef\xbb\xbf")):
        raise ManifestError(f"{label} is not canonical LF-terminated JSON")
    try:
        value = json.loads(
            payload.decode("utf-8", errors="strict"),
            object_pairs_hook=unique_json_object,
            parse_constant=reject_json_constant,
        )
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ManifestError(f"{label} is not strict JSON") from error
    if not isinstance(value, dict):
        raise ManifestError(f"{label} root must be an object")
    if payload != canonical_json_bytes(value):
        raise ManifestError(f"{label} is not canonical JSON")
    return value, payload


def verify_promotion_receipt(
    receipt: Path, signature: Path, summary: Path, **values: object
) -> None:
    expected = promotion_receipt_data(**values)
    expected_name = f"Epoch2DWalkEngine-{values['tag']}-PROMOTION-RECEIPT.json"
    expected_summary_name = f"Epoch2DWalkEngine-{values['tag']}-SITE-HANDOFF.txt"
    if (receipt.name != expected_name
            or signature.name != f"{expected_name}.hmac-sha256"
            or summary.name != expected_summary_name):
        raise ManifestError("promotion control filename is not canonical")
    if (receipt.parent.resolve(strict=True) != signature.parent.resolve(strict=True)
            or receipt.parent.resolve(strict=True) != summary.parent.resolve(strict=True)
            or receipt.parent.resolve(strict=True) != values["handoff_root"].resolve(strict=True)):
        raise ManifestError("promotion control files are outside the audited Site handoff")
    validate_handoff_inventory(
        handoff_root=values["handoff_root"],
        capture_root=values["capture_root"],
        identities=promotion_handoff_identities(expected),
        control_names=(receipt.name, signature.name, summary.name),
    )
    data, payload = strict_json_document(receipt, "promotion receipt")
    require_exact_fields(data, PROMOTION_ROOT_FIELDS, "promotion receipt")
    if not isinstance(data.get("build_receipt"), dict):
        raise ManifestError("promotion receipt build_receipt must be an object")
    if not isinstance(data.get("linux_abi"), dict):
        raise ManifestError("promotion receipt linux_abi must be an object")
    if not isinstance(data.get("native_capture_bundle"), dict):
        raise ManifestError("promotion receipt native_capture_bundle must be an object")
    require_exact_fields(
        data["build_receipt"], PROMOTION_FILE_FIELDS, "promotion build receipt"
    )
    require_exact_fields(
        data["linux_abi"], PROMOTION_FILE_FIELDS, "promotion Linux ABI receipt"
    )
    require_exact_fields(
        data["native_capture_bundle"],
        PROMOTION_CAPTURE_FIELDS,
        "promotion capture bundle",
    )
    assets = data.get("assets")
    if not isinstance(assets, list) or len(assets) != 15:
        raise ManifestError("promotion receipt must contain exactly 15 assets")
    for index, item in enumerate(assets):
        if not isinstance(item, dict):
            raise ManifestError(f"promotion asset[{index}] must be an object")
        require_exact_fields(item, PROMOTION_FILE_FIELDS, f"promotion asset[{index}]")
    for label in (
        "build_artifact", "promotion_inputs_artifact", "native_evidence_artifact"
    ):
        identity = data.get(label)
        if not isinstance(identity, dict):
            raise ManifestError(f"promotion receipt {label} must be an object")
        require_exact_fields(identity, PROMOTION_ARTIFACT_FIELDS, label)
    signature_lines = strict_text_lines(signature, "promotion HMAC signature")
    expected_hmac = hmac.new(
        read_promotion_secret(), payload, hashlib.sha256
    ).hexdigest()
    expected_signature_line = f"{expected_hmac}  {receipt.name}"
    if (len(signature_lines) != 1
            or not hmac.compare_digest(signature_lines[0], expected_signature_line)):
        raise ManifestError("promotion receipt HMAC signature does not authenticate")
    if data != expected:
        raise ManifestError("promotion receipt identity or immutable input inventory does not match")
    expected_summary = site_handoff_summary_bytes(
        expected, receipt, signature, values["linux_abi"]
    )
    if summary.read_bytes() != expected_summary:
        raise ManifestError("Site handoff summary is not the exact canonical context record")


def capture_dimensions(
    capture: dict[str, object], index: int
) -> tuple[int, int]:
    viewport_class = require_json_string(
        capture.get("viewport_class"), f"capture[{index}].viewport_class")
    if viewport_class not in VIEWPORT_LOGICAL_SIZES:
        raise ManifestError(f"capture[{index}].viewport_class is not supported")
    logical_width = require_json_integer(
        capture.get("logical_width"), f"capture[{index}].logical_width", 1)
    logical_height = require_json_integer(
        capture.get("logical_height"), f"capture[{index}].logical_height", 1)
    logical_size = (logical_width, logical_height)
    if logical_size not in VIEWPORT_LOGICAL_SIZES[viewport_class]:
        raise ManifestError(
            f"capture[{index}] logical size {logical_size!r} is not an exact "
            f"{viewport_class} viewport"
        )
    dpi = require_json_integer(capture.get("dpi"), f"capture[{index}].dpi", 1)
    if dpi not in ALLOWED_DPI:
        raise ManifestError(f"capture[{index}].dpi is not an accepted native DPI")
    scaled_width, width_remainder = divmod(logical_width * dpi, 96)
    scaled_height, height_remainder = divmod(logical_height * dpi, 96)
    if width_remainder or height_remainder:
        raise ManifestError(
            f"capture[{index}] logical size and DPI do not produce integral drawable dimensions"
        )
    drawable_width = require_json_integer(
        capture.get("drawable_width"), f"capture[{index}].drawable_width", 1)
    drawable_height = require_json_integer(
        capture.get("drawable_height"), f"capture[{index}].drawable_height", 1)
    if (drawable_width, drawable_height) != (scaled_width, scaled_height):
        raise ManifestError(
            f"capture[{index}] drawable size does not match logical size at declared DPI"
        )
    return drawable_width, drawable_height


def png_dimensions(path: Path) -> tuple[int, int]:
    """Fully validate the narrow RGB/RGBA PNG contract used by native captures."""
    payload = path.read_bytes()
    if not payload.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ManifestError(f"capture is not a PNG: {path}")
    cursor = 8
    dimensions: tuple[int, int] | None = None
    channels = 0
    idat = bytearray()
    saw_idat = False
    ended_idat = False
    saw_iend = False
    while cursor < len(payload):
        if len(payload) - cursor < 12:
            raise ManifestError(f"capture has a truncated PNG chunk: {path}")
        length = struct.unpack(">I", payload[cursor:cursor + 4])[0]
        chunk_type = payload[cursor + 4:cursor + 8]
        chunk_end = cursor + 12 + length
        if chunk_end > len(payload):
            raise ManifestError(f"capture has a truncated PNG chunk payload: {path}")
        chunk_data = payload[cursor + 8:cursor + 8 + length]
        expected_crc = struct.unpack(">I", payload[cursor + 8 + length:chunk_end])[0]
        actual_crc = binascii.crc32(chunk_type + chunk_data) & 0xFFFF_FFFF
        if actual_crc != expected_crc:
            raise ManifestError(f"capture has an invalid PNG chunk CRC: {path}")
        if dimensions is None and chunk_type != b"IHDR":
            raise ManifestError(f"capture PNG does not begin with IHDR: {path}")
        if chunk_type == b"IHDR":
            if dimensions is not None or length != 13:
                raise ManifestError(f"capture has an invalid or duplicate IHDR: {path}")
            width, height, bit_depth, color_type, compression, filtering, interlace = (
                struct.unpack(">IIBBBBB", chunk_data)
            )
            if width == 0 or height == 0 or width > 16_384 or height > 16_384:
                raise ManifestError(f"capture PNG dimensions are invalid: {path}")
            if (bit_depth, color_type) not in ((8, 2), (16, 2), (8, 6), (16, 6)):
                raise ManifestError(
                    f"capture must be an 8/16-bit RGB or RGBA PNG: {path}"
                )
            if compression != 0 or filtering != 0 or interlace != 0:
                raise ManifestError(
                    f"capture PNG uses an unsupported encoding mode: {path}"
                )
            dimensions = (width, height)
            channels = (3 if color_type == 2 else 4) * (bit_depth // 8)
        elif chunk_type == b"IDAT":
            if dimensions is None or ended_idat:
                raise ManifestError(f"capture has out-of-order IDAT chunks: {path}")
            saw_idat = True
            idat.extend(chunk_data)
        elif chunk_type == b"IEND":
            if length != 0 or not saw_idat:
                raise ManifestError(f"capture has an invalid IEND chunk: {path}")
            saw_iend = True
            cursor = chunk_end
            if cursor != len(payload):
                raise ManifestError(f"capture has data after its PNG IEND: {path}")
            break
        else:
            if saw_idat:
                ended_idat = True
            if chunk_type[:1].isupper():
                raise ManifestError(
                    f"capture contains an unsupported critical PNG chunk: {path}"
                )
        cursor = chunk_end
    if dimensions is None or not saw_iend or not idat:
        raise ManifestError(f"capture is not a complete PNG: {path}")
    width, height = dimensions
    row_size = 1 + width * channels
    expected_size = row_size * height
    decoder = zlib.decompressobj()
    try:
        decoded = decoder.decompress(bytes(idat), expected_size + 1)
        decoded += decoder.flush()
    except zlib.error as error:
        raise ManifestError(f"capture PNG image data cannot be decoded: {path}") from error
    if (not decoder.eof or decoder.unused_data or decoder.unconsumed_tail
            or len(decoded) != expected_size):
        raise ManifestError(f"capture PNG image data is incomplete or oversized: {path}")
    if any(decoded[offset] > 4 for offset in range(0, expected_size, row_size)):
        raise ManifestError(f"capture PNG uses an invalid row filter: {path}")
    return dimensions


def verify_capture_manifest(
    manifest: Path, expected_hash: str, commit: str, tag: str, asset_root: Path
) -> None:
    if HASH_RE.fullmatch(expected_hash) is None:
        raise ManifestError("capture manifest SHA-256 must be 64 lowercase hex characters")
    if sha256_file(manifest) != expected_hash:
        raise ManifestError("capture manifest SHA-256 does not match the dispatch input")
    payload = manifest.read_bytes()
    if not payload or payload.startswith(b"\xef\xbb\xbf") or b"\r" in payload:
        raise ManifestError("capture manifest must be non-empty UTF-8 without BOM or CR")
    try:
        data = json.loads(
            payload.decode("utf-8", errors="strict"),
            object_pairs_hook=unique_json_object,
            parse_constant=reject_json_constant,
        )
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ManifestError("capture manifest is not strict JSON/UTF-8") from error
    if not isinstance(data, dict):
        raise ManifestError("capture manifest root must be an object")
    require_exact_fields(data, CAPTURE_ROOT_FIELDS, "capture manifest")
    if data.get("schema") != CAPTURE_SCHEMA:
        raise ManifestError("capture manifest schema is not supported")
    if data.get("source_commit") != commit or COMMIT_RE.fullmatch(commit) is None:
        raise ManifestError("capture manifest does not bind the exact source commit")
    if data.get("release_tag") != tag or TAG_RE.fullmatch(tag) is None:
        raise ManifestError("capture manifest does not bind v0.7.51")
    if data.get("authority") != "native-packaged-vulkan" or data.get("accepted") is not True:
        raise ManifestError("capture evidence is not accepted native packaged Vulkan evidence")
    require_json_string(data.get("accepted_by"), "accepted_by")
    accepted_utc = require_json_string(data.get("accepted_utc"), "accepted_utc")
    if re.fullmatch(
        r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z",
        accepted_utc,
    ) is None:
        raise ManifestError("capture evidence lacks a canonical UTC acceptance time")
    try:
        datetime.strptime(accepted_utc, "%Y-%m-%dT%H:%M:%SZ").replace(
            tzinfo=timezone.utc)
    except ValueError as error:
        raise ManifestError("capture evidence has an invalid UTC acceptance time") from error
    host = data.get("host")
    if not isinstance(host, dict):
        raise ManifestError("capture evidence lacks native host/Vulkan identity")
    require_exact_fields(host, CAPTURE_HOST_FIELDS, "capture host")
    if require_json_string(host.get("platform"), "host.platform") != "Windows x64":
        raise ManifestError("capture evidence was not recorded on Windows x64")
    require_json_string(host.get("vulkan_device"), "host.vulkan_device")
    runtime = data.get("runtime_archive")
    if not isinstance(runtime, dict):
        raise ManifestError("capture manifest lacks runtime archive identity")
    require_exact_fields(runtime, CAPTURE_RUNTIME_FIELDS, "runtime_archive")
    runtime_filename = safe_relative_path(
        require_json_string(runtime.get("filename"), "runtime_archive.filename"))
    expected_runtime_filename = (
        f"Epoch2DWalkEngine-{tag}-runner-windows-x64.zip"
    )
    if runtime_filename != expected_runtime_filename:
        raise ManifestError("capture evidence names the wrong Windows runtime archive")
    expected_runtime_bytes = require_json_integer(
        runtime.get("bytes"), "runtime_archive.bytes", 1)
    expected_runtime_hash = require_json_string(
        runtime.get("sha256"), "runtime_archive.sha256")
    if HASH_RE.fullmatch(expected_runtime_hash) is None:
        raise ManifestError("runtime_archive.sha256 is invalid")
    captures = data.get("captures")
    if not isinstance(captures, list) or not captures:
        raise ManifestError("capture manifest has no captures")
    scenarios: set[str] = set()
    scenario_order: list[str] = []
    semantic_phases: set[tuple[str, str, str, str]] = set()
    capture_paths: set[str] = set()
    capture_hashes: set[str] = set()
    resolved_asset_root = asset_root.resolve(strict=True)
    runtime_path = resolved_asset_root / runtime_filename
    if runtime_path.is_symlink() or not runtime_path.is_file():
        raise ManifestError("capture bundle lacks its exact packaged runtime archive")
    if runtime_path.stat().st_size != expected_runtime_bytes:
        raise ManifestError("capture runtime archive byte count mismatch")
    if sha256_file(runtime_path) != expected_runtime_hash:
        raise ManifestError("capture runtime archive SHA-256 mismatch")
    for index, capture in enumerate(captures):
        if not isinstance(capture, dict):
            raise ManifestError(f"capture {index} is not an object")
        require_exact_fields(capture, CAPTURE_FIELDS, f"capture[{index}]")
        scenario = require_json_string(capture.get("scenario"), f"capture[{index}].scenario")
        if scenario in scenarios:
            raise ManifestError(f"capture scenario is invalid or duplicate: {scenario!r}")
        if scenario not in CAPTURE_SCENARIO_MATRIX:
            raise ManifestError(f"capture scenario is not part of the required matrix: {scenario!r}")
        scenarios.add(scenario)
        scenario_order.append(scenario)
        expected_group, expected_subject, expected_phase, expected_viewport = (
            CAPTURE_SCENARIO_MATRIX[scenario]
        )
        group = require_json_string(capture.get("group"), f"capture[{index}].group")
        subject = require_json_string(
            capture.get("subject"), f"capture[{index}].subject")
        phase = require_json_string(capture.get("phase"), f"capture[{index}].phase")
        viewport_class = require_json_string(
            capture.get("viewport_class"), f"capture[{index}].viewport_class")
        semantic_key = (group, subject, phase, viewport_class)
        if semantic_key in semantic_phases:
            raise ManifestError(f"capture semantic phase is duplicated: {semantic_key!r}")
        semantic_phases.add(semantic_key)
        if semantic_key != (
            expected_group, expected_subject, expected_phase, expected_viewport
        ):
            raise ManifestError(
                f"capture scenario metadata does not match the required matrix: {scenario}"
            )
        filename = safe_relative_path(
            require_json_string(capture.get("filename"), f"capture[{index}].filename"))
        if filename != f"captures/{scenario}.png":
            raise ManifestError(f"capture filename is not canonical for scenario: {scenario}")
        if filename == runtime_filename or filename in capture_paths:
            raise ManifestError(f"capture filename is duplicated: {filename}")
        capture_paths.add(filename)
        expected_dimensions = capture_dimensions(capture, index)
        expected_bytes = require_json_integer(
            capture.get("bytes"), f"capture[{index}].bytes", 1)
        expected_capture_hash = require_json_string(
            capture.get("sha256"), f"capture[{index}].sha256")
        if HASH_RE.fullmatch(expected_capture_hash) is None:
            raise ManifestError(f"capture[{index}].sha256 is invalid")
        if expected_capture_hash in capture_hashes:
            raise ManifestError(
                f"capture image content is duplicated across phases: {scenario}"
            )
        capture_hashes.add(expected_capture_hash)
        capture_path = resolved_asset_root / filename
        if capture_path.is_symlink() or not capture_path.is_file():
            raise ManifestError(f"capture file is missing or non-regular: {filename}")
        if capture_path.stat().st_size != expected_bytes:
            raise ManifestError(f"capture byte count mismatch: {filename}")
        if sha256_file(capture_path) != expected_capture_hash:
            raise ManifestError(f"capture SHA-256 mismatch: {filename}")
        if png_dimensions(capture_path) != expected_dimensions:
            raise ManifestError(f"capture dimensions mismatch: {filename}")
    if tuple(scenario_order) != CAPTURE_SCENARIOS:
        required = set(CAPTURE_SCENARIOS)
        missing = sorted(required - scenarios)
        extra = sorted(scenarios - required)
        if missing or extra:
            raise ManifestError(
                f"capture matrix differs; missing={missing!r} extra={extra!r}"
            )
        raise ManifestError("capture scenarios are not in the required matrix order")
    manifest_relative = safe_relative_path(
        manifest.resolve(strict=True).relative_to(resolved_asset_root).as_posix())
    expected_files = sorted_paths(
        [manifest_relative, runtime_filename, *capture_paths]
    )
    if inventory(resolved_asset_root) != expected_files:
        raise ManifestError("capture bundle file set differs from its manifest")


def verify_capture_bundle(
    archive: Path, expected_hash: str, commit: str, tag: str, output: Path
) -> None:
    if HASH_RE.fullmatch(expected_hash) is None:
        raise ManifestError("capture bundle SHA-256 must be 64 lowercase hex characters")
    if sha256_file(archive) != expected_hash:
        raise ManifestError("capture bundle SHA-256 does not match the dispatch input")
    if output.exists() and any(output.iterdir()):
        raise ManifestError("capture extraction directory is not empty")
    output.mkdir(parents=True, exist_ok=True)
    extracted: set[str] = set()
    expanded_bytes = 0
    with zipfile.ZipFile(archive, "r") as bundle:
        for entry in bundle.infolist():
            raw_name = entry.filename.rstrip("/")
            if not raw_name:
                continue
            name = safe_relative_path(raw_name)
            unix_mode = entry.external_attr >> 16
            if stat.S_ISLNK(unix_mode):
                raise ManifestError(f"capture bundle contains a symlink: {name}")
            if entry.is_dir():
                (output / name).mkdir(parents=True, exist_ok=True)
                continue
            if name in extracted:
                raise ManifestError(f"capture bundle contains duplicate file: {name}")
            if entry.file_size > 64 * 1024 * 1024:
                raise ManifestError(f"capture bundle entry is unreasonably large: {name}")
            expanded_bytes += entry.file_size
            if expanded_bytes > 512 * 1024 * 1024:
                raise ManifestError("capture bundle expands beyond the release evidence limit")
            extracted.add(name)
            destination = output / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            with bundle.open(entry, "r") as source, destination.open("wb") as target:
                while block := source.read(1024 * 1024):
                    target.write(block)
    manifest = output / "native-capture.json"
    if not manifest.is_file():
        raise ManifestError("capture bundle lacks native-capture.json at its root")
    verify_capture_manifest(
        manifest, sha256_file(manifest), commit, tag, output)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser()
    commands = result.add_subparsers(dest="command", required=True)
    for name in ("write", "verify"):
        command = commands.add_parser(name)
        command.add_argument("--root", type=Path, required=True)
        command.add_argument("--manifest", type=Path, required=True)
    checksum = commands.add_parser("checksum")
    checksum.add_argument("--archive", type=Path, required=True)
    checksum.add_argument("--sidecar", type=Path, required=True)
    publication = commands.add_parser("verify-publication")
    publication.add_argument("--asset-root", type=Path, required=True)
    publication.add_argument("--public-root", type=Path, required=True)
    publication.add_argument("--tag", required=True)
    git_verify = commands.add_parser("verify-git-tree")
    git_verify.add_argument("--repository", type=Path, required=True)
    git_verify.add_argument("--treeish", required=True)
    git_verify.add_argument("--root", type=Path, required=True)
    git_verify.add_argument("--exclude-prefix", action="append", default=[])
    capture = commands.add_parser("verify-capture")
    capture.add_argument("--manifest", type=Path, required=True)
    capture.add_argument("--sha256", required=True)
    capture.add_argument("--commit", required=True)
    capture.add_argument("--tag", required=True)
    capture.add_argument("--asset-root", type=Path, required=True)
    capture_bundle = commands.add_parser("verify-capture-bundle")
    capture_bundle.add_argument("--archive", type=Path, required=True)
    capture_bundle.add_argument("--sha256", required=True)
    capture_bundle.add_argument("--commit", required=True)
    capture_bundle.add_argument("--tag", required=True)
    capture_bundle.add_argument("--output", type=Path, required=True)
    for name in ("write-promotion-receipt", "verify-promotion-receipt"):
        promotion = commands.add_parser(name)
        promotion.add_argument("--receipt", type=Path, required=True)
        promotion.add_argument("--signature", type=Path, required=True)
        promotion.add_argument("--summary", type=Path, required=True)
        promotion.add_argument("--asset-root", type=Path, required=True)
        promotion.add_argument("--handoff-root", type=Path, required=True)
        promotion.add_argument("--capture-root", type=Path, required=True)
        promotion.add_argument("--build-receipt", type=Path, required=True)
        promotion.add_argument("--linux-abi", type=Path, required=True)
        promotion.add_argument("--capture-bundle", type=Path, required=True)
        promotion.add_argument("--capture-url", required=True)
        promotion.add_argument("--capture-sha256", required=True)
        promotion.add_argument("--commit", required=True)
        promotion.add_argument("--tag", required=True)
        promotion.add_argument("--build-run-id", required=True)
        promotion.add_argument("--build-run-attempt", required=True)
        promotion.add_argument("--promotion-run-id", required=True)
        promotion.add_argument("--promotion-run-attempt", required=True)
        promotion.add_argument("--event-name", required=True)
        promotion.add_argument("--run-sha", required=True)
        promotion.add_argument("--run-ref", required=True)
        promotion.add_argument("--workflow-sha", required=True)
        promotion.add_argument("--workflow-ref", required=True)
        promotion.add_argument("--repository", required=True)
        promotion.add_argument("--workflow", required=True)
        promotion.add_argument("--publication-target", required=True)
        promotion.add_argument("--github-status", required=True)
        for artifact in (
            "build-artifact", "promotion-inputs-artifact", "native-evidence-artifact"
        ):
            promotion.add_argument(f"--{artifact}-id", required=True)
            promotion.add_argument(f"--{artifact}-name", required=True)
            promotion.add_argument(f"--{artifact}-sha256", required=True)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        if arguments.command == "write":
            write_manifest(arguments.root, arguments.manifest)
        elif arguments.command == "verify":
            verify_manifest(arguments.root, arguments.manifest)
        elif arguments.command == "checksum":
            verify_checksum(arguments.archive, arguments.sidecar)
        elif arguments.command == "verify-publication":
            verify_publication(
                arguments.asset_root, arguments.public_root, arguments.tag
            )
        elif arguments.command == "verify-git-tree":
            verify_git_tree(
                arguments.repository,
                arguments.treeish,
                arguments.root,
                arguments.exclude_prefix,
            )
        elif arguments.command == "verify-capture":
            verify_capture_manifest(
                arguments.manifest,
                arguments.sha256,
                arguments.commit,
                arguments.tag,
                arguments.asset_root,
            )
        elif arguments.command == "verify-capture-bundle":
            verify_capture_bundle(
                arguments.archive,
                arguments.sha256,
                arguments.commit,
                arguments.tag,
                arguments.output,
            )
        elif arguments.command in (
            "write-promotion-receipt", "verify-promotion-receipt"
        ):
            values = {
                "asset_root": arguments.asset_root,
                "handoff_root": arguments.handoff_root,
                "capture_root": arguments.capture_root,
                "build_receipt": arguments.build_receipt,
                "linux_abi": arguments.linux_abi,
                "capture_bundle": arguments.capture_bundle,
                "capture_url": arguments.capture_url,
                "capture_sha256": arguments.capture_sha256,
                "commit": arguments.commit,
                "tag": arguments.tag,
                "build_run_id": arguments.build_run_id,
                "build_run_attempt": arguments.build_run_attempt,
                "promotion_run_id": arguments.promotion_run_id,
                "promotion_run_attempt": arguments.promotion_run_attempt,
                "event_name": arguments.event_name,
                "run_sha": arguments.run_sha,
                "run_ref": arguments.run_ref,
                "workflow_sha": arguments.workflow_sha,
                "workflow_ref": arguments.workflow_ref,
                "repository": arguments.repository,
                "workflow": arguments.workflow,
                "publication_target": arguments.publication_target,
                "github_status": arguments.github_status,
                "build_artifact_id": arguments.build_artifact_id,
                "build_artifact_name": arguments.build_artifact_name,
                "build_artifact_sha256": arguments.build_artifact_sha256,
                "promotion_inputs_artifact_id": arguments.promotion_inputs_artifact_id,
                "promotion_inputs_artifact_name": arguments.promotion_inputs_artifact_name,
                "promotion_inputs_artifact_sha256": arguments.promotion_inputs_artifact_sha256,
                "native_evidence_artifact_id": arguments.native_evidence_artifact_id,
                "native_evidence_artifact_name": arguments.native_evidence_artifact_name,
                "native_evidence_artifact_sha256": arguments.native_evidence_artifact_sha256,
            }
            if arguments.command == "write-promotion-receipt":
                write_promotion_receipt(
                    arguments.receipt, arguments.signature, arguments.summary, **values
                )
            else:
                verify_promotion_receipt(
                    arguments.receipt, arguments.signature, arguments.summary, **values
                )
    except (
        ManifestError,
        OSError,
        subprocess.CalledProcessError,
        zipfile.BadZipFile,
    ) as error:
        print(f"strict manifest verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
