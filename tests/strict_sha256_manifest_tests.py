#!/usr/bin/env python3

from __future__ import annotations

import binascii
import copy
from functools import lru_cache
import hashlib
import json
import os
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest
import zipfile
import zlib


REPOSITORY = Path(__file__).resolve().parents[1]
TOOL = REPOSITORY / "tools" / "strict_sha256_manifest.py"


def capture_matrix() -> list[tuple[str, str, str, str, str]]:
    matrix = [
        ("live-compact", "live", "workspace", "overview", "compact"),
        ("live-mission-compact-top", "live-page", "mission", "top", "compact"),
        ("live-mission-compact-bottom", "live-page", "mission", "bottom", "compact"),
        ("live-view-compact-top", "live-page", "view", "top", "compact"),
        ("live-view-compact-bottom", "live-page", "view", "bottom", "compact"),
        ("live-details-compact-top", "live-page", "details", "top", "compact"),
        ("live-details-compact-bottom", "live-page", "details", "bottom", "compact"),
        ("live-default", "live", "workspace", "overview", "default"),
        ("live-ultrawide", "live", "workspace", "overview", "ultrawide"),
        ("live-ultrawide-pip", "live", "pip", "visible", "ultrawide"),
        ("course-overview", "course", "world", "overview", "default"),
        ("lake-approach", "lake", "human", "approach", "default"),
        ("lake-immersion", "lake", "human", "immersion", "default"),
        ("lake-exit", "lake", "human", "exit", "default"),
        ("human-heel-strike", "gait", "human", "heel-strike", "default"),
        ("human-flat-support", "gait", "human", "flat-support", "default"),
        ("human-toe-off", "gait", "human", "toe-off", "default"),
    ]
    for species in ("chicken", "dog", "hexapod"):
        matrix.extend(
            (
                (f"{species}-contact-loaded", "species-contact", species, "loaded", "default"),
                (f"{species}-contact-transfer", "species-contact", species, "transfer", "default"),
                (f"{species}-lake-entry", "species-lake", species, "entry", "default"),
                (f"{species}-lake-exit", "species-lake", species, "exit", "default"),
            )
        )
    matrix.extend(
        (
            ("equipment-plant", "equipment", "human-carbine", "plant", "default"),
            ("equipment-aim", "equipment", "human-carbine", "aim", "default"),
            ("equipment-fire", "equipment", "human-carbine", "fire", "default"),
            ("equipment-recovery", "equipment", "human-carbine", "recovery", "default"),
        )
    )
    for page in ("presets", "structure", "motors", "art", "test"):
        matrix.extend(
            (
                (f"riglab-{page}-compact-top", "rig-lab-page", page, "top", "compact"),
                (
                    f"riglab-{page}-compact-bottom",
                    "rig-lab-page",
                    page,
                    "bottom",
                    "compact",
                ),
            )
        )
    for page in ("presets", "structure", "motors", "test"):
        matrix.extend(
            (
                (f"riglab-{page}-normal", "rig-lab", page, "normal", "default"),
                (f"riglab-{page}-zoom", "rig-lab", page, "zoom", "default"),
            )
        )
    for species in ("human", "chicken", "dog", "hexapod"):
        matrix.extend(
            (
                (f"riglab-art-{species}-normal", "rig-lab-art", species, "normal", "default"),
                (f"riglab-art-{species}-zoom", "rig-lab-art", species, "zoom", "default"),
            )
        )
    matrix.append(("terminal-state", "terminal", "active-rig", "frozen", "default"))
    return matrix


VIEWPORT_SIZE = {
    "compact": (1368, 768),
    "default": (2048, 1152),
    "ultrawide": (3440, 1440),
}


def png_chunk(chunk_type: bytes, payload: bytes) -> bytes:
    checksum = binascii.crc32(chunk_type + payload) & 0xFFFF_FFFF
    return struct.pack(">I", len(payload)) + chunk_type + payload + struct.pack(">I", checksum)


@lru_cache(maxsize=None)
def compressed_rgb_rows(width: int, height: int) -> bytes:
    row = b"\x00" + b"\x00\x00\x00" * width
    return zlib.compress(row * height, level=9)


def rgb_png(width: int, height: int, marker: str = "capture") -> bytes:
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + png_chunk(b"tEXt", f"scenario\x00{marker}".encode("ascii"))
        + png_chunk(b"IDAT", compressed_rgb_rows(width, height))
        + png_chunk(b"IEND", b"")
    )


