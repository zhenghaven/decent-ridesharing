import os
import platform
import sys
import shutil

gs_enableHunter = True

gs_defaultBuildDirPath = "./build"
gs_defaultHunterProjPath = "./hunter"
gs_defaultHunterSharedPath = "../hunter"

gs_hunterBaseName = "_Base"

gs_absBuildDirPath = os.path.abspath(gs_defaultBuildDirPath)
gs_absHunterProjPath = os.path.abspath(gs_defaultHunterProjPath)
gs_absHunterSharedPath = os.path.abspath(gs_defaultHunterSharedPath)

gs_absHunterBaseProjPath = os.path.abspath(gs_defaultHunterProjPath + "/" + gs_hunterBaseName)
gs_absHunterBaseSharedPath = os.path.abspath(gs_defaultHunterSharedPath + "/" + gs_hunterBaseName)

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

def HasUnexpectedHunterFiles():
	return not(IsDirOrNotExist(gs_absHunterProjPath) and IsDirOrNotExist(gs_absHunterBaseProjPath) and 
				IsDirOrNotExist(gs_absHunterSharedPath) and IsDirOrNotExist(gs_absHunterBaseSharedPath))

def SetupHunterDir():
	sys.stdout.write("-- Setting up Hunter directory...\n")
	sys.stdout.flush()
	
	if os.path.isdir(gs_absHunterBaseProjPath) and not(os.path.isdir(gs_absHunterBaseSharedPath)):
		#Create shared shared hunter dir if needed, move _Base dir.
		CreateDirIfNotExist(gs_absHunterSharedPath)
		shutil.move(gs_absHunterBaseProjPath, gs_absHunterSharedPath)
	
	if not(os.path.isdir(gs_absHunterBaseSharedPath)) and not(os.path.isdir(gs_absHunterBaseProjPath)):
		#Create shared shared hunter base dir
		CreateDirIfNotExist(gs_absHunterBaseSharedPath)
	
	if os.path.isdir(gs_absHunterBaseSharedPath) and not(os.path.isdir(gs_absHunterBaseProjPath)):
		#create sym link.
		os.symlink(gs_absHunterBaseSharedPath, gs_absHunterBaseProjPath, True)
	
	sys.stdout.write("-- Hunter directory is ready.\n")
	sys.stdout.flush()

def SetupBuildDirWin():
	return os.system("cmake -G \"Visual Studio 15 2017 Win64\" ../")

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

if gs_enableHunter and HasUnexpectedHunterFiles():
	sys.stdout.write("ERROR: some reserved directory paths is occupied by files.\n")
	sys.stdout.flush()
	ExitProcess(1)

if gs_enableHunter:
	SetupHunterDir()

SetupBuildDir()

ExitProcess()