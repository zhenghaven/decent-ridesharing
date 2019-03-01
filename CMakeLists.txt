cmake_minimum_required(VERSION 3.11)

#==========================================================
#   Options for Decent Ride Sharing Project
#==========================================================

set(Enc_Proj_Name_List PassengerMgm TripPlaner DriverMgm TripMatcher Billing Payment)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(HUNTER_BOOST_RUNTIME_STATIC ON CACHE BOOL "Switch for using boost runtime static linking." FORCE)

if(APPLE)
	message(FATAL_ERROR "Mac OS is not supported for now.")
endif()
#==========================================================
#   Setup Hunter
#==========================================================

set(gate_dir "${CMAKE_CURRENT_LIST_DIR}/hunter/gate")
set(gate_module "${gate_dir}/cmake/HunterGate.cmake")
set(HUNTER_KEEP_PACKAGE_SOURCES ON)

get_filename_component(gate_module "${gate_module}" ABSOLUTE)

if(NOT EXISTS "${gate_module}")
	message(FATAL_ERROR "${gate_module} module not found!")
endif()

message(STATUS "Including HunterGate: ${gate_module}")
include("${gate_module}")

get_filename_component(HUNTER_ROOT "${CMAKE_CURRENT_LIST_DIR}/hunter" ABSOLUTE)
HunterGate(URL "x" SHA1 "xxxxxxxx" FILEPATH ${CMAKE_CURRENT_LIST_DIR}/HunterConfig.cmake)

#==========================================================
#   Start Decent Ride Sharing Project
#==========================================================
project(DecentRidesharing)

#==========================================================
#   Add Hunter Libraries
#==========================================================
set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)
set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF)

hunter_add_package(Boost COMPONENTS filesystem)
hunter_add_package(OpenSSL)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ${HUNTER_BOOST_RUNTIME_STATIC})
set(Boost_NO_SYSTEM_PATHS ON)
find_package(Boost CONFIG REQUIRED system filesystem)

#==========================================================
#   Setup options
#==========================================================

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
	set(COMMON_OPTIONS /W3 /wd4996 /we4239 /we4002 /we4700 /we4305 /EHsc /MP)
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

#Intel SGX SDK
include(${CMAKE_CURRENT_LIST_DIR}/libs/decent-ra-api/cmake/FindSgxSdk.cmake)

##Header only libraries
set(TCLAP_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/libs/tclap/include)


# set(DECENT_API_DECENT_SERVER ON CACHE BOOL "Add decent server module." FORCE)
set(DECENT_API_DECENT_APP ON CACHE BOOL "Add decent app module." FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/decent-ra-api)

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

