#!/bin/bash

if [ ! -d "./build" ]; then
	mkdir build
fi

BUILD_TYPE=DEBUG

for in_option in "$@"
do
	if [ "$in_option" == "--sim" ] || [ "$in_option" == "-s" ]; then
		BUILD_TYPE=DebugSimulation
	elif [ "$in_option" == "--release" ] || [ "$in_option" == "-r" ]; then
		BUILD_TYPE=RELEASE
	fi
done

pushd build

cmake ../ -DCMAKE_BUILD_TYPE=$BUILD_TYPE

popd

read -p "Finished. Press any key to exit..."
