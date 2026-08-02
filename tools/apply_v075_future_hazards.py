from pathlib import Path

root = Path(__file__).resolve().parents[1]
workflow_path = root / '.github/workflows/validate-runner-v075.yml'
workflow = workflow_path.read_text(encoding='utf-8')
workflow = workflow.replace(
    'permissions:\n  contents: write\n  pull-requests: read\n',
    'permissions:\n  contents: write\n  actions: read\n  pull-requests: write\n',
    1,
)
job = r'''

  publish-v075:
    if: github.head_ref == 'agent/publish-runner-v075-rescue' && github.event.pull_request.number == 30
    runs-on: ubuntu-24.04
    timeout-minutes: 30
    env:
      GH_TOKEN: ${{ github.token }}
      GH_REPO: ${{ github.repository }}
      PR_NUMBER: ${{ github.event.pull_request.number }}
      VALIDATION_RUN_ID: '30749571655'
      VALIDATION_ARTIFACT_ID: '8834215522'
      VALIDATION_ARTIFACT_DIGEST: 'sha256:d4a6565322e7d672e2a4d9d9fe7b12e7d9cb9c519f624c40f482a4a9e3e6ace3'
      RELEASE_TAG: v0.7.5
      RELEASE_ARCHIVE: Runner-v0.7.5-windows-x64.zip
    steps:
      - name: Checkout publisher script
        uses: actions/checkout@v4
        with:
          ref: ${{ github.head_ref }}
          fetch-depth: 0

      - name: Preserve publisher outside checkout
        shell: bash
        run: cp tools/publish_runner_v075.sh "$RUNNER_TEMP/publish_runner_v075.sh"

      - name: Checkout exact main release source
        uses: actions/checkout@v4
        with:
          ref: main
          fetch-depth: 0

      - name: Publish, audit, and clean Runner v0.7.5
        shell: bash
        run: bash "$RUNNER_TEMP/publish_runner_v075.sh"
'''
if '  publish-v075:' not in workflow:
    workflow = workflow.rstrip() + job + '\n'
workflow_path.write_text(workflow, encoding='utf-8', newline='\n')

publisher = r'''#!/usr/bin/env bash
set -euo pipefail

test "$(gh pr view 28 --json state --jq .state)" = 'MERGED'
test "$(gh api "repos/$GH_REPO/actions/artifacts/$VALIDATION_ARTIFACT_ID" --jq .expired)" = 'false'
test "$(gh api "repos/$GH_REPO/actions/artifacts/$VALIDATION_ARTIFACT_ID" --jq .workflow_run.id)" = "$VALIDATION_RUN_ID"
test "$(gh api "repos/$GH_REPO/actions/artifacts/$VALIDATION_ARTIFACT_ID" --jq .digest)" = "$VALIDATION_ARTIFACT_DIGEST"

git pull --ff-only origin main
python3 - <<'PY'
from pathlib import Path
import re

path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')
text = re.sub(r'\*\*Target:\*\* Runner v[^\n]+', '**Target:** Runner v0.7.5', text, count=1)
text = re.sub(
    r'\*\*Release state:\*\*[^\n]+',
    "**Release state:** PUBLISHED — v0.7.5 assets independently audited; awaiting Adam's live packaged-runtime confirmation",
    text,
    count=1,
)
plain_verified = {'WALK-CHECKPOINT-071'}
live_pending = {
    'WALK-DUCK-067', 'WALK-DUCK-068', 'WALK-TERRAIN-069',
    'WALK-PIP-070', 'WALK-CHICKEN-072', 'WALK-CURRICULUM-073',
    'WALK-MASTERY-074', 'WALK-FLIP-075', 'WALK-STAGES-076',
    'WALK-MONOPED-077',
}
for mission in sorted(plain_verified | live_pending):
    status = 'PACKAGE VERIFIED' if mission in plain_verified else 'PACKAGE VERIFIED — LIVE ACCEPTANCE PENDING'
    pattern = rf'(### {re.escape(mission)}[^\n]*\n\*\*Status:\*\*)[^\n]*'
    text, count = re.subn(pattern, rf'\1 {status}', text, count=1)
    if count != 1:
        raise SystemExit(f'could not finalize {mission}')
evidence = '''

## v0.7.5 immutable release evidence

