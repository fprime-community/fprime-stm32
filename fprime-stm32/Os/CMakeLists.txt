
####
# Per-chip-family OSAL backend dispatch. Add a new family's subdirectory
# here as support for it is added -- each subdirectory gates itself to its
# own platform via restrict_platforms(), since a family's OSAL backend is
# never relevant to any other platform.
####

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Stm32H7")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Stm32F4")