class StrictManifestTests(unittest.TestCase):
    maxDiff = None

    def run_tool(
        self,
        *arguments: object,
        succeeds: bool = True,
        environment: dict[str, str] | None = None,
    ) -> subprocess.CompletedProcess[str]:
        process_environment = os.environ.copy()
        if environment is not None:
            process_environment.update(environment)
        result = subprocess.run(
            [sys.executable, str(TOOL), *(str(argument) for argument in arguments)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=process_environment,
        )
        if succeeds and result.returncode != 0:
            self.fail(result.stderr)
        if not succeeds and result.returncode == 0:
            self.fail("strict verifier accepted an invalid artifact")
        return result

    def write_capture_manifest(self, base: Path, data: dict[str, object]) -> tuple[Path, str]:
        manifest = base / "native-capture.json"
        manifest.write_text(
            json.dumps(data, sort_keys=True, separators=(",", ":")) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        return manifest, hashlib.sha256(manifest.read_bytes()).hexdigest()

    def make_capture_fixture(
        self, base: Path
    ) -> tuple[dict[str, object], Path, Path, str, str]:
        commit = "1" * 40
        runtime_name = "Epoch2DWalkEngine-v0.7.51-runner-windows-x64.zip"
        runtime = base / runtime_name
        runtime.write_bytes(b"exact immutable Phase A packaged runtime")
        captures: list[dict[str, object]] = []
        captures_root = base / "captures"
        captures_root.mkdir()
        for scenario, group, subject, phase, viewport_class in capture_matrix():
            logical_width, logical_height = VIEWPORT_SIZE[viewport_class]
            payload = rgb_png(logical_width, logical_height, scenario)
            filename = f"captures/{scenario}.png"
            (base / filename).write_bytes(payload)
            captures.append(
                {
                    "scenario": scenario,
                    "group": group,
                    "subject": subject,
                    "phase": phase,
                    "filename": filename,
                    "viewport_class": viewport_class,
                    "logical_width": logical_width,
                    "logical_height": logical_height,
                    "drawable_width": logical_width,
                    "drawable_height": logical_height,
                    "dpi": 96,
                    "bytes": len(payload),
                    "sha256": hashlib.sha256(payload).hexdigest(),
                }
            )
        data: dict[str, object] = {
            "schema": "epoch2dwalk-native-capture-v2",
            "source_commit": commit,
            "release_tag": "v0.7.51",
            "authority": "native-packaged-vulkan",
            "accepted": True,
            "accepted_by": "native-eye-test-operator",
            "accepted_utc": "2026-09-05T12:34:56Z",
            "host": {"platform": "Windows x64", "vulkan_device": "test-device"},
            "runtime_archive": {
                "filename": runtime_name,
                "bytes": runtime.stat().st_size,
                "sha256": hashlib.sha256(runtime.read_bytes()).hexdigest(),
            },
            "captures": captures,
        }
        manifest, digest = self.write_capture_manifest(base, data)
        return data, manifest, runtime, commit, digest

    def make_promotion_fixture(self, base: Path) -> dict[str, object]:
        tag = "v0.7.51"
        commit = "2" * 40
        build_run_id = "41001"
        build_run_attempt = "2"
        asset_root = base / "phase-a"
        asset_root.mkdir()
        prefix = f"Epoch2DWalkEngine-{tag}"
        archive_names = (
            f"{prefix}-runner-windows-x64.zip",
            f"{prefix}-runner-linux-x86_64.tar.gz",
            f"{prefix}-dev-windows-x64.zip",
            f"{prefix}-dev-linux-x86_64.tar.gz",
            f"{prefix}-source.zip",
        )
        asset_names: list[str] = []
        for archive_name in archive_names:
            manifest_stem = (
                archive_name[:-4]
                if archive_name.endswith(".zip")
                else archive_name[:-7]
            )
            asset_names.extend(
                (
                    archive_name,
                    f"{archive_name}.sha256",
                    f"{manifest_stem}.manifest.sha256",
                )
            )
        for index, name in enumerate(asset_names):
            (asset_root / name).write_bytes(
                f"immutable phase-a object {index:02d} {name}\n".encode("utf-8")
            )
        linux_abi = asset_root / f"{prefix}-linux-abi.txt"
        linux_abi.write_bytes(b"highest_required_glibc=2.39\nhighest_required_glibcxx=3.4.33\n")
        build_receipt = asset_root / "BUILD-RECEIPT.txt"
        lines = [
            "product=Epoch2DWalkEngine",
            "phase=build",
            f"build_run_id={build_run_id}",
            f"build_run_attempt={build_run_attempt}",
            f"source_commit={commit}",
            f"release_tag={tag}",
            "object_count=15",
        ]
        for name in asset_names:
            path = asset_root / name
            lines.append(
                f"asset={name} bytes={path.stat().st_size} "
                f"sha256={hashlib.sha256(path.read_bytes()).hexdigest()}"
            )
        build_receipt.write_bytes(("\n".join(lines) + "\n").encode("utf-8"))
        capture_bundle = base / "native-captures.zip"
        capture_bundle.write_bytes(b"immutable external native capture bundle")
        evidence = base / "promotion"
        evidence.mkdir()
        for name in asset_names:
            (evidence / name).write_bytes((asset_root / name).read_bytes())
        (evidence / build_receipt.name).write_bytes(build_receipt.read_bytes())
        (evidence / linux_abi.name).write_bytes(linux_abi.read_bytes())
        (evidence / capture_bundle.name).write_bytes(capture_bundle.read_bytes())
        capture_root = base / "verified-captures"
        capture_root.mkdir()
        (capture_root / "native-capture.json").write_bytes(
            b'{"schema":"test-canonical-capture-tree"}\n'
        )
        (capture_root / "frames").mkdir()
        (capture_root / "frames" / "human.png").write_bytes(b"native frame bytes")
        handoff_capture_root = evidence / "native-captures"
        handoff_capture_root.mkdir()
        (handoff_capture_root / "native-capture.json").write_bytes(
            (capture_root / "native-capture.json").read_bytes()
        )
        (handoff_capture_root / "frames").mkdir()
        (handoff_capture_root / "frames" / "human.png").write_bytes(
            (capture_root / "frames" / "human.png").read_bytes()
        )
        receipt = evidence / f"{prefix}-PROMOTION-RECEIPT.json"
        signature = evidence / f"{receipt.name}.hmac-sha256"
        summary = evidence / f"{prefix}-SITE-HANDOFF.txt"
        return {
            "receipt": receipt,
            "signature": signature,
            "summary": summary,
            "asset_root": asset_root,
            "handoff_root": evidence,
            "capture_root": capture_root,
            "build_receipt": build_receipt,
            "linux_abi": linux_abi,
            "capture_bundle": capture_bundle,
            "capture_url": (
                "https://epoch.adamrushford.chatgpt.site/downloads/candidates/"
                f"v0.7.51/{commit}/{prefix}-native-captures.zip"
            ),
            "capture_sha256": hashlib.sha256(capture_bundle.read_bytes()).hexdigest(),
            "commit": commit,
            "tag": tag,
            "build_run_id": build_run_id,
            "build_run_attempt": build_run_attempt,
            "promotion_run_id": "52001",
            "promotion_run_attempt": "3",
            "event_name": "workflow_dispatch",
            "run_sha": commit,
            "run_ref": "refs/heads/main",
            "workflow_sha": commit,
            "repository": "epoch/epoch2dwalkengine",
            "workflow": ".github/workflows/release.yml",
            "workflow_ref": (
                "epoch/epoch2dwalkengine/.github/workflows/release.yml@refs/heads/main"
            ),
            "publication_target": "site",
            "github_status": "unavailable-http-403",
            "build_artifact_id": "61001",
            "build_artifact_name": (
                f"epoch2dwalk-v0751-build-candidate-{build_run_attempt}"
            ),
            "build_artifact_sha256": "a" * 64,
            "promotion_inputs_artifact_id": "62001",
            "promotion_inputs_artifact_name": (
                "epoch2dwalk-v0751-promotion-inputs-3"
            ),
            "promotion_inputs_artifact_sha256": "b" * 64,
            "native_evidence_artifact_id": "62002",
            "native_evidence_artifact_name": (
                "epoch2dwalk-v0751-native-visual-evidence-3"
            ),
            "native_evidence_artifact_sha256": "c" * 64,
            "secret": "promotion-test-secret-0123456789abcdef",
        }

    @staticmethod
    def promotion_cli(fixture: dict[str, object]) -> list[object]:
        result: list[object] = []
        for key in (
            "receipt",
            "signature",
            "summary",
            "asset_root",
            "handoff_root",
            "capture_root",
            "build_receipt",
            "linux_abi",
            "capture_bundle",
            "capture_url",
            "capture_sha256",
            "commit",
            "tag",
            "build_run_id",
            "build_run_attempt",
            "promotion_run_id",
            "promotion_run_attempt",
            "event_name",
            "run_sha",
            "run_ref",
            "workflow_sha",
            "workflow_ref",
            "repository",
            "workflow",
            "publication_target",
            "github_status",
            "build_artifact_id",
            "build_artifact_name",
            "build_artifact_sha256",
            "promotion_inputs_artifact_id",
            "promotion_inputs_artifact_name",
            "promotion_inputs_artifact_sha256",
            "native_evidence_artifact_id",
            "native_evidence_artifact_name",
            "native_evidence_artifact_sha256",
        ):
            result.extend((f"--{key.replace('_', '-')}", fixture[key]))
        return result

    def write_promotion_fixture(self, fixture: dict[str, object]) -> None:
        self.run_tool(
            "write-promotion-receipt",
            *self.promotion_cli(fixture),
            environment={"RUNNER_PROTECTED_RELEASE_APPROVAL": str(fixture["secret"])},
        )

    def verify_promotion_fixture(
        self, fixture: dict[str, object], *, succeeds: bool = True
    ) -> subprocess.CompletedProcess[str]:
        return self.run_tool(
            "verify-promotion-receipt",
            *self.promotion_cli(fixture),
            succeeds=succeeds,
            environment={"RUNNER_PROTECTED_RELEASE_APPROVAL": str(fixture["secret"])},
        )

    def verify_capture(
        self,
        base: Path,
        manifest: Path,
        commit: str,
        digest: str,
        *,
        succeeds: bool = True,
    ) -> subprocess.CompletedProcess[str]:
        return self.run_tool(
            "verify-capture",
            "--manifest",
            manifest,
            "--sha256",
            digest,
            "--commit",
            commit,
            "--tag",
            "v0.7.51",
            "--asset-root",
            base,
            succeeds=succeeds,
        )

    def assert_repeated_capture_failure(
        self,
        base: Path,
        manifest: Path,
        commit: str,
        digest: str,
        expected_message: str,
    ) -> None:
        first = self.verify_capture(base, manifest, commit, digest, succeeds=False)
        second = self.verify_capture(base, manifest, commit, digest, succeeds=False)
        self.assertEqual(first.stderr, second.stderr)
        self.assertIn(expected_message, first.stderr)

    def test_manifest_round_trip_and_exact_file_set(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "payload"
            root.mkdir()
            (root / "alpha file.txt").write_bytes(b"alpha\n")
            (root / "nested").mkdir()
            (root / "nested" / "zeta.bin").write_bytes(b"\x00\xff")
            manifest = base / "manifest.sha256"
            self.run_tool("write", "--root", root, "--manifest", manifest)
            self.run_tool("verify", "--root", root, "--manifest", manifest)
            payload = manifest.read_bytes()
            self.assertTrue(payload.endswith(b"\n"))
            self.assertNotIn(b"\r", payload)
            (root / "extra.txt").write_bytes(b"extra")
            self.run_tool(
                "verify", "--root", root, "--manifest", manifest, succeeds=False
            )

    def test_rejects_grammar_order_traversal_crlf_and_bom(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "payload"
            root.mkdir()
            (root / "a").write_bytes(b"a")
            (root / "b").write_bytes(b"b")
            a_hash = hashlib.sha256(b"a").hexdigest()
            b_hash = hashlib.sha256(b"b").hexdigest()
            invalid_payloads = (
                f"{a_hash.upper()}  a\n".encode(),
                f"{a_hash} a\n".encode(),
                f"{b_hash}  b\n{a_hash}  a\n".encode(),
                f"{a_hash}  ../a\n".encode(),
                f"{a_hash}  a\r\n{b_hash}  b\r\n".encode(),
                b"\xef\xbb\xbf" + f"{a_hash}  a\n{b_hash}  b\n".encode(),
            )
            for index, payload in enumerate(invalid_payloads):
                manifest = base / f"invalid-{index}.sha256"
                manifest.write_bytes(payload)
                self.run_tool(
                    "verify", "--root", root, "--manifest", manifest, succeeds=False
                )

    def test_checksum_is_exact(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            archive = base / "archive.zip"
            archive.write_bytes(b"archive")
            digest = hashlib.sha256(b"archive").hexdigest()
            sidecar = base / "archive.zip.sha256"
            sidecar.write_bytes(f"{digest}  archive.zip\n".encode())
            self.run_tool("checksum", "--archive", archive, "--sidecar", sidecar)
            sidecar.write_bytes(f"{digest}\n".encode())
            self.run_tool(
                "checksum", "--archive", archive, "--sidecar", sidecar, succeeds=False
            )

    def test_post_publication_verification_is_exact_repeatable_and_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            public_root = Path(temporary) / "anonymous-public-download"
            public_root.mkdir()
            asset_names = sorted(
                path.name
                for path in fixture["asset_root"].iterdir()
                if path.name != "BUILD-RECEIPT.txt"
                and not path.name.endswith("-linux-abi.txt")
            )
            self.assertEqual(len(asset_names), 15)
            for name in asset_names:
                (public_root / name).write_bytes(
                    (fixture["asset_root"] / name).read_bytes()
                )
            command = (
                "verify-publication",
                "--asset-root",
                fixture["asset_root"],
                "--public-root",
                public_root,
                "--tag",
                fixture["tag"],
            )
            first = self.run_tool(*command)
            second = self.run_tool(*command)
            self.assertEqual(first.stderr, second.stderr)

            changed = public_root / asset_names[0]
            changed.write_bytes(changed.read_bytes() + b"post-publication mutation")
            altered = self.run_tool(*command, succeeds=False)
            self.assertIn("not byte-identical", altered.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            public_root = Path(temporary) / "incomplete-public-download"
            public_root.mkdir()
            missing = self.run_tool(
                "verify-publication",
                "--asset-root",
                fixture["asset_root"],
                "--public-root",
                public_root,
                "--tag",
                fixture["tag"],
                succeeds=False,
            )
            self.assertIn("exact 15-object allowlist", missing.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            public_root = Path(temporary) / "extra-public-download"
            public_root.mkdir()
            for path in fixture["asset_root"].iterdir():
                if path.name == "BUILD-RECEIPT.txt" or path.name.endswith("-linux-abi.txt"):
                    continue
                (public_root / path.name).write_bytes(path.read_bytes())
            nested = public_root / "unexpected" / "foreign.bin"
            nested.parent.mkdir()
            nested.write_bytes(b"unlisted")
            extra = self.run_tool(
                "verify-publication",
                "--asset-root",
                fixture["asset_root"],
                "--public-root",
                public_root,
                "--tag",
                fixture["tag"],
                succeeds=False,
            )
            self.assertIn("exact 15-object allowlist", extra.stderr)

    def test_promotion_receipt_round_trip_is_canonical_and_stateless(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            self.write_promotion_fixture(fixture)
            receipt = fixture["receipt"]
            signature = fixture["signature"]
            first_receipt = receipt.read_bytes()
            first_signature = signature.read_bytes()
            first_summary = fixture["summary"].read_bytes()
            self.assertTrue(first_receipt.endswith(b"\n"))
            self.assertNotIn(b"\r", first_receipt)
            self.assertNotIn(str(fixture["secret"]).encode("utf-8"), first_receipt)
            first = self.verify_promotion_fixture(fixture)
            second = self.verify_promotion_fixture(fixture)
            self.assertEqual(first.stderr, second.stderr)
            self.assertEqual(receipt.read_bytes(), first_receipt)
            self.assertEqual(signature.read_bytes(), first_signature)
            self.assertIn(
                b"replay_policy=not-enforced-by-receipt-use-external-site-one-use-gate\n",
                first_summary,
            )

    def test_promotion_receipt_rejects_missing_and_mismatched_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            self.write_promotion_fixture(fixture)
            fixture["signature"].unlink()
            result = self.verify_promotion_fixture(fixture, succeeds=False)
            self.assertIn("recursive file set differs", result.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            self.write_promotion_fixture(fixture)
            fixture["receipt"].unlink()
            result = self.verify_promotion_fixture(fixture, succeeds=False)
            self.assertIn("recursive file set differs", result.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            self.write_promotion_fixture(fixture)
            fixture["capture_bundle"].write_bytes(b"mismatched capture bundle")
            result = self.verify_promotion_fixture(fixture, succeeds=False)
            self.assertIn("native capture bundle SHA-256 does not match", result.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            self.write_promotion_fixture(fixture)
            first_asset = next(
                path
                for path in fixture["asset_root"].iterdir()
                if path.name.endswith("runner-windows-x64.zip")
            )
            first_asset.write_bytes(b"changed after Phase A")
            result = self.verify_promotion_fixture(fixture, succeeds=False)
            self.assertIn("BUILD-RECEIPT.txt does not exactly bind", result.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            self.write_promotion_fixture(fixture)
            handoff_asset = next(
                path
                for path in fixture["handoff_root"].iterdir()
                if path.name.endswith("runner-linux-x86_64.tar.gz")
            )
            handoff_asset.write_bytes(b"mismatched Site handoff copy")
            result = self.verify_promotion_fixture(fixture, succeeds=False)
            self.assertIn("Site handoff copy does not match", result.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            self.write_promotion_fixture(fixture)
            fixture["build_receipt"].unlink()
            result = self.verify_promotion_fixture(fixture, succeeds=False)
            self.assertIn("strict manifest verification failed", result.stderr)

    def test_promotion_receipt_rejects_foreign_context_and_artifact_identity(self) -> None:
        mutations = (
            ("promotion_run_id", "52002"),
            ("promotion_run_attempt", "4"),
            ("build_run_id", "41002"),
            ("build_run_attempt", "5"),
            ("run_sha", "3" * 40),
            ("run_ref", "refs/heads/foreign"),
            ("workflow_sha", "3" * 40),
            (
                "workflow_ref",
                "epoch/epoch2dwalkengine/.github/workflows/release.yml@refs/heads/foreign",
            ),
            ("build_artifact_id", "61009"),
            ("build_artifact_sha256", "d" * 64),
            ("promotion_inputs_artifact_id", "62009"),
            ("promotion_inputs_artifact_sha256", "e" * 64),
            ("native_evidence_artifact_id", "62010"),
            ("native_evidence_artifact_sha256", "f" * 64),
            ("repository", "foreign/epoch2dwalkengine"),
            ("workflow", ".github/workflows/foreign.yml"),
        )
        for field, value in mutations:
            with self.subTest(field=field), tempfile.TemporaryDirectory() as temporary:
                fixture = self.make_promotion_fixture(Path(temporary))
                self.write_promotion_fixture(fixture)
                fixture[field] = value
                result = self.verify_promotion_fixture(fixture, succeeds=False)
                self.assertIn("strict manifest verification failed", result.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            fixture["repository"] = "foreign/epoch2dwalkengine"
            fixture["workflow_ref"] = (
                "foreign/epoch2dwalkengine/.github/workflows/release.yml@refs/heads/main"
            )
            self.write_promotion_fixture(fixture)
            fixture["repository"] = "epoch/epoch2dwalkengine"
            fixture["workflow_ref"] = (
                "epoch/epoch2dwalkengine/.github/workflows/release.yml@refs/heads/main"
            )
            result = self.verify_promotion_fixture(fixture, succeeds=False)
            self.assertIn("identity or immutable input inventory", result.stderr)

    def test_promotion_receipt_rejects_tamper_forgery_and_extra_objects(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            self.write_promotion_fixture(fixture)
            data = json.loads(fixture["receipt"].read_text(encoding="utf-8"))
            data["site_handoff_readiness"] = "published"
            fixture["receipt"].write_bytes(
                (json.dumps(data, sort_keys=True, separators=(",", ":")) + "\n").encode()
            )
            result = self.verify_promotion_fixture(fixture, succeeds=False)
            self.assertIn("HMAC signature does not authenticate", result.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            fixture["secret"] = "wrong-signing-secret-0123456789abcdef"
            self.write_promotion_fixture(fixture)
            fixture["secret"] = "expected-signing-secret-0123456789abcdef"
            result = self.verify_promotion_fixture(fixture, succeeds=False)
            self.assertIn("HMAC signature does not authenticate", result.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            self.write_promotion_fixture(fixture)
            payload = fixture["receipt"].read_text(encoding="utf-8")
            payload = payload.replace(
                '"schema":"epoch2dwalk-promotion-receipt-v2"',
                '"schema":"epoch2dwalk-promotion-receipt-v2","schema":"epoch2dwalk-promotion-receipt-v2"',
                1,
            )
            fixture["receipt"].write_text(payload, encoding="utf-8", newline="\n")
            result = self.verify_promotion_fixture(fixture, succeeds=False)
            self.assertIn("duplicate JSON key", result.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            (fixture["asset_root"] / "unexpected-object.bin").write_bytes(b"foreign")
            result = self.run_tool(
                "write-promotion-receipt",
                *self.promotion_cli(fixture),
                succeeds=False,
                environment={
                    "RUNNER_PROTECTED_RELEASE_APPROVAL": str(fixture["secret"])
                },
            )
            self.assertIn("exact 15-object set", result.stderr)

    def test_promotion_handoff_rejects_nested_extras_and_altered_capture(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            (fixture["handoff_root"] / "native-captures" / "frames" / "extra.bin").write_bytes(
                b"unlisted"
            )
            result = self.run_tool(
                "write-promotion-receipt",
                *self.promotion_cli(fixture),
                succeeds=False,
                environment={
                    "RUNNER_PROTECTED_RELEASE_APPROVAL": str(fixture["secret"])
                },
            )
            self.assertIn("recursive file set differs", result.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            capture = fixture["handoff_root"] / "native-captures" / "frames" / "human.png"
            capture.write_bytes(b"altered after verification")
            result = self.run_tool(
                "write-promotion-receipt",
                *self.promotion_cli(fixture),
                succeeds=False,
                environment={
                    "RUNNER_PROTECTED_RELEASE_APPROVAL": str(fixture["secret"])
                },
            )
            self.assertIn("extracted capture differs", result.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_promotion_fixture(Path(temporary))
            (fixture["handoff_root"] / "native-captures" / "unlisted-empty").mkdir()
            result = self.run_tool(
                "write-promotion-receipt",
                *self.promotion_cli(fixture),
                succeeds=False,
                environment={
                    "RUNNER_PROTECTED_RELEASE_APPROVAL": str(fixture["secret"])
                },
            )
            self.assertIn("recursive directory set differs", result.stderr)

    def test_capture_matrix_round_trip_bundle_and_repeated_verification(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            data, manifest, runtime, commit, digest = self.make_capture_fixture(base)
            self.assertEqual(len(data["captures"]), 60)
            first = self.verify_capture(base, manifest, commit, digest)
            second = self.verify_capture(base, manifest, commit, digest)
            self.assertEqual(first.stdout, second.stdout)
            self.assertEqual(first.stderr, second.stderr)
            bundle = base / "native-captures.zip"
            with zipfile.ZipFile(bundle, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                archive.write(manifest, "native-capture.json")
                archive.write(runtime, runtime.name)
                for capture in data["captures"]:
                    archive.write(base / capture["filename"], capture["filename"])
            bundle_hash = hashlib.sha256(bundle.read_bytes()).hexdigest()
            extracted = base / "extracted"
            self.run_tool(
                "verify-capture-bundle",
                "--archive",
                bundle,
                "--sha256",
                bundle_hash,
                "--commit",
                commit,
                "--tag",
                "v0.7.51",
                "--output",
                extracted,
            )

    def test_capture_accepts_exact_compact_and_high_dpi_drawable_profiles(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            data, _, _, commit, _ = self.make_capture_fixture(base)
            compact = data["captures"][0]
            compact_payload = rgb_png(1368, 799, "live-compact-1368x799")
            (base / compact["filename"]).write_bytes(compact_payload)
            compact.update(
                {
                    "logical_width": 1368,
                    "logical_height": 799,
                    "drawable_width": 1368,
                    "drawable_height": 799,
                    "bytes": len(compact_payload),
                    "sha256": hashlib.sha256(compact_payload).hexdigest(),
                }
            )
            default = next(
                capture
                for capture in data["captures"]
                if capture["scenario"] == "live-default"
            )
            high_dpi_payload = rgb_png(3072, 1728, "live-default-144dpi")
            (base / default["filename"]).write_bytes(high_dpi_payload)
            default.update(
                {
                    "drawable_width": 3072,
                    "drawable_height": 1728,
                    "dpi": 144,
                    "bytes": len(high_dpi_payload),
                    "sha256": hashlib.sha256(high_dpi_payload).hexdigest(),
                }
            )
            manifest, digest = self.write_capture_manifest(base, data)
            first = self.verify_capture(base, manifest, commit, digest)
            second = self.verify_capture(base, manifest, commit, digest)
            self.assertEqual(first.stderr, second.stderr)

    def test_capture_rejects_missing_group_and_wrong_order(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            data, _, _, commit, _ = self.make_capture_fixture(base)
            captures = data["captures"]
            removed = [item for item in captures if item["group"] == "equipment"]
            data["captures"] = [item for item in captures if item["group"] != "equipment"]
            for capture in removed:
                (base / capture["filename"]).unlink()
            manifest, digest = self.write_capture_manifest(base, data)
            self.assert_repeated_capture_failure(
                base, manifest, commit, digest, "capture matrix differs; missing=")

        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            data, _, _, commit, _ = self.make_capture_fixture(base)
            data["captures"][0], data["captures"][1] = (
                data["captures"][1], data["captures"][0])
            manifest, digest = self.write_capture_manifest(base, data)
            self.assert_repeated_capture_failure(
                base, manifest, commit, digest, "not in the required matrix order")

    def test_capture_rejects_wrong_logical_drawable_size_and_dpi(self) -> None:
        mutations = (
            ("logical_width", 1367, "is not an exact compact viewport"),
            ("dpi", 110, "is not an accepted native DPI"),
            ("drawable_width", 1367, "does not match logical size at declared DPI"),
        )
        for field, value, expected in mutations:
            with self.subTest(field=field), tempfile.TemporaryDirectory() as temporary:
                base = Path(temporary)
                data, _, _, commit, _ = self.make_capture_fixture(base)
                data["captures"][0][field] = value
                manifest, digest = self.write_capture_manifest(base, data)
                self.assert_repeated_capture_failure(
                    base, manifest, commit, digest, expected)

        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            data, _, _, commit, _ = self.make_capture_fixture(base)
            capture = data["captures"][0]
            wrong_png = rgb_png(1367, 768, "wrong-png-dimensions")
            (base / capture["filename"]).write_bytes(wrong_png)
            capture["bytes"] = len(wrong_png)
            capture["sha256"] = hashlib.sha256(wrong_png).hexdigest()
            manifest, digest = self.write_capture_manifest(base, data)
            self.assert_repeated_capture_failure(
                base, manifest, commit, digest, "capture dimensions mismatch")

    def test_capture_rejects_duplicate_phase_scenario_and_image(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            data, _, _, commit, _ = self.make_capture_fixture(base)
            human_flat = next(
                item for item in data["captures"] if item["scenario"] == "human-flat-support")
            human_flat["phase"] = "heel-strike"
            manifest, digest = self.write_capture_manifest(base, data)
            self.assert_repeated_capture_failure(
                base, manifest, commit, digest, "semantic phase is duplicated")

        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            data, _, _, commit, _ = self.make_capture_fixture(base)
            heel = next(
                item for item in data["captures"] if item["scenario"] == "human-heel-strike")
            flat = next(
                item for item in data["captures"] if item["scenario"] == "human-flat-support")
            duplicate = (base / heel["filename"]).read_bytes()
            (base / flat["filename"]).write_bytes(duplicate)
            flat["bytes"] = len(duplicate)
            flat["sha256"] = hashlib.sha256(duplicate).hexdigest()
            manifest, digest = self.write_capture_manifest(base, data)
            self.assert_repeated_capture_failure(
                base, manifest, commit, digest, "image content is duplicated across phases")

        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            data, _, _, commit, _ = self.make_capture_fixture(base)
            data["captures"][1]["scenario"] = data["captures"][0]["scenario"]
            manifest, digest = self.write_capture_manifest(base, data)
            self.assert_repeated_capture_failure(
                base, manifest, commit, digest, "scenario is invalid or duplicate")

    def test_capture_rejects_malformed_metadata_adversarially(self) -> None:
        mutations = (
            (lambda data: data["captures"][0].__setitem__("dpi", True), "must be an integer"),
            (lambda data: data["captures"][0].__setitem__("unexpected", 1), "fields differ"),
            (lambda data: data.__setitem__("accepted_utc", "2026-02-30T12:34:56Z"),
             "invalid UTC acceptance time"),
            (lambda data: data["host"].__setitem__("platform", "Linux x64"),
             "was not recorded on Windows x64"),
            (lambda data: data["captures"][0].__setitem__("logical_width", 1368.0),
             "must be an integer"),
        )
        for mutation, expected in mutations:
            with self.subTest(expected=expected), tempfile.TemporaryDirectory() as temporary:
                base = Path(temporary)
                original, _, _, commit, _ = self.make_capture_fixture(base)
                data = copy.deepcopy(original)
                mutation(data)
                manifest, digest = self.write_capture_manifest(base, data)
                self.assert_repeated_capture_failure(
                    base, manifest, commit, digest, expected)

        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            data, manifest, _, commit, _ = self.make_capture_fixture(base)
            payload = json.dumps(data, sort_keys=True, separators=(",", ":"))
            payload = payload.replace(
                '"schema":"epoch2dwalk-native-capture-v2"',
                '"schema":"epoch2dwalk-native-capture-v2","schema":"epoch2dwalk-native-capture-v2"',
                1,
            )
            manifest.write_text(payload + "\n", encoding="utf-8", newline="\n")
            digest = hashlib.sha256(manifest.read_bytes()).hexdigest()
            self.assert_repeated_capture_failure(
                base, manifest, commit, digest, "duplicate JSON key")

        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            data, _, _, commit, _ = self.make_capture_fixture(base)
            data["captures"][0]["dpi"] = float("nan")
            manifest, digest = self.write_capture_manifest(base, data)
            self.assert_repeated_capture_failure(
                base, manifest, commit, digest, "non-finite JSON number")

    def test_capture_rejects_corrupt_png_and_runtime_identity(self) -> None:
        corruptions = ("truncated", "crc", "trailing")
        for corruption in corruptions:
            with self.subTest(corruption=corruption), tempfile.TemporaryDirectory() as temporary:
                base = Path(temporary)
                data, _, _, commit, _ = self.make_capture_fixture(base)
                capture = data["captures"][0]
                capture_path = base / capture["filename"]
                valid_png = capture_path.read_bytes()
                if corruption == "truncated":
                    invalid_png = valid_png[:24]
                elif corruption == "crc":
                    invalid_png = bytearray(valid_png)
                    invalid_png[invalid_png.index(b"IDAT") + 4] ^= 0x01
                    invalid_png = bytes(invalid_png)
                else:
                    invalid_png = valid_png + b"trailing corruption"
                capture_path.write_bytes(invalid_png)
                capture["bytes"] = len(invalid_png)
                capture["sha256"] = hashlib.sha256(invalid_png).hexdigest()
                manifest, digest = self.write_capture_manifest(base, data)
                self.assert_repeated_capture_failure(
                    base, manifest, commit, digest, "capture")

        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            data, _, runtime, commit, _ = self.make_capture_fixture(base)
            runtime.write_bytes(b"different runtime")
            manifest, digest = self.write_capture_manifest(base, data)
            self.assert_repeated_capture_failure(
                base, manifest, commit, digest, "runtime archive byte count mismatch")


if __name__ == "__main__":
    unittest.main()
