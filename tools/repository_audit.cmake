if(NOT DEFINED RUNNER_SOURCE_DIR)
    message(FATAL_ERROR "RUNNER_SOURCE_DIR was not provided")
endif()

foreach(required IN ITEMS
        AGENTS.md CHANGELOG.md missioncache.md README.md
        docs/SANDHYBRID_INTEGRATION_BRIDGE.md
        docs/RUNNER_V0718_RUNTIME_RECOVERY.md
        docs/RUNNER_V0719_GENERAL_LOCOMOTION.md
        docs/RUNNER_V0720_UI_PREVIEW_ICON.md
        docs/RUNNER_V0721_READABLE_TELEMETRY.md
        docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md
        docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md
        docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md
        docs/RUNNER_V0725_ART_LEG_HOTFIX.md
        docs/RUNNER_V0726_TRAINING_TRUTH.md
        docs/RUNNER_V0727_RIG_TRAINING_EVIDENCE.md
        docs/RUNNER_V0728_COURSE_COMPLETION.md
        docs/RUNNER_V0729_MODULAR_ART_REMAKE.md
        docs/RUNNER_V0730_SUSTAINED_WALK_RECOVERY.md
        docs/RUNNER_V0731_ACTIVE_TERRAIN_CURRICULUM_ART.md
        docs/RUNNER_V0732_SHUTTLE_FACING_HANDS.md
        tests/v0725_art_leg_hotfix_tests.cpp
        tests/v0726_training_truth_tests.cpp
        tests/v0730_cold_start_tests.cpp
        tests/v0731_crouch_learning_tests.cpp
        tests/v0732_shuttle_facing_tests.cpp
        tests/v0728_course_completion_tests.cpp
        tests/v0729_modular_art_tests.cpp
        src/course_completion_diagnostic.cpp
        src/course_completion_diagnostic.hpp
        assets/optional/runner_armor_concepts/runtime/foot_side.ppm
        assets/optional/runner_armor_concepts/runtime/forearm_side.ppm
        assets/optional/runner_armor_concepts/runtime/hand_side.ppm
        assets/optional/runner_armor_concepts/runtime/helmet_side.ppm
        assets/optional/runner_armor_concepts/runtime/shin_side.ppm
        assets/optional/runner_armor_concepts/runtime/thigh_side.ppm
        assets/optional/runner_armor_concepts/runtime/torso_side.ppm
        assets/optional/runner_armor_concepts/runtime/upper_arm_side.ppm
        assets/optional/runner_armor_concepts/runtime/weapon_side.ppm
        assets/ui/runner_icon_source.rgba.zlib.b64
        tools/generate_runner_icon.py
        tools/generate_runner_armor_assets.py
        tools/art_sources/runner_v0729_modular_atlas.png
        tools/art_sources/runner_user_modular_sheet.png
        tests/v0718_runtime_recovery_tests.cpp
        tests/v0719_general_locomotion_tests.cpp
        tests/v0720_ui_tests.cpp
        tests/v0721_readable_telemetry_tests.cpp
        tests/v0721_rig_gait_tests.cpp
        tests/v0723_gray_frame_tests.cpp
        tests/v0724_structural_metrics_icon_tests.cpp
        src/locomotion_strategy.hpp
        src/preview_sync.hpp
        src/training_explainer.hpp
        src/ui_render_contract.hpp
        src/ui_frame_probe.hpp
        src/runner_icon.rc.in
        src/ui_layout.hpp)
    if(NOT EXISTS "${RUNNER_SOURCE_DIR}/${required}")
        message(FATAL_ERROR "Missing required repository file: ${required}")
    endif()
endforeach()

if(EXISTS "${RUNNER_SOURCE_DIR}/assets/ui/runner_icon_source.png")
    message(FATAL_ERROR "Corrupted legacy screenshot PNG remains in source tree")
endif()

