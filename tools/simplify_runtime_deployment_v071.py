from pathlib import Path

path = Path("CMakeLists.txt")
text = path.read_text(encoding="utf-8")
old = '''    add_custom_command(TARGET EpochRunner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:EpochRunner>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${EPOCHRUNNER_SHADER_OUTPUT_DIR}"
            "$<TARGET_FILE_DIR:EpochRunner>/shaders"
        VERBATIM
    )

    if(WIN32)
        add_custom_command(TARGET EpochRunner POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:EpochRunner>
                $<TARGET_FILE_DIR:EpochRunner>
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()

    install(TARGETS EpochRunner RUNTIME DESTINATION .)
    if(WIN32)
        install(FILES $<TARGET_RUNTIME_DLLS:EpochRunner> DESTINATION .)
    endif()
'''
new = '''    add_custom_command(TARGET EpochRunner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:EpochRunner>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${EPOCHRUNNER_SHADER_OUTPUT_DIR}"
            "$<TARGET_FILE_DIR:EpochRunner>/shaders"
        VERBATIM
    )

    install(TARGETS EpochRunner RUNTIME DESTINATION .)
'''
if old not in text:
    raise RuntimeError("missing generated runtime dependency deployment block")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