- Pull request: `#28`
- Linux validation job: `91501086187` — passed
- Windows application/package job: `91501178733` — passed
- Validation workflow run: `30749571655`
- Validated workflow artifact: `8834215522`
- Workflow artifact digest: `sha256:d4a6565322e7d672e2a4d9d9fe7b12e7d9cb9c519f624c40f482a4a9e3e6ace3`
- Release tag: `v0.7.5`
- Release page: `https://github.com/Autodidac/Vulkan_AI_Walking_Training_Simulation/releases/tag/v0.7.5`
- Full Windows build and all three test suites: passed
- Build-tree Vulkan/package diagnostics: passed
- Installed executable and executable-relative `run.bat`: passed from an unrelated working directory
- ZIP extraction, per-file manifest comparison, release-asset re-download, and byte comparison: passed by the publisher
- Live screenshot-level behavior remains explicitly pending Adam's released-package confirmation. Any contradictory result reopens its exact mission.
'''
if '## v0.7.5 immutable release evidence' not in text:
    text += evidence
path.write_text(text, encoding='utf-8', newline='\n')
PY

mkdir -p validation
cat > validation/v0.7.5.md <<'EOF'
# Runner v0.7.5 release evidence

- Pull request: #28
- Validation workflow run: 30749571655
- Linux job: 91501086187 — passed
- Windows job: 91501178733 — passed
- Artifact ID: 8834215522
- Artifact digest: sha256:d4a6565322e7d672e2a4d9d9fe7b12e7d9cb9c519f624c40f482a4a9e3e6ace3
- Published tag: v0.7.5
- Windows full build, tests, diagnostics, install, launcher, ZIP, checksum, manifest, and published-asset byte audit: passed
EOF

git rm -f --ignore-unmatch .github/workflows/validate-runner-v075.yml
git rm -f --ignore-unmatch .github/workflows/publish-runner-v075.yml
git rm -f --ignore-unmatch .github/workflows/publish-runner-v075-rescue.yml
git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git add missioncache.md validation/v0.7.5.md
if ! git diff --cached --quiet; then
  git commit -m 'Finalize Runner v0.7.5 release evidence'
  git push origin HEAD:main
fi

source_sha="$(git rev-parse HEAD)"
test "$(git rev-parse origin/main)" = "$source_sha"

gh api "repos/$GH_REPO/actions/artifacts/$VALIDATION_ARTIFACT_ID/zip" > validation-artifact.zip
mkdir artifact package audit
unzip -q validation-artifact.zip -d artifact
unzip -q "artifact/$RELEASE_ARCHIVE" -d package
cp missioncache.md package/missioncache.md
cp RELEASE_NOTES_v0.7.5.md package/RELEASE_NOTES_v0.7.5.md
(cd package && zip -q -X -r "../$RELEASE_ARCHIVE" .)
sha256sum "$RELEASE_ARCHIVE" | awk '{print toupper($1)}' > "$RELEASE_ARCHIVE.sha256"
find package -type f -print0 | sort -z | while IFS= read -r -d '' file; do
  hash="$(sha256sum "$file" | awk '{print toupper($1)}')"
  printf '%s  %s\n' "$hash" "${file#package/}"
done > Runner-v0.7.5-windows-x64.manifest.sha256
unzip -q "$RELEASE_ARCHIVE" -d audit
find audit -type f -print0 | sort -z | while IFS= read -r -d '' file; do
  hash="$(sha256sum "$file" | awk '{print toupper($1)}')"
  printf '%s  %s\n' "$hash" "${file#audit/}"
done > audit.manifest.sha256
cmp Runner-v0.7.5-windows-x64.manifest.sha256 audit.manifest.sha256
test "$(sha256sum "$RELEASE_ARCHIVE" | awk '{print toupper($1)}')" = "$(cat "$RELEASE_ARCHIVE.sha256")"
grep -q 'WALK-SAND-078' audit/missioncache.md
grep -q 'WALK-HAZARD-079' audit/missioncache.md

if gh release view "$RELEASE_TAG" >/dev/null 2>&1; then
  echo 'Release already exists; verifying it instead of duplicating it.'
else
  git fetch origin --tags
  if git ls-remote --exit-code --tags origin "refs/tags/$RELEASE_TAG" >/dev/null 2>&1; then
    git push origin ":refs/tags/$RELEASE_TAG"
  fi
  git tag -a "$RELEASE_TAG" -m 'Runner v0.7.5'
  git push origin "$RELEASE_TAG"
  gh release create "$RELEASE_TAG" \
    "$RELEASE_ARCHIVE" \
    "$RELEASE_ARCHIVE.sha256" \
    Runner-v0.7.5-windows-x64.manifest.sha256 \
    --target "$source_sha" \
    --title 'Runner v0.7.5' \
    --notes-file RELEASE_NOTES_v0.7.5.md
fi

mkdir published
gh release download "$RELEASE_TAG" --dir published
cmp "$RELEASE_ARCHIVE" "published/$RELEASE_ARCHIVE"
cmp "$RELEASE_ARCHIVE.sha256" "published/$RELEASE_ARCHIVE.sha256"
cmp Runner-v0.7.5-windows-x64.manifest.sha256 published/Runner-v0.7.5-windows-x64.manifest.sha256
test "$(gh release view "$RELEASE_TAG" --json isDraft --jq .isDraft)" = 'false'
test "$(gh release view "$RELEASE_TAG" --json isPrerelease --jq .isPrerelease)" = 'false'

gh pr close "$PR_NUMBER" --comment 'Runner v0.7.5 published and independently audited.'
git push origin --delete agent/runner-v075-crouch-walk || true
git push origin --delete agent/publish-runner-v075-rescue || true
'''
publisher_path = root / 'tools/publish_runner_v075.sh'
publisher_path.write_text(publisher, encoding='utf-8', newline='\n')
publisher_path.chmod(0o755)

Path(__file__).unlink()
print('materialized v0.7.5 publisher into the existing validated workflow')
