
# test-Types.exe
add_executable(test-Types test-Types.c)
target_link_libraries(test-Types PRIVATE WonRes)

# test-Names.exe
add_executable(test-Names test-Names.c)
target_link_libraries(test-Names PRIVATE WonRes)

# test-Langs.exe
add_executable(test-Langs test-Langs.c)
target_link_libraries(test-Langs PRIVATE WonRes)

# test-AllA.exe
add_executable(test-AllA test-AllA.c)
target_link_libraries(test-AllA PRIVATE WonRes)

# test-AllW.exe
add_executable(test-AllW test-AllW.c)
target_link_libraries(test-AllW PRIVATE WonRes)

# test-FindA.exe
add_executable(test-FindA test-FindA.c test-Find_res.rc)
target_link_libraries(test-FindA PRIVATE WonRes)

# test-FindW.exe
add_executable(test-FindW test-FindW.c test-Find_res.rc)
target_link_libraries(test-FindW PRIVATE WonRes)

# test-UpdA.exe
add_executable(test-UpdA test-UpdA.c)
target_link_libraries(test-UpdA PRIVATE WonRes)

# test-UpdW.exe
add_executable(test-UpdW test-UpdW.c)
target_link_libraries(test-UpdW PRIVATE WonRes)

# test-StrA.exe
add_executable(test-StrA test-StrA.c test-Str_res.rc)
target_link_libraries(test-StrA PRIVATE WonRes)

# test-StrW.exe
add_executable(test-StrW test-StrW.c test-Str_res.rc)
target_link_libraries(test-StrW PRIVATE WonRes)

# tests
add_test(NAME Types COMMAND test-Types WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME Names COMMAND test-Names WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME Langs COMMAND test-Langs WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME AllA COMMAND test-AllA shell32.dll WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME AllW COMMAND test-AllW shell32.dll WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME FindA COMMAND test-FindA shell32.dll WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME FindW COMMAND test-FindW shell32.dll WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME UpdateA COMMAND test-UpdA WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME UpdateW COMMAND test-UpdW WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME StrA COMMAND test-StrA WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME StrW COMMAND test-StrW WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
