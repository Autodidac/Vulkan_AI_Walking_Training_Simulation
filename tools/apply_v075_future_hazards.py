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
Path(__file__).unlink()
print('materialized v0.7.5 publisher into the existing validated workflow')
