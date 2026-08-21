#include "locomotion_strategy.hpp"
#include "ppo.hpp"
#include "simulation.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

namespace runner::sim {
struct EnvironmentTestAccess {
    static void shuttle(Environment& e, ShuttleState s) noexcept {
        const float prior_facing=e.shuttle_state_.facing_direction;
        e.shuttle_state_=s;
        if((prior_facing<0.0f)!=(s.facing_direction<0.0f))
            e.mirror_rig_about_root();
    }
    static void root_x(Environment& e, float x) noexcept {
        const float d=x-e.particles_[e.blueprint_.root_node].position.x;
        for(Particle& p:e.particles_){p.position.x+=d;p.previous.x+=d;}
        e.previous_pelvis_.x+=d;
    }
    static void ready(Environment& e,std::uint32_t cycles,float stable,float distance=0.0f) noexcept {
        e.alternating_steps_=cycles;e.single_leg_cycles_=cycles;
        e.longest_stable_stance_seconds_=stable;
        e.distance_travelled_=distance;e.shuttle_distance_travelled_=distance;
    }
    static void rebuild(Environment& e) noexcept { e.rebuild_course_features(); }
    static void configure_facing_target(Environment& e,float facing,float distance=4.0f) noexcept {
        ShuttleState state{};state.facing_direction=facing;state.locomotion_direction=facing;
        state.completed_turns=2u;shuttle(e,state);
        const Vec2 mount=e.equipment_mount_position();
        e.equipment_target_.active=true;e.equipment_target_.position={mount.x+facing*distance,mount.y};
        e.equipment_target_.radius=0.42f;
    }
    static void drive_equipment(Environment& e,int steps) noexcept {
        for(int step=0;step<steps;++step){std::array<float,action_count> action{};
            const Vec2 delta=e.equipment_target_.position-e.equipment_mount_position();
            const float desired=std::atan2(delta.y,delta.x);
            const float local=e.facing_direction()<0.0f?wrap_angle(pi-desired):wrap_angle(desired);
            action[equipment_state_action]=1.0f;
            action[equipment_aim_action]=clamp(local/(pi*0.42f),-1.0f,1.0f);
            action[equipment_trigger_action]=1.0f;e.update_equipment(action,1.0f/60.0f);}
    }
    static void coast_equipment(Environment& e,int steps) noexcept {
        std::array<float,action_count> action{};
        for(int step=0;step<steps;++step)e.update_equipment(action,1.0f/60.0f);
    }};}

namespace {
namespace sim=runner::sim; namespace rl=runner::rl; namespace loco=runner::locomotion;
void require(bool v,std::string_view m){if(v)return;std::cerr<<"Runner v0.7.32 failure: "<<m<<'\n';std::exit(EXIT_FAILURE);}
bool same(const sim::ShuttleState&a,const sim::ShuttleState&b)noexcept{
 return a.phase==b.phase&&a.facing_direction==b.facing_direction
 &&a.locomotion_direction==b.locomotion_direction&&a.phase_origin_x==b.phase_origin_x
 &&a.phase_seconds==b.phase_seconds&&a.completed_turns==b.completed_turns;}
sim::ShuttleState turn(float dt,int n){sim::ShuttleState s{};s.phase=sim::ShuttlePhase::turning;
 s.locomotion_direction=0.0f;for(int i=0;i<n;++i)s=sim::advance_shuttle_state(s,13.25f,dt);return s;}
std::vector<float> fingerprint(const sim::Environment&e){std::vector<float>r;
 for(const sim::CourseFeature&f:e.course_features())r.insert(r.end(),{static_cast<float>(f.kind),
 f.center.x,f.center.y,f.half_extent.x,f.half_extent.y,f.radius,static_cast<float>(f.marker_sequence)});return r;}
}

