cmake_minimum_required(VERSION 3.17)
project(app3d-core
    VERSION 0.0.1
    LANGUAGES CXX
    DESCRIPTION "App3d Core Lib")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fexperimental-library")
add_definitions(-D_LIBCPP_NO_VCRUNTIME)

function(gather_sources target_name)
    set(options RECURSE)
    set(oneValueArgs)
    set(multiValueArgs)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "${options}" "${oneValueArgs}" "${multiValueArgs}")

    set(APP_MODULES_LIST "")

    # Проходим по всем переданным аргументам после target_name
    foreach(BASE_DIR ${ARG_UNPARSED_ARGUMENTS})
        list(APPEND APP_MODULES_LIST "${BASE_DIR}/*.cpp")

        if(APPLE)
            list(APPEND APP_MODULES_LIST "${BASE_DIR}/*.mm")
        endif()
    endforeach()

    set(SOURCE_FILES "" PARENT_SCOPE) # Объявляем переменную на уровне родителя

    foreach(APP_MODULE_PATH ${APP_MODULES_LIST})
        if(ARG_RECURSE)
            file(GLOB_RECURSE APP_MODULE_SRC ${APP_MODULE_PATH})
        else()
            file(GLOB APP_MODULE_SRC ${APP_MODULE_PATH})
        endif()

        if(WIN32)
            list(FILTER APP_MODULE_SRC EXCLUDE REGEX "_linux\\.cpp$")
        else()
            list(FILTER APP_MODULE_SRC EXCLUDE REGEX "_win32\\.cpp$")

            if(APPLE)
                list(FILTER APP_MODULE_SRC EXCLUDE REGEX "_linux\\.cpp$")
            endif(APPLE)
        endif(WIN32)

        list(APPEND SOURCE_FILES ${APP_MODULE_SRC})
    endforeach()

    set(SOURCE_FILES ${SOURCE_FILES} PARENT_SCOPE) # Устанавливаем переменную на уровне родителя
endfunction()

gather_sources(${PROJECT_NAME} RECURSE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/"
)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

find_package(TBB REQUIRED)
target_link_libraries(${PROJECT_NAME} TBB::tbb)