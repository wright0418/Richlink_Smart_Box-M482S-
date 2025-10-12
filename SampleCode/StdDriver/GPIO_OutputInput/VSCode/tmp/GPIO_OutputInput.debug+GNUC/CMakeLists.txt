cmake_minimum_required(VERSION 3.27)

# Roots
include("../roots.cmake")

set(CONTEXT GPIO_OutputInput.debug+GNUC)
set(TARGET ${CONTEXT})
set(OUT_DIR "${SOLUTION_ROOT}/out/GPIO_OutputInput/GNUC/debug")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BIN_FILE "GPIO_OutputInput.bin")
set(LD_SCRIPT "${SOLUTION_ROOT}/../../../../Library/Device/Nuvoton/M480/Source/GCC/gcc_arm.ld")
set(LD_SCRIPT_PP ${LD_SCRIPT})

# Processor Options
set(CPU Cortex-M4)
set(FPU SP_FPU)

# Toolchain config map
include("toolchain.cmake")

# Setup project
project(${CONTEXT} LANGUAGES ASM C)

# Compilation database
add_custom_target(database
  COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json" "${OUT_DIR}"
)
set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})

# Setup context
add_executable(${CONTEXT})
set_target_properties(${CONTEXT} PROPERTIES PREFIX "" SUFFIX ".elf" OUTPUT_NAME "GPIO_OutputInput")
set_target_properties(${CONTEXT} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR})
add_library(${CONTEXT}_GLOBAL INTERFACE)

# Includes
target_include_directories(${CONTEXT} PUBLIC
  $<$<COMPILE_LANGUAGE:C,CXX>:
    "${SOLUTION_ROOT}/../../../../Library/Device/Nuvoton/M480/Include"
    "${SOLUTION_ROOT}/../../../../Library/StdDriver/inc"
    "${SOLUTION_ROOT}/../../../../Library/CMSIS/Include"
  >
  "${CMSIS_PACK_ROOT}/Nuvoton/NuMicroM4_DFP/1.0.1/Device/M480/Include"
)

# Defines
target_compile_definitions(${CONTEXT} PUBLIC
)

# Compile options
target_compile_options(${CONTEXT} PUBLIC
  $<$<COMPILE_LANGUAGE:ASM>:
    "SHELL:${ASM_CPU}"
    "SHELL:${ASM_FLAGS}"
  >
  $<$<COMPILE_LANGUAGE:C>:
    "SHELL:${CC_CPU}"
    "SHELL:${CC_FLAGS}"
    "SHELL:-std=gnu11"
    "SHELL:--specs=nano.specs"
    "SHELL:-Wno-return-type"
    "SHELL:-Wno-unused-but-set-variable"
    "SHELL:-fdata-sections"
    "SHELL:-ffunction-sections"
    "SHELL:-fno-builtin"
    "SHELL:-fshort-enums"
    "SHELL:-funsigned-char"
    "SHELL:-mthumb"
    "SHELL:-Wno-main"
    "SHELL:-gdwarf-4"
  >
)

# Compile Options Abstractions
add_library(${CONTEXT}_ABSTRACTIONS INTERFACE)
cbuild_set_options_flags(ASM "debug" "" "" "" ASM_OPTIONS_FLAGS_${CONTEXT})
cbuild_set_options_flags(CC "debug" "" "" "" CC_OPTIONS_FLAGS_${CONTEXT})
target_compile_options(${CONTEXT}_ABSTRACTIONS INTERFACE
  $<$<COMPILE_LANGUAGE:ASM>:
    "SHELL:${ASM_OPTIONS_FLAGS_${CONTEXT}}"
  >
  $<$<COMPILE_LANGUAGE:C>:
    "SHELL:${CC_OPTIONS_FLAGS_${CONTEXT}}"
  >
)

# Add groups and components
include("groups.cmake")
include("components.cmake")

target_link_libraries(${CONTEXT} PUBLIC
  Group_CMSIS
  Group_User
  Group_Library
)

# Linker options
target_link_options(${CONTEXT} PUBLIC
  "SHELL:${LD_CPU}"
  "SHELL:${_LS}\"${LD_SCRIPT_PP}\""
  "SHELL:-Xlinker"
  "SHELL:--gc-sections"
  "SHELL:--specs=nano.specs"
  "SHELL:-Wl,-Map,\"${SOLUTION_ROOT}/out/GPIO_OutputInput/GNUC/debug/GPIO_OutputInput.map\""
)
set_target_properties(${CONTEXT} PROPERTIES LINK_DEPENDS ${LD_SCRIPT})

# Bin Conversion
 add_custom_command(TARGET ${CONTEXT} POST_BUILD COMMAND ${CMAKE_OBJCOPY} ${ELF2BIN})
