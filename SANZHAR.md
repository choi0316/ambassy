                                                          Building Android:
	
Installing Repo:

	./init-android

	P.S. in case of errors related to git run:
		git config --global user.name "Your Name"
		git config --global user.email "you@example.com"

Building Android:

	./build-android

Preparing SD Card:

	cd android
	sudo fdisk -l //find path to SD card  
	sudo device/xilinx/common/scripts/mksdcard.sh [SDPath] zcu102

Xilinx ZCU102
Set boot mode of the board to "SD Boot". Insert SD card to the board.


Connect external monitor using DisplayPort for zcu102-*. Please note that DisplayPort must be connected before board power-on. External monitor should support 1920x1080 resolution.


Connect USB mouse (and optionally USB keyboard)


Attach iVeia Ozzy FMC for zcu102_ozzy-* targets. Refer to https://support.iveia.com/ for more detailed guide about usage of iVeia Ozzy FMC


Power on the board


If after booting you see only Android home screen background without other UI elements (like Status Bar, Home button, etc), then MALI libraries with a workaround for ES1 silicon should be used. Open serial debug console and perform the following to install libraries:

 	su
    stop
    mount -o remount,rw /system
    mv /system/lib/egl/libGLES_mali.so_es1 /system/lib/egl/libGLES_mali.so
    mv /system/lib64/egl/libGLES_mali.so_es1 /system/lib64/egl/libGLES_mali.so
    sync
    mount -o remount,ro /system
    start

Generate BOOT.BIN: (REQUIRES embassy/project) //Did not have time to figure out how to write dockerfile

    cd docker
    sudo ./build.sh
    sudo ./run.sh
    mkdir /home/android/work/android6
    cd /home/android/work
    mv `ls | grep -v android6` android6
    cd android6
    mkdir boot
    ./gen-boot.sh
    sudo cp /boot/BOOT.BIN [SDPath]/BOOT/

Making optee_client: (EXIT from docker)
	
	exit
	docker pull bitriseio/android-ndk
	cd ../optee/optee_client 
    ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk APP_PLATFORM=android-21 APP_ABI=arm64-v8a 
    EXPORT_DIR=`pwd`/out make CROSS_COMPILE=aarch64-linux-gnu-

	sudo cp libs/arm64-v8a/libteec.so /media/`whoami`/SYSTEM/lib
    sudo cp libs/arm64-v8a/libteec.so /media/`whoami`/SYSTEM/lib64
    sudo cp libs/arm64-v8a/tee-supplicant /media/`whoami`/SYSTEM/bin
	
Making optee_examples:


	cd ../optee_examples/ICAP_WRITE
	sudo  ./build-test-docker.sh
    sudo mkdir -p /media/`whoami`/SYSTEM/vendor/lib/optee_armtz
    sudo mkdir -p /media/`whoami`/ROOT/data/vendor
    sudo mkdir -p /media/`whoami`/ROOT/data/vendor/tee
    sudo mkdir -p /media/`whoami`/ROOT/data/vendor/tee/optee_armtz
	cp ta/*.ta /media/`whoami`/SYSTEM/vendor/lib/optee_armtz
	cp ta/*.ta /media/`whoami`/ROOT/data/vendor/tee/optee_armtz
	cp libs/arm64-v8a/tee_embassy /media/`whoami`/SYSTEM/bin
	cp ../init.rc /media/`whoami`/ROOT/

	

