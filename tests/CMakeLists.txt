include(CTest)

set(test_script ${CMAKE_CURRENT_LIST_DIR}/run_test.py)

file(GLOB files ${CMAKE_CURRENT_LIST_DIR}/tests/*)
foreach(file ${files})
    get_filename_component(short_name ${file} NAME)
    add_test(
        NAME ${short_name}
        COMMAND ${test_script} $<TARGET_FILE:lfscc> ${file}
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    )
endforeach()
set_tests_properties(${file_without_ext} PROPERTIES TIMEOUT 40)
