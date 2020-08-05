cmake_minimum_required(VERSION 3.11)

#==========================================================
#   Options for Decent Ride Sharing Project
#==========================================================

#Enclave project list:
set(ENCLAVE_PROJECT_LIST PassengerMgm TripPlaner DriverMgm TripMatcher Billing Payment)
set(ENCLAVE_PLATFORM_SGX_PROJECT_LIST PassengerMgm TripPlaner DriverMgm TripMatcher Billing Payment)
set(ENCLAVE_PLATFORM_NON_ENCLAVE_PROJECT_LIST)

#Client project list:
set(CLIENT_PROJECT_LIST Passenger Driver)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(APPLE)
	message(WARNING "Mac OS is not supported by Intel SGX for now. Trying to setup in simulation mode...")
	set(CMAKE_BUILD_TYPE DebugSimulation)
endif()

if(NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/libs/decent-ra-api/cmake)
	message(FATAL_ERROR "Cannot find Decent RA API module. Please make sure it has been checked out.")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/libs/decent-ra-api/cmake)

#==========================================================
#   Start Decent Ride Sharing Project
#==========================================================
project(DecentRide)

#==========================================================
#   Setup options
#==========================================================

set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)
set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
set(Boost_NO_SYSTEM_PATHS ON)

if (WIN32)
	macro(get_WIN32_WINNT version)
		if(CMAKE_SYSTEM_VERSION)
			set(ver ${CMAKE_SYSTEM_VERSION})
			string(REGEX MATCH "^([0-9]+).([0-9])" ver ${ver})
			string(REGEX MATCH "^([0-9]+)" verMajor ${ver})
			# Check for Windows 10, b/c we'll need to convert to hex 'A'.
			if("${verMajor}" MATCHES "10")
				set(verMajor "A")
				string(REGEX REPLACE "^([0-9]+)" ${verMajor} ver ${ver})
			endif()
			# Remove all remaining '.' characters.
			string(REPLACE "." "" ver ${ver})
			# Prepend each digit with a zero.
			string(REGEX REPLACE "([0-9A-Z])" "0\\1" ver ${ver})
			set(${version} "0x${ver}")
		endif()
	endmacro()

	get_WIN32_WINNT(win_ver)
	message(STATUS "CMAKE_SYSTEM_VERSION: " "${CMAKE_SYSTEM_VERSION}")
	message(STATUS "_WIN32_WINNT: " "${win_ver}")
	add_definitions(-D_WIN32_WINNT=${win_ver})
endif(WIN32)

if(MSVC)
	set(COMMON_OPTIONS /W3 /wd4996 /we4239 /we4002 /we4700 /we4305 /EHsc /MP /d2FH4-)
	set(DEBUG_OPTIONS /MTd /Od /Zi /DDEBUG)
	set(SIMULATE_OPTIONS /DSIMULATING_ENCLAVE)
	set(RELEASE_OPTIONS /MT /Ox /Oi /Ob2 /fp:fast /GR- /DEDEBUG)# /DNDEBUG
	
	set(COMMON_ENCLAVE_CXX_OPTIONS )
	
	set(ENCLAVE_LINKER_OPTIONS "/NODEFAULTLIB /NOENTRY")
	set(APP_DEBUG_LINKER_OPTIONS "/NODEFAULTLIB:libc.lib /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcd.lib /NODEFAULTLIB:msvcrtd.lib")
	set(APP_RELEASE_LINKER_OPTIONS "/NODEFAULTLIB:libc.lib /NODEFAULTLIB:libcmtd.lib /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcd.lib /NODEFAULTLIB:msvcrtd.lib")
	
	set(COMMON_APP_DEFINES WIN32_LEAN_AND_MEAN CURL_STATICLIB BOOST_DATE_TIME_NO_LIB)
	set(COMMON_ENCLAVE_DEFINES ENCLAVE_ENVIRONMENT)
	
	set(Additional_Sys_Lib )
else()
	set(COMMON_OPTIONS -pthread)
	set(DEBUG_OPTIONS -O0 -g -DDEBUG -UNDEBUG -UEDEBUG)
	set(SIMULATE_OPTIONS -DSIMULATING_ENCLAVE)
	set(RELEASE_OPTIONS -O2 -UEDEBUG -UDEBUG) #-DNDEBUG defined by default
	
	set(COMMON_ENCLAVE_CXX_OPTIONS -std=c++11)
	
	set(ENCLAVE_LINKER_OPTIONS "")
	set(APP_DEBUG_LINKER_OPTIONS "")
	set(APP_RELEASE_LINKER_OPTIONS "")
	
	set(COMMON_APP_DEFINES CURL_STATICLIB BOOST_DATE_TIME_NO_LIB)
	set(COMMON_ENCLAVE_DEFINES ENCLAVE_ENVIRONMENT)
	
	set(Additional_Sys_Lib rt pthread)
