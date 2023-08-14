## Copyright (c) 2012, 2013, 2014 Aldebaran Robotics. All rights reserved.
## Use of this source code is governed by a BSD-style license that can be
## found in the COPYING file.
##
## qi CMake project
## ================
##
## Parameters:
## -----------
## QI_VERSION
## ~~~~~~~~~~
## A version number that will be used for this project. If this variable is
## undefined, CMake will read the project version number from the
## `project.json` file in the source directory.
##
## BUILD_TESTING
## ~~~~~~~~~~~~~
## If set to true, enables building the tests. See the documentation of the
## `CTest` module for more details on this variable.
##
## WITH_SYSTEMD
## ~~~~~~~~~~~~
## If set to true, enables building libqi with usage of systemd libraries.
## This variable is ignored if pkg-config is not found on the system or if
## the systemd library is not found using pkg-config.
##
## WITH_BOOST_LOCALE
## ~~~~~~~~~~~~~~~~~
## If set to true, enables building libqi with usage of Boost locale library.
## This variable is ignored if the library was not found in Boost.
##
## Targets
## -------
## The targets defined by the project are:
## - `qi::ka`: header-only library.
## - `qi::qi`: shared library.
## - `qi::testssl`: static library, links with `qi::qi`.
## - `qi::testsession`: static library, links with `qi::qi`.
##
## All targets are defined for:
## - libqi as a CMake subproject (through `add_subdirectory`).
## - a build directory of the project.
## - an installation of the project.
##
## The last two cases are implemented by generating package configuration
## files (`X-config.cmake`) in the build directory and in the installation
## directory.

cmake_minimum_required(VERSION 3.23)

# - Parse project description file
file(READ "${CMAKE_CURRENT_LIST_DIR}/project.json" project_json)

# - Define the project version number.
if(NOT QI_VERSION)
  string(JSON QI_VERSION GET "${project_json}" version)
endif()

include(cmake/ParseVersion.cmake)
parse_version(QI "${QI_VERSION}")
message(STATUS "Project version is ${QI_VERSION_FULL} (interpreted as ${QI_VERSION} for CMake)")

cmake_minimum_required(VERSION 3.5)
project(naoqi_libqi)
find_package(ament_cmake REQUIRED)
find_package(OpenSSL REQUIRED)

include(CMakeDependentOption)
include(CheckSymbolExists)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(FeatureSummary)
include(cmake/BuildType.cmake)
include(cmake/MakePackageConfigFile.cmake)

# We always want boost filesystem v3
add_definitions("-DBOOST_FILESYSTEM_VERSION=3")
add_definitions("-DBOOST_ASIO_DISABLE_STD_CHRONO")

if(MSVC)
  add_definitions(" -DWIN32_LEAN_AND_MEAN -DNOMINMAX")
endif()
option(BUILD_TESTING "Build the testing tree." "${build_testing_default}")
include(CTest)