int main(){
 sim::ShuttleState stalled_backup{};stalled_backup.phase=sim::ShuttlePhase::backing;
 stalled_backup.locomotion_direction=-1.0f;stalled_backup.phase_origin_x=5.0f;
 stalled_backup=sim::advance_shuttle_state(stalled_backup,5.0f,
  sim::shuttle_backup_timeout_seconds+0.01f);
 require(stalled_backup.phase==sim::ShuttlePhase::turning
  &&stalled_backup.locomotion_direction==0.0f,"bounded stalled backup");
 sim::ShuttleState s{};
 s=sim::advance_shuttle_state(s,sim::shuttle_right_boundary,1.0f/60.0f);
 require(s.phase==sim::ShuttlePhase::braking&&s.facing_direction==1.0f
  &&s.locomotion_direction==0.0f,"right boundary brake");
 s=sim::advance_shuttle_state(s,sim::shuttle_right_boundary,sim::shuttle_brake_seconds-0.01f);
 require(s.phase==sim::ShuttlePhase::braking,"early brake completion");
 s=sim::advance_shuttle_state(s,sim::shuttle_right_boundary,0.02f,
  sim::shuttle_brake_speed+0.01f);
 require(s.phase==sim::ShuttlePhase::braking,"moving brake completion");
 s=sim::advance_shuttle_state(s,sim::shuttle_right_boundary,0.02f,
  sim::shuttle_brake_speed-0.01f);
 require(s.phase==sim::ShuttlePhase::backing&&s.locomotion_direction==-1.0f,
  "brake-to-backup");
 s=sim::advance_shuttle_state(s,sim::shuttle_right_boundary-sim::shuttle_backup_distance+0.01f,1.0f/60.0f);
 require(s.phase==sim::ShuttlePhase::backing,"early backup completion");
 s=sim::advance_shuttle_state(s,sim::shuttle_right_boundary-sim::shuttle_backup_distance-0.01f,1.0f/60.0f);
 require(s.phase==sim::ShuttlePhase::turning&&s.locomotion_direction==0.0f,"backup-to-turn");
 const sim::ShuttleState invalid=s;
 s=sim::advance_shuttle_state(s,13.25f,sim::shuttle_turn_seconds-0.01f);
 require(s.phase==sim::ShuttlePhase::turning,"early turn completion");
 s=sim::advance_shuttle_state(s,13.25f,0.02f);
 require(s.phase==sim::ShuttlePhase::traverse&&s.facing_direction==-1.0f
  &&s.locomotion_direction==-1.0f&&s.completed_turns==1u,"full-rig left turn");
 s=sim::advance_shuttle_state(s,sim::shuttle_left_boundary,1.0f/60.0f);
 require(s.phase==sim::ShuttlePhase::braking&&s.facing_direction==-1.0f
  &&s.locomotion_direction==0.0f,"left boundary brake");
 s=sim::advance_shuttle_state(s,sim::shuttle_left_boundary,sim::shuttle_brake_seconds+0.01f);
 require(s.phase==sim::ShuttlePhase::backing&&s.locomotion_direction==1.0f,
  "left brake-to-backup");
 s=sim::advance_shuttle_state(s,sim::shuttle_left_boundary+sim::shuttle_backup_distance+0.01f,1.0f/60.0f);
 s=sim::advance_shuttle_state(s,-3.25f,sim::shuttle_turn_seconds+0.01f);
 require(s.phase==sim::ShuttlePhase::traverse&&s.facing_direction==1.0f
  &&s.completed_turns==2u,"symmetric repeated shuttle");

 require(same(sim::advance_shuttle_state(invalid,std::numeric_limits<float>::quiet_NaN(),1.0f/60.0f),invalid)
  &&same(sim::advance_shuttle_state(invalid,0.0f,std::numeric_limits<float>::infinity()),invalid)
  &&same(sim::advance_shuttle_state(invalid,0.0f,-1.0f),invalid),"adversarial input");
 require(same(turn(1.0f/60.0f,24),turn(1.0f/60.0f,24)),"repeated determinism");
 for(const sim::ShuttleState c:{turn(1.0f/20.0f,8),turn(1.0f/60.0f,24),turn(1.0f/240.0f,96)})
  require(c.phase==sim::ShuttlePhase::traverse&&c.facing_direction==-1.0f
   &&c.completed_turns==1u,"20/60/240 Hz turn equivalence");

 require(sim::accepted_directed_odometer_progress(0.02f,1.0f/60.0f,1.0f)==0.02f
  &&sim::accepted_directed_odometer_progress(-0.02f,1.0f/60.0f,-1.0f)==0.02f
  &&sim::accepted_directed_odometer_progress(0.02f,1.0f/60.0f,-1.0f)==0.0f
  &&sim::accepted_directed_odometer_progress(0.02f,1.0f/60.0f,0.0f)==0.0f
  &&sim::accepted_directed_odometer_progress(1.0f,1.0f/60.0f,1.0f)==0.0f,"odometer rejection");

 loco::Signals signals{};signals.uprightness=1.0f;signals.left_supported=true;signals.right_supported=true;
 signals.left_support_x=-0.2f;signals.right_support_x=0.2f;signals.requested_direction=-1.0f;
 signals.near_rise=0.20f;
 require(loco::plan(signals).direction==-1.0f&&loco::plan(signals).step_up,
  "left locomotion and step-up intent");
 signals.turning=true;const loco::Plan held=loco::plan(signals);
 require(held.intent==loco::Intent::hold&&held.direction==0.0f&&held.brake
  &&held.cadence_hz==0.0f,"turn gait suppression");

 signals.turning=false;signals.near_rise=0.0f;signals.gait_cycles=8u;
 signals.dynamic_hazard_active=true;signals.dynamic_hazard_safe=false;
 const loco::Plan hazard_hold=loco::plan(signals);
 require(hazard_hold.intent==loco::Intent::hold&&hazard_hold.direction==0.0f
  &&hazard_hold.brake,"unsafe active granular hazard was traversed");
 signals.dynamic_hazard_safe=true;
 const loco::Plan safe_active=loco::plan(signals);
 require(safe_active.intent==loco::Intent::walk&&safe_active.direction==-1.0f
  &&!safe_active.brake,"safe active granular terrain suppressed traversal");
 for(int repeat=0;repeat<4;++repeat)
  require(loco::plan(signals).intent==loco::Intent::walk,
   "safe active granular plan was not deterministic");
 signals.dynamic_hazard_active=false;
 require(loco::plan(signals).intent==loco::Intent::walk,
  "settled granular terrain did not resume traversal");
 const auto ka=rl::solve_two_link_sagittal(1.0f,1.0f,{-0.25f,-1.6f},1.0f);
 const auto kb=rl::solve_two_link_sagittal(1.0f,1.0f,{0.25f,-1.6f},1.0f);
 const auto ma=rl::solve_two_link_sagittal(1.0f,1.0f,{0.25f,-1.6f},-1.0f);
 const auto mb=rl::solve_two_link_sagittal(1.0f,1.0f,{-0.25f,-1.6f},-1.0f);
 require(ka.valid&&kb.valid&&ka.upper.x>0.0f&&kb.upper.x>0.0f
  &&ma.valid&&mb.valid&&ma.upper.x<0.0f&&mb.upper.x<0.0f,"same-facing knees");
 const runner::Vec2 authored_hand{0.12f,-1.45f};
 const runner::Vec2 lead=rl::authored_opposed_swing_target(authored_hand,runner::pi*0.5f,0.405f,0.0f,1.0f);
 const runner::Vec2 trail=rl::authored_opposed_swing_target(authored_hand,runner::pi*1.5f,0.405f,0.0f,1.0f);
 const runner::Vec2 mirror=rl::authored_opposed_swing_target(authored_hand,runner::pi*0.5f,0.405f,0.0f,-1.0f);
 require(lead.x>authored_hand.x+0.38f&&trail.x<authored_hand.x-0.38f
  &&mirror.x<authored_hand.x-0.38f&&lead.y==authored_hand.y&&trail.y==authored_hand.y,
  "authored-rest phase-opposed arm gait");
 require(rl::crouch_teacher_authority(181u)>0.10f
  &&rl::crouch_teacher_authority(rl::crouch_teacher_handoff_update-1u)>0.0f
  &&rl::crouch_teacher_authority(rl::crouch_teacher_handoff_update)==0.0f,"crouch handoff");

 sim::Environment outbound{sim::CreatureBlueprint::humanoid(),0x7320u};
 outbound.set_course(sim::CourseStage::shuttle,0.30f);outbound.set_course_motion_enabled(true);
 require(outbound.course_speed()==0.0f&&outbound.course_progress()==0.0f,
  "shuttle lesson inherited a moving course frame");
 outbound.set_course_motion_enabled(false);
 require(outbound.shuttle_enabled()&&outbound.course_features().empty(),"early obstacles");
 require(!sim::shuttle_dynamic_course_ready(1u,14u,2.0f)
  &&!sim::shuttle_dynamic_course_ready(2u,13u,2.0f)
  &&!sim::shuttle_dynamic_course_ready(2u,14u,1.99f)
  &&sim::shuttle_dynamic_course_ready(2u,14u,2.0f),
  "complete baseline readiness boundaries");
 sim::EnvironmentTestAccess::ready(outbound,14u,2.0f);sim::EnvironmentTestAccess::rebuild(outbound);
 require(outbound.course_features().empty(),"clean first traversal");
 sim::ShuttleState returning{};returning.facing_direction=-1.0f;returning.locomotion_direction=-1.0f;
 returning.completed_turns=2u;sim::EnvironmentTestAccess::shuttle(outbound,returning);
 sim::EnvironmentTestAccess::root_x(outbound,9.5f);sim::EnvironmentTestAccess::rebuild(outbound);
 require(outbound.course_features().size()==2u&&outbound.course_features()[0].center.x<9.5f
  &&outbound.course_features()[1].center.x<outbound.course_features()[0].center.x,"return features");
 const std::vector<float> expected=fingerprint(outbound);
 sim::Environment repeated{sim::CreatureBlueprint::humanoid(),0x7320u};
 repeated.set_course(sim::CourseStage::shuttle,0.30f);repeated.set_course_motion_enabled(false);
 sim::EnvironmentTestAccess::ready(repeated,14u,2.0f);
 sim::EnvironmentTestAccess::shuttle(repeated,returning);
 sim::EnvironmentTestAccess::root_x(repeated,9.5f);sim::EnvironmentTestAccess::rebuild(repeated);
 require(fingerprint(repeated)==expected,"repeated-seed features");
 sim::ShuttleState backing{};backing.phase=sim::ShuttlePhase::backing;backing.locomotion_direction=-1.0f;
 sim::EnvironmentTestAccess::shuttle(outbound,backing);sim::EnvironmentTestAccess::rebuild(outbound);
 require(outbound.course_features().empty(),"reverse cleanup");

 outbound.set_course(sim::CourseStage::balance,0.25f);
 require(outbound.course_features().empty()&&!outbound.shuttle_enabled(),"lesson cleanup");

 constexpr std::array factories{&sim::CreatureBlueprint::chicken,&sim::CreatureBlueprint::biped,
  &sim::CreatureBlueprint::humanoid,&sim::CreatureBlueprint::quadruped,&sim::CreatureBlueprint::crawler4,
  &sim::CreatureBlueprint::hexapod,&sim::CreatureBlueprint::monoped};
 for(std::size_t i=0;i<factories.size();++i){sim::Environment e{factories[i](),0x732100u+i};
  e.set_course(sim::CourseStage::shuttle,0.30f);e.set_course_motion_enabled(false);
  require(e.shuttle_enabled()&&e.facing_direction()==1.0f&&e.locomotion_direction()==1.0f,"all-rig shuttle");}
 sim::Environment reflected{sim::CreatureBlueprint::humanoid(),0x740419u};
 reflected.set_course(sim::CourseStage::shuttle,0.30f);
 reflected.set_course_motion_enabled(false);
 const auto right_observations=reflected.observation();
 const std::vector<sim::Particle> right_particles{
  reflected.particles().begin(),reflected.particles().end()};
 const float reflected_root_x=right_particles[reflected.blueprint().root_node].position.x;
 sim::ShuttleState reflected_return{};
 reflected_return.facing_direction=-1.0f;
 reflected_return.locomotion_direction=-1.0f;
 reflected_return.completed_turns=1u;
 sim::EnvironmentTestAccess::shuttle(reflected,reflected_return);
 const auto left_observations=reflected.observation();
 for(std::size_t node=0;node<right_particles.size();++node){
  const runner::Vec2 right_offset=right_particles[node].position
   - runner::Vec2{reflected_root_x,0.0f};
  const runner::Vec2 left_offset=reflected.particles()[node].position
   - runner::Vec2{reflected_root_x,0.0f};
  require(std::abs(right_offset.x+left_offset.x)<1.0e-5f
   &&std::abs(right_offset.y-left_offset.y)<1.0e-5f,
   "turn state did not physically reflect the articulated plant");}
 for(std::size_t index=0;index<right_observations.size();++index){
  if(index==44u)continue;
  require(std::abs(right_observations[index]-left_observations[index])<1.0e-5f,
   "reflected plant changed a facing-local controller observation");}
 require(right_observations[44]==1.0f&&left_observations[44]==-1.0f,
  "reflected observation omitted explicit world facing");
 sim::Environment symmetry_right{sim::CreatureBlueprint::humanoid(),0x740420u};
 sim::Environment symmetry_left{sim::CreatureBlueprint::humanoid(),0x740420u};
 symmetry_right.set_course(sim::CourseStage::shuttle,0.30f);
 symmetry_left.set_course(sim::CourseStage::shuttle,0.30f);
 symmetry_right.set_course_motion_enabled(false);
 symmetry_left.set_course_motion_enabled(false);
 sim::ShuttleState symmetry_return{};
 symmetry_return.facing_direction=-1.0f;
 symmetry_return.locomotion_direction=-1.0f;
 symmetry_return.completed_turns=1u;
 sim::EnvironmentTestAccess::shuttle(symmetry_left,symmetry_return);
 const float symmetry_right_origin=symmetry_right.particles()[symmetry_right.blueprint().root_node].position.x;
 const float symmetry_left_origin=symmetry_left.particles()[symmetry_left.blueprint().root_node].position.x;
 float maximum_action_mismatch{};
 float maximum_pose_mismatch{};
 constexpr float mirrored_pose_epsilon=5.0e-4f; // Cross-toolchain FMA drift stays sub-millimeter.
 int first_pose_mismatch=-1;
 for(int step=0;step<45;++step){
  const auto right_action=rl::walking_teacher_action(symmetry_right);
  const auto left_action=rl::walking_teacher_action(symmetry_left);
  for(std::size_t index=0;index<right_action.size();++index)
   maximum_action_mismatch=std::max(maximum_action_mismatch,
    std::abs(right_action[index]-left_action[index]));
  const sim::StepResult right_result=symmetry_right.step(right_action);
  const sim::StepResult left_result=symmetry_left.step(left_action);
  const float right_root=symmetry_right.particles()[symmetry_right.blueprint().root_node].position.x;
  const float left_root=symmetry_left.particles()[symmetry_left.blueprint().root_node].position.x;
  float frame_mismatch=std::abs((right_root-symmetry_right_origin)
   +(left_root-symmetry_left_origin));
  for(std::size_t node=0;node<symmetry_right.particles().size();++node){
   const sim::Particle& rp=symmetry_right.particles()[node];
   const sim::Particle& lp=symmetry_left.particles()[node];
   frame_mismatch=std::max(frame_mismatch,std::abs((rp.position.x-right_root)
    +(lp.position.x-left_root)));
   frame_mismatch=std::max(frame_mismatch,std::abs(rp.position.y-lp.position.y));}
  maximum_pose_mismatch=std::max(maximum_pose_mismatch,frame_mismatch);
  if(first_pose_mismatch<0&&frame_mismatch>mirrored_pose_epsilon)first_pose_mismatch=step;
  if(right_result.terminated||left_result.terminated)break;}
 if(!(maximum_action_mismatch<1.0e-5f&&maximum_pose_mismatch<mirrored_pose_epsilon
  &&first_pose_mismatch<0&&symmetry_right.invalid_reason()==sim::InvalidMotion::none
  &&symmetry_left.invalid_reason()==sim::InvalidMotion::none))
  std::cerr<<"symmetry action="<<maximum_action_mismatch
   <<" pose="<<maximum_pose_mismatch<<" first="<<first_pose_mismatch
   <<" right-invalid="<<static_cast<int>(symmetry_right.invalid_reason())
   <<" left-invalid="<<static_cast<int>(symmetry_left.invalid_reason())<<'\n';
 require(maximum_action_mismatch<1.0e-5f&&maximum_pose_mismatch<mirrored_pose_epsilon
  &&first_pose_mismatch<0&&symmetry_right.invalid_reason()==sim::InvalidMotion::none
  &&symmetry_left.invalid_reason()==sim::InvalidMotion::none,
  "facing-local teacher and physical plant diverged under reflection");
 require(sim::directional_backward_brace_ratio({0.0f,1.0f},{-0.32f,0.95f},-1.0f)
   <sim::backward_brace_activation_ratio
  &&sim::directional_backward_brace_ratio({0.0f,1.0f},{0.32f,0.95f},-1.0f)
   >sim::backward_brace_activation_ratio,
  "return acceptance cannot distinguish forward posture from backpedal bracing");
 const auto humanoid_reflex=rl::topology_runtime_reflex_authority(
  sim::CreatureBlueprint::humanoid(),sim::CourseStage::shuttle);
 const auto quadruped_reflex=rl::topology_runtime_reflex_authority(
  sim::CreatureBlueprint::quadruped(),sim::CourseStage::shuttle);
 require(humanoid_reflex.support>=0.90f&&humanoid_reflex.body>=0.70f
  &&quadruped_reflex.support>=0.90f&&quadruped_reflex.body>=0.58f,
  "post-handoff shuttle omitted the topology direction reflex");
 sim::Environment post_handoff{sim::CreatureBlueprint::humanoid(),0x739419u};
 post_handoff.set_course(sim::CourseStage::shuttle,0.30f);
 post_handoff.set_course_motion_enabled(true);
 sim::ShuttleState post_handoff_return{};
 post_handoff_return.facing_direction=-1.0f;
 post_handoff_return.locomotion_direction=-1.0f;
 post_handoff_return.completed_turns=1u;
 sim::EnvironmentTestAccess::shuttle(post_handoff,post_handoff_return);
 sim::EnvironmentTestAccess::root_x(post_handoff,6.0f);
 const float return_origin=post_handoff.particles()[post_handoff.blueprint().root_node].position.x;
 std::array<float,sim::action_count> forward_specialized{};
 forward_specialized.fill(0.85f);
 for(int step=0;step<300;++step){
  const auto action=rl::effective_policy_action(post_handoff,forward_specialized,
   sim::CourseStage::shuttle,0.0f);
  if(post_handoff.step(action).terminated)break;}
 const float return_x=post_handoff.particles()[post_handoff.blueprint().root_node].position.x;
 require(post_handoff.invalid_reason()==sim::InvalidMotion::none
  &&return_x<return_origin-1.0f&&post_handoff.course_progress()==0.0f
  &&post_handoff.gait_cycles()>=2u
  &&post_handoff.maximum_backward_brace_seconds()
   <=sim::sustained_backward_brace_limit_seconds,
  "post-handoff return moved left while backpedaling or without a real gait");

 sim::Environment physical{sim::CreatureBlueprint::biped(),0x7323u};
 physical.set_course(sim::CourseStage::shuttle,0.30f);physical.set_course_motion_enabled(false);
 sim::ShuttlePhase prior_phase=physical.shuttle_phase();
 sim::StepResult physical_result{};
 for(int step=0;step<2400;++step){
  physical_result=physical.step(rl::walking_teacher_action(physical));
  if(physical.shuttle_phase()!=prior_phase)
   prior_phase=physical.shuttle_phase();
  if(physical_result.terminated)break;}
 require(physical.invalid_reason()==sim::InvalidMotion::none
  &&physical.completed_shuttle_turns()>=1u&&physical.distance_travelled()>=12.0f
  &&physical.gait_cycles()>=6u
  &&physical.maximum_backward_brace_seconds()
   <=sim::sustained_backward_brace_limit_seconds,
  "physical teacher shuttle traversed by dragging or backward bracing");
 const auto reverse_humanoid_probe=[](){
  sim::Environment environment{sim::CreatureBlueprint::humanoid(),0x9e3779b9u};
  environment.set_course(sim::CourseStage::shuttle,0.30f);environment.set_course_motion_enabled(false);
  for(int step=0;step<2400;++step){
   if(environment.step(rl::walking_teacher_action(environment)).terminated)break;}
  return std::array<float,6>{environment.distance_travelled(),environment.elapsed_seconds(),
   static_cast<float>(environment.completed_shuttle_turns()),
   static_cast<float>(environment.invalid_reason()),static_cast<float>(environment.gait_cycles()),
   environment.maximum_backward_brace_seconds()};};
 const std::array<float,6> reverse_first=reverse_humanoid_probe();
 const std::array<float,6> reverse_repeated=reverse_humanoid_probe();
 require(reverse_first==reverse_repeated&&reverse_first[0]>=12.0f&&reverse_first[1]>=19.9f
  &&reverse_first[2]>=1.0f&&reverse_first[3]==static_cast<float>(sim::InvalidMotion::none)
  &&reverse_first[4]>=6.0f
  &&reverse_first[5]<=sim::sustained_backward_brace_limit_seconds,
  "repeated return traversal accepted leftward backpedaling");
 sim::Environment equipment{sim::CreatureBlueprint::humanoid(),0x7322u};
 equipment.set_course(sim::CourseStage::shuttle,0.30f);
 equipment.set_course_motion_enabled(false);
 equipment.configure_equipment(sim::WeaponClass::sidearm,4.0f);
 sim::EnvironmentTestAccess::configure_facing_target(equipment,1.0f);
 const float root_x=equipment.particles()[equipment.blueprint().root_node].position.x;
 const runner::Vec2 right_mount=equipment.equipment_mount_position();
 sim::EnvironmentTestAccess::configure_facing_target(equipment,-1.0f);
 const runner::Vec2 mirrored_same_pose_mount=equipment.equipment_mount_position();
 require(std::abs((right_mount.x-root_x)+(mirrored_same_pose_mount.x-root_x))<1.0e-5f
  &&std::abs(right_mount.y-mirrored_same_pose_mount.y)<1.0e-5f,
  "equipment mount did not follow exact whole-rig facing reflection");
 sim::EnvironmentTestAccess::configure_facing_target(equipment,1.0f);
 sim::EnvironmentTestAccess::drive_equipment(equipment,90);
 require(equipment.shots_fired()>0u&&equipment.target_hits()>0u,
  "right-facing fixed-step equipment did not fire and hit");
 equipment.configure_equipment(sim::WeaponClass::carbine,4.0f);
 sim::EnvironmentTestAccess::configure_facing_target(equipment,-1.0f,6.0f);
 sim::EnvironmentTestAccess::drive_equipment(equipment,1);
 require(std::abs(std::abs(equipment.equipment_aim_angle())-runner::pi)<0.05f,
  "left-facing equipment did not initially aim along mirrored facing");
 sim::EnvironmentTestAccess::drive_equipment(equipment,89);
 require(equipment.shots_fired()>0u&&equipment.target_hits()>0u,
  "left-facing fixed-step equipment did not mirror aim, fire, and hit");
 equipment.configure_equipment(sim::WeaponClass::none,4.0f);
 sim::EnvironmentTestAccess::configure_facing_target(equipment,1.0f);
 sim::EnvironmentTestAccess::drive_equipment(equipment,90);
 require(equipment.shots_fired()==0u&&equipment.equipment_projectiles().empty(),
  "unarmed equipment path created a shot");
 sim::Environment dropped{sim::CreatureBlueprint::humanoid(),0x7330u};
 dropped.set_course(sim::CourseStage::equipment_targets,0.30f);
 dropped.configure_equipment(sim::WeaponClass::launcher,4.0f);
 const runner::Vec2 carried_position=dropped.equipment_display_position();
 require(std::abs(carried_position.x-dropped.equipment_mount_position().x)<1.0e-6f
  &&std::abs(carried_position.y-dropped.equipment_mount_position().y)<1.0e-6f,
  "carried equipment display position diverged from its graph mount");
 dropped.disarm_equipment();
 const runner::Vec2 released_position=dropped.equipment_display_position();
 sim::EnvironmentTestAccess::coast_equipment(dropped,12);
 const runner::Vec2 fallen_weapon=dropped.equipment_display_position();
 require(dropped.equipment_state()==sim::EquipmentState::disarmed
  &&std::abs(released_position.x-carried_position.x)<1.0e-6f
  &&std::abs(released_position.y-carried_position.y)<1.0e-6f
  &&fallen_weapon.y<released_position.y,
  "dropped/disarmed equipment has no visible fixed-step world trajectory"); rl::PpoTrainer preview_equipment{sim::CreatureBlueprint::humanoid(),8u,false};
 preview_equipment.configure_preview_equipment(sim::WeaponClass::sidearm,3.0f);
 for(int frame=0;frame<45;++frame)preview_equipment.step_preview(1.0f/60.0f);
 require(preview_equipment.preview().weapon_class()==sim::WeaponClass::sidearm
  &&preview_equipment.preview().shots_fired()>0u
  &&preview_equipment.preview().target_hits()>0u,
  "Rig Lab preview equipment selector did not drive real firing physics"); std::cout<<"Runner v0.7.32 shuttle/facing tests passed\n";return EXIT_SUCCESS;
}