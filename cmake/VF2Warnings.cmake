function(vf2_set_project_warnings target_name)
    if(WIN32 AND NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_definitions(${target_name} PRIVATE
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS
            _CRT_NONSTDC_NO_DEPRECATE
        )
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
        if(VF2_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
        # The ASan/UBSan flags used to live only in the non-MSVC branch, so
        # VF2_ENABLE_SANITIZERS=ON was a silent no-op on the canonical Windows
        # checkout - the gate reported as "run" while instrumenting nothing.
        # A gate that cannot fail is worse than no gate, so MSVC gets the
        # flags too and the configure step says which one is in effect.
        if(VF2_ENABLE_SANITIZERS)
            target_compile_options(${target_name} PRIVATE /fsanitize=address /Zi)
            target_link_options(${target_name} PRIVATE /INCREMENTAL:NO)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
            -Wstrict-prototypes
        )
        if(VF2_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()

        if(VF2_ENABLE_SANITIZERS)
            target_compile_options(${target_name} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
            )
            target_link_options(${target_name} PRIVATE
                -fsanitize=address,undefined
            )
        endif()
    endif()
endfunction()