if(CMAKE_CXX_COMPILER_ID STREQUAL Clang OR CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  set(IS_GCC_OR_CLANG TRUE)
else()
  set(IS_GCC_OR_CLANG FALSE)
endif()

# Enable more warnings. This is set globally for every targets of this project,
# but is not reflected on the interface of this project libraries. Consumers
# of these libraries are free to add (or not) these flags themselves.
if(IS_GCC_OR_CLANG)
  add_compile_options(-Wall -Wextra)
elseif(MSVC)
  add_compile_options(/W3)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# Remove this line to use QT if usable
set(WITH_QT5_CORE OFF)

# Add QI_WITH_TESTS definitions to all targets in the build, to enable export
# of some internal symbols needed by tests.
if(BUILD_TESTING)
  add_compile_definitions(QI_WITH_TESTS)
endif()

# Output everything directly in predefined directories of the build tree.
# This is required by the SDK layout implementation.
# Also write a "path.conf" file, which is required for executing some tests.
set(sdk_dir "${CMAKE_BINARY_DIR}/sdk")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${sdk_dir}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${sdk_dir}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${sdk_dir}/lib")
set(path_conf_dir "${sdk_dir}/share/qi")
set(path_conf_file_path "${path_conf_dir}/path.conf")
if(NOT EXISTS "${path_conf_file_path}")
  file(MAKE_DIRECTORY "${path_conf_dir}")
  file(TOUCH "${path_conf_file_path}")
endif()

##############################################################################
# External Packages
##############################################################################
# Boost
set(
  boost_required_components
    atomic
    chrono
    date_time
    filesystem
    program_options
    random
    regex
    thread
)
find_package(
  Boost REQUIRED
  COMPONENTS ${boost_required_components}
  OPTIONAL_COMPONENTS locale
)
# Used by "qi-config.cmake".
set(BOOST_VERSION "${Boost_VERSION}")

set(with_boost_locale_descr
  "Enable building libqi with usage of Boost Locale libraries."
)
# We do not rely on the "Boost_LOCALE_FOUND" variable normally set by the
# FindBoost CMake module because Conan generated files for Boost fail to
# declare which components are found or not.
set(has_boost_locale FALSE)
if(TARGET Boost::locale)
  set(has_boost_locale TRUE)
endif()
cmake_dependent_option(
  WITH_BOOST_LOCALE
  "${with_boost_locale_descr}"
  ON has_boost_locale OFF
)
add_feature_info(
    WITH_BOOST_LOCALE
    WITH_BOOST_LOCALE
    "${with_boost_locale_descr}"
)

set(BOOST_COMPONENTS ${boost_required_components})
if(WITH_BOOST_LOCALE)
  list(APPEND BOOST_COMPONENTS locale)
endif()

# OpenSSL
find_package(OpenSSL REQUIRED)

# Parse the exact version number to remove the letter suffix so we may
# reuse it in find_package.
parse_version(OPENSSL "${OPENSSL_VERSION}")

# Threads
if(UNIX)
  find_package(Threads REQUIRED)
endif()

# systemd
set(has_systemd FALSE)
find_package(PkgConfig MODULE)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(SYSTEMD systemd)
  set(has_systemd SYSTEMD_FOUND)
endif()

set(with_systemd_descr
  "Enable building libqi with usage of systemd libraries."
)
cmake_dependent_option(
  WITH_SYSTEMD
  "${with_systemd_descr}"
  OFF # Disabled by default.
  has_systemd OFF
)
add_feature_info(
  WITH_SYSTEMD
  WITH_SYSTEMD
  "${with_systemd_descr}"
)

##############################################################################
# Convenience library: cxx_standard
##############################################################################
add_library(cxx_standard INTERFACE)
add_library(qi::internal::cxx_standard ALIAS cxx_standard)

# The project requires at least C++17.
target_compile_features(
  cxx_standard
  INTERFACE
    cxx_std_17
)

set_target_properties(
  cxx_standard
  PROPERTIES
    EXPORT_NAME internal::cxx_standard
)

set(KA_H
  ka/algorithm.hpp
  ka/base64.hpp
  ka/ark/inputiter.hpp
  ka/ark/mutable.hpp
  ka/concept.hpp
  ka/conceptpredicate.hpp
  ka/empty.hpp
  ka/errorhandling.hpp
  ka/flatten.hpp
  ka/functional.hpp
  ka/functor.hpp
  ka/functorcontainer.hpp
  ka/indexed.hpp
  ka/integersequence.hpp
  ka/iterator.hpp
  ka/macro.hpp
  ka/macroregular.hpp
  ka/memory.hpp
  ka/moveoncopy.hpp
  ka/mutablestore.hpp
  ka/mutex.hpp
  ka/opt.hpp
  ka/productiter.hpp
  ka/range.hpp
  ka/relationpredicate.hpp
  ka/scoped.hpp
  ka/sha1.hpp
  ka/src.hpp
  ka/testutils.hpp
  ka/typetraits.hpp
  ka/unit.hpp
  ka/uri.hpp
  ka/uri/io.hpp
  ka/uri/uri_fwd.hpp
  ka/uri/uri.hpp
  ka/uri/parsing.hpp
  ka/utility.hpp
)

#### }}}

qi_install_header(${QI_H} ${QITYPE_H} ${QIM_H} ${QIPERF_H} ${KA_H} KEEP_RELATIVE_PATHS)

  PRIVATE
    src/application.cpp
    src/application_p.hpp
    src/buffer.cpp
    src/buffer_p.hpp
    src/bufferreader.cpp
    src/clock.cpp
    src/consoleloghandler.cpp
    src/csvloghandler.cpp
    src/dlfcn.cpp
    src/eventloop.cpp
    src/eventloop_p.hpp
    src/fileloghandler.cpp
    src/future.cpp
    src/headfileloghandler.cpp
    src/iocolor.cpp
    src/locale-light.cpp
    src/log.cpp
    src/log_p.hpp
    src/messaging/applicationsession.cpp
    src/messaging/applicationsession_internal.cpp
    src/messaging/applicationsession_internal.hpp
    src/messaging/authprovider.cpp
    src/messaging/authprovider_p.hpp
    src/messaging/boundobject.cpp
    src/messaging/boundobject.hpp
    src/messaging/clientauthenticator.cpp
    src/messaging/clientauthenticator_p.hpp
    src/messaging/message.cpp
    src/messaging/message.hpp
    src/messaging/messagedispatcher.cpp
    src/messaging/messagedispatcher.hpp
    src/messaging/messagesocket.cpp
    src/messaging/messagesocket.hpp
    src/messaging/objecthost.cpp
    src/messaging/objecthost.hpp
    src/messaging/objectregistrar.cpp
    src/messaging/objectregistrar.hpp
    src/messaging/remoteobject.cpp
    src/messaging/remoteobject_p.hpp
    src/messaging/server.cpp
    src/messaging/server.hpp
    src/messaging/servicedirectory.cpp
    src/messaging/servicedirectory.hpp
    src/messaging/servicedirectory_p.hpp
    src/messaging/servicedirectoryclient.cpp
    src/messaging/servicedirectoryclient.hpp
    src/messaging/servicedirectoryproxy.cpp
    src/messaging/serviceinfo.cpp
    src/messaging/serviceinfo_p.hpp
    src/messaging/session.cpp
    src/messaging/session_p.hpp
    src/messaging/sessionservice.cpp
    src/messaging/sessionservice.hpp
    src/messaging/sessionservices.cpp
    src/messaging/sessionservices.hpp
    src/messaging/sock/accept.hpp
    src/messaging/sock/common.hpp
    src/messaging/sock/concept.hpp
    src/messaging/sock/connect.hpp
    src/messaging/sock/connectedstate.hpp
    src/messaging/sock/connectingstate.hpp
    src/messaging/sock/disconnectedstate.hpp
    src/messaging/sock/disconnectingstate.hpp
    src/messaging/sock/error.hpp
    src/messaging/sock/macrolog.hpp
    src/messaging/sock/networkasio.cpp
    src/messaging/sock/networkasio.hpp
    src/messaging/sock/option.hpp
    src/messaging/sock/receive.hpp
    src/messaging/sock/resolve.hpp
    src/messaging/sock/send.hpp
    src/messaging/sock/socketptr.hpp
    src/messaging/sock/socketwithcontext.hpp
    src/messaging/sock/sslcontextptr.hpp
    src/messaging/sock/traits.hpp
    src/messaging/ssl/ssl.cpp
    src/messaging/streamcontext.cpp
    src/messaging/streamcontext.hpp
    src/messaging/tcpmessagesocket.cpp
    src/messaging/tcpmessagesocket.hpp
    src/messaging/tcpscheme.cpp
    src/messaging/transportserver.cpp
    src/messaging/transportserver.hpp
    src/messaging/transportserverasio_p.cpp
    src/messaging/transportserverasio_p.hpp
    src/messaging/transportsocketcache.cpp
    src/messaging/transportsocketcache.hpp
    src/messaging/uri.cpp
    src/messaging/url.cpp
    src/os.cpp
    src/path.cpp
    src/path_conf.cpp
    src/perf/dataperf.cpp
    src/perf/dataperf_p.hpp
    src/perf/dataperfsuite.cpp
    src/perf/dataperfsuite_p.hpp
    src/perf/measure.cpp
    src/periodictask.cpp
    src/print.cpp
    src/ptruid.cpp
    src/registration.cpp
    src/sdklayout-boost.cpp
    src/sdklayout.hpp
    src/strand.cpp
    src/tailfileloghandler.cpp
    src/type/anyfunction.cpp
    src/type/anymodule.cpp
    src/type/anyobject.cpp
    src/type/anyreference.cpp
    src/type/anyvalue.cpp
    src/type/binarycodec.cpp
    src/type/binarycodec_p.hpp
    src/type/dynamicobject.cpp
    src/type/dynamicobjectbuilder.cpp
    src/type/genericobject.cpp
    src/type/jsoncodec_p.hpp
    src/type/jsondecoder.cpp
    src/type/jsonencoder.cpp
    src/type/manageable.cpp
    src/type/metamethod.cpp
    src/type/metamethod_p.hpp
    src/type/metaobject.cpp
    src/type/metaobject_p.hpp
    src/type/metaproperty.cpp
    src/type/metasignal.cpp
    src/type/metasignal_p.cpp
    src/type/metasignal_p.hpp
    src/type/objecttypebuilder.cpp
    src/type/signal.cpp
    src/type/signal_p.hpp
    src/type/signalspy.cpp
    src/type/signature.cpp
    src/type/signatureconvertor.cpp
    src/type/signatureconvertor.hpp
    src/type/staticobjecttype.cpp
    src/type/structtypeinterface.cpp
    src/type/traceanalyzer.cpp
    src/type/type.cpp
    src/type/typeinterface.cpp
    src/utils.cpp
    src/utils.hpp
    src/version.cpp
)

