# cmake configfile from zlmediakit, thanks
the ffmpeg version is later than 4.1

depdent:
	sudo apt install libavfilter-dev  libavformat-dev libpostproc-dev libswscale-dev libavdevice-dev

x86 build:
	cd build;cmake ..;make

arm cross build:
	cd build;./cross_build.sh
