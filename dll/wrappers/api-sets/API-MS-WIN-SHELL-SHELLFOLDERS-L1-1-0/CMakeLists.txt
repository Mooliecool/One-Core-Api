
add_definitions(-D__WINESRC__)
include_directories(${REACTOS_SOURCE_DIR}/include/reactos/wine)
spec2def(API-MS-WIN-SHELL-SHELLFOLDERS-L1-1-0.dll API-MS-WIN-SHELL-SHELLFOLDERS-L1-1-0.spec)

list(APPEND SOURCE
    API-MS-WIN-SHELL-SHELLFOLDERS-L1-1-0.rc
    ${CMAKE_CURRENT_BINARY_DIR}/API-MS-WIN-SHELL-SHELLFOLDERS-L1-1-0_stubs.c
    ${CMAKE_CURRENT_BINARY_DIR}/API-MS-WIN-SHELL-SHELLFOLDERS-L1-1-0.def)

add_library(API-MS-WIN-SHELL-SHELLFOLDERS-L1-1-0 SHARED ${SOURCE})
set_module_type(API-MS-WIN-SHELL-SHELLFOLDERS-L1-1-0 win32dll ENTRYPOINT 0)
target_link_libraries(API-MS-WIN-SHELL-SHELLFOLDERS-L1-1-0 wine)
add_importlibs(API-MS-WIN-SHELL-SHELLFOLDERS-L1-1-0 shell32)
add_cd_file(TARGET API-MS-WIN-SHELL-SHELLFOLDERS-L1-1-0 DESTINATION reactos/system32 FOR all)