file(READ "${RUNNER_SOURCE_DIR}/CMakeLists.txt" cmake_text)
foreach(reference IN ITEMS
        "project(Runner VERSION 0.7.32 LANGUAGES CXX)"
        "generate_runner_icon.py"
        "runner_icon_source.png"
        "runner_icon_source.sha256"
        "RunnerV0724StructuralMetricsIconTests"
        "RunnerV0725ArtLegHotfixTests"
        "RunnerV0726TrainingTruthTests"
        "RunnerV0730ColdStartTests"
        "RunnerV0731CrouchLearningTests"
        "Runner.V0731CrouchLearning"
        "RunnerV0732ShuttleFacingTests"
        "Runner.V0730ReferenceFrame"
        "COMMAND RunnerV0730ColdStartTests --references"
        "COMMAND RunnerV0730ColdStartTests --learner"
        "set_tests_properties(Runner.V0730ReferenceFrame PROPERTIES TIMEOUT 1800)"
        "set_tests_properties(Runner.V0730ColdStart PROPERTIES TIMEOUT 1800)"
        "RunnerV0728CourseCompletionTests"
        "RunnerV0729ModularArtTests"
        "Runner.ArtDiagnostic"
        "RUNNER_V0725_ART_LEG_HOTFIX.md"
        "RUNNER_V0726_TRAINING_TRUTH.md"
        "RUNNER_V0727_RIG_TRAINING_EVIDENCE.md"
        "RUNNER_V0728_COURSE_COMPLETION.md"
        "RUNNER_V0729_MODULAR_ART_REMAKE.md"
        "RUNNER_V0730_SUSTAINED_WALK_RECOVERY.md"
        "RUNNER_V0731_ACTIVE_TERRAIN_CURRICULUM_ART.md"
        "RUNNER_V0732_SHUTTLE_FACING_HANDS.md"
        "RUNNER_V0724_STRUCTURAL_METRICS_ICON.md"
        "runner_icon.rc")
    string(FIND "${cmake_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "CMake/versioned package contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/missioncache.md" mission_text)
foreach(reference IN ITEMS
        "WALK-SCREENSHOT-ICON-296"
        "WALK-BONE-LENGTH-297"
        "WALK-LOAD-BEARING-298"
        "WALK-AUTO-STIFFNESS-299"
        "WALK-DEBUG-TRUTH-300"
        "WALK-PROGRESS-301"
        "WALK-TOTALS-302"
        "WALK-VISUAL-303"
        "WALK-STATE-304"
        "WALK-REGRESSION-305"
        "WALK-RELEASE-306"
        "WALK-COMPACT-ARMOR-307"
        "WALK-STANCE-EXTENSION-308"
        "WALK-CHAIN-IK-309"
        "WALK-STARTUP-310"
        "WALK-STATE-311"
        "WALK-REGRESSION-312"
        "WALK-RELEASE-313"
        "WALK-RIG-ROLE-314"
        "WALK-RIG-ROLE-315"
        "WALK-RIG-RESET-316"
        "WALK-RIG-RETRY-317"
        "WALK-PREVIEW-318"
        "WALK-PREVIEW-319"
        "WALK-TELEMETRY-320"
        "WALK-ART-321"
        "WALK-STATE-322"
        "WALK-REGRESSION-323"
        "WALK-RELEASE-324"
        "WALK-RIG-REJECTION-325"
        "WALK-RIG-EVALUATION-326"
        "WALK-RIG-DIAGNOSTIC-327"
        "WALK-WINDOWS-CONSTEXPR-328"
        "WALK-PIPELINE-TIMING-329"
        "WALK-RIG-STATE-330"
        "WALK-RIG-DOC-331"
        "WALK-RIG-RELEASE-332"
        "WALK-COURSE-334"
        "WALK-MATERIAL-335"
        "WALK-TERRAIN-RENDER-336"
        "WALK-MATERIAL-TRUTH-337"
        "WALK-DEBUG-338"
        "WALK-FRAME-339"
        "WALK-CARRIED-340"
        "WALK-CARRIED-341"
        "WALK-COURSE-DIAGNOSTIC-342"
        "WALK-STATE-DOC-343"
        "WALK-RELEASE-344"
        "WALK-ART-REMAKE-345"
        "WALK-ART-RIG-346"
        "WALK-ART-KEY-347"
        "WALK-ART-REGRESSION-348"
        "WALK-ART-ORTHO-349"
        "WALK-RELEASE-350"
        "WALK-UPDATE-FLOW-351"
        "WALK-LEARNABLE-GAIT-352"
        "WALK-INCREMENTAL-RETENTION-353"
        "WALK-RESET-CONVERGENCE-354"
        "WALK-CURRICULUM-355"
        "WALK-COLD-START-356"
        "WALK-RELEASE-357"
        "WALK-MULTI-HANDOFF-358"
        "WALK-HUMANOID-COLD-359"
        "WALK-LESSON-CLOCK-360"
        "WALK-CROUCH-OWNERSHIP-361"
        "WALK-LAUNCH-CONTACT-362"
        "WALK-ACTIVE-TERRAIN-363"
        "WALK-ART-TRANSFORM-364"
        "WALK-ART-ALL-RIGS-365"
        "WALK-RIG-IDENTITY-366"
        "WALK-FRAME-TRUTH-367"
        "WALK-RELEASE-368"
        "WALK-EVOLVING-RIG-369"
        "WALK-CROUCH-HINGE-380"
        "WALK-FACING-GAIT-381"
        "WALK-HAND-ART-382"
        "WALK-SHUTTLE-COURSE-383"
        "WALK-RELEASE-384")
    string(FIND "${mission_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Mission cache continuity contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/simulation.cpp" simulation_text)
foreach(reference IN ITEMS
        "project_structure_rigid"
        "primary_leg_segment"
        "maximum_bone_length_error_ratio"
        "InvalidMotion::structural_compression"
        "chain_convergence_passes = 16"
        "structural_error > 0.020f"
        "advance_shuttle_state"
        "accepted_directed_odometer_progress"
        "rebuild_course_features"
        "ShuttlePhase::turning")
    string(FIND "${simulation_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Walking-leg rigidity contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/autonomy_curriculum.cpp" curriculum_text)
string(FIND "${curriculum_text}" "RigMutationKind::structural_stiffness" stiffness_mutation_pos)
if(NOT stiffness_mutation_pos EQUAL -1)
    message(FATAL_ERROR "Automatic structural-stiffness mutation remains enabled")
endif()
foreach(reference IN ITEMS
        "rig_optimization_candidate"
        "RigOptimizationMode::morphology_evolve"
        "evolve_rig_candidate(source, generation)"
        "automatic_rig_tuning_candidate(source, generation)"
        "optimization_mode_"
        "constexpr std::size_t agents = 6"
        "environment.step(raw_action)"
        "rig_complexity_cost(candidate)")
    string(FIND "${curriculum_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Explicit rig optimization routing missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/autonomy_persistence.cpp" autonomy_persistence_text)
foreach(reference IN ITEMS
        "RUNAUTONOMY 17"
        "version != 16 && version != 17"
        "job.optimization_mode"
        "command.optimization_mode"
        "RigOptimizationMode::control_optimize"
        "RigOptimizationMode::morphology_evolve")
    string(FIND "${autonomy_persistence_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Rig-scoped optimization persistence missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/ppo.hpp" ppo_text)
foreach(reference IN ITEMS
        "training_semantics_version = 0x0007'3201u"
        "lesson_teacher_authority"
        "crouch_teacher_handoff_update"
        "lesson_update() const noexcept"
        "completed_episode_passes_stage_checks"
        "foundational_walk_teacher_handoff_update"
        "guided_rollout_imitation_weight"
        "strict_evaluation_quality_bit"
        "incremental_locomotion_candidate")
    string(FIND "${ppo_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Training semantics contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/ppo_trainer.cpp" trainer_text)
foreach(reference IN ITEMS
        "completed_episode_passes_stage_checks"
        "transition.guided_action = guided"
        "accumulate_imitation_gradient"
        "foundational_walk_teacher_handoff_update")
    string(FIND "${trainer_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.31 learner contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/deformable_terrain.hpp" terrain_text)
foreach(reference IN ITEMS
        "launch_pad_half_width = 0.70f"
        "launch_transition_width = 0.55f"
        "deformation_scale = 0.06f"
        "TerrainRegion::shallow_water"
        "TerrainRegion::hole")
    string(FIND "${terrain_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.31 active-terrain contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/training_explainer.hpp" explainer_text)
foreach(reference IN ITEMS
        "training_work"
        "result.training_work * 0.80f + result.mastery * 0.20f"
        "PASSED STAGE CHECKS")
    string(FIND "${explainer_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Truthful telemetry contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/app.cpp" app_text)
foreach(reference IN ITEMS
        "LESSON COMPLETION"
        "MASTERY PASSES"
        "PASSED STAGE CHECKS"
        "FAILED STAGE CHECKS"
        "FEATURES CLEARED"
        "runner-v0732-shuttle-autosave.eppo"
        "PACKAGED COURSE EYE TEST"
        "ORTHOGRAPHIC ART CHECK"
        "STRICT SIDE ELEVATION - NO PERSPECTIVE OR FORESHORTENING"
        "RETAINED WALK PROOF - ZERO AUTHORITY"
        "run_walk_eye_test_proof"
        "draw_body_segments"
        "draw_fitted_armor"
        "SkinEnvelopeDimensions"
        "Modular armor is the exclusive presentation"
        "draw_oriented_pixel_art"
        "optional_upper_arm_art"
        "optional_forearm_art"
        "optional_hand_art"
        "CONTROL OPTIMIZE"
        "MORPHOLOGY EVOLVE"
        "set_rig_optimization_mode"
        "optional_thigh_art"
        "optional_shin_art")
    string(FIND "${app_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.28 application contract missing: ${reference}")
    endif()
endforeach()
string(FIND "${app_text}" "shoulder_cap_radius" legacy_shoulder_cap_pos)
if(NOT legacy_shoulder_cap_pos EQUAL -1)
    message(FATAL_ERROR "procedural shoulder-cap presentation remains under modular armor")
endif()
file(READ "${RUNNER_SOURCE_DIR}/src/renderer.hpp" renderer_header_text)
string(FIND "${renderer_header_text}" "maximum_frame_vertex_bytes" art_budget_pos)
if(art_budget_pos EQUAL -1)
    message(FATAL_ERROR "v0.7.29 shared renderer vertex-budget contract missing")
endif()

file(READ "${RUNNER_SOURCE_DIR}/src/pixel_art.hpp" pixel_art_header_text)
file(READ "${RUNNER_SOURCE_DIR}/src/pixel_art.cpp" pixel_art_source_text)
foreach(reference IN ITEMS
        "transparent_key"
        "chroma_keyed"
        "transparent(Color color)")
    string(FIND "${pixel_art_header_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.29 keyed pixel-art contract missing: ${reference}")
    endif()
endforeach()
foreach(reference IN ITEMS "magenta_key" "corner_indices" "loaded.transparent_key")
    string(FIND "${pixel_art_source_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.29 chroma-key detection missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/tools/generate_runner_armor_assets.py" armor_generator_text)
foreach(reference IN ITEMS
        "EXPECTED_SOURCE_SIZE = (1403, 1121)"
        "upper_arm_side.ppm"
        "forearm_side.ppm"
        "thigh_side.ppm"
        "shin_side.ppm"
        "KEY = (255, 0, 255)")
    string(FIND "${armor_generator_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.29 deterministic armor generator missing: ${reference}")
    endif()
endforeach()

foreach(obsolete_art IN ITEMS
        assets/optional/runner_armor_concepts/PROVENANCE.md
        assets/optional/runner_armor_concepts/runner_armor_concepts.webp
        assets/optional/runner_armor_concepts/source)
    if(EXISTS "${RUNNER_SOURCE_DIR}/${obsolete_art}")
        message(FATAL_ERROR "Obsolete concept-sheet package remains: ${obsolete_art}")
    endif()
endforeach()
file(READ "${RUNNER_SOURCE_DIR}/src/main.cpp" main_text)
string(FIND "${main_text}" "--diagnose-art" art_diagnostic_pos)
if(art_diagnostic_pos EQUAL -1)
    message(FATAL_ERROR "v0.7.29 packaged art-budget diagnostic launch contract missing")
endif()
string(FIND "${main_text}" "--art-eye-test" art_eye_test_pos)
if(art_eye_test_pos EQUAL -1)
    message(FATAL_ERROR "v0.7.29 packaged orthographic art eye-test launch contract missing")
endif()
string(FIND "${main_text}" "--course-eye-test" course_eye_test_pos)
if(course_eye_test_pos EQUAL -1)
    message(FATAL_ERROR "v0.7.28 packaged course eye-test launch contract missing")
endif()
string(FIND "${main_text}" "--walk-eye-test" walk_eye_test_pos)
if(walk_eye_test_pos EQUAL -1)
    message(FATAL_ERROR "v0.7.30 packaged retained-walk eye-test launch contract missing")
endif()
string(FIND "${main_text}" "application.prepare_walk_eye_test(error)" walk_eye_prepare_pos)
string(FIND "${main_text}" "SDL_CreateWindow(" window_create_pos)
if(walk_eye_prepare_pos EQUAL -1 OR window_create_pos EQUAL -1
        OR walk_eye_prepare_pos GREATER window_create_pos)
    message(FATAL_ERROR "v0.7.31 walk eye proof must complete before the Vulkan window is created")
endif()
string(FIND "${app_text}" "Color{}" opaque_default_pos)
if(NOT opaque_default_pos EQUAL -1)
    message(FATAL_ERROR "Opaque default Color remains in application border rendering")
endif()

foreach(reference IN ITEMS
        "font::make_bitmap_font_metrics"
        "TRAINING SAMPLES READY"
        "format_work_counter")
    string(FIND "${app_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.25 readable font/progress contract missing: ${reference}")
    endif()
endforeach()
string(FIND "${app_text}" "ui_font_scale" legacy_font_scale_pos)
if(NOT legacy_font_scale_pos EQUAL -1)
    message(FATAL_ERROR "Legacy bitmap-cell font multiplier remains")
endif()

file(READ "${RUNNER_SOURCE_DIR}/src/ui_font.hpp" font_text)
foreach(reference IN ITEMS
        "130f33fe31d73564a35a622f3bb5ddcc2b5105d5"
        "default_logical_height = 16.0F"
        "make_bitmap_font_metrics"
        "case '%'")
    string(FIND "${font_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "EpochGui font synchronization missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/assets/ui/runner_icon_source.rgba.zlib.b64" source_text)
foreach(reference IN ITEMS
        "WIDTH=320"
        "HEIGHT=320"
        "RGBA_SHA256=6b623661307a430c6ec8cf5689531324dc30249137a7005155fa047592dcb1ad"
        "ZLIB_BASE64_BEGIN"
        "ZLIB_BASE64_END")
    string(FIND "${source_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Exact screenshot pixel source contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/tools/generate_runner_icon.py" generator_text)
foreach(reference IN ITEMS
        "runner_icon_source.rgba.zlib.b64"
        "SOURCE_RGBA_SHA256"
        "source_png_hash"
        "resize_nearest")
    string(FIND "${generator_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Screenshot icon generator contract missing: ${reference}")
    endif()
endforeach()
foreach(forbidden IN ITEMS "rounded_background" "polygon(" "gold =" "cyan =")
    string(FIND "${generator_text}" "${forbidden}" pos)
    if(NOT pos EQUAL -1)
        message(FATAL_ERROR "Synthetic icon drawing remains: ${forbidden}")
    endif()
endforeach()

foreach(reference IN ITEMS
        "draw_oriented_pixel_art(canvas, optional_torso_art"
        "art::support_boot_transform"
        "rig.support_branch_mask(motor)"
        "rig.node_support_mask(index)"
        "prepare_art_diagnostic_rig"
        "prepare_art_fallen_eye_test"
        "optional_art_enabled = impl_->optional_foot_art.loaded()")
    string(FIND "${app_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.27 runtime art contract missing: ${reference}")
    endif()
endforeach()
string(FIND "${simulation_text}" "minimum_stance_ratio" stance_ratio_pos)
string(FIND "${simulation_text}" "solve_chain_ik" chain_ik_pos)
if(stance_ratio_pos EQUAL -1 OR chain_ik_pos EQUAL -1)
    message(FATAL_ERROR "v0.7.25 stance-chain correction is missing")
endif()

foreach(reference IN ITEMS
        "motor_drives_support_branch"
        "clear_totals = false"
        "preview_last_reset_reason")
    string(FIND "${ppo_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.27 PPO contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/simulation.hpp" simulation_header_text)
foreach(reference IN ITEMS
        "course_motion_enabled_"
        "set_course_motion_enabled"
        "authored_foundational_gait_cadence_hz"
        "micro_motion_window")
    string(FIND "${simulation_header_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.27 static-preview contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/rig_training_diagnostic.cpp" rig_training_diagnostic_text)
foreach(reference IN ITEMS
        "updates >= 1200u"
        "retained_probe_invalid_runs == 0u"
        "result.teacher_authority == 0.0f"
        "trainer.best_policy_parameters()"
        "RigCase{ \"humanoid\", sim::CreatureBlueprint::humanoid() }"
        "WalkEyeTestProof run_walk_eye_test_proof"
        "left_air != right_air")
    string(FIND "${rig_training_diagnostic_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.31 cold-start acceptance contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/course_completion_diagnostic.cpp" course_diagnostic_text)
foreach(reference IN ITEMS
        "launch_contact"
        "active_terrain"
        "safe_runway"
        "material_regions"
        "water_and_holes"
        "delayed_material_pressure"
        "equipment_off_identity"
        "same_course_preview"
        "CourseStage::combat_course"
        "frame_independent")
    string(FIND "${course_diagnostic_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.28 course diagnostic contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/training_checkpoint.cpp" checkpoint_text)
foreach(reference IN ITEMS
        "'E', 'P', 'P', 'O', '3', '2'"
        "v0731_checkpoint_magic"
        "v0730_checkpoint_magic"
        "data.lesson_update"
        "v0727_checkpoint_magic"
        "v0727_parameter_count = 8'017u"
        "migrate_v0727_parameters"
        "NEW MATERIAL/EQUIPMENT CHANNELS NEUTRAL")
    string(FIND "${checkpoint_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.28 checkpoint migration contract missing: ${reference}")
    endif()
endforeach()

foreach(reference IN ITEMS
        "TerrainRegion::hole"
        "advanced_material_pressure_ready"
        "limb_crossings_"
        "manipulator_endpoint"
        "update_equipment"
        "apply_water_forces")
    string(FIND "${simulation_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.28 physical course contract missing: ${reference}")
    endif()
endforeach()
string(FIND "${simulation_text}" "append_material_features" legacy_material_adapter_pos)
if(NOT legacy_material_adapter_pos EQUAL -1)
    message(FATAL_ERROR "Legacy material-to-hazard adapter remains")
endif()

file(READ "${RUNNER_SOURCE_DIR}/.github/workflows/release.yml" release_workflow_text)
string(FIND "${release_workflow_text}" "--diagnose-art" art_workflow_pos)
if(art_workflow_pos EQUAL -1)
    message(FATAL_ERROR "Release workflow does not run the v0.7.29 art diagnostic")
endif()
string(FIND "${release_workflow_text}" "--diagnose-rig-training" cold_start_workflow_pos)
if(cold_start_workflow_pos EQUAL -1)
    message(FATAL_ERROR "Release workflow does not run the v0.7.31 cold-start diagnostic")
endif()
string(FIND "${release_workflow_text}" "--diagnose-walk-eye" walk_eye_workflow_pos)
if(walk_eye_workflow_pos EQUAL -1)
    message(FATAL_ERROR "Release workflow does not run the v0.7.32 raw-policy walk-eye diagnostic")
endif()
string(FIND "${release_workflow_text}" "--diagnose-course" course_workflow_pos)
if(course_workflow_pos EQUAL -1)
    message(FATAL_ERROR "Release workflow does not run the v0.7.28 course diagnostic")
endif()
file(GLOB release_notes "${RUNNER_SOURCE_DIR}/RELEASE_NOTES*.md")
if(release_notes)
    message(FATAL_ERROR "Per-release note files remain; CHANGELOG.md is canonical")
endif()
if(EXISTS "${RUNNER_SOURCE_DIR}/release-notes.md")
    message(FATAL_ERROR "Redundant lowercase release-notes.md remains")
endif()

foreach(stale IN ITEMS
        PUBLISH_TRIGGER_v0710.tmp
        .github/workflows/dispatch-runner-v0715-patch.yml
        .github/workflows/publish-runner-v0714-final.yml
        .github/workflows/refine-runner-v0715.yml
        tools/apply_v0724_structural_metrics_icon.py
        tools/run_v0724_migration.py
        tools/cache_v0724_structural_metrics_icon.py
        tools/fix_v0724_scoped_rigidity.py
        tools/v0724-trigger.txt
        tools/v0724-pr-target-trigger.txt
        tools/v0724-prtarget-kick2.txt
        tools/v0724-reopen-trigger.txt
        tools/v0724-rescue-trigger.txt
        tools/apply_v0725_art_leg_hotfix.py
        tools/apply_v0725_font_sync.py
        .github/workflows/fix-v0724-scoped-rigidity.yml)
    if(EXISTS "${RUNNER_SOURCE_DIR}/${stale}")
        message(FATAL_ERROR "Temporary v0.7.24 migration file remains: ${stale}")
    endif()
endforeach()

message(STATUS "Runner v0.7.32 repository hygiene passed")
