if(NOT DEFINED RUNNER_SOURCE_DIR)
    message(FATAL_ERROR "RUNNER_SOURCE_DIR was not provided")
endif()
if(NOT DEFINED RUNNER_RELEASE_AUDIT_MODE)
    set(RUNNER_RELEASE_AUDIT_MODE candidate)
endif()
if(NOT RUNNER_RELEASE_AUDIT_MODE STREQUAL "candidate"
        AND NOT RUNNER_RELEASE_AUDIT_MODE STREQUAL "final")
    message(FATAL_ERROR
        "RUNNER_RELEASE_AUDIT_MODE must be candidate or final")
endif()
if(RUNNER_RELEASE_AUDIT_MODE STREQUAL "final")
    foreach(required_variable IN ITEMS
            RUNNER_RELEASE_COMMIT RUNNER_RELEASE_TAG
            RUNNER_RELEASE_CAPTURE_SHA256 RUNNER_RELEASE_CAPTURE_URL
            RUNNER_RELEASE_BUILD_RUN_ID RUNNER_RELEASE_BUILD_RUN_ATTEMPT
            RUNNER_RELEASE_PROMOTION_RUN_ID
            RUNNER_RELEASE_PROMOTION_RUN_ATTEMPT RUNNER_RELEASE_REPOSITORY
            RUNNER_RELEASE_EVENT_NAME RUNNER_RELEASE_RUN_SHA
            RUNNER_RELEASE_RUN_REF RUNNER_RELEASE_WORKFLOW_SHA
            RUNNER_RELEASE_WORKFLOW_REF RUNNER_RELEASE_GITHUB_STATUS
            RUNNER_RELEASE_PUBLICATION_TARGET RUNNER_RELEASE_ASSET_ROOT
            RUNNER_RELEASE_HANDOFF_ROOT RUNNER_RELEASE_CAPTURE_ROOT
            RUNNER_RELEASE_BUILD_RECEIPT RUNNER_RELEASE_LINUX_ABI
            RUNNER_RELEASE_CAPTURE_BUNDLE RUNNER_RELEASE_PROMOTION_RECEIPT
            RUNNER_RELEASE_PROMOTION_SIGNATURE RUNNER_RELEASE_HANDOFF_SUMMARY
            RUNNER_RELEASE_BUILD_ARTIFACT_ID RUNNER_RELEASE_BUILD_ARTIFACT_NAME
            RUNNER_RELEASE_BUILD_ARTIFACT_SHA256
            RUNNER_RELEASE_PROMOTION_INPUTS_ARTIFACT_ID
            RUNNER_RELEASE_PROMOTION_INPUTS_ARTIFACT_NAME
            RUNNER_RELEASE_PROMOTION_INPUTS_ARTIFACT_SHA256
            RUNNER_RELEASE_NATIVE_EVIDENCE_ARTIFACT_ID
            RUNNER_RELEASE_NATIVE_EVIDENCE_ARTIFACT_NAME
            RUNNER_RELEASE_NATIVE_EVIDENCE_ARTIFACT_SHA256)
        if(NOT DEFINED ${required_variable})
            message(FATAL_ERROR
                "Final repository audit requires ${required_variable}")
        endif()
    endforeach()
    string(LENGTH "${RUNNER_RELEASE_COMMIT}" release_commit_length)
    if(NOT release_commit_length EQUAL 40
            OR NOT RUNNER_RELEASE_COMMIT MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR
            "Final repository audit requires a full lowercase Git commit SHA")
    endif()
    if(NOT RUNNER_RELEASE_TAG STREQUAL "v0.7.51")
        message(FATAL_ERROR
            "Final repository audit requires proposed tag v0.7.51")
    endif()
    string(LENGTH "${RUNNER_RELEASE_CAPTURE_SHA256}" capture_hash_length)
    if(NOT capture_hash_length EQUAL 64
            OR NOT RUNNER_RELEASE_CAPTURE_SHA256 MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR
            "Final repository audit requires a lowercase immutable capture-bundle SHA-256")
    endif()
    foreach(run_id_variable IN ITEMS
            RUNNER_RELEASE_BUILD_RUN_ID RUNNER_RELEASE_BUILD_RUN_ATTEMPT
            RUNNER_RELEASE_PROMOTION_RUN_ID RUNNER_RELEASE_PROMOTION_RUN_ATTEMPT
            RUNNER_RELEASE_BUILD_ARTIFACT_ID
            RUNNER_RELEASE_PROMOTION_INPUTS_ARTIFACT_ID
            RUNNER_RELEASE_NATIVE_EVIDENCE_ARTIFACT_ID)
        if(NOT "${${run_id_variable}}" MATCHES "^[1-9][0-9]*$")
            message(FATAL_ERROR
                "Final repository audit requires a positive ${run_id_variable}")
        endif()
    endforeach()
    foreach(artifact_hash_variable IN ITEMS
            RUNNER_RELEASE_BUILD_ARTIFACT_SHA256
            RUNNER_RELEASE_PROMOTION_INPUTS_ARTIFACT_SHA256
            RUNNER_RELEASE_NATIVE_EVIDENCE_ARTIFACT_SHA256)
        string(LENGTH "${${artifact_hash_variable}}" artifact_hash_length)
        if(NOT artifact_hash_length EQUAL 64
                OR NOT "${${artifact_hash_variable}}" MATCHES "^[0-9a-f]+$")
            message(FATAL_ERROR
                "Final repository audit requires a lowercase ${artifact_hash_variable}")
        endif()
    endforeach()
    if(NOT "${RUNNER_RELEASE_EVENT_NAME}" STREQUAL "workflow_dispatch"
            OR NOT "${RUNNER_RELEASE_RUN_SHA}" STREQUAL "${RUNNER_RELEASE_COMMIT}"
            OR NOT "${RUNNER_RELEASE_RUN_REF}" STREQUAL "refs/heads/main"
            OR NOT "${RUNNER_RELEASE_WORKFLOW_SHA}" STREQUAL "${RUNNER_RELEASE_COMMIT}"
            OR NOT "${RUNNER_RELEASE_WORKFLOW_REF}" STREQUAL
                "${RUNNER_RELEASE_REPOSITORY}/.github/workflows/release.yml@${RUNNER_RELEASE_RUN_REF}")
        message(FATAL_ERROR
            "Final repository audit requires the exact executing event, run, ref, and workflow identity")
    endif()
    if(NOT RUNNER_RELEASE_PUBLICATION_TARGET STREQUAL "site"
            AND NOT RUNNER_RELEASE_PUBLICATION_TARGET STREQUAL "github")
        message(FATAL_ERROR
            "Final repository audit requires the exact Site or GitHub publication target")
    endif()
endif()
file(READ "${RUNNER_SOURCE_DIR}/run.sh" RUNNER_LINUX_LAUNCHER HEX)
if(RUNNER_LINUX_LAUNCHER MATCHES "0d0a")
    message(FATAL_ERROR "run.sh must use LF line endings for /usr/bin/env portability")
