if (NOT DEFINED PROJECT_SOURCE_DIR OR NOT DEFINED BINARY_DIR OR NOT DEFINED WSJT_ENABLE_OMNIRIG)
  message (FATAL_ERROR "构建边界测试缺少 CMake 参数")
endif ()

file (READ "${PROJECT_SOURCE_DIR}/CMakeLists.txt" cmake_source)
file (READ "${PROJECT_SOURCE_DIR}/TransceiverFactory.cpp" factory_source)

if (NOT cmake_source MATCHES "option[ \t\r\n]*\\([ \t\r\n]*WSJT_ENABLE_OMNIRIG[^\\)]*ON[ \t\r\n]*\\)")
  message (FATAL_ERROR "WSJT_ENABLE_OMNIRIG 没有保持默认 ON")
endif ()
if (NOT cmake_source MATCHES "if[ \t\r\n]*\\([ \t\r\n]*WIN32[ \t\r\n]+AND[ \t\r\n]+WSJT_ENABLE_OMNIRIG[ \t\r\n]*\\)")
  message (FATAL_ERROR "Windows OmniRig 路径没有受明确构建选项控制")
endif ()
if (NOT factory_source MATCHES "#if defined \\(WIN32\\) && defined \\(WSJT_ENABLE_OMNIRIG\\)")
  message (FATAL_ERROR "OmniRig 工厂注册没有受明确构建选项控制")
endif ()

file (READ "${BINARY_DIR}/CMakeCache.txt" cmake_cache)
if (WSJT_ENABLE_OMNIRIG STREQUAL "OFF")
  if (NOT cmake_cache MATCHES "WSJT_ENABLE_OMNIRIG:BOOL=OFF")
    message (FATAL_ERROR "OFF 配置的 CMakeCache 没有记录 WSJT_ENABLE_OMNIRIG=OFF")
  endif ()
elseif (WSJT_ENABLE_OMNIRIG STREQUAL "ON")
  if (NOT cmake_cache MATCHES "WSJT_ENABLE_OMNIRIG:BOOL=ON")
    message (FATAL_ERROR "ON 配置的 CMakeCache 没有记录 WSJT_ENABLE_OMNIRIG=ON")
  endif ()
else ()
  message (FATAL_ERROR "WSJT_ENABLE_OMNIRIG 必须为 ON 或 OFF")
endif ()
if (WSJT_ENABLE_OMNIRIG STREQUAL "OFF" AND cmake_cache MATCHES "(^|\\n)DUMPCPP")
  message (FATAL_ERROR "OFF 配置仍查找或缓存了 dumpcpp")
endif ()

set (target_metadata "")
foreach (metadata_file
    "${BINARY_DIR}/CMakeFiles/wsjt_qt.dir/DependInfo.cmake"
    "${BINARY_DIR}/CMakeFiles/wsjt_qt.dir/build.make"
    "${BINARY_DIR}/CMakeFiles/wsjt_qt.dir/flags.make"
    "${BINARY_DIR}/CMakeFiles/wsjt_qt.dir/link.txt"
    "${BINARY_DIR}/CMakeFiles/jtdx.dir/link.txt"
    "${BINARY_DIR}/CMakeFiles/jtdx.dir/linkLibs.rsp"
    "${BINARY_DIR}/CMakeFiles/jtdx.rsp"
    "${BINARY_DIR}/CMakeFiles/jtdx.dir/link.rsp"
    "${BINARY_DIR}/build.ninja")
  if (EXISTS "${metadata_file}")
    file (READ "${metadata_file}" metadata)
    string (APPEND target_metadata "\n${metadata}")
  endif ()
endforeach ()

if (NOT EXISTS "${BINARY_DIR}/build.ninja"
    AND NOT EXISTS "${BINARY_DIR}/CMakeFiles/jtdx.dir/link.txt"
    AND NOT EXISTS "${BINARY_DIR}/CMakeFiles/jtdx.dir/linkLibs.rsp"
    AND NOT EXISTS "${BINARY_DIR}/CMakeFiles/jtdx.rsp")
  message (FATAL_ERROR "未找到 jtdx 最终可执行文件的链接元数据")
endif ()

if (NOT target_metadata MATCHES "HamlibTransceiver\\.cpp")
  message (FATAL_ERROR "${WSJT_ENABLE_OMNIRIG} 配置缺少 Hamlib 后端")
endif ()
if (NOT target_metadata MATCHES "TCITransceiver\\.cpp|HRDTransceiver\\.cpp|DXLabSuiteCommanderTransceiver\\.cpp")
  message (FATAL_ERROR "${WSJT_ENABLE_OMNIRIG} 配置缺少其他正常后端")
endif ()

if (WSJT_ENABLE_OMNIRIG STREQUAL "OFF")
  if (target_metadata MATCHES "OmniRigTransceiver\\.cpp|Qt5AxContainer|Qt5AxBase|WSJT_ENABLE_OMNIRIG")
    message (FATAL_ERROR "OFF 配置的目标元数据仍包含 OmniRig/ActiveQt wrapper")
  endif ()
  message (STATUS "OmniRig OFF 构建边界：PASS；Hamlib/其他后端保留")
else ()
  if (NOT target_metadata MATCHES "OmniRigTransceiver\\.cpp")
    message (FATAL_ERROR "ON 配置的目标元数据缺少 OmniRigTransceiver.cpp")
  endif ()
  if (NOT target_metadata MATCHES "Qt5AxContainer|Qt5AxBase")
    message (FATAL_ERROR "ON 配置的目标元数据缺少 ActiveQt wrapper 链接")
  endif ()
  if (NOT target_metadata MATCHES "WSJT_ENABLE_OMNIRIG")
    message (FATAL_ERROR "ON 配置的目标元数据缺少 WSJT_ENABLE_OMNIRIG 编译宏")
  endif ()
  message (STATUS "OmniRig ON 构建边界：PASS；COM/ActiveQt/Rig 1/2 与 Hamlib/其他后端保留")
endif ()