foreach(Proj_Name IN ITEMS ${Enc_Proj_Name_List})
	
	# Enclave Project Files:
	set(SOURCEDIR_${Proj_Name}_App ${SOURCEDIR}/${Proj_Name}_App)
	set(SOURCEDIR_${Proj_Name}_Enc ${SOURCEDIR}/${Proj_Name}_Enc)
	file(GLOB_RECURSE SOURCES_${Proj_Name}_App ${SOURCEDIR_${Proj_Name}_App}/*.[ch]*)
	file(GLOB_RECURSE SOURCES_${Proj_Name}_Enc ${SOURCEDIR_${Proj_Name}_Enc}/*.[ch]*)
	file(GLOB_RECURSE SOURCES_${Proj_Name}_EDL ${SOURCEDIR_${Proj_Name}_Enc}/*.edl)
	
endforeach()

# Passenger Files:
set(SOURCEDIR_Passenger ${SOURCEDIR}/Passenger)
file(GLOB_RECURSE SOURCES_Passenger ${SOURCEDIR_Passenger}/*.[ch]*)

# Driver Files:
set(SOURCEDIR_Driver ${SOURCEDIR}/Driver)
file(GLOB_RECURSE SOURCES_Driver ${SOURCEDIR_Driver}/*.[ch]*)


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

if(MSVC)
	set(Enclave_Prog_Prefix )
	set(Enclave_Prog_Postfix dll)
	set(Enclave_Bin_Path "${CMAKE_BINARY_DIR}/$<CONFIG>")
else()
	set(Enclave_Prog_Prefix ${CMAKE_SHARED_LIBRARY_PREFIX})
	set(Enclave_Prog_Postfix so)
	set(Enclave_Bin_Path "${CMAKE_BINARY_DIR}")
endif()

#==========================================================
#   Enclave Projects
#==========================================================

foreach(Proj_Name IN ITEMS ${Enc_Proj_Name_List})

	###########################################################
	### EDL
	###########################################################
	
	set(EDL_T_SRC_OUTPUT ${SOURCEDIR_${Proj_Name}_Enc}/Enclave_t.h ${SOURCEDIR_${Proj_Name}_Enc}/Enclave_t.c)
	set(EDL_U_SRC_OUTPUT ${SOURCEDIR_${Proj_Name}_App}/Enclave_u.h ${SOURCEDIR_${Proj_Name}_App}/Enclave_u.c)

	add_custom_command(OUTPUT ${EDL_T_SRC_OUTPUT}
		COMMAND "${INTEL_SGX_EDGER_PATH}"  
		--trusted "${SOURCEDIR_${Proj_Name}_Enc}/Enclave.edl" 
		--search-path "${SOURCEDIR_Common_Enc}" 
		--search-path "${SOURCEDIR_${Proj_Name}_Enc}" 
		--search-path "${DECENT_API_EDL_DIR}" 
		--search-path "${INTEL_SGX_SDK_INCLUDE_DIR}"
		WORKING_DIRECTORY "${SOURCEDIR_${Proj_Name}_Enc}"
		DEPENDS "${SOURCEDIR_${Proj_Name}_Enc}/Enclave.edl"
		COMMENT "Processing EDL for enclave..."
	)

	add_custom_command(OUTPUT ${EDL_U_SRC_OUTPUT}
		COMMAND "${INTEL_SGX_EDGER_PATH}"  
		--untrusted "${SOURCEDIR_${Proj_Name}_Enc}/Enclave.edl" 
		--search-path "${SOURCEDIR_Common_Enc}" 
		--search-path "${SOURCEDIR_${Proj_Name}_Enc}" 
		--search-path "${DECENT_API_EDL_DIR}" 
		--search-path "${INTEL_SGX_SDK_INCLUDE_DIR}"
		WORKING_DIRECTORY "${SOURCEDIR_${Proj_Name}_App}"
		DEPENDS "${SOURCEDIR_${Proj_Name}_Enc}/Enclave.edl"
		COMMENT "Processing EDL for app..."
	)

	add_custom_target(${Proj_Name}_EDL DEPENDS ${EDL_T_SRC_OUTPUT} ${EDL_U_SRC_OUTPUT})
	set_target_properties(${Proj_Name}_EDL PROPERTIES FOLDER "${Proj_Name}")

	###########################################################
	### Enclave
	###########################################################

	set(${Proj_Name}_Enclave_File "${Enclave_Prog_Prefix}${Proj_Name}_Enclave.signed.${Enclave_Prog_Postfix}")
	set(${Proj_Name}_Enclave_Lib "${Enclave_Prog_Prefix}${Proj_Name}_Enclave$<$<CONFIG:Debug>:${CMAKE_DEBUG_POSTFIX}>.${Enclave_Prog_Postfix}")

	add_library(${Proj_Name}_Enclave SHARED ${SOURCES_Common} ${SOURCES_Common_EDL} ${SOURCES_Common_Enc} ${SOURCES_${Proj_Name}_Enc} ${SOURCES_${Proj_Name}_EDL})
	#defines:
	target_compile_definitions(${Proj_Name}_Enclave PRIVATE ${COMMON_ENCLAVE_DEFINES})
	#compiler flags:
	target_compile_options(${Proj_Name}_Enclave PRIVATE ${INTEL_SGX_SDK_C_FLAGS} $<$<COMPILE_LANGUAGE:CXX>:${INTEL_SGX_SDK_CXX_FLAGS} ${COMMON_ENCLAVE_CXX_OPTIONS}>)
	#linker flags:
	set_target_properties(${Proj_Name}_Enclave PROPERTIES LINK_FLAGS "${ENCLAVE_LINKER_OPTIONS} ${INTEL_SGX_SDK_LINKER_FLAGS_T}")
	set_target_properties(${Proj_Name}_Enclave PROPERTIES FOLDER "${Proj_Name}")

	add_custom_command(TARGET ${Proj_Name}_Enclave
		POST_BUILD
		COMMAND "${INTEL_SGX_SIGNER_PATH}" sign 
		-key "${CMAKE_CURRENT_LIST_DIR}/Enclave_private.pem" 
		-enclave "${Enclave_Bin_Path}/${${Proj_Name}_Enclave_Lib}" 
		-out "${CMAKE_BINARY_DIR}/${${Proj_Name}_Enclave_File}" 
		-config "${SOURCEDIR_${Proj_Name}_Enc}/Enclave.config.xml"
	)

	target_link_libraries(${Proj_Name}_Enclave 
		${WHOLE_ARCHIVE_FLAG_BEGIN} 
		IntelSGX::SDK_Trusted_Whole_lib 
		${WHOLE_ARCHIVE_FLAG_END}
		${GROUP_FLAG_BEGIN}
		IntelSGX::SDK_Trusted_stdc 
		IntelSGX::SDK_Trusted_cxx 
		IntelSGX::SDK_Trusted_service 
		IntelSGX::SDK_Trusted_key_exchange 
		IntelSGX::SDK_Trusted_crypto 
		DecentRa_App_Enclave 
		mbedcrypto_enclave 
		mbedx509_enclave 
		mbedtls_enclave 
		${GROUP_FLAG_END}
	)

	add_dependencies(${Proj_Name}_Enclave ${Proj_Name}_EDL)

	###########################################################
	### App
	###########################################################

	add_executable(${Proj_Name}_App ${SOURCES_Common} ${SOURCES_Common_EDL} ${SOURCES_Common_App} ${SOURCES_${Proj_Name}_App} ${SOURCES_${Proj_Name}_EDL})
	#includes:
	target_include_directories(${Proj_Name}_App PRIVATE ${TCLAP_INCLUDE_DIR})
	#defines:
	target_compile_definitions(${Proj_Name}_App PRIVATE ${COMMON_APP_DEFINES} ENCLAVE_FILENAME="${${Proj_Name}_Enclave_File}" TOKEN_FILENAME="${Proj_Name}_Enclave.token")
	#linker flags:
	set_target_properties(${Proj_Name}_App PROPERTIES LINK_FLAGS_DEBUG "${APP_DEBUG_LINKER_OPTIONS}")
	set_target_properties(${Proj_Name}_App PROPERTIES LINK_FLAGS_DEBUGSIMULATION "${APP_DEBUG_LINKER_OPTIONS}")
	set_target_properties(${Proj_Name}_App PROPERTIES LINK_FLAGS_RELEASE "${APP_RELEASE_LINKER_OPTIONS}")
	set_target_properties(${Proj_Name}_App PROPERTIES FOLDER "${Proj_Name}")

	target_link_libraries(${Proj_Name}_App 
		${COMMON_STANDARD_LIBRARIES} 
		IntelSGX::SDK_Untrusted
		DecentRa_App_App 
		jsoncpp_lib_static 
		mbedcrypto 
		mbedx509 
		mbedtls 
		Boost::filesystem
		Boost::system
		${Additional_Sys_Lib}
	)

	add_dependencies(${Proj_Name}_App ${Proj_Name}_Enclave)
	
endforeach()


#==========================================================
#   Client Apps
#==========================================================

set(Nor_Proj_Name_List Passenger Driver)

foreach(Proj_Name IN ITEMS ${Nor_Proj_Name_List})

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
