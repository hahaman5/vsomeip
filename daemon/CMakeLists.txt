# Copyright (C) 2015-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8)

# Daemon
add_executable(vsomeipd vsomeipd.cpp)
if (${STATIC_BUILD} MATCHES "ON")
  target_link_libraries(vsomeipd vsomeip vsomeip-cfg vsomeip-sd ${Boost_LIBRARIES} ${DL_LIBRARY} ${DLT_LIBRARIES})
else()
  target_link_libraries(vsomeipd vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY} ${DLT_LIBRARIES})
endif ()

install (
    TARGETS vsomeipd
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
)

###################################################################################################
