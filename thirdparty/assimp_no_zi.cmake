# Assimp's patch step (see the FetchContent_Declare in CMakeLists.txt here): strips the
# /Zi it hand-appends to its compiler flags so it cannot collide with the project-wide /Z7
# embedded debug format (MSVC D9025). Runs in assimp's source directory; idempotent.
file(READ CMakeLists.txt TBX_ASSIMP_CMAKE_TEXT)
string(REPLACE " /Zi" "" TBX_ASSIMP_CMAKE_TEXT "${TBX_ASSIMP_CMAKE_TEXT}")
file(WRITE CMakeLists.txt "${TBX_ASSIMP_CMAKE_TEXT}")
