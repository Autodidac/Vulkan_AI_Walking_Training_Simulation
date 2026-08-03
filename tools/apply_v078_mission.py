from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]; p=R/'missioncache.md'; t=p.read_text()
def status(mission,value):
 global t
 pattern=rf'(### {re.escape(mission)}[^\n]*\n\*\*Status:\*\*)[^\n]*'
 t,c=re.subn(pattern,rf'\1 {value}',t,count=1)
 if c!=1: raise RuntimeError(f'missing {mission}')
if t.count('**Target:** Runner v0.7.7')!=1: raise RuntimeError('target')
t=t.replace('**Target:** Runner v0.7.7','**Target:** Runner v0.7.8',1)
t,c=re.subn(r'\*\*Release state:\*\*[^\n]*','**Release state:** IN PROGRESS - v0.7.7 published and audited; v0.7.8 deformable terrain/material release under validation',t,count=1)
if c!=1: raise RuntimeError('release state')
for m in ('WALK-RIGSTANCE-084','WALK-CROUCH-085','WALK-EXPLORE-086','WALK-FEET-087','WALK-RELEASE-088','WALK-SLIDE-089','WALK-UPDATES-090'):
 status(m,'VERIFIED IN v0.7.7 - reopen on contradictory packaged-runtime evidence')
for m in ('WALK-SAND-078','WALK-HAZARD-079'):
 status(m,'IN PROGRESS - implementation materialized by v0.7.8 mission set below')
if 'WALK-SAND-091' in t: raise RuntimeError('v0.7.8 missions already present')
t=t.rstrip()+'''


## v0.7.8 deformable terrain and falling-material completion

### WALK-SAND-091 — Deterministic deformable sand terrain
**Status:** IN PROGRESS

Replace analytic sine-only ground with a seeded fixed-cost sand-cell heightfield. Foot pressure compacts and sinks loose support, displaces conserved volume into adjacent mounds, and relaxes unstable slopes. The same state must drive collision, live view, PIP, observation, evaluation, and replay.

### WALK-MATERIAL-092 — Persistent falling sand, rocks, and debris
**Status:** IN PROGRESS

Falling material owns persistent position, velocity, radius, density, and kind. Sand deposits into the terrain field; rocks and debris bounce, roll, settle, and transfer impact velocity. No hazard may tunnel, teleport, silently disappear while active, or exist only as a regenerated render curve.

### WALK-BURIAL-093 — Burial, obstruction, and free-space observations
**Status:** IN PROGRESS

Expose terrain firmness, looseness, slope, burial depth, incoming velocity, time-to-impact, density, head/torso/support obstruction, and the safer escape direction to the policy without removing existing gait state.

### WALK-ESCAPE-094 — Evade, brace, escape, and honest failure
**Status:** IN PROGRESS

Reward reducing burial and moving toward available free space. Permit partial penetration into loose material for recovery training, but terminate sustained head-and-torso burial when surrounding material leaves no practical escape. Do not grant survival to a motionless rig hidden beneath debris.

### WALK-RELEASE-095 — Publish audited Runner v0.7.8
**Status:** IN PROGRESS

Build and test Linux and the complete Windows Vulkan application, verify the installed executable and run.bat from an unrelated directory, audit ZIP/checksum/manifest and re-downloaded release assets, then remove temporary workflows and branches. Live packaged-runtime evidence remains authoritative and reopens exact missions when contradictory.
'''
p.write_text(t+'\n')
Path(__file__).unlink()
