# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_property(ALLOW_EMPTY TRUE)
zephyr_library_include_directories(${ZEPHYR_BASE}/subsys/bluetooth)
add_subdirectory(hci)
add_subdirectory(custom)

Pets
private
tracker
private
test