function(runner_require_exact_epochsimengine_version
    actual_version required_version resolved_origin)
    if("${actual_version}" STREQUAL "${required_version}")
        return()
    endif()

    if("${actual_version}" STREQUAL "")
        set(actual_display "<missing>")
    else()
        set(actual_display "${actual_version}")
    endif()
    message(FATAL_ERROR
        "Epoch2DWalkEngine requires exact EpochSimEngine ${required_version}, "
        "but ${resolved_origin} reports ${actual_display}. Clear a stale "
        "FETCHCONTENT_SOURCE_DIR_RUNNER_SANDHYBRID override or point "
        "RUNNER_SANDHYBRID_PACKAGE_ROOT at the exact installed dependency.")
endfunction()

# Script-mode entry point used by the deterministic positive/negative CTest
# fixture. Normal project inclusion deliberately only defines the guard.
if(DEFINED RUNNER_VERIFY_EPOCHSIMENGINE_ACTUAL
    AND DEFINED RUNNER_VERIFY_EPOCHSIMENGINE_REQUIRED
    AND DEFINED RUNNER_VERIFY_EPOCHSIMENGINE_ORIGIN)
    runner_require_exact_epochsimengine_version(
        "${RUNNER_VERIFY_EPOCHSIMENGINE_ACTUAL}"
        "${RUNNER_VERIFY_EPOCHSIMENGINE_REQUIRED}"
        "${RUNNER_VERIFY_EPOCHSIMENGINE_ORIGIN}")
endif()
