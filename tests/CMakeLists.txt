
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

# test-UpdA.exe
add_executable(test-UpdA test-UpdA.c)
target_link_libraries(test-UpdA PRIVATE WonRes)

# test-UpdW.exe
add_executable(test-UpdW test-UpdW.c)
target_link_libraries(test-UpdW PRIVATE WonRes)
