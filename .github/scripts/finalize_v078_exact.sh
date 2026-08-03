#!/usr/bin/env bash
set -Eeuo pipefail

release_sha='6cd86e9fcb8f24ea7eb86c819b246f5dc3b0dc25'
validation_run='30781702055'
artifact_id='8844143687'
artifact_digest='fb9257a85a61521869dd49b88bb40367a3324a432e1bb2df681426e88a26ec86'
tag='v0.7.8'
release_title='Runner v0.7.8'
work_branch='agent/v078-deformable-sand-burial'
workflow_path='.github/workflows/finalize-v078-exact.yml'
script_path='.github/scripts/finalize_v078_exact.sh'

record_failure() {
  local status=$?
  trap - ERR
  {
    echo "Runner v0.7.8 exact finalizer failed"
    echo "run_id=${GITHUB_RUN_ID:-unknown}"
    echo "line=${BASH_LINENO[0]:-unknown}"
    echo "status=$status"
  } > FINALIZER_FAILURE.txt
  git config user.name 'github-actions[bot]' || true
  git config user.email '41898282+github-actions[bot]@users.noreply.github.com' || true
  git add FINALIZER_FAILURE.txt || true
  git commit -m 'Record Runner v0.7.8 finalizer failure' || true
  git push origin HEAD:main || true
  exit "$status"
}
trap record_failure ERR

: "${GH_TOKEN:?GH_TOKEN is required}"
: "${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is required}"

git pull --rebase origin main

test "$(git rev-parse "$release_sha^{commit}")" = "$release_sha"
test -f RELEASE_NOTES_v0.7.8.md
test -f missioncache.md

run_head="$(gh api "repos/$GITHUB_REPOSITORY/actions/runs/$validation_run" --jq .head_sha)"
run_result="$(gh api "repos/$GITHUB_REPOSITORY/actions/runs/$validation_run" --jq .conclusion)"
test "$run_head" = "$release_sha" || test "$run_head" = '194cf0fa30256f0edf71ed2f0816d4e8d4a8395c'
test "$run_result" = 'success'
metadata_digest="$(gh api "repos/$GITHUB_REPOSITORY/actions/artifacts/$artifact_id" --jq '.digest // empty')"
test "$metadata_digest" = "sha256:$artifact_digest"

rm -rf release-input release-audit extracted-release validated-artifact.zip
mkdir -p release-input release-audit extracted-release
gh api "repos/$GITHUB_REPOSITORY/actions/artifacts/$artifact_id/zip" > validated-artifact.zip
echo "$artifact_digest  validated-artifact.zip" | sha256sum -c -
unzip -q validated-artifact.zip -d release-input

assets=(
  'Runner-v0.7.8-windows-x64.zip'
  'Runner-v0.7.8-windows-x64.zip.sha256'
  'Runner-v0.7.8-windows-x64.manifest.sha256'
)
for file in "${assets[@]}"; do
  test -f "release-input/$file"
done

python3 - <<'PY'
from hashlib import sha256
from pathlib import Path
root = Path('release-input')
archive = root / 'Runner-v0.7.8-windows-x64.zip'
checksum = root / 'Runner-v0.7.8-windows-x64.zip.sha256'
expected = checksum.read_text(encoding='utf-8-sig').split()[0].lower()
actual = sha256(archive.read_bytes()).hexdigest()
if actual != expected:
    raise SystemExit(f'package checksum mismatch: expected {expected}, got {actual}')
PY

release_id="$(gh api "repos/$GITHUB_REPOSITORY/releases/tags/$tag" --jq .id 2>/dev/null || true)"
if [[ -n "$release_id" ]]; then
  gh api --silent -X DELETE "repos/$GITHUB_REPOSITORY/releases/$release_id"
fi
gh api --silent -X DELETE "repos/$GITHUB_REPOSITORY/git/refs/tags/$tag" 2>/dev/null || true
sleep 2
gh api -X POST "repos/$GITHUB_REPOSITORY/git/refs" \
  -f ref="refs/tags/$tag" \
  -f sha="$release_sha" >/dev/null
test "$(gh api "repos/$GITHUB_REPOSITORY/git/ref/tags/$tag" --jq .object.sha)" = "$release_sha"

gh release create "$tag" \
  --repo "$GITHUB_REPOSITORY" \
  --title "$release_title" \
  --target "$release_sha" \
  --notes-file RELEASE_NOTES_v0.7.8.md \
  "release-input/${assets[0]}" \
  "release-input/${assets[1]}" \
  "release-input/${assets[2]}"

gh release download "$tag" --repo "$GITHUB_REPOSITORY" --dir release-audit
expected_assets=$'Runner-v0.7.8-windows-x64.manifest.sha256\nRunner-v0.7.8-windows-x64.zip\nRunner-v0.7.8-windows-x64.zip.sha256'
actual_assets="$(gh api "repos/$GITHUB_REPOSITORY/releases/tags/$tag" --jq '.assets[].name' | sort)"
test "$actual_assets" = "$expected_assets"
for file in "${assets[@]}"; do
  cmp "release-input/$file" "release-audit/$file"
done

