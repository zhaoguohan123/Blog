cmake_minimum_required(VERSION 3.10)

project(ZghCppPro)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.15")
    # this is for MSVC_RUNTIME_LIBRARY property below
    cmake_policy(SET CMP0091 NEW)
endif()

# 清除build文件下的东西
file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/build)

if(MSVC)
    # see https://stackoverflow.com/questions/28178978/how-to-generate-pdb-files-for-release-build-with-cmake-flags/31264946
    # release 模式下生成调试日志
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi /MT")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF" CACHE STRING "" FORCE)

    set(COMMSUPPW_LIBRARY comsuppw)

else(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -s")

    set(COMMSUPPW_LIBRARY)
endif(MSVC)

# DWINVER=0x0601: 指定了windows的平台或版本
add_definitions(-DUNICODE -D_UNICODE -DOLDMSVCRT -DWINVER=0x0601 -DNOMINMAX)

# 包含文件
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/CppDesignModel)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/CommonTools)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ComLib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Service)
include_directories("C:/Boost/include/boost-1_82")

# 引入boost库 begin
LINK_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/ComLib/)
link_directories("C:/Boost/lib")
# 引入boost库 end

# file(GLOB...) RELATIVE:相对路径  所有源文件用COMMON_FILES代替
file(GLOB COMMON_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} Common/*.cpp Common/*.h)
source_group(common FILES ${COMMON_FILES})

# ----------------------- Get proc info ------------------------------------------ #
file(GLOB GET_PROC_INFO_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CommonTools/GetProcInfo/*.cpp CommonTools/GetProcInfo/*.h)
source_group(GetProcInfo FILES ${GET_PROC_INFO_FILES})

# -----------------------GetTaskMsgProc------------------------------------------ #
file(GLOB ASSIST_COLLECT_POC_INFO_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CommonTools/AssistCollectProc/*.cpp CommonTools/AssistCollectProc/*.h)
source_group(AssistCollectProc FILES ${ASSIST_COLLECT_POC_INFO_FILES})

# -----------------------SetWinSound----------------------------------------------#
file(GLOB SET_WINDOWS_SOUND RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CommonTools/SetWinSound/*.cpp CommonTools/SetWinSound/*.h)
source_group(SetWinSound FILES ${SET_WINDOWS_SOUND})

# ----------------------- project ListConfigIni------------------------------------------ #
file(GLOB LIST_CONFIG_INI_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CommonTools/ListConfigIni/*.cpp CommonTools/ListConfigIni/*.h)
source_group(ListConfigIni FILES ${LIST_CONFIG_INI_FILES})

# ----------------------- project JobQueue------------------------------------------ #
file(GLOB JOB_QUEUE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CommonTools/JobQueue/*.cpp CommonTools/JobQueue/*.h)
source_group(JobQueue FILES ${JOB_QUEUE_FILES})

# ---------------------- project Protect Process --------------------------------------------- #
file(GLOB PROTEST_PROCESS_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CommonTools/ProcessProtect/*.cpp CommonTools/ProcessProtect/*.h)
source_group(ProcessProtect FILES ${PROTEST_PROCESS_FILES})

# ----------------------- project CppDesignModel ------------------------------------------ #
file(GLOB CPPDESIGNMODEL_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CppDesignModel/*.cpp CppDesignModel/*.h)
source_group(CppDesignModel FILES ${CPPDESIGNMODEL_FILES})



add_executable(CppDesignModel ${COMMON_FILES} ${CPPDESIGNMODEL_FILES} )

set_target_properties(CppDesignModel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/CppDesignModel"
)

# ----------------------- project CppDesignModel------------------------------------------ #

# ----------------------- project Service ------------------------------------------ #
file(GLOB SERVICE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} Service/*.cpp Service/*.h)
source_group(Service FILES ${SERVICE_FILES})

add_executable(Service ${COMMON_FILES} ${SERVICE_FILES} )

set_target_properties(Service PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/Service"
)
# ----------------------- project Service ------------------------------------------ #

# ----------------------- project CommonTools------------------------------------------ #
file(GLOB COMMON_TOOLS_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CommonTools/*.cpp CommonTools/*.h)
source_group(CommonTools FILES ${COMMON_TOOLS_FILES})

add_compile_options("/std:c++17")
add_executable(CommonTools
                ${COMMON_FILES}
                ${COMMON_TOOLS_FILES}
                ${GET_PROC_INFO_FILES}
                ${ASSIST_COLLECT_POC_INFO_FILES}
                ${SET_WINDOWS_SOUND}
                ${LIST_CONFIG_INI_FILES}
                ${JOB_QUEUE_FILES}
                ${PROTEST_PROCESS_FILES}
                )
target_link_libraries(CommonTools ${Boost_LIBRARIES})
target_link_libraries(CommonTools ${CMAKE_CURRENT_SOURCE_DIR}/ComLib/spdlog/x64/lib/spdlog.lib)
# 添加库文件到项目

# 修改编译属性: 1.指定输出目录
set_target_properties(CommonTools PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/CommonTools"
)

# ----------------------- project CommonTools------------------------------------------ #
