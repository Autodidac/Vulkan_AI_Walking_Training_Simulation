#!/usr/bin/env python3
from __future__ import annotations
import re, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def rw(path): return (ROOT/path).read_text(encoding='utf-8')
def ww(path,text): (ROOT/path).write_text(text,encoding='utf-8',newline='\n')
def rep(text,old,new,label):
    n=text.count(old)
    if n!=1: raise RuntimeError(f'{label}: expected one match, found {n}')
    return text.replace(old,new,1)

def cache():
    text=rw('missioncache.md'); marker='# Runner v0.7.18 equipment, carry, and target curriculum'
    finding='''## v0.7.17 fifth Linux validation finding — full retraction and topology evidence

Clean run `31074108059` passed repository/art audits, compilation, all Stand cases, all paired vertical crouch cases, the guided-squat helper, the ordered stage matrix, optional-art parsing, and the warmed concurrency benchmark. The remaining exact defects were:

- horizontal press retraction stopped only 0.62 m above authored head height, keeping obstacle weight above the `<0.15` recovery gate even after the body restored;
- hexapod multi-support press behavior was still inferred through head geometry instead of explicit non-paired support topology;
- the direct crouch-walk evidence unit still supplied five gait cycles after the production requirement was raised to eight;
- monoped's deliberately ignored one-frame post-completion speed spike remained stored in `maximum_speed_kmh_`, reappearing as an overspeed invalidation immediately after the bounded recovery grace.

The correction must fully retract every press above the recovered body, define multi-support press topology from semantic supports and chain pairing, update the fixture to eight cycles, and discard only an overspeed sample explicitly ignored during the bounded recovery-settling window. Later overspeed remains terminal. Full Linux, Windows, package, optional-art fallback, archive, publication, re-download, and cleanup gates remain mandatory.

'''
    if '## v0.7.17 fifth Linux validation finding' not in text:
        if marker not in text: raise RuntimeError('v0.7.18 marker missing')
        text=text.replace(marker,finding+marker,1)
    ww('missioncache.md',text)

def implement():
    h=rw('src/simulation.hpp')
    h=rep(h,'        const float start = standing_head_top\n            + (horizontal_body_plan ? 0.62f : 1.10f);','        const float start = standing_head_top + 1.10f;','full press retraction')
    h=rep(h,'        [[nodiscard]] bool horizontal_multi_support_plan() const noexcept\n        {\n            return horizontal_body_plan() && !paired_leg_chains()\n                && support_seed_count() >= 4u;\n        }','        [[nodiscard]] bool horizontal_multi_support_plan() const noexcept\n        {\n            return !paired_leg_chains() && !monopedal_gait()\n                && support_seed_count() >= 4u;\n        }','semantic multi-support topology')
    ww('src/simulation.hpp',h)

    c=rw('src/simulation.cpp')
    old='''        if (course_stage_ == CourseStage::duck_press
            && (!duck_press_completed_ || duck_recovery_settling)
            && (frame_gate == InvalidMotion::sustained_flight
                || frame_gate == InvalidMotion::overspeed
                || frame_gate == InvalidMotion::collapsed_posture
                || frame_gate == InvalidMotion::fallen))
            frame_gate = InvalidMotion::none;
'''
    new='''        if (course_stage_ == CourseStage::duck_press
            && (!duck_press_completed_ || duck_recovery_settling)
            && (frame_gate == InvalidMotion::sustained_flight
                || frame_gate == InvalidMotion::overspeed
                || frame_gate == InvalidMotion::collapsed_posture
                || frame_gate == InvalidMotion::fallen))
        {
            if (duck_recovery_settling
                && frame_gate == InvalidMotion::overspeed)
            {
                maximum_speed_kmh_ = std::min(49.0f,
                    std::abs(forward_speed_) * 3.6f);
            }
            frame_gate = InvalidMotion::none;
        }
'''
    c=rep(c,old,new,'bounded ignored-speed reset')
    ww('src/simulation.cpp',c)

    t=rw('tests/core_tests.cpp')
    t=rep(t,'    require(sim::stage_skill_evidence(sim::CourseStage::crouch_walk,\n            5u, 3.0f, 0u, 0.0f, 0u, 4u),','    require(sim::stage_skill_evidence(sim::CourseStage::crouch_walk,\n            8u, 3.0f, 0u, 0.0f, 0u, 4u),','crouch-walk fixture cycles')
    t=rep(t,'            && sim::CreatureBlueprint::quadruped().horizontal_multi_support_plan()\n            && sim::CreatureBlueprint::crawler4().horizontal_multi_support_plan(),','            && sim::CreatureBlueprint::quadruped().horizontal_multi_support_plan()\n            && sim::CreatureBlueprint::crawler4().horizontal_multi_support_plan()\n            && sim::CreatureBlueprint::hexapod().horizontal_multi_support_plan(),','hexapod topology test')
    ww('tests/core_tests.cpp',t)

def main():
    if len(sys.argv)!=2 or sys.argv[1] not in {'cache','implement'}: return 2
    cache() if sys.argv[1]=='cache' else implement(); return 0
if __name__=='__main__': raise SystemExit(main())