endif()
file(READ "${RUNNER_SOURCE_DIR}/run.bat" RUNNER_WINDOWS_LAUNCHER)
foreach(reference IN ITEMS
        [=[if "%~1"=="" pause]=]
        [=[exit /b %RUNNER_RESULT%]=])
    string(FIND "${RUNNER_WINDOWS_LAUNCHER}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "run.bat must return diagnostics without an interactive pause: ${reference}")
    endif()
endforeach()
file(READ "${RUNNER_SOURCE_DIR}/examples/library_probe.cpp" RUNNER_LIBRARY_PROBE)
if(NOT RUNNER_LIBRARY_PROBE MATCHES "Epoch2DWalkEngine 0\\.7\\.51 library API, task graph")
    message(FATAL_ERROR "Installed library example does not report v0.7.51")
endif()
file(READ "${RUNNER_SOURCE_DIR}/examples/installed_consumer/CMakeLists.txt"
    RUNNER_INSTALLED_CONSUMER)
string(FIND "${RUNNER_INSTALLED_CONSUMER}"
    "find_package(Epoch2DWalkEngine 0.7.51 EXACT CONFIG REQUIRED)"
    installed_consumer_exact_version_pos)
if(installed_consumer_exact_version_pos EQUAL -1)
    message(FATAL_ERROR
        "Installed consumer does not require the exact v0.7.51 package")
endif()

foreach(required IN ITEMS
        .gitattributes AGENTS.md CHANGELOG.md missioncache.md README.md
        run.bat run.sh
        include/Epoch2DWalkEngine/Engine.hpp
        cmake/Epoch2DWalkEngineConfig.cmake.in
        examples/library_probe.cpp
        examples/installed_consumer/CMakeLists.txt
        docs/EPOCH2D_WALK_ENGINE_LIBRARY.md
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
        docs/RUNNER_V0733_GRANULAR_FACING_RIGLAB.md
        docs/RUNNER_V0734_CURRICULUM_SAFE_RIG_OPTIMIZATION.md
        docs/RUNNER_V0735_PIP_POSTURE_TRUTH.md
        docs/RUNNER_V0736_AUTHORED_GAIT_RUNTIME.md
        docs/RUNNER_V0737_HYBRID_LOCOMOTION_TERRAIN.md
        docs/RUNNER_V0738_TURN_TOPOLOGY_STATS.md
        docs/RUNNER_V0739_STATIC_CELLS_DIRECTION.md
        docs/RUNNER_V0740_PHYSICAL_FACING_RETURN.md
        docs/RUNNER_V0741_FOUR_RIG_NATURAL_GAIT.md
        docs/RUNNER_V0742_SPECIES_ANATOMY_SCALE.md
        docs/RUNNER_V0743_AUTHORED_LIVE_MORPHOLOGY.md
        docs/RUNNER_V0744_DOG_ART_RIG_ASSEMBLY.md
        docs/RUNNER_V0745_HUMAN_SUPPORT_SPECIES_ART.md
        docs/RUNNER_V0746_EXACT_CELLS_NATURAL_GAIT_TURN.md
        docs/RUNNER_V0747_SPECIES_RIGLAB_MASTERY_RELEASE.md
        docs/RUNNER_V0748_CONTACT_LED_GAIT_SPECIES_ART.md
        assets/optional/species_runtime/chicken_body_side.ppm
        docs/RUNNER_V0749_PHYSICAL_COMBAT_TERRAIN_ART.md
        docs/RUNNER_V0750_PERSISTENT_DIRECTOR_ART_AUTHORING.md
        docs/EPOCH2D_WALK_ENGINE_V0751_PRODUCT_CLOSURE.md
        cmake/RequireExactEpochSimEngineVersion.cmake
        assets/optional/species_runtime/chicken_head_side.ppm
        assets/optional/species_runtime/chicken_upper_leg_side.ppm
        assets/optional/species_runtime/chicken_lower_leg_side.ppm
        assets/optional/species_runtime/chicken_foot_side.ppm
        assets/optional/species_runtime/chicken_tail_side.ppm
        assets/optional/species_runtime/dog_body_side.ppm
        assets/optional/species_runtime/dog_head_side.ppm
        assets/optional/species_runtime/dog_upper_leg_side.ppm
        assets/optional/species_runtime/dog_lower_leg_side.ppm
        assets/optional/species_runtime/dog_foot_side.ppm
        assets/optional/species_runtime/dog_tail_side.ppm
        assets/optional/species_runtime/hexapod_body_side.ppm
        assets/optional/species_runtime/hexapod_head_side.ppm
        assets/optional/species_runtime/hexapod_upper_leg_side.ppm
        assets/optional/species_runtime/hexapod_lower_leg_side.ppm
        assets/optional/species_runtime/hexapod_foot_side.ppm
        assets/optional/species_runtime/hexapod_tail_side.ppm
        assets/optional/species_atlases/chicken_modules.png
        assets/optional/species_atlases/dog_modules.png
        assets/optional/species_atlases/hexapod_modules.png
        assets/references/armor_source_reference.png
        tests/v0725_art_leg_hotfix_tests.cpp
        tests/v0726_training_truth_tests.cpp
        tests/v0730_cold_start_tests.cpp
        tests/v0731_crouch_learning_tests.cpp
        tests/v0732_shuttle_facing_tests.cpp
        tests/v0734_curriculum_rig_tests.cpp
        tests/v0735_pip_posture_tests.cpp
        tests/v0736_authored_runtime_tests.cpp
        tests/v0737_locomotion_terrain_tests.cpp
        tests/v0728_course_completion_tests.cpp
        tests/v0729_modular_art_tests.cpp
        src/course_completion_diagnostic.cpp
        src/course_completion_diagnostic.hpp
        src/hybrid_brain_diagnostic.cpp
        src/hybrid_brain_diagnostic.hpp
        src/raw_inference_diagnostic.cpp
        src/raw_inference_diagnostic.hpp
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
        tools/art_sources/runner_v0744_dog_modules_source.png
        tools/generate_species_art_assets.py
        tools/strict_sha256_manifest.py
        tests/v0744_dog_art_rig_tests.cpp
        tests/v0750_persistent_director_art_tests.cpp
        tests/v0751_director_challenge_tests.cpp
        tests/v0751_crouch_authority_tests.cpp
        tests/v0751_external_host_tests.cpp
        tests/raw_authority_tests.cpp
        tests/raw_learning_provenance_tests.cpp
        tests/raw_learning_test_access.hpp
        tests/v0751_lake_challenge_tests.cpp
        tests/v0751_physical_equipment_tests.cpp
        tests/software_frame_capture_tests.cpp
        tests/strict_sha256_manifest_tests.py
        tests/cmake/stale_epochsimengine_override/CMakeLists.txt
        tests/cmake/stale_epochsimengine_override/fake-2.5.9/CMakeLists.txt
        tests/cmake/verify_stale_epochsimengine_override.cmake
        tests/v0718_runtime_recovery_tests.cpp
        tests/v0719_general_locomotion_tests.cpp
        tests/v0720_ui_tests.cpp
        tests/v0721_readable_telemetry_tests.cpp
        tests/v0721_rig_gait_tests.cpp
        tests/v0723_gray_frame_tests.cpp
        tests/v0724_structural_metrics_icon_tests.cpp
        src/ai_director.hpp
        src/ai_director_io.hpp
        src/atomic_file.cpp
        src/atomic_file.hpp
        src/art_authoring.hpp
        src/integration.hpp
        src/raw_learning_provenance.hpp
        src/course_geometry.hpp
        src/locomotion_strategy.hpp
        src/presentation_session.hpp
        src/preview_sync.hpp
        src/species_art_layout.hpp
        src/training_explainer.hpp
        src/ui_render_contract.hpp
        src/ui_frame_probe.hpp
        src/software_frame_capture.hpp
        src/runner_icon.rc.in
        src/ui_layout.hpp
        tools/visual_capture.cpp)
    if(NOT EXISTS "${RUNNER_SOURCE_DIR}/${required}")
        message(FATAL_ERROR "Missing required repository file: ${required}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/.gitignore" gitignore_text)
string(FIND "${gitignore_text}"
    "/epoch2dwalk-v0751-*-autonomy.state" v0751_autonomy_ignore_pos)
if(v0751_autonomy_ignore_pos EQUAL -1)
    message(FATAL_ERROR
        "Generated v0.7.51 autonomy ledgers are not ignored without deleting user state")
endif()

file(READ "${RUNNER_SOURCE_DIR}/tests/raw_learning_provenance_tests.cpp"
    raw_learning_provenance_test_text)
foreach(reference IN ITEMS
        "0x0007'5112u"
        "0x0007'5113u"
        "0x0007'5114u"
        "historical weights silently resumed under current load-qualified semantics")
    string(FIND "${raw_learning_provenance_test_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "Raw-learning historical transfer-only coverage missing: ${reference}")
    endif()
endforeach()

if(EXISTS "${RUNNER_SOURCE_DIR}/assets/ui/runner_icon_source.png")
    message(FATAL_ERROR "Corrupted legacy screenshot PNG remains in source tree")
endif()
if(EXISTS "${RUNNER_SOURCE_DIR}/assets/ui/runner_icon_concept.svg")
    message(FATAL_ERROR
        "Unreferenced legacy RUNNER icon concept would leak into the runtime asset package")
endif()

file(READ "${RUNNER_SOURCE_DIR}/tools/strict_sha256_manifest.py"
    strict_manifest_text)
foreach(reference IN ITEMS
        [=[ENTRY_RE = re.compile(r"([0-9a-f]{64})  ([^\r\n]+)")]=]
        "manifest must use LF-only line endings"
        "manifest must not contain a UTF-8 byte-order mark"
        "manifest paths are not bytewise sorted"
        "manifest contains duplicate paths"
        "manifest file set mismatch"
        [=["ls-tree",]=]
        [=["-rz",]=]
        [=["hash-object", "--no-filters"]=]
        "def verify_capture_bundle("
        [=[CAPTURE_SCHEMA = "epoch2dwalk-native-capture-v2"]=]
        "CAPTURE_SCENARIO_MATRIX = {"
        "CAPTURE_SCENARIOS = tuple(CAPTURE_SCENARIO_MATRIX)"
        "VIEWPORT_LOGICAL_SIZES = {"
        [=["compact": frozenset({(1368, 768), (1368, 799)})]=]
        [=[ALLOWED_DPI = frozenset({96, 120, 144, 168, 192})]=]
        "capture_dimensions(capture, index)"
        "capture semantic phase is duplicated"
        "capture image content is duplicated across phases"
        "capture matrix differs; missing="
        "capture scenarios are not in the required matrix order"
        "png_dimensions(capture_path)"
        "capture bundle file set differs from its manifest"
        "runtime_archive.sha256 is invalid"
        [=[PROMOTION_SCHEMA = "epoch2dwalk-promotion-receipt-v2"]=]
        [=[PROMOTION_SECRET_ENV = "RUNNER_PROTECTED_RELEASE_APPROVAL"]=]
        "def write_promotion_receipt("
        "def verify_promotion_receipt("
        "strict-context-bound-15-object-handoff-ready"
        "Site handoff recursive file set differs from the exact allowlist"
        "replay_policy=not-enforced-by-receipt-use-external-site-one-use-gate"
        "artifact_policy=reused-exact-build-candidate-no-rebuild"
        "Phase-A artifact file set is not the exact 15-object set plus its two receipts"
        "hmac.compare_digest"
        "promotion_run_attempt"
        "zipfile.BadZipFile")
    string(FIND "${strict_manifest_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Strict release-manifest contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/tools/visual_capture.cpp"
    visual_capture_tool_text)
string(FIND "${visual_capture_tool_text}"
    "Epoch2DWalkEngine v{} deterministic production-vertex visual captures"
    product_capture_identity_pos)
if(product_capture_identity_pos EQUAL -1)
    message(FATAL_ERROR
        "Production-vertex evidence manifest does not lead with Epoch2DWalkEngine identity")
endif()
string(FIND "${visual_capture_tool_text}"
    "Runner {} deterministic production-vertex visual captures"
    stale_capture_identity_pos)
if(NOT stale_capture_identity_pos EQUAL -1)
    message(FATAL_ERROR
        "Production-vertex evidence manifest still presents Runner as the product")
endif()
foreach(required_live_capture_route IN ITEMS
        [=["live-mission", "live-view", "live-details"]=]
        [=["4K concise Details page"]=])
    string(FIND "${visual_capture_tool_text}"
        "${required_live_capture_route}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "Production-vertex evidence tool lacks accepted Live route contract: ${required_live_capture_route}")
    endif()
endforeach()
string(FIND "${visual_capture_tool_text}" "\"live-history\""
    stale_live_capture_route_pos)
if(NOT stale_live_capture_route_pos EQUAL -1)
    message(FATAL_ERROR
        "Production-vertex evidence tool still captures a retired Live History route")
endif()

foreach(required_capture_scenario IN ITEMS
        live-compact
        live-mission-compact-top live-mission-compact-bottom
        live-view-compact-top live-view-compact-bottom
        live-details-compact-top live-details-compact-bottom
        live-default live-ultrawide live-ultrawide-pip
        course-overview lake-approach lake-immersion lake-exit
        human-heel-strike human-flat-support human-toe-off
        chicken-contact-loaded chicken-contact-transfer chicken-lake-entry chicken-lake-exit
        dog-contact-loaded dog-contact-transfer dog-lake-entry dog-lake-exit
        hexapod-contact-loaded hexapod-contact-transfer hexapod-lake-entry hexapod-lake-exit
        equipment-plant equipment-aim equipment-fire equipment-recovery
        riglab-presets-compact-top riglab-presets-compact-bottom
        riglab-structure-compact-top riglab-structure-compact-bottom
        riglab-motors-compact-top riglab-motors-compact-bottom
        riglab-art-compact-top riglab-art-compact-bottom
        riglab-test-compact-top riglab-test-compact-bottom
        riglab-presets-normal riglab-presets-zoom
        riglab-structure-normal riglab-structure-zoom
        riglab-motors-normal riglab-motors-zoom
        riglab-test-normal riglab-test-zoom
        riglab-art-human-normal riglab-art-human-zoom
        riglab-art-chicken-normal riglab-art-chicken-zoom
        riglab-art-dog-normal riglab-art-dog-zoom
        riglab-art-hexapod-normal riglab-art-hexapod-zoom
        terminal-state)
    string(FIND "${strict_manifest_text}" "\"${required_capture_scenario}\"" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "Strict release-manifest matrix lacks scenario: ${required_capture_scenario}")
    endif()
endforeach()

foreach(stale_capture_scenario IN ITEMS
        live-status-compact-bottom
        live-totals-compact-top live-totals-compact-bottom
        live-advanced-compact-top live-advanced-compact-bottom)
    string(FIND "${strict_manifest_text}" "\"${stale_capture_scenario}\"" pos)
    if(NOT pos EQUAL -1)
        message(FATAL_ERROR
            "Strict release-manifest matrix retains retired Live route: ${stale_capture_scenario}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/CMakeLists.txt" cmake_text)
foreach(reference IN ITEMS
        "project(Epoch2DWalkEngine VERSION 0.7.51 LANGUAGES CXX)"
        "RUNNER_SANDHYBRID_SOURCE_ID \"dffedf526a83fce477ebe18376cf75a88598e738\""
        "find_package(EpochSimEngine 2.5.29 EXACT CONFIG REQUIRED"
        "Epoch2DWalkEngine::Epoch2DWalkEngine"
        "Epoch2DWalkEngineTargets"
        "Epoch2DWalkEngineDevelopment"
        "COMPONENT RunnerRuntime"
        "RunnerPackageMetadataSync"
        "add_dependencies(Runner RunnerPackageMetadataSync)"
        [=["${CMAKE_CURRENT_SOURCE_DIR}/README.md"]=]
        [=["${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md"]=]
        [=["${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"]=]
        [=["${CMAKE_CURRENT_SOURCE_DIR}/missioncache.md"]=]
        "runner_runtime_notice IN ITEMS sdl3 vulkan-loader"
        "docs/third-party"
        "src/atomic_file.cpp"
        "DESTINATION examples COMPONENT Epoch2DWalkEngineDevelopment"
        "DESTINATION docs/third-party/EpochSimEngine"
        "Epoch2DWalkEngine.LibraryAPI"
        "EPOCH2D_WALK_ENGINE_LIBRARY.md"
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
        "RunnerV0734CurriculumRigTests"
        "Runner.V0734CurriculumRig"
        "RunnerV0735PipPostureTests"
        "Runner.V0735PipPosture"
        "RunnerV0736AuthoredRuntimeTests"
        "Runner.V0736AuthoredRuntime"
        "RunnerV0737LocomotionTerrainTests"
        "Runner.V0737LocomotionTerrain"
        "RunnerV0742SpeciesRigTests"
        "Runner.V0742SpeciesRig"
        "RunnerV0744DogArtRigTests"
        "Runner.V0744DogArtRig"
        "RunnerV0748ContactArtTests"
        "Runner.V0748ContactArt"
        "RunnerRigDefaults"
        "RunnerV0749PhysicalCombatTerrainTests"
        "Runner.V0749PhysicalCombatTerrain"
        RunnerV0750PersistentDirectorArtTests
        Runner.V0750PersistentDirectorArt
        RunnerV0751DirectorChallengeTests
        Runner.V0751DirectorChallenge
        RunnerV0751CrouchAuthorityTests
        Runner.V0751CrouchAuthority
        RunnerV0751ExternalHostTests
        Runner.V0751ExternalHost
        RunnerV0751LakeChallengeTests
        Runner.V0751LakeChallenge
        RunnerV0751PhysicalEquipmentTests
        Runner.V0751PhysicalEquipment
        RunnerSoftwareFrameCaptureTests
        Runner.SoftwareFrameCapture
        RunnerVisualCapture
        RunnerVisualCaptures
        "human.rig"
        "chicken.rig"
        "dog.rig"
        "hexapod.rig"
        "Runner.HybridBrainDiagnostic"
        "src/hybrid_brain_diagnostic.cpp"
        "Runner.RawInferenceDiagnostic"
        "src/raw_inference_diagnostic.cpp"
        "src/raw_inference_diagnostic.hpp"
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
        "RUNNER_V0733_GRANULAR_FACING_RIGLAB.md"
        "RUNNER_V0734_CURRICULUM_SAFE_RIG_OPTIMIZATION.md"
        "RUNNER_V0735_PIP_POSTURE_TRUTH.md"
        "RUNNER_V0736_AUTHORED_GAIT_RUNTIME.md"
        "RUNNER_V0737_HYBRID_LOCOMOTION_TERRAIN.md"
        "RUNNER_V0738_TURN_TOPOLOGY_STATS.md"
        "RUNNER_V0739_STATIC_CELLS_DIRECTION.md"
        "RUNNER_V0740_PHYSICAL_FACING_RETURN.md"
        "RUNNER_V0741_FOUR_RIG_NATURAL_GAIT.md"
        "RUNNER_V0742_SPECIES_ANATOMY_SCALE.md"
        "RUNNER_V0743_AUTHORED_LIVE_MORPHOLOGY.md"
        "RUNNER_V0724_STRUCTURAL_METRICS_ICON.md"
        "RUNNER_V0744_DOG_ART_RIG_ASSEMBLY.md"
        "RUNNER_V0745_HUMAN_SUPPORT_SPECIES_ART.md"
        "RUNNER_V0746_EXACT_CELLS_NATURAL_GAIT_TURN.md"
        "RUNNER_V0747_SPECIES_RIGLAB_MASTERY_RELEASE.md"
        "RUNNER_V0748_CONTACT_LED_GAIT_SPECIES_ART.md"
        "RUNNER_V0749_PHYSICAL_COMBAT_TERRAIN_ART.md"
        RUNNER_V0750_PERSISTENT_DIRECTOR_ART_AUTHORING.md
        EPOCH2D_WALK_ENGINE_V0751_PRODUCT_CLOSURE.md
        "runner_icon.rc")
    string(FIND "${cmake_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "CMake/versioned package contract missing: ${reference}")
    endif()
endforeach()

foreach(reference IN ITEMS
        "tests/cmake/verify_stale_epochsimengine_override.cmake"
        [=[RUNNER_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/stale-epochsimengine-override]=])
    string(FIND "${cmake_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "Exact EpochSimEngine negative-test wrapper is not registered: ${reference}")
    endif()
endforeach()
string(FIND "${cmake_text}"
    "add_test(NAME Runner.ExactEpochSimEngineVersionGuardRejectsStaleOverride"
    stale_rejection_test_pos)
string(FIND "${cmake_text}" "add_test(NAME Runner.RepositoryHygiene"
    repository_hygiene_test_pos)
if(stale_rejection_test_pos EQUAL -1 OR repository_hygiene_test_pos EQUAL -1
        OR NOT stale_rejection_test_pos LESS repository_hygiene_test_pos)
    message(FATAL_ERROR
        "Exact-version rejection test ordering is not auditable")
endif()
math(EXPR stale_rejection_section_length
    "${repository_hygiene_test_pos} - ${stale_rejection_test_pos}")
string(SUBSTRING "${cmake_text}" ${stale_rejection_test_pos}
    ${stale_rejection_section_length} stale_rejection_test_text)
string(FIND "${stale_rejection_test_text}" "WILL_FAIL"
    stale_will_fail_pos)
if(NOT stale_will_fail_pos EQUAL -1)
    message(FATAL_ERROR
        "Exact-version rejection still accepts an arbitrary failing configure")
endif()

file(READ
    "${RUNNER_SOURCE_DIR}/tests/cmake/verify_stale_epochsimengine_override.cmake"
    stale_epochsimengine_wrapper_text)
foreach(reference IN ITEMS
        "Exact-version negative fixture is incomplete"
        "Stale EpochSimEngine 2.5.9 override was accepted as 2.5.29"
        "Epoch2DWalkEngine requires exact EpochSimEngine 2.5.29"
        "reports 2.5.9"
        "failed for the wrong reason")
    string(FIND "${stale_epochsimengine_wrapper_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "Exact-version negative wrapper is not fail-closed: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/atomic_file.hpp" atomic_file_header_text)
string(FIND "${atomic_file_header_text}" "windows.h" atomic_header_windows_pos)
if(NOT atomic_header_windows_pos EQUAL -1)
    message(FATAL_ERROR
        "Platform-neutral atomic-file header exposes windows.h to SDK consumers")
endif()
foreach(reference IN ITEMS
        "bool atomic_replace_file("
        "const std::filesystem::path& temporary"
        "const std::filesystem::path& destination")
    string(FIND "${atomic_file_header_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Atomic-file public declaration missing: ${reference}")
    endif()
endforeach()
file(READ "${RUNNER_SOURCE_DIR}/src/atomic_file.cpp" atomic_file_source_text)
foreach(reference IN ITEMS
        "#include <windows.h>"
        "MoveFileExW"
        "MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH"
        "std::filesystem::rename(temporary, destination")
    string(FIND "${atomic_file_source_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Atomic-file implementation missing: ${reference}")
    endif()
endforeach()
string(FIND "${cmake_text}"
    [=[$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>]=]
    epoch2dwalk_install_include_root_pos)
string(FIND "${cmake_text}"
    [=[$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Epoch2DWalkEngine>]=]
    epoch2dwalk_stale_install_include_pos)
if(epoch2dwalk_install_include_root_pos EQUAL -1
        OR NOT epoch2dwalk_stale_install_include_pos EQUAL -1)
    message(FATAL_ERROR
        "Exported Epoch2DWalkEngine target does not advertise the true include root")
endif()

file(READ "${RUNNER_SOURCE_DIR}/cmake/Epoch2DWalkEngineConfig.cmake.in"
    epoch2dwalk_config_text)
string(FIND "${epoch2dwalk_config_text}"
    "find_dependency(EpochSimEngine 2.5.29 EXACT CONFIG REQUIRED)"
    epoch2dwalk_dependency_pos)
if(epoch2dwalk_dependency_pos EQUAL -1)
    message(FATAL_ERROR
        "Installed Epoch2DWalkEngine package does not pin its branded EpochSimEngine ABI")
endif()
foreach(version_source IN ITEMS cmake_text epoch2dwalk_config_text)
    string(FIND "${${version_source}}" "EpochSimEngine 2.5.28" unpublished_dependency_pos)
    if(NOT unpublished_dependency_pos EQUAL -1)
        message(FATAL_ERROR
            "Release package contract claims unpublished EpochSimEngine v2.5.28")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/simulation.hpp" public_simulation_text)
foreach(reference IN ITEMS
        "anatomy_action_count = 12"
        "equipment_action_count = 3"
        "static_assert(action_count == 15u)"
        "observation_count = 84u"
        "human_left_toe_action = 10u"
        "human_right_toe_action = 11u"
        "human_left_heel_node = 13u"
        "human_right_toe_node = 18u")
    string(FIND "${public_simulation_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "v0.7.51 public observation/action/topology contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/integration.hpp" public_integration_text)
foreach(reference IN ITEMS
        "api_version = 7u"
        "action_layout_schema_version = 2u"
        "canonical_snapshot_schema_version = 5u"
        "player_intent_schema_version = 1u"
        "provenance_.retained_guidance")
    string(FIND "${public_integration_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "v0.7.51 external-host version contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/include/Epoch2DWalkEngine/Engine.hpp"
    epoch2dwalk_umbrella_text)
foreach(reference IN ITEMS
        "#include \"integration.hpp\""
        "#include \"raw_inference_diagnostic.hpp\""
        "#include \"raw_learning_provenance.hpp\""
        "namespace epoch2dwalk = runner")
    string(FIND "${epoch2dwalk_umbrella_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "Installed Epoch2DWalkEngine umbrella is incomplete: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/vcpkg.json" vcpkg_text)
string(FIND "${vcpkg_text}" "\"version-semver\": \"0.7.51\"" vcpkg_version_pos)
if(vcpkg_version_pos EQUAL -1)
    message(FATAL_ERROR "vcpkg metadata is not versioned for v0.7.51")
endif()

file(READ "${RUNNER_SOURCE_DIR}/README.md" readme_text)
foreach(reference IN ITEMS
        "Epoch2DWalkEngine-v0.7.51-runner-windows-x64.zip"
        "Epoch2DWalkEngine-v0.7.51-runner-linux-x86_64.tar.gz"
        "Epoch2DWalkEngine-v0.7.51-dev-windows-x64.zip"
        "Epoch2DWalkEngine-v0.7.51-dev-linux-x86_64.tar.gz"
        "Epoch2DWalkEngine-v0.7.51-source.zip"
        "exact UTF-8-without-BOM, LF-terminated text"
        "`lowercase-sha256  exact-archive-filename`"
        "exact publicly consumable `EpochSimEngine 2.5.29`"
        "integration contract to API 7, action-layout schema 2, and canonical snapshot schema 5"
        "persisted RUNRIG7 19-node rig"
        "84 observations"
        "fixed 15-slot public action layout"
        "12 topology-masked anatomy slots and 3 separate equipment slots"
        "76-observation/13-action physical two-hand equipment ABI under `0x0007'5109`"
        "Current `0x0007'5115` binds RUNRIG7's 84-observation/15-action schema"
        "Guidance is explicit at every policy-input call"
        "`0x0007'5110` is the historical foundational-input/mixed-origin boundary"
        "`0x0007'5111` is the whole-composite authority/plant boundary"
        "`0x0007'5112` is the support-feedback observation boundary"
        "`0x0007'5113` is the CrouchWalk authority boundary"
        "None of `0x0007'5109`, `0x0007'5110`, `0x0007'5111`, `0x0007'5112`, or `0x0007'5113` can silently resume under `5115`"
        "all five historical boundaries remain explicit unverified transfer-only inputs"
        "product-owned `epoch2dwalk-v0751-*` paths"
        "Checksummed `EPPO51` checkpoints"
        "--diagnose-raw-inference"
        "per-joint network/effective-command identity")
    string(FIND "${readme_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.51 README release contract missing: ${reference}")
    endif()
endforeach()
string(FIND "${readme_text}" "default twelve-motor Human" current_human_motor_pos)
string(FIND "${readme_text}" "default eight-motor Human" stale_human_motor_pos)
if(current_human_motor_pos EQUAL -1 OR NOT stale_human_motor_pos EQUAL -1)
    message(FATAL_ERROR
        "v0.7.51 README does not identify the canonical twelve-motor Human")
endif()

file(READ "${RUNNER_SOURCE_DIR}/CHANGELOG.md" changelog_text)
foreach(reference IN ITEMS
        "## 0.7.51"
        "canonical snapshot schema to version 5"
        "canonical RUNRIG7 19-node Human"
        "84 observations"
        "fixed 15-slot topology-masked action layout"
        "physical two-hand equipment and historical 76/13 policy layout first entered under `0x0007'5109`"
        "`0x0007'5110` is the historical foundational-input/mixed-origin boundary"
        "Current training semantics `0x0007'5115`"
        "Explicit guidance preserves the prior observation contract everywhere else"
        "`0x0007'5112` is the support-feedback observation boundary"
        "`0x0007'5113` is the CrouchWalk authority boundary"
        "None of `0x0007'5109`, `0x0007'5110`, `0x0007'5111`, `0x0007'5112`, or `0x0007'5113` can silently resume under `5115`"
        "explicit unverified weight-transfer-only inputs"
        "product-owned `epoch2dwalk-v0751-*` paths"
        "Raw inference")
    string(FIND "${changelog_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.51 changelog truth contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/docs/EPOCH2D_WALK_ENGINE_V0751_PRODUCT_CLOSURE.md"
    v0751_release_text)
foreach(reference IN ITEMS
        "world_bottom = -25.3125 m"
        "world_top = +25.3125 m"
        "resident_ground_boundary_offset"
        "Integration API 7"
        "Schema-5 snapshots"
        "19 nodes"
        "84 values"
        "12 topology-masked anatomy slots plus 3 separate equipment slots"
        "physical grip frame and historical 76/13 policy ABI first entered under `0x0007'5109`"
        "`0x0007'5110` is the historical foundational-input/mixed-origin boundary"
        "Current `0x0007'5115`"
        "`0x0007'5112` is the Human Uneven Walk support-feedback observation boundary"
        "`0x0007'5113` is the CrouchWalk authority boundary"
        "Explicit guidance keeps every non-target Raw scope phase-scrubbed"
        "`0x0007'5109`, `0x0007'5110`, `0x0007'5111`, `0x0007'5112`, and `0x0007'5113` are unverified transfer-only"
        "product-owned `epoch2dwalk-v0751-*` paths"
        "Checksummed `EPPO51` state"
        "exact publicly consumable `EpochSimEngine 2.5.29`"
        "Raw inference gate"
        "scripted gait replacement"
        "network joint command beside the effective powered motor command")
    string(FIND "${v0751_release_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.51 focused release document missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/docs/EPOCH2D_WALK_ENGINE_LIBRARY.md"
    epoch2dwalk_library_doc_text)
foreach(reference IN ITEMS
        "CanonicalSnapshot` schema 5"
        "public integration API version is `7`"
        "retained-policy provenance schema 3"
        "19 canonical nodes"
        "84 observations"
        "physical two-hand equipment ABI and historical 76-observation/13-action policy layout first entered under training semantics `0x0007'5109`"
        "`0x0007'5110` is the historical foundational-input/mixed-origin boundary"
        "Current semantics are `0x0007'5115`"
        "`0x0007'5112` is the support-feedback observation boundary"
        "`0x0007'5113` is the CrouchWalk authority boundary"
        "retained integration uses the artifact's recorded guidance"
        "None of `0x0007'5109`, `0x0007'5110`, `0x0007'5111`, `0x0007'5112`, or `0x0007'5113` can resume under `5115`"
        "explicit unverified-transfer path"
        "Epoch2DWalkEngine-v0.7.51-runner-windows-x64.zip"
        "Epoch2DWalkEngine-v0.7.51-runner-linux-x86_64.tar.gz"
        "Epoch2DWalkEngine-v0.7.51-dev-windows-x64.zip"
        "Epoch2DWalkEngine-v0.7.51-dev-linux-x86_64.tar.gz"
        "Epoch2DWalkEngine-v0.7.51-source.zip"
        "15-object release set")
    string(FIND "${epoch2dwalk_library_doc_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "Epoch2DWalkEngine v0.7.51 library/release documentation missing: ${reference}")
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
        "WALK-SHUTTLE-POSTURE-414"
        "WALK-SINGLE-AND-AVIAN-SUPPORT-415"
        "WALK-SCOPED-STATS-416"
        "WALK-RELEASE-417"
        "WALK-IMMUTABLE-MACRO-TERRAIN-418"
        "WALK-SELF-PROPELLED-RETURN-419"
        "WALK-RELEASE-420"
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
        "WALK-RELEASE-384"
        "WALK-LIMB-FACING-385"
        "WALK-HAND-SCALE-386"
        "WALK-GRANULAR-SKY-387"
        "WALK-RELEASE-388"
        "WALK-MATERIAL-TRUTH-389"
        "WALK-WEAPON-RUNTIME-390"
        "WALK-PROPORTION-GAIT-391"
        "WALK-RIGLAB-LAYOUT-392"
        "WALK-RIGLAB-WORLD-393"
        "WALK-FACING-IK-394"
        "WALK-STRICT-HUMANOID-395"
        "WALK-CROUCH-RIG-RESET-396"
        "WALK-RIG-LEDGER-397"
        "WALK-RELEASE-398"
        "WALK-PIP-PROPORTIONS-399"
        "WALK-BACKWARD-BRACE-400"
        "WALK-LINEAGE-TRUTH-401"
        "WALK-RELEASE-402"
        "WALK-REPOSITORY-CLEANUP-403"
        "WALK-AUTHORED-GAIT-404"
        "WALK-ENGAGEMENT-FIRE-405"
        "WALK-SHUTTLE-LESSON-406"
        "WALK-TERRAIN-LIFECYCLE-407"
        "WALK-RELEASE-408"
        "WALK-FORWARD-POSTURE-409"
        "WALK-MULTISUPPORT-PROGRESS-410"
        "WALK-GRANULAR-COHERENCE-411"
        "WALK-HYBRID-RUNTIME-BRAIN-413"
        "WALK-RELEASE-412"
        "WALK-AUTHORED-POSE-437"
        "WALK-LIVE-MORPHOLOGY-438"
        "WALK-RIG-DIAGNOSTICS-439"
        "WALK-HUMAN-MODULE-SCALE-440"
        "WALK-SPECIES-ATTACHMENTS-441"
        "WALK-STANCE-GAIT-442"
        "WALK-RELEASE-443"
        "WALK-DOG-ART-444"
        "WALK-SPECIES-RIG-BEAUTY-445"
        "WALK-RELEASE-446"
        "WALK-HUMAN-ANIMATION-447"
        "WALK-HUMAN-STANCE-448"
        "WALK-SPECIES-ART-FIT-449"
        "WALK-RELEASE-450"
        "WALK-ZOMBIE-LEGACY-451"
        "WALK-HUMAN-NATURAL-GAIT-452"
        "WALK-SIMENGINE-MATERIAL-CELLS-453"
        "WALK-TURN-MASTERY-454"
        "WALK-SPECIES-RIG-RECOVERY-455"
        "WALK-SPECIES-PRESENTATION-456"
        "WALK-RIGLAB-FULL-LIVE-457"
        "WALK-MASTERY-COMMIT-458"
        "WALK-RELEASE-459"
        "WALK-SPECIES-ART-TRUTH-460"
        "WALK-HUMAN-CONTACT-GAIT-461"
        "WALK-HUMAN-ARM-COUNTERSWING-462"
        "WALK-V0748-ACCEPTANCE-463"
        "WALK-ENGINE-LIBRARY-464"
        "WALK-ENGINE-EXAMPLE-465"
        "WALK-ENGINE-CROSSPLATFORM-466"
        "WALK-ENGINE-SITE-467"
        "WALK-RAW-POLICY-PROOF-468"
        "Current-identity/semantics decision (2026-09-05)"
        "rename only its persistence namespace from `runner-v0751-*` to `epoch2dwalk-v0751-*`"
        "These remain release-blocking and no long retraining has been started.")
    string(FIND "${mission_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Mission cache continuity contract missing: ${reference}")
    endif()
endforeach()

if(RUNNER_RELEASE_AUDIT_MODE STREQUAL "final")
    execute_process(
        COMMAND git rev-parse HEAD
        WORKING_DIRECTORY "${RUNNER_SOURCE_DIR}"
        OUTPUT_VARIABLE audited_head
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE audited_head_result)
    if(NOT audited_head_result EQUAL 0
            OR NOT "${audited_head}" STREQUAL "${RUNNER_RELEASE_COMMIT}")
        message(FATAL_ERROR
            "Final repository audit is not running against the approved commit")
    endif()
    execute_process(
        COMMAND git diff --quiet --exit-code HEAD --
        WORKING_DIRECTORY "${RUNNER_SOURCE_DIR}"
        RESULT_VARIABLE audited_tree_diff_result)
    if(NOT audited_tree_diff_result EQUAL 0)
        message(FATAL_ERROR
            "Final repository audit refuses tracked source bytes that differ from the approved commit")
    endif()
    execute_process(
        COMMAND git tag --list "${RUNNER_RELEASE_TAG}"
        WORKING_DIRECTORY "${RUNNER_SOURCE_DIR}"
        OUTPUT_VARIABLE existing_release_tag
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE release_tag_query_result)
    if(NOT release_tag_query_result EQUAL 0 OR NOT existing_release_tag STREQUAL "")
        message(FATAL_ERROR
            "Final pre-tag audit refuses an existing ${RUNNER_RELEASE_TAG} tag")
    endif()

    # Phase A archives this exact commit before native review can exist.  Final
    # audit therefore consumes the protected, external Phase-B receipt rather
    # than demanding impossible post-build PASS claims inside the candidate
    # tree.  The verifier binds the receipt to the exact Phase-A object bytes,
    # native bundle, repository/workflow/run identity, and handoff readiness.
    find_program(RUNNER_RELEASE_PYTHON_EXECUTABLE NAMES python3 python REQUIRED)
    execute_process(
        COMMAND "${RUNNER_RELEASE_PYTHON_EXECUTABLE}"
            "${RUNNER_SOURCE_DIR}/tools/strict_sha256_manifest.py"
            verify-promotion-receipt
            --receipt "${RUNNER_RELEASE_PROMOTION_RECEIPT}"
            --signature "${RUNNER_RELEASE_PROMOTION_SIGNATURE}"
            --summary "${RUNNER_RELEASE_HANDOFF_SUMMARY}"
            --asset-root "${RUNNER_RELEASE_ASSET_ROOT}"
            --handoff-root "${RUNNER_RELEASE_HANDOFF_ROOT}"
            --capture-root "${RUNNER_RELEASE_CAPTURE_ROOT}"
            --build-receipt "${RUNNER_RELEASE_BUILD_RECEIPT}"
            --linux-abi "${RUNNER_RELEASE_LINUX_ABI}"
            --capture-bundle "${RUNNER_RELEASE_CAPTURE_BUNDLE}"
            --capture-url "${RUNNER_RELEASE_CAPTURE_URL}"
            --capture-sha256 "${RUNNER_RELEASE_CAPTURE_SHA256}"
            --commit "${RUNNER_RELEASE_COMMIT}"
            --tag "${RUNNER_RELEASE_TAG}"
            --build-run-id "${RUNNER_RELEASE_BUILD_RUN_ID}"
            --build-run-attempt "${RUNNER_RELEASE_BUILD_RUN_ATTEMPT}"
            --promotion-run-id "${RUNNER_RELEASE_PROMOTION_RUN_ID}"
            --promotion-run-attempt "${RUNNER_RELEASE_PROMOTION_RUN_ATTEMPT}"
            --event-name "${RUNNER_RELEASE_EVENT_NAME}"
            --run-sha "${RUNNER_RELEASE_RUN_SHA}"
            --run-ref "${RUNNER_RELEASE_RUN_REF}"
            --workflow-sha "${RUNNER_RELEASE_WORKFLOW_SHA}"
            --workflow-ref "${RUNNER_RELEASE_WORKFLOW_REF}"
            --repository "${RUNNER_RELEASE_REPOSITORY}"
            --workflow ".github/workflows/release.yml"
            --publication-target "${RUNNER_RELEASE_PUBLICATION_TARGET}"
            --github-status "${RUNNER_RELEASE_GITHUB_STATUS}"
            --build-artifact-id "${RUNNER_RELEASE_BUILD_ARTIFACT_ID}"
            --build-artifact-name "${RUNNER_RELEASE_BUILD_ARTIFACT_NAME}"
            --build-artifact-sha256 "${RUNNER_RELEASE_BUILD_ARTIFACT_SHA256}"
            --promotion-inputs-artifact-id "${RUNNER_RELEASE_PROMOTION_INPUTS_ARTIFACT_ID}"
            --promotion-inputs-artifact-name "${RUNNER_RELEASE_PROMOTION_INPUTS_ARTIFACT_NAME}"
            --promotion-inputs-artifact-sha256 "${RUNNER_RELEASE_PROMOTION_INPUTS_ARTIFACT_SHA256}"
            --native-evidence-artifact-id "${RUNNER_RELEASE_NATIVE_EVIDENCE_ARTIFACT_ID}"
            --native-evidence-artifact-name "${RUNNER_RELEASE_NATIVE_EVIDENCE_ARTIFACT_NAME}"
            --native-evidence-artifact-sha256 "${RUNNER_RELEASE_NATIVE_EVIDENCE_ARTIFACT_SHA256}"
        WORKING_DIRECTORY "${RUNNER_SOURCE_DIR}"
        RESULT_VARIABLE promotion_receipt_result
        OUTPUT_VARIABLE promotion_receipt_output
        ERROR_VARIABLE promotion_receipt_error)
    if(NOT promotion_receipt_result EQUAL 0)
        message(FATAL_ERROR
            "Final external promotion receipt audit failed:\n${promotion_receipt_output}${promotion_receipt_error}")
    endif()
endif()

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
        "rig_complexity_cost(candidate)"
        "rig_optimization_ready"
        "retarget_checkpoint_for_rig")
    string(FIND "${curriculum_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Explicit rig optimization routing missing: ${reference}")
    endif()
endforeach()


file(READ "${RUNNER_SOURCE_DIR}/src/autonomy.hpp" autonomy_header_text)
foreach(reference IN ITEMS
        "shuttle_mastery_evidence"
        "metrics.evaluation_distance >= walk_mastery_distance"
        "metrics.evaluation_stride_events >= walk_mastery_stride_events")
    string(FIND "${autonomy_header_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Round-trip turn mastery contract missing: ${reference}")
    endif()
endforeach()
file(READ "${RUNNER_SOURCE_DIR}/src/autonomy_persistence.cpp" autonomy_persistence_text)
foreach(reference IN ITEMS
        "RUNAUTONOMY 29"
        "RUNAUTONOMY 30"
        "version < 16 || version > 30"
        "saved_digest != checkpoint.whole_content_digest"
        "unaided_human != (version == 30)"
        "if (version < 28)"
        "if (version == 28)"
        "if (version >= 27)"
        "if (version >= 17)"
        "director_marker != \"DIRECTOR\""
        "loaded_schema != director::State::schema_version"
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
        "training_semantics_version = 0x0007'5115u"
        "encode_foundational_human_support_observation"
        "human_support_feedback()"
        "guidance_mode == sim::GuidanceMode::raw_policy_audit"
        "RawPpoReceipt"
        "lesson_teacher_authority"
        "crouch_teacher_handoff_update"
        "lesson_update() const noexcept"
        "completed_episode_passes_stage_checks"
        "foundational_walk_teacher_handoff_update"
        "guided_rollout_imitation_weight"
        "strict_evaluation_quality_bit"
        "MotionEvidenceFailure::backward_brace"
        "incremental_locomotion_candidate"
        "multi_support_two_link_teacher_action"
        "multi_support_progress_truth"
        "runtime_safety_authority"
        "biped_stance_vertical_reach"
        "human_casual_arm_rest_target")
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
        message(FATAL_ERROR "Foundational learner contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/deformable_terrain.hpp" terrain_text)
foreach(reference IN ITEMS
        "launch_pad_half_width = 0.70f"
        "launch_transition_width = 0.55f"
        "sandhybrid::SceneCell state"
        "structural_aux = 0x04000000u"
        "authored_metadata = 0x01u"
        "cells_[nearest_index(course_x)].height"
        "an observation only; it may not excavate, compact, or raise a berm"
        "TerrainRegion::shallow_water"
        "TerrainRegion::hole"
        "static_assert(world_bottom == -25.3125f)"
        "static_assert(world_top == 25.3125f)"
        "sandhybrid::resident_ground_boundary_offset"
        "sandhybrid::resident_transition_material"
        "sandhybrid::resident_ground_sample")
    string(FIND "${terrain_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Active-terrain contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/tests/deformable_terrain_tests.cpp" immutable_terrain_test_text)
foreach(reference IN ITEMS
        "authored macro/fine cells changed under repeated contact pressure"
        "excavation modified immutable authored course cells"
        "explicit dropped-cell excavation failed or was nondeterministic"
        "falling-cell cascade did not expose an unsafe mixed-material window"
        "canonical 640x360 map does not straddle the authored surface"
        "canonical map leaves the visible underground viewport as fallback fill"
        "canonical map omitted a SandHybrid surface or substrate layer"
        "different seeds produced an identical canonical substrate map")
    string(FIND "${immutable_terrain_test_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.40 immutable-cell terrain regression contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/tests/v0732_shuttle_facing_tests.cpp" shuttle_facing_test_text)
foreach(reference IN ITEMS
        "outbound.set_course_motion_enabled(true)"
        "outbound.course_speed()==0.0f&&outbound.course_progress()==0.0f"
        "shuttle lesson inherited a moving course frame"
        "turn state did not physically reflect the articulated plant"
        "facing-local teacher or a valid directed physical plant diverged under reflection"
        "post-handoff return moved left while backpedaling or without a real gait"
        "physical teacher shuttle traversed by dragging or backward bracing"
        "repeated return traversal accepted leftward backpedaling")
    string(FIND "${shuttle_facing_test_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.40 self-propelled shuttle regression contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/training_explainer.hpp" explainer_text)
foreach(reference IN ITEMS
        "training_work"
        "result.training_work * 0.80f + result.mastery * 0.20f"
        "PASSED STAGE CHECKS"
        "FINAL CHECKS WAITING"
        "FINAL CHECKS RUNNING"
        "FINAL CHECKS COMPLETE")
    string(FIND "${explainer_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Truthful telemetry contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/app.cpp" app_text)
foreach(reference IN ITEMS
        "LESSON COMPLETION"
        "TRAINING LEVEL"
        "PASSED STAGE CHECKS"
        "FAILED STAGE CHECKS"
        "FEATURES CLEARED"
        "creature_species_paths"
        "PACKAGED COURSE EYE TEST"
        "PACKAGED ORTHOGRAPHIC ART TEST"
        "STRICT SIDE ELEVATION - NO PERSPECTIVE OR FORESHORTENING"
        "PACKAGED RETAINED WALK PROOF"
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
        "optional_shin_art"
        "presentation_pixel_scale(scale)"
        "PRIOR LINEAGE"
        "BACKWARD BRACE PREVIEW")
    string(FIND "${app_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.28 application contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/simulation.hpp" species_paths_text)
foreach(reference IN ITEMS
        "struct CreatureSpeciesPaths"
        "epoch2dwalk-v0751-"
        "-autosave.eppo"
        "-evolved.rig"
        "-autonomy.state")
    string(FIND "${species_paths_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "Product-owned species path contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/autonomy_commands.cpp" autonomy_commands_text)
foreach(reference IN ITEMS
        "current_prefix{"
        "epoch2dwalk-v0751-"
        "prior_prefix{ \"runner-v0750-\" }"
        "legacy_lifetime_version = \"V0.7.50\""
        "LIFETIME LEDGER IMPORT QUEUED - CURRENT TRAINING STARTS FRESH")
    string(FIND "${autonomy_commands_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.50 lifetime-only checkpoint migration contract missing: ${reference}")
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
foreach(reference IN ITEMS
        [=[std::filesystem::path{ "README.md" }]=]
        [=[std::filesystem::path{ "CHANGELOG.md" }]=]
        [=[std::filesystem::path{ "LICENSE" }]=]
        [=[std::filesystem::path{ "missioncache.md" }]=]
        [=[std::string_view{ "# Epoch2DWalkEngine" }]=]
        [=[std::string_view{ "## 0.7.51" }]=]
        [=[std::string_view{ "MIT License" }]=]
        [=[std::string_view{ "## v0.7.51 direct visual acceptance" }]=]
        "input.is_open()"
        "input.bad()")
    string(FIND "${main_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "Packaged-runtime top-level document validation missing: ${reference}")
    endif()
endforeach()
string(FIND "${main_text}" "!input.eof()" stale_document_eof_pos)
if(NOT stale_document_eof_pos EQUAL -1)
    message(FATAL_ERROR
        "Packaged-runtime document validation still mistakes iterator completion for stream eof state")
endif()
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
string(FIND "${main_text}" "--diagnose-hybrid-brain" hybrid_brain_diagnostic_pos)
if(hybrid_brain_diagnostic_pos EQUAL -1)
    message(FATAL_ERROR "v0.7.37 packaged hybrid-brain diagnostic launch contract missing")
endif()
string(FIND "${main_text}" "--diagnose-speed-walk" speed_walk_diagnostic_pos)
if(speed_walk_diagnostic_pos EQUAL -1)
    message(FATAL_ERROR "v0.7.51 packaged Speed Walk graph diagnostic launch contract missing")
endif()
string(FIND "${main_text}" "--diagnose-raw-inference" raw_inference_diagnostic_pos)
if(raw_inference_diagnostic_pos EQUAL -1)
    message(FATAL_ERROR
        "v0.7.51 retained-network Raw inference release gate is not implemented")
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
        "telemetry::latest_test_title"
        "telemetry::latest_test_explanation"
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
        "draw_authored_pixel_art"
        "art::articulated_boot_transform"
        "rig.support_branch_mask(motor)"
        "rig.node_support_mask(index)"
        "prepare_art_diagnostic_rig"
        "prepare_art_fallen_eye_test"
        "impl_->live_art_enabled = impl_->optional_foot_art.loaded()"
        "impl_->rig_lab_art_enabled = false")
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
        "micro_motion_window"
        "CasualGaitEvidence"
        "bounded_casual_window"
        "human_casual_gait_plan")
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
        "RigCase{ \"human\", sim::CreatureBlueprint::humanoid() }"
        "RigCase{ \"dog\", sim::CreatureBlueprint::crawler4() }"
        "RigCase{ \"chicken\", sim::CreatureBlueprint::chicken() }"
        "RigCase{ \"hexapod\", sim::CreatureBlueprint::hexapod() }"
        "release_distance(cases[index].blueprint)"
        "release_gait_cycles(cases[index].blueprint)"
        "WalkEyeTestProof run_walk_eye_test_proof"
        "left_air != right_air")
    string(FIND "${rig_training_diagnostic_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.42 four-rig cold-start acceptance contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/rig_training_diagnostic.hpp" rig_training_diagnostic_header_text)
string(FIND "${rig_training_diagnostic_header_text}"
    "std::array<RigTrainingResult, 4>" four_rig_report_pos)
if(four_rig_report_pos EQUAL -1)
    message(FATAL_ERROR "v0.7.42 production rig-training report does not contain exactly four canonical rigs")
endif()

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
        "'E', 'P', 'P', 'O', '4', '2'"
        "v0741_checkpoint_magic"
        "'E', 'P', 'P', 'O', '4', '1'"
        "v0740_checkpoint_magic"
        "'E', 'P', 'P', 'O', '4', '0'"
        "v0739_checkpoint_magic"
        "'E', 'P', 'P', 'O', '3', '9'"
        "v0738_checkpoint_magic"
        "'E', 'P', 'P', 'O', '3', '8'"
        "'E', 'P', 'P', 'O', '3', '7'"
        "v0736_checkpoint_magic"
        "'E', 'P', 'P', 'O', '3', '6'"
        "v0735_checkpoint_magic"
        "'E', 'P', 'P', 'O', '3', '4'"
        "v0733_checkpoint_magic"
        "v0732_checkpoint_magic"
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

# The release must be dispatched against an immutable pre-tag commit. Every
# diagnostic is required in both build-tree and independently extracted
# Windows/Linux runs; a single occurrence cannot stand in for a package test.
foreach(diagnostic IN ITEMS
        "--version"
        "--diagnose-vulkan"
        "--diagnose-package"
        "--diagnose-acceptance"
        "--diagnose-camera"
        "--diagnose-ui"
        "--diagnose-art"
        "--diagnose-rig-training"
        "--diagnose-walk-eye"
        "--diagnose-raw-inference"
        "--diagnose-speed-walk"
        "--diagnose-course"
        "--diagnose-hybrid-brain")
    string(REGEX MATCHALL "${diagnostic}" diagnostic_workflow_matches
        "${release_workflow_text}")
    list(LENGTH diagnostic_workflow_matches diagnostic_workflow_count)
    if(diagnostic_workflow_count LESS 4)
        message(FATAL_ERROR
            "Release workflow does not run ${diagnostic} in all four build-tree/extracted matrices")
    endif()
endforeach()
string(FIND "${app_text}" "telemetry::final_check_label" stale_final_check_pos)
if(NOT stale_final_check_pos EQUAL -1)
    message(FATAL_ERROR
        "Removed compact final-check label returned to the progressive Details UI")
endif()

foreach(reference IN ITEMS
        "name: Epoch2DWalkEngine audited cross-platform release"
        "workflow_dispatch:"
        "phase:"
        "default: build"
        "commit_sha:"
        "release_tag:"
        "default: site"
        "build_run_id:"
        "native_capture_bundle_url:"
        "native_capture_bundle_sha256:"
        [=[ref: ${{ inputs.commit_sha }}]=]
        [=[test "$(git rev-parse HEAD)" = "$RUNNER_CANDIDATE_SHA"]=]
        [=[test "$GITHUB_WORKFLOW_SHA" = "$RUNNER_CANDIDATE_SHA"]=]
        [=[test "$GITHUB_REF" = refs/heads/main]=]
        [=[test "$GITHUB_WORKFLOW_REF" = "$GITHUB_REPOSITORY/.github/workflows/release.yml@$GITHUB_REF"]=]
        [=[if: inputs.phase == 'build']=]
        [=[if: inputs.phase == 'promote']=]
        "RUNNER_RELEASE_AUDIT_MODE=candidate"
        "RUNNER_RELEASE_AUDIT_MODE=final"
        "cmake --preset linux-release --fresh"
        "ctest --preset linux-release"
        "cmake --build --preset windows-release --target RunnerVisualCaptures --parallel"
        "windows-2025"
        "ubuntu-24.04"
        "gcc-14"
        "Xvfb"
        "VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json"
        "llvmpipe|lavapipe"
        "2048x1152x24"
        "Epoch2DWalkEngineInstalledConsumer"
        "linux-split-prefix-consumer"
        "windows-split-prefix-consumer"
        "README.md CHANGELOG.md LICENSE missioncache.md"
        "docs/third-party/sdl3-LICENSE.txt"
        "docs/third-party/vulkan-loader-LICENSE.txt"
        "docs/third-party/EpochSimEngine/README.md"
        [=[ldd "$runtime_audit/Runner"]=]
        "libvulkan.so.1"
        "not found"
        "highest_required_glibc="
        "highest_required_glibcxx="
        "tools/strict_sha256_manifest.py write"
        "tools/strict_sha256_manifest.py verify"
        "tools/strict_sha256_manifest.py checksum"
        "tools/strict_sha256_manifest.py verify-git-tree"
        "tools/strict_sha256_manifest.py verify-capture-bundle"
        "tools/strict_sha256_manifest.py write-promotion-receipt"
        "tools/strict_sha256_manifest.py verify-publication"
        "tests/strict_sha256_manifest_tests.py -v"
        "--max-redirs 0"
        "https://epoch.adamrushford.chatgpt.site/downloads/candidates/v0.7.51/"
        "native-captures.zip"
        [=[sha256sum "$runtime_archive" > "$runtime_archive.sha256"]=]
        [=[sha256sum "$sdk_archive" > "$sdk_archive.sha256"]=]
        "[Text.UTF8Encoding]::new($false)"
        "run.sh"
        "run.bat"
        "epoch2d-$tag-installed-unrelated-linux"
        "epoch2d-$tag-installed-unrelated-windows"
        [=[(cd "$installed_unrelated" && "$installed_launcher" "$argument")]=]
        [=[& (Join-Path $runtime "run.bat") $argument]=]
        [=[(cd "$unrelated" && "$runtime_audit/run.sh" "$argument")]=]
        [=[& (Join-Path $runtimeAudit "run.bat") $argument]=]
        "git archive --format=zip"
        "Epoch2DWalkEngine-$tag-runner-windows-x64.zip"
        "Epoch2DWalkEngine-$tag-runner-linux-x86_64.tar.gz"
        "Epoch2DWalkEngine-$tag-dev-windows-x64.zip"
        "Epoch2DWalkEngine-$tag-dev-linux-x86_64.tar.gz"
        "Epoch2DWalkEngine-$tag-source.zip"
        "Epoch2DWalkEngine-$tag-source.manifest.sha256"
        "--exclude-prefix release-assets/"
        "NUL-safe exact-commit source audit failed"
        "epoch2dwalk-v0751-release"
        "assemble-build-candidate:"
        "validate-promotion:"
        "BUILD-RECEIPT.txt"
        "phase=build"
        "build_run_attempt=$GITHUB_RUN_ATTEMPT"
        [=[run-id: ${{ inputs.build_run_id }}]=]
        [=[actions/runs/$RUNNER_BUILD_RUN_ID]=]
        "build_artifact_sha256"
        "promotion_inputs_artifact_sha256"
        "native_evidence_artifact_sha256"
        "artifact-digest"
        "RUNNER_PROTECTED_RELEASE_APPROVAL"
        "PROMOTION-RECEIPT.json"
        "hmac-sha256"
        "GITHUB_RUN_ATTEMPT"
        "GITHUB_REPOSITORY"
        "RUNNER_RELEASE_PROMOTION_RECEIPT"
        "object_count=15"
        [=[if: inputs.phase == 'promote' && inputs.publication_target == 'github']=]
        "GitHub publication unavailable; no tag or release was created."
        "GitHub tag availability probe failed; no tag or release was created."
        "gh api"
        "gh pr list"
        "git ls-remote --exit-code --tags"
        "gh release delete"
        "gh release create"
        "gh release download"
        "gh release edit"
        "public_download=\"$RUNNER_TEMP/epoch2d-$tag-github-public\""
        "env -u GH_TOKEN -u GITHUB_TOKEN"
        "--retry-all-errors"
        [=[uses: actions/checkout@11d5960a326750d5838078e36cf38b85af677262]=]
        [=[uses: actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02]=]
        [=[uses: actions/download-artifact@d3f86a106a0bac45b974a628896c90dbdf5c8093]=])
    string(FIND "${release_workflow_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Release workflow audit contract missing: ${reference}")
    endif()
endforeach()

string(FIND "${release_workflow_text}"
    "cmake --build --preset windows-release --target RunnerVisualCaptures --parallel"
    visual_capture_build_pos)
string(FIND "${release_workflow_text}"
    "ctest --preset windows-release --output-on-failure -V"
    windows_ctest_pos)
if(visual_capture_build_pos EQUAL -1 OR windows_ctest_pos EQUAL -1
        OR NOT visual_capture_build_pos LESS windows_ctest_pos)
    message(FATAL_ERROR
        "Phase A must explicitly build and execute RunnerVisualCaptures before Windows CTest")
endif()

string(FIND "${release_workflow_text}"
    [=[(cd "$installed_unrelated" && "$installed_launcher" "$argument")]=]
    linux_installed_launcher_pos)
string(FIND "${release_workflow_text}"
    [=[tar -C "$runtime" -czf "$runtime_archive" .]=]
    linux_runtime_archive_pos)
string(FIND "${release_workflow_text}"
    [=[(cd "$unrelated" && "$runtime_audit/run.sh" "$argument")]=]
    linux_extracted_launcher_pos)
if(linux_installed_launcher_pos EQUAL -1 OR linux_runtime_archive_pos EQUAL -1
        OR linux_extracted_launcher_pos EQUAL -1
        OR NOT linux_installed_launcher_pos LESS linux_runtime_archive_pos
        OR NOT linux_runtime_archive_pos LESS linux_extracted_launcher_pos)
    message(FATAL_ERROR
        "Linux runtime must pass its installed-tree launcher before archive and its extracted launcher afterward")
endif()

string(FIND "${release_workflow_text}"
    [=[& (Join-Path $runtime "run.bat") $argument]=]
    windows_installed_launcher_pos)
string(FIND "${release_workflow_text}"
    [=[Compress-Archive -LiteralPath (Get-ChildItem -LiteralPath $runtime -Force | ForEach-Object FullName)]=]
    windows_runtime_archive_pos)
string(FIND "${release_workflow_text}"
    [=[& (Join-Path $runtimeAudit "run.bat") $argument]=]
    windows_extracted_launcher_pos)
if(windows_installed_launcher_pos EQUAL -1 OR windows_runtime_archive_pos EQUAL -1
        OR windows_extracted_launcher_pos EQUAL -1
        OR NOT windows_installed_launcher_pos LESS windows_runtime_archive_pos
        OR NOT windows_runtime_archive_pos LESS windows_extracted_launcher_pos)
    message(FATAL_ERROR
        "Windows runtime must pass its installed-tree launcher before archive and its extracted launcher afterward")
endif()

foreach(unpinned_action IN ITEMS
        "actions/checkout@v"
        "actions/upload-artifact@v"
        "actions/download-artifact@v")
    string(FIND "${release_workflow_text}" "${unpinned_action}" unpinned_action_pos)
    if(NOT unpinned_action_pos EQUAL -1)
        message(FATAL_ERROR
            "Release-path action is not pinned to a full commit SHA: ${unpinned_action}")
    endif()
endforeach()
string(REGEX MATCHALL "uses: [^\n\r]+" release_action_lines
    "${release_workflow_text}")
foreach(release_action_line IN LISTS release_action_lines)
    string(REGEX MATCH "uses: [^@]+@([0-9a-f]+)" action_sha_match
        "${release_action_line}")
    string(LENGTH "${CMAKE_MATCH_1}" action_sha_length)
    if(action_sha_match STREQUAL "" OR NOT action_sha_length EQUAL 40)
        message(FATAL_ERROR
            "Release-path action is not pinned to a full commit SHA: ${release_action_line}")
    endif()
endforeach()
string(REGEX MATCHALL "RUNNER_PROTECTED_RELEASE_APPROVAL"
    protected_secret_mentions "${release_workflow_text}")
list(LENGTH protected_secret_mentions protected_secret_mention_count)
if(NOT protected_secret_mention_count EQUAL 2)
    message(FATAL_ERROR
        "Protected HMAC secret must occur only as one step-scoped key/value binding")
endif()
string(FIND "${release_workflow_text}"
    [=[          RUNNER_PROTECTED_RELEASE_APPROVAL: ${{ secrets.RUNNER_PROTECTED_RELEASE_APPROVAL }}]=]
    signing_step_secret_pos)
if(signing_step_secret_pos EQUAL -1)
    message(FATAL_ERROR
        "Protected HMAC secret is not scoped to the signing/verifying step")
endif()
string(FIND "${release_workflow_text}"
    "    env:\n      RUNNER_PROTECTED_RELEASE_APPROVAL:"
    job_scoped_protected_secret_pos)
if(NOT job_scoped_protected_secret_pos EQUAL -1)
    message(FATAL_ERROR
        "Protected HMAC secret must not be job-scoped")
endif()

string(FIND "${release_workflow_text}" "push:\n    tags:" tag_trigger_pos)
if(NOT tag_trigger_pos EQUAL -1)
    message(FATAL_ERROR "Release workflow still builds only after a release tag exists")
endif()
string(FIND "${release_workflow_text}"
    "RUNNER_NATIVE_VISUAL_ACCEPTED_COMMIT" tracked_capture_claim_pos)
if(NOT tracked_capture_claim_pos EQUAL -1)
    message(FATAL_ERROR
        "Release workflow uses an impossible self-referential tracked capture acceptance claim")
endif()
string(FIND "${release_workflow_text}" "release-approval-and-site-handoff:" approval_job_pos)
string(FIND "${release_workflow_text}" "assemble-build-candidate:" build_assembly_job_pos)
string(FIND "${release_workflow_text}" "validate-promotion:" promotion_validation_job_pos)
string(FIND "${release_workflow_text}" "github-publication:" github_job_pos)
string(FIND "${release_workflow_text}" [=[git tag -a "$tag"]=] tag_create_pos)
if(build_assembly_job_pos EQUAL -1 OR promotion_validation_job_pos EQUAL -1 OR
        approval_job_pos EQUAL -1 OR github_job_pos EQUAL -1 OR tag_create_pos EQUAL -1 OR
        NOT build_assembly_job_pos LESS promotion_validation_job_pos OR
        NOT promotion_validation_job_pos LESS approval_job_pos OR
        NOT approval_job_pos LESS github_job_pos OR NOT github_job_pos LESS tag_create_pos)
    message(FATAL_ERROR
        "Two-phase build, capture validation, protected promotion, and tag ordering is invalid")
endif()

math(EXPR approval_section_length "${github_job_pos} - ${approval_job_pos}")
string(SUBSTRING "${release_workflow_text}" ${approval_job_pos}
    ${approval_section_length} approval_section_text)
string(FIND "${approval_section_text}" "cmp --" approval_runtime_compare_pos)
string(FIND "${approval_section_text}" "handoff=site-handoff" approval_handoff_pos)
string(FIND "${approval_section_text}"
    "tools/strict_sha256_manifest.py write-promotion-receipt"
    approval_receipt_write_pos)
string(FIND "${approval_section_text}"
    "-DRUNNER_RELEASE_AUDIT_MODE=final" approval_final_audit_pos)
string(FIND "${approval_section_text}"
    "Revalidate protected-run context and transferred artifact identities"
    approval_context_revalidation_pos)
string(FIND "${approval_section_text}"
    [=[RUNNER_PROTECTED_RELEASE_APPROVAL: ${{ secrets.RUNNER_PROTECTED_RELEASE_APPROVAL }}]=]
    approval_secret_exposure_pos)
if(approval_runtime_compare_pos EQUAL -1 OR approval_handoff_pos EQUAL -1 OR
        approval_receipt_write_pos EQUAL -1 OR approval_final_audit_pos EQUAL -1 OR
        approval_context_revalidation_pos EQUAL -1 OR approval_secret_exposure_pos EQUAL -1 OR
        NOT approval_context_revalidation_pos LESS approval_secret_exposure_pos OR
        NOT approval_runtime_compare_pos LESS approval_handoff_pos OR
        NOT approval_handoff_pos LESS approval_receipt_write_pos OR
        NOT approval_receipt_write_pos LESS approval_final_audit_pos)
    message(FATAL_ERROR
            "Protected promotion must compare exact native/runtime bytes, assemble the handoff, authenticate its external receipt, then run final audit")
endif()

string(LENGTH "${release_workflow_text}" release_workflow_length)
math(EXPR github_section_length "${release_workflow_length} - ${github_job_pos}")
string(SUBSTRING "${release_workflow_text}" ${github_job_pos}
    ${github_section_length} github_section_text)
string(FIND "${github_section_text}" "gh release create" github_draft_create_pos)
string(FIND "${github_section_text}"
    "tools/strict_sha256_manifest.py verify-publication"
    github_prepublication_audit_pos)
string(FIND "${github_section_text}" "gh release edit" github_publish_pos)
if(github_draft_create_pos EQUAL -1 OR github_prepublication_audit_pos EQUAL -1 OR
        github_publish_pos EQUAL -1 OR
        NOT github_draft_create_pos LESS github_prepublication_audit_pos OR
        NOT github_prepublication_audit_pos LESS github_publish_pos)
    message(FATAL_ERROR
        "GitHub publication must byte-audit the exact draft before making it public")
endif()
math(EXPR github_public_tail_length
    "${github_section_length} - ${github_publish_pos}")
string(SUBSTRING "${github_section_text}" ${github_publish_pos}
    ${github_public_tail_length} github_public_tail_text)
string(FIND "${github_public_tail_text}"
    "env -u GH_TOKEN -u GITHUB_TOKEN" github_anonymous_download_pos)
string(FIND "${github_public_tail_text}"
    "tools/strict_sha256_manifest.py verify-publication"
    github_postpublication_audit_pos)
string(FIND "${github_public_tail_text}"
    "cleanup_needed=0" github_rollback_disable_pos)
if(github_anonymous_download_pos EQUAL -1 OR
        github_postpublication_audit_pos EQUAL -1 OR
        github_rollback_disable_pos EQUAL -1 OR
        NOT github_anonymous_download_pos LESS github_postpublication_audit_pos OR
        NOT github_postpublication_audit_pos LESS github_rollback_disable_pos)
    message(FATAL_ERROR
        "GitHub rollback must remain armed through anonymous post-publication byte/hash verification")
endif()

# Native evidence cannot precede or cause the authoritative platform build. The
# promotion phase must consume an already completed run and must not rebuild or
# repackage the Windows candidate it compares with the capture bundle.
string(FIND "${release_workflow_text}"
    "tools/strict_sha256_manifest.py verify-capture-bundle"
    first_capture_verification_pos)
if(first_capture_verification_pos EQUAL -1 OR
        first_capture_verification_pos LESS promotion_validation_job_pos)
    message(FATAL_ERROR
        "Native capture evidence is still required before the immutable build candidate exists")
endif()
math(EXPR promotion_section_length
    "${approval_job_pos} - ${promotion_validation_job_pos}")
string(SUBSTRING "${release_workflow_text}" ${promotion_validation_job_pos}
    ${promotion_section_length} promotion_validation_text)
foreach(forbidden_promotion_step IN ITEMS
        "cmake --build"
        "Compress-Archive"
        "git archive --format=zip")
    string(FIND "${promotion_validation_text}" "${forbidden_promotion_step}"
        forbidden_promotion_pos)
    if(NOT forbidden_promotion_pos EQUAL -1)
        message(FATAL_ERROR
            "Promotion rebuilds or repackages an immutable phase-one artifact: ${forbidden_promotion_step}")
    endif()
endforeach()
string(FIND "${release_workflow_text}" "v0.7.50" stale_release_version_pos)
if(NOT stale_release_version_pos EQUAL -1)
    message(FATAL_ERROR
        "Release workflow still contains a current v0.7.50 release claim")
endif()
string(FIND "${release_workflow_text}"
    "Epoch2DWalkEngine-$tag-dev-windows-x86_64" stale_windows_sdk_arch_pos)
if(NOT stale_windows_sdk_arch_pos EQUAL -1)
    message(FATAL_ERROR
        "Release workflow uses the nonexistent windows-x86_64 SDK asset spelling")
endif()

file(READ "${RUNNER_SOURCE_DIR}/.github/workflows/runner-pr-validation.yml"
    pr_workflow_text)
foreach(reference IN ITEMS
        "-DRUNNER_BUILD_APP=OFF"
        "-DRUNNER_BUILD_TESTS=ON"
        "RUNNER_BUILD_APP=OFF unexpectedly discovered Python"
        "test ! -d build/pr-linux/generated-assets"
        "Runner\\.V0720Ui|Runner\\.V0724StructuralMetricsIcon")
    string(FIND "${pr_workflow_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "Platform-neutral app-off CI boundary missing: ${reference}")
    endif()
endforeach()
string(FIND "${pr_workflow_text}"
    "sudo apt-get install -y g++-14 ninja-build python3"
    stale_headless_python_install_pos)
if(NOT stale_headless_python_install_pos EQUAL -1)
    message(FATAL_ERROR
        "Platform-neutral app-off CI still installs Python")
endif()
file(READ "${RUNNER_SOURCE_DIR}/.gitattributes" gitattributes_text)
string(FIND "${gitattributes_text}" "release-assets export-ignore" release_assets_export_ignore_pos)
if(release_assets_export_ignore_pos EQUAL -1)
    message(FATAL_ERROR "Exact-commit source archives can include prior release binaries")
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

message(STATUS "Epoch2DWalkEngine v0.7.51 repository hygiene passed")
