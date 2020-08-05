import os
import platform
import sys
import shutil

gs_defaultBuildDirPath = "./build"

gs_absBuildDirPath = os.path.abspath(gs_defaultBuildDirPath)

gs_absOriWorkDir = os.path.abspath("./")

def ExitProcess(errorCode = 0):
	sys.stdout.write("Finished. Press enter to exit...")
	sys.stdout.flush()
	sys.stdin.read(1)
	exit(errorCode)

def CreateDirIfNotExist(path):
	if not(os.path.isdir(path)):
		os.makedirs(path)

def IsDirOrNotExist(path):
	return os.path.isdir(path) or not(os.path.exists(path))

def HasUnexpectedBuildFiles():
	return not(IsDirOrNotExist(gs_absBuildDirPath))

def SetupBuildDirWin():
	return os.system("cmake -G \"Visual Studio 16 2019\" ../")

def SetupBuildDirPosix():
	return os.system("cmake ../")

def SetupBuildDir():
	sys.stdout.write("-- Setting up build directory...\n")
	sys.stdout.flush()
	CreateDirIfNotExist(gs_absBuildDirPath)
	os.chdir(gs_absBuildDirPath)
	
	errCode = 1
	if os.name == 'nt':
		errCode = SetupBuildDirWin()
	elif os.name == 'posix':
		errCode = SetupBuildDirPosix()
	
	os.chdir(gs_absOriWorkDir)
	sys.stdout.write("-- Build directory is ready.\n")
	sys.stdout.flush()
	return errCode

################################################################
# Main procedures
################################################################

if HasUnexpectedBuildFiles():
	sys.stdout.write("ERROR: some reserved directory paths is occupied by files.\n")
	sys.stdout.flush()
	ExitProcess(1)

SetupBuildDir()

ExitProcess()