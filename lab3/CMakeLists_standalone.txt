# for msvc multi-config cmakes

if(NOT CMAKE_CONFIGURATION_TYPES)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_CONFIGURATION_TYPES RelWithDebInfo)
else()
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_BUILD_TYPE}")
endif()
endif()

project(assign)

cmake_minimum_required(VERSION 2.8)
# make  RelWithDebInfo the default build type.

# Note that the RelWithDebInfo build type does
# NOT generate debug version actually.

if(NOT CMAKE_BUILD_TYPE)
	if(CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE ${CMAKE_CONFIGURATION_TYPES})
	else()
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
	endif()
endif()

# set cmake compiler flags.
set(CMAKE_CXX_STANDARD 11)

if(NOT MSVC)

set(CMAKE_CXX_FLAGS "-std=c++11   ")
set(CMAKE_CXX_FLAGS_RELEASE "  -O3  -DNDEBUG ")

if(CYGWIN)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "  -O2 -g ")
set(CMAKE_CXX_FLAGS_DEBUG "  -O0  -g3 -Wa,-mbig-obj")
else()
set(CMAKE_CXX_FLAGS_DEBUG "  -O0  -g3 ")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "  -O3 ")
endif()

else()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /wd4267 /wd4291 /wd4141 /wd4146 /wd4099 /wd4065 /wd4624")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /bigobj " )
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD /O2 /Ob2 ") #removed the /DNDEBUG 
endif()


#set project directories.
set(PROJECT_ROOT_PATH "${CMAKE_SOURCE_DIR}")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib")

#set(INCLUDE_DIR ${PROJECT_ROOT_PATH}/include)
#set(LINK_DIR /usr/local/lib ${CMAKE_BINARY_DIR}/lib)
set(LINK_DIR /usr/local/lib)
include_directories(${INCLUDE_DIR}  )

link_directories(${LINK_DIR} )
add_definitions( 
-D__STDC_LIMIT_MACROS
-D__STDC_CONSTANT_MACROS
)

if(MSVC)
add_definitions(
${LLVM_DEFINITIONS}
-DNOMINMAX
-D__STDC_VERSION__=199901L
)
endif()

# set plateform specific environment vars.
set(PATH_Delimer :)
if(MSVC)
set(ADDITIONAL_LLVM_DEBUG_PATH  "D:/development/LLVMDbg/" )
set(ADDITIONAL_LLVM_RELEASE_PATH "D:/development/LLVMRel/")
set(PATH_Delimer ;)
else()
set(ADDITIONAL_LLVM_PATH "")
endif()

enable_testing()

# find llvm config
if(LLVM_DIR)
find_package(LLVM REQUIRED CONFIG HINTS ${LLVM_DIR} ${LLVM_DIR}/lib/cmake/llvm  
                NO_DEFAULT_PATH)
else()
#MESSAGE("search:${CMAKE_BUILD_TYPE}")
        if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
                MESSAGE("collect:RelWithDebInfo")    
                                                        # for some reason  we use llvmr instead of llvmra
                find_package(LLVM REQUIRED CONFIG HINTS  ${ADDITIONAL_LLVM_RELEASE_PATH} /usr/local/llvmra/ /usr/local/llvmra/lib/cmake/llvm/    /usr/local/llvm/   /usr/local/bin/llvm   ~/bin/llvm/   
                )
        elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
                MESSAGE("collect:Debug")
                find_package(LLVM REQUIRED CONFIG HINTS  ${ADDITIONAL_LLVM_DEBUG_PATH} /usr/local/llvmd/ /usr/local/llvmd/lib/cmake/llvm/    /usr/local/llvm/   /usr/local/bin/llvm   ~/bin/llvm/    
                )
        else()
                find_package(LLVM REQUIRED CONFIG HINTS   ${ADDITIONAL_LLVM_RELEASE_PATH}  /usr/local/llvmr/    ~/bin/llvm/    
                )
        endif()
endif()

message(STATUS "LLVM_CONFIG_FOUND:${LLVM_DIR}")

add_definitions(${LLVM_DEFINITIONS})

# Find the libraries that correspond to the LLVM components
# that we wish to use, currently we collect all.
llvm_map_components_to_libnames(llvm_libs all)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS}  )

link_directories( ${LLVM_LIBRARY_DIRS} )

file(GLOB SOURCE "./*.cpp")
add_executable(assignment ${SOURCE})
if(MSVC)
list(REMOVE_ITEM llvm_libs LTO )
target_link_libraries(assignment
${llvm_libs}
)
else()
target_link_libraries(assignment
                ${llvm_libs}
                pthread
                z
                curses
                )
endif()