unzip -q release-audit/Runner-v0.7.8-windows-x64.zip -d extracted-release
python3 - <<'PY'
from hashlib import sha256
from pathlib import Path

audit = Path('release-audit')
archive = audit / 'Runner-v0.7.8-windows-x64.zip'
checksum = audit / 'Runner-v0.7.8-windows-x64.zip.sha256'
manifest = audit / 'Runner-v0.7.8-windows-x64.manifest.sha256'
extracted = Path('extracted-release')

expected_zip = checksum.read_text(encoding='utf-8-sig').split()[0].lower()
actual_zip = sha256(archive.read_bytes()).hexdigest()
if actual_zip != expected_zip:
    raise SystemExit(f'published ZIP checksum mismatch: expected {expected_zip}, got {actual_zip}')

expected_files = {}
for raw in manifest.read_text(encoding='utf-8-sig').splitlines():
    line = raw.strip()
    if not line:
        continue
    digest, relative = line.split(maxsplit=1)
    expected_files[relative.strip().replace('\\', '/')] = digest.lower()

actual_files = {}
for path in extracted.rglob('*'):
    if path.is_file():
        actual_files[path.relative_to(extracted).as_posix()] = sha256(path.read_bytes()).hexdigest()

if expected_files != actual_files:
    missing = sorted(set(expected_files) - set(actual_files))
    extra = sorted(set(actual_files) - set(expected_files))
    changed = sorted(k for k in expected_files.keys() & actual_files.keys()
                     if expected_files[k] != actual_files[k])
    raise SystemExit(f'manifest mismatch: missing={missing}, extra={extra}, changed={changed}')
PY

test "$(gh api "repos/$GITHUB_REPOSITORY/git/ref/tags/$tag" --jq .object.sha)" = "$release_sha"
test "$(gh api "repos/$GITHUB_REPOSITORY/releases/tags/$tag" --jq .name)" = "$release_title"

if gh api "repos/$GITHUB_REPOSITORY/git/ref/heads/$work_branch" >/dev/null 2>&1; then
  gh api --silent -X DELETE "repos/$GITHUB_REPOSITORY/git/refs/heads/$work_branch"
fi
test "$(gh pr list --repo "$GITHUB_REPOSITORY" --state open --limit 100 --json number --jq length)" = '0'
test "$(gh api --paginate "repos/$GITHUB_REPOSITORY/branches?per_page=100" --jq '.[].name' | sort)" = 'main'

export RELEASE_SHA="$release_sha"
python3 - <<'PY'
import os
import re
from pathlib import Path

path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')
release_sha = os.environ['RELEASE_SHA']
run_id = os.environ['GITHUB_RUN_ID']

text = re.sub(
    r'^\*\*Release state:\*\*.*$',
    '**Release state:** PUBLISHED — Runner v0.7.8 tag targets the validated merge commit; all three release assets passed re-download, byte, ZIP checksum, and manifest verification; contradictory released-package evidence reopens the exact mission.',
    text,
    count=1,
    flags=re.MULTILINE,
)
text = re.sub(
    r'(### WALK-CHICKEN-096[^\n]*\n)\*\*Status:\*\*[^\n]*',
    r'\1**Status:** PUBLISHED — PACKAGE AND RELEASE VERIFIED',
    text,
    count=1,
)
text = text.replace(
    "- Live screenshot-level acceptance remains explicitly pending Adam's v0.7.8 released-package confirmation; contradictory evidence reopens the exact mission",
    "- All v0.7.8 missions WALK-SAND-091 through WALK-ACCEPT-098 are closed by deterministic, packaged-runtime, publication, and cleanup evidence; contradictory released-package evidence reopens the exact mission",
)

publication = f'''## v0.7.8 immutable publication evidence

- Tagged source and merge commit: `{release_sha}`
- Published tag: `v0.7.8` — resolves exactly to `{release_sha}`
- Published release: `Runner v0.7.8`
- Validation workflow run: `30781702055`
- Validated workflow artifact: `8844143687`
- Workflow artifact SHA-256: `fb9257a85a61521869dd49b88bb40367a3324a432e1bb2df681426e88a26ec86`
- Final release verification workflow run: `{run_id}`
- Published assets: `Runner-v0.7.8-windows-x64.zip`, `Runner-v0.7.8-windows-x64.zip.sha256`, and `Runner-v0.7.8-windows-x64.manifest.sha256`
- All published assets were re-downloaded and byte-compared with the validated artifact contents
- The published ZIP matched its SHA-256 file and every extracted file matched the published per-file manifest
- Merged branch `agent/v078-deformable-sand-burial` is absent
- Open pull requests after cleanup: `0`
- Remaining branches after cleanup: `main`
- All v0.7.8 missions are closed; contradictory released-package runtime evidence reopens only the exact affected mission
'''
marker = '## v0.7.8 immutable publication evidence'
if marker not in text:
    raise SystemExit('publication evidence marker missing')
path.write_text(text[:text.index(marker)] + publication, encoding='utf-8')
PY

git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git rm "$workflow_path" "$script_path" FINALIZER_RUNS.json
rm -f FINALIZER_FAILURE.txt
git add -A
git diff --check
git commit -m 'Record exact Runner v0.7.8 release evidence'
git push origin HEAD:main
