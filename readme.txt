depdent:
	sudo apt install libavfilter-dev  libavformat-dev libpostproc-dev libswscale-dev

x86 build:
	cd build;cmake ..;make

arm cross build:
	cd build;./cross_build.sh
