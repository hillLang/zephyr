zephyr_library()

zephyr_library_sources(
  x86_64.c
  xuk.c
  xuk-stubs-copy.c   # <-- generated, see below
)

# We want to include two non-x86_64 stubs as sections/symbols in our
# link (one 16 bit code for SMP real mode bootstraping, the other a 32
# bit hook for OS protected mode entry).  This is tedious to do with
# the linker directly, so the mechanism picked here is to have a C
# file (which really is all assembly) import them with ".incbin"
# statements.  But the location of the generated output doesn't match
# our build directory, so instead of trying to preprocess our C file
# to contain the proper path we just copy it to the binary directory.
# As it happens, cmake is very happy to detect and use a C file
# located in CMAKE_CURRENT_BINARY_DIR.
#
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xuk-stubs-copy.c
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_CURRENT_SOURCE_DIR}/xuk-stubs.c
          ${CMAKE_CURRENT_BINARY_DIR}/xuk-stubs-copy.c
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/xuk-stub16.bin
          ${CMAKE_CURRENT_BINARY_DIR}/xuk-stub32.bin
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xuk-stub16.bin
  COMMAND ${CMAKE_C_COMPILER} -m16 -Os ${X86_64_BASE_CFLAGS}  -imacros ${AUTOCONF_H}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/xuk-stub16.c
    -o ${CMAKE_CURRENT_BINARY_DIR}/xuk-stub16.o
  COMMAND ${CMAKE_OBJCOPY} -O binary -j .text
    ${CMAKE_CURRENT_BINARY_DIR}/xuk-stub16.o
    ${CMAKE_CURRENT_BINARY_DIR}/xuk-stub16.bin
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xuk-stub32.bin
  COMMAND ${CMAKE_C_COMPILER} -m32 -Os ${X86_64_BASE_CFLAGS} -imacros ${AUTOCONF_H}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/xuk-stub32.c
    -o ${CMAKE_CURRENT_BINARY_DIR}/xuk-stub32.o
  COMMAND ${CMAKE_C_COMPILER} -m32 ${X86_64_BASE_CFLAGS}
    -Wl,--build-id=none -nostdlib -nodefaultlibs -nostartfiles
    -T ${CMAKE_CURRENT_SOURCE_DIR}/xuk-stub32.ld
    ${CMAKE_CURRENT_BINARY_DIR}/xuk-stub32.o
    -o ${CMAKE_CURRENT_BINARY_DIR}/xuk-stub32.elf
  COMMAND ${CMAKE_OBJCOPY} -O binary
    ${CMAKE_CURRENT_BINARY_DIR}/xuk-stub32.elf
    ${CMAKE_CURRENT_BINARY_DIR}/xuk-stub32.bin
)

# The zephyr.elf file generated for an x86_64 binary is a 64 bit
# binary, but Qemu requires a traditional i386 file (because the entry
# point from multiboot is in 386 protected mode).  Do a relink dance
# with objcopy to convert.
#
set(qkernel_file ${CMAKE_BINARY_DIR}/zephyr-qemu.elf)
add_custom_target(qemu_kernel_target DEPENDS ${qkernel_file})
add_custom_command(
  OUTPUT ${qkernel_file}
  COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_BINARY_DIR}/zephyr/zephyr.elf ${CMAKE_CURRENT_BINARY_DIR}/zephyr-qemu.bin
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/qemuinc.c ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_C_COMPILER} -m32 -c ${CMAKE_CURRENT_BINARY_DIR}/qemuinc.c -o ${CMAKE_CURRENT_BINARY_DIR}/zephyr-qemu.o
  COMMAND ${CMAKE_C_COMPILER} -m32 -T ${CMAKE_CURRENT_SOURCE_DIR}/xuk64.ld
          -Wl,--build-id=none -nostdlib -nodefaultlibs -nostartfiles
          -o ${qkernel_file} ${CMAKE_CURRENT_BINARY_DIR}/zephyr-qemu.o
  )
