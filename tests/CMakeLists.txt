
# test-Types.exe
add_executable(test-Types test-Types.c)
target_link_libraries(test-Types PRIVATE WonRes)

# test-Names.exe
add_executable(test-Names test-Names.c)
target_link_libraries(test-Names PRIVATE WonRes)

# test-Langs.exe
add_executable(test-Langs test-Langs.c)
target_link_libraries(test-Langs PRIVATE WonRes)