qi_stage_cmake(qimodule-config.cmake)
include(qimodule-config.cmake)

qi_create_lib(qi SHARED
    ${QI_H}     ${QI_C}
    ${QITYPE_H} ${QITYPE_C}
    ${QIM_H}    ${QIM_C}
    ${QIPERF_H} ${QIPERF_C}
    ${QI_SOCKET_H}
    ${KA_H}
  DEPENDS
    ASSUME_SYSTEM_INCLUDE
    BOOST
    BOOST_ATOMIC
    BOOST_DATE_TIME
    BOOST_THREAD
    BOOST_CHRONO
    BOOST_FILESYSTEM
    BOOST_LOCALE
    BOOST_REGEX
    BOOST_PROGRAM_OPTIONS
    BOOST_RANDOM
  SUBMODULE ${_tp_qi}
)



#### Add optional libs {{{
if(WIN32)
  qi_use_lib(qi WSOCK IPHLPAPI PSAPI WSA) #Really?
endif()

qi_use_lib(qi OPENSSL)

if(WITH_BOOST_LOCALE)
  target_compile_definitions(
    qi_objects
    PRIVATE
      WITH_BOOST_LOCALE
  )
  target_sources(
    qi_objects
    PUBLIC
      FILE_SET source_headers
        TYPE HEADERS
        FILES
          qi/translator.hpp
    PRIVATE
      src/translator.cpp
  )
  target_link_libraries(
    qi_objects
    PRIVATE
      Boost::locale
  )
endif()

if (WITH_SYSTEMD)
  qi_use_lib(qi SYSTEMD)
endif()

if (UNIX)
  qi_use_lib(qi PTHREAD)
endif()

if(ANDROID)
  target_link_libraries(qi_objects PUBLIC log)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
  target_link_libraries(qi_objects PUBLIC dl rt)
endif()

if(WIN32)
  target_link_libraries(qi_objects PUBLIC wsock32 ws2_32 iphlpapi psapi)
endif()
#### }}}

qi_stage_lib(qi INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/compat")

# Install a share/qi/path.conf so that libqi's sdkPrefix can be found
file(WRITE "${CMAKE_BINARY_DIR}/path.conf" "# Installed by qibuild. Do not remove\n")
qi_install_data(${CMAKE_BINARY_DIR}/path.conf SUBFOLDER "qi")

option(BUILD_EXAMPLES "build examples" ON)
if (BUILD_EXAMPLES)
  add_subdirectory("examples")
endif()

##############################################################################
feature_summary(WHAT ALL)

ament_export_include_directories(include)
ament_export_libraries(qi)
# ament_package(CONFIG_EXTRAS libqi-extras.cmake.em)
ament_package()