endif()

set(DEBUG_OPTIONS ${COMMON_OPTIONS} ${DEBUG_OPTIONS})
set(DEBUGSIM_OPTIONS ${COMMON_OPTIONS} ${DEBUG_OPTIONS} ${SIMULATE_OPTIONS})
set(RELEASE_OPTIONS ${COMMON_OPTIONS} ${RELEASE_OPTIONS})

if(MSVC)
	set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
endif()

add_compile_options(
	"$<$<CONFIG:Debug>:${DEBUG_OPTIONS}>"
	"$<$<CONFIG:DebugSimulation>:${DEBUGSIM_OPTIONS}>"
	"$<$<CONFIG:Release>:${RELEASE_OPTIONS}>"
)

#Remove all standard libraries dependency here so that enclave DLL can be 
# compiled properly. And it will be added back later for non-enclave apps.
set(COMMON_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES_INIT}")
separate_arguments(COMMON_STANDARD_LIBRARIES)
set(CMAKE_CXX_STANDARD_LIBRARIES "")
set(CMAKE_C_STANDARD_LIBRARIES "")

if(MSVC) 
	#Removed Basic Runtime Checks in MSVC
	STRING (REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	STRING (REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	STRING (REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	STRING (REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
endif()

set(CMAKE_CXX_FLAGS_DEBUGSIMULATION ${CMAKE_CXX_FLAGS_DEBUG})
set(CMAKE_C_FLAGS_DEBUGSIMULATION ${CMAKE_C_FLAGS_DEBUG})
set(CMAKE_EXE_LINKER_FLAGS_DEBUGSIMULATION ${CMAKE_EXE_LINKER_FLAGS_DEBUG})
set(CMAKE_SHARED_LINKER_FLAGS_DEBUGSIMULATION ${CMAKE_SHARED_LINKER_FLAGS_DEBUG})
set(CMAKE_STATIC_LINKER_FLAGS_DEBUGSIMULATION ${CMAKE_STATIC_LINKER_FLAGS_DEBUG})

set(CMAKE_CONFIGURATION_TYPES Release Debug DebugSimulation)
set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS Debug DebugSimulation)

#-------
# Flag results:
#-------
message(STATUS "Final set of flags")
message(STATUS "==================================================")
message(STATUS "CMake Standard Libraries:")
message(STATUS "${CMAKE_CXX_STANDARD_LIBRARIES_INIT}")
message(STATUS "CMake CXX Flags:")
message(STATUS "${CMAKE_CXX_FLAGS}")
message(STATUS "CMake CXX Flags (Debug):")
message(STATUS "${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMake CXX Flags (Release):")
message(STATUS "${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CMake CXX Link Flags:")
message(STATUS "${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMake CXX Link Flags (Debug):")
message(STATUS "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
message(STATUS "CMake CXX Link Flags (Release):")
message(STATUS "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
message(STATUS "==================================================")
message(STATUS "")

#==========================================================
#   Add submodules
#==========================================================

##Header only libraries
set(TCLAP_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/libs/tclap/include)


# set(DECENT_API_DECENT_SERVER ON CACHE BOOL "Add decent server module." FORCE)
set(DECENT_API_DECENT_APP ON CACHE BOOL "Add decent app module." FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/decent-ra-api)

#Intel SGX SDK
include(EnclavePlatformSgx)

#Non-Enclave
include(EnclavePlatformNonEnclave)

#==========================================================
#   Add files
#==========================================================

set(SOURCEDIR ${CMAKE_CURRENT_LIST_DIR}/sources)
file(GLOB_RECURSE SOURCES ${SOURCEDIR}/*.[ch]* ${SOURCEDIR}/*.edl)

# Common Files
set(SOURCEDIR_Common ${SOURCEDIR}/Common)
file(GLOB_RECURSE SOURCES_Common ${SOURCEDIR_Common}/*.[ch]*)

# Common App Files
set(SOURCEDIR_Common_App ${SOURCEDIR}/Common_App)
file(GLOB_RECURSE SOURCES_Common_App ${SOURCEDIR_Common_App}/*.[ch]*)

# Common Enc Files
set(SOURCEDIR_Common_Enc ${SOURCEDIR}/Common_Enc)
file(GLOB_RECURSE SOURCES_Common_Enc ${SOURCEDIR_Common_Enc}/*.[ch]*)
file(GLOB_RECURSE SOURCES_Common_EDL ${SOURCEDIR_Common_Enc}/*.edl)

foreach(Proj_Name IN ITEMS ${ENCLAVE_PROJECT_LIST})
	
	# Enclave Project Files:
	set(SOURCEDIR_${Proj_Name}_App ${SOURCEDIR}/${Proj_Name}_App)
	set(SOURCEDIR_${Proj_Name}_Enc ${SOURCEDIR}/${Proj_Name}_Enc)
	file(GLOB_RECURSE SOURCES_${Proj_Name}_App ${SOURCEDIR_${Proj_Name}_App}/*.[ch]*)
	file(GLOB_RECURSE SOURCES_${Proj_Name}_Enc ${SOURCEDIR_${Proj_Name}_Enc}/*.[ch]*)
	file(GLOB_RECURSE SOURCES_${Proj_Name}_EDL ${SOURCEDIR_${Proj_Name}_Enc}/*.edl)
	
endforeach()

foreach(Proj_Name IN ITEMS ${CLIENT_PROJECT_LIST})
	
	# Client Project Files:
	set(SOURCEDIR_${Proj_Name} ${SOURCEDIR}/${Proj_Name})
	file(GLOB_RECURSE SOURCES_${Proj_Name} ${SOURCEDIR_${Proj_Name}}/*.[ch]*)
	
endforeach()


#==========================================================
#   Setup filters
#==========================================================

source_group(TREE ${SOURCEDIR} FILES ${SOURCES})

#==========================================================
#   G++ linking flags
#==========================================================

if(MSVC)
	set(WHOLE_ARCHIVE_FLAG_BEGIN "")
	set(WHOLE_ARCHIVE_FLAG_END "")
	set(GROUP_FLAG_BEGIN "")
	set(GROUP_FLAG_END "")
else()
	set(WHOLE_ARCHIVE_FLAG_BEGIN -Wl,--whole-archive)
	set(WHOLE_ARCHIVE_FLAG_END -Wl,--no-whole-archive)
	set(GROUP_FLAG_BEGIN -Wl,--start-group)
	set(GROUP_FLAG_END -Wl,--end-group)
endif()

#==========================================================
#   Enclave Projects
#==========================================================

foreach(Proj_Name IN ITEMS ${ENCLAVE_PROJECT_LIST})

	if(${Proj_Name} IN_LIST ENCLAVE_PLATFORM_SGX_PROJECT_LIST)
	
		setup_sgx_edl_project(${Proj_Name})
		setup_sgx_enclave_project(${Proj_Name})
		setup_sgx_app_project(${Proj_Name})
	
	elseif(${Proj_Name} IN_LIST ENCLAVE_PLATFORM_NON_ENCLAVE_PROJECT_LIST)
	
		setup_non_enclave_enclave_project(${Proj_Name})
		setup_non_enclave_app_project(${Proj_Name})
	
	else()
	
		message(WARNING "The enclave project, ${Proj_Name}, has not been specified a enclave platform type. It will not be added to the solution.")
	
	endif()

endforeach()

#==========================================================
#   Client Apps
#==========================================================


foreach(Proj_Name IN ITEMS ${CLIENT_PROJECT_LIST})

	add_executable(${Proj_Name} ${SOURCES_Common} ${SOURCES_Common_App} ${SOURCES_${Proj_Name}})
	#includes:
	target_include_directories(${Proj_Name} PRIVATE ${TCLAP_INCLUDE_DIR})
	#defines:
	target_compile_definitions(${Proj_Name} PRIVATE ${COMMON_APP_DEFINES} DECENT_PURE_CLIENT)
	#linker flags:
	set_target_properties(${Proj_Name} PROPERTIES LINK_FLAGS_DEBUG "${APP_DEBUG_LINKER_OPTIONS}")
	set_target_properties(${Proj_Name} PROPERTIES LINK_FLAGS_DEBUGSIMULATION "${APP_DEBUG_LINKER_OPTIONS}")
	set_target_properties(${Proj_Name} PROPERTIES LINK_FLAGS_RELEASE "${APP_RELEASE_LINKER_OPTIONS}")
	set_target_properties(${Proj_Name} PROPERTIES FOLDER "Client")

	target_link_libraries(${Proj_Name} 
		${COMMON_STANDARD_LIBRARIES} 
		#IntelSGX::SDK_Untrusted
		DecentRa_App_App 
		jsoncpp_lib_static 
		mbedcrypto 
		mbedx509 
		mbedtls 
		Boost::filesystem
		Boost::system
		${Additional_Sys_Lib}
	)

endforeach()
