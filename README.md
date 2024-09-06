# Initializing Build Environment
## Installing JDK
Please check [AOSP: Installing the JDK](https://source.android.com/source/initializing#installing-the-jdk) and [AOSP: JDK Requirements](https://source.android.com/source/requirements#jdk) for the detailed instructions to install proper version of the JDK. Please use OpenJDK 7 to build Android 6. On Ubuntu LTS 14.04 it can be installed with:

`$ sudo apt-get install openjdk-7-jdk` 

## Installing packages
Please follow [AOSP: Installing Required Packages](https://source.android.com/source/initializing#installing-required-packages-ubuntu-1404) to initialize build environment depending on your build host. Please note that builds are tested with 64-bit Ubuntu LTS 14.04 hosts. For the Ubuntu LTS 14.04 the following packages are required:

`$ sudo apt-get install git-core gnupg flex bison gperf build-essential zip curl zlib1g-dev gcc-multilib g++-multilib libc6-dev-i386 lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z-dev ccache libgl1-mesa-dev libxml2-utils xsltproc unzip`

In addition to the packages from the AOSP guide, please install the following for the SD card initialization scripts:               
    
`$ sudo apt-get install dosfstools e2fsprogs parted`


# Downloading the Source
## Installing Repo
```
$ mkdir -p ~/bin  
$ PATH=~/bin:$PATH
$ curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
$ chmod a+x ~/bin/repo    
```
## Initializing a Repo client
* Create an empty directory to hold your working files:

     ```
     $ mkdir WORKING_DIRECTORY
     $ cd WORKING_DIRECTORY
     ```

* Configure git with your real name and email address:

     ```
     $ git config --global user.name "Your Name"
     $ git config --global user.email "you@example.com"
     ```

* Initialize repo client with URL for the manifest and branch specified:

    ```
    $ repo init -u git://github.com/MentorEmbedded/mpsoc-manifest.git -b zynqmp-android_6.0.1 
    ```
* The following branches are available to use with the "-b" flag:

    | Branch               | Description |
    |----------------------|:-----------:|
    | zynqmp-android_6.0.1 | Android 6.0.1 based on android-6.0.1_r74 AOSP tag |

* Pull down the source tree:

    ```
    $ repo sync -c
    ```

# Download MALI 400 Userspace Binaries
* Download by using "Download Mali-400 User Space Components" link from https://www.xilinx.com/products/design-tools/embedded-software/petalinux-sdk/arm-mali-400-software-download.html
* Save it to the Android top build foder
* Run:

    ```
    $ wget https://www.xilinx.com/publications/products/tools/mali-400-userspace.tar
    $ mkdir -p tmp_mali && tar -xf mali-400-userspace.tar -C ./tmp_mali && mkdir -p vendor/xilinx/zynqmp/proprietary && cp -r tmp_mali/mali/Android/android-6.0.1/MALI-userspace/r6p2-01rel0/* vendor/xilinx/zynqmp/proprietary/ && rm -rf tmp_mali/
    ```

* You should have the following files after that:

    ```
    $ tree vendor/xilinx/zynqmp/proprietary/
    vendor/xilinx/zynqmp/proprietary/
    ├── lib64
    │   ├── libGLES_mali.so
    │   └── libGLES_mali.so_es1
    ├── libGLES_mali.so
    └── libGLES_mali.so_es1
    ```

# Build the code
* Set up build environment:

    ```
    $ source build/envsetup.sh
    ```

* Choose a target:

    ```
    $ lunch zcu102-eng
    ```

* The following lunch targets are supported (zcu102-* and ultrazed_eg_iocc-* targets use DisplayPort as display output, zcu102_ozzy-* targets use external LVDS LCD attached via FMC board as display output):

    | Target               | Description |
    |----------------------|-------------|
    | zcu102-eng | Development configuration of Android for ZCU102 board |
    | zcu102-userdebug | Production version of Android for ZCU102 board with root access and debuggability |
    | zcu102_ozzy-eng | Development configuration of Android for ZCU102 with iVeia Ozzy FMC board |
    | zcu102_ozzy-userdebug | Production version of Android for ZCU102 board with iVeia Ozzy FMC with root access and debuggability |
    | ultrazed_eg_iocc-eng | Development configuration of Android for Avnet UltraZed-EG SoM with IO Carrier Card |
    | ultrazed_eg_iocc-userdebug | Production version of Android for Avnet UltraZed-EG SoM with IO Carrier Card with root access and debuggability |

* Build the code:

   ```
   $ make -j8
   ```

# Preparing SD Card
Run the following script to prepare bootable SD card. Use path to your SD card instead of **/dev/mmcblk0**. Use **zcu102**, **zcu102_ozzy** or **ultrazed_eg_iocc** as a second argument to specify which product subfolder in **out/target/product/** to use.

```
$ cd WORKING_DIRECTORY
$ sudo device/xilinx/common/scripts/mksdcard.sh /dev/mmcblk0 zcu102
```

# Running the Build
## Xilinx ZCU102
* Set boot mode of the board to "SD Boot". Insert SD card to the board.
* Connect external monitor using DisplayPort for zcu102-*. Please note that DisplayPort must be connected before board power-on. External monitor should support 1920x1080 resolution.
* Connect USB mouse (and optionally USB keyboard) as shown below:
![ZCU102](http://www.wiki.xilinx.com/file/view/base-trd-2016-3-board-setup.png/602660656/777x568/base-trd-2016-3-board-setup.png)
* Attach iVeia Ozzy FMC for zcu102_ozzy-* targets. Refer to https://support.iveia.com/ for more detailed guide about usage of iVeia Ozzy FMC
* Power on the board
* If after booting you see only Android home screen background without other UI elements (like Status Bar, Home button, etc), then MALI libraries with a workaround for ES1 silicon should be used. Open serial debug console and perform the following to install libraries:

    ```
    $ su
    $ stop
    $ mount -o remount,rw /system
    $ mv /system/lib/egl/libGLES_mali.so_es1 /system/lib/egl/libGLES_mali.so
    $ mv /system/lib64/egl/libGLES_mali.so_es1 /system/lib64/egl/libGLES_mali.so
    $ sync
    $ mount -o remount,ro /system
    $ start
    ```
## Avnet UltraZed-EG with IOCC
* Install SoM on IOCC. Check official UltraZed-EG documentation for details.
* Use J11 to connect to USB Serial debug console. Check official UltraZed-EG documentation for details.
* Set boot mode of the board to "SD Boot" - on SoM set SW2[1-4] to OFF-ON-OFF-ON. 
* Insert SD card to the board.
* Connect external monitor using DisplayPort. Please note that DisplayPort must be connected before board power-on. External monitor should support 1920x1080 resolution.
* Make sure that the following jumpers are set on IOCC - J1[2:3], J2[2:3], JP1 OFF.
* Power on the board
* If after booting you see only Android home screen background without other UI elements (like Status Bar, Home button, etc), then MALI libraries with a workaround for ES1 silicon should be used. Open serial debug console and perform the following to install libraries:

    $ su
    $ stop
    $ mount -o remount,rw /system
    $ mv /system/lib/egl/libGLES_mali.so_es1 /system/lib/egl/libGLES_mali.so
    $ mv /system/lib64/egl/libGLES_mali.so_es1 /system/lib64/egl/libGLES_mali.so
    $ sync
    $ mount -o remount,ro /system
    $ start

# Reproducing

- script: `gen-boot.sh`


1. `build-linux.sh` Build the Linux (OP-TEE Linux Driver) v
2. `build-tf.sh` ARM-tf v (workaround)
3. `build-u-boot.sh` U-BOOT v
4. `build-optee.sh` Build optee v (we do need Xilinx' gcc.........)
5. `build-fw.sh` Build fsbl/pmufw from xsdk 2018.2
6. Create `boot.bin`
7. Copy to SD

- Setup adb

- Push TAs.

- ndk
- https://hub.docker.com/r/bitriseio/android-ndk/
- ./docker/run-ndk.sh


TODO: dockerize this.
# optee_client v
	$ ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk APP_PLATFORM=android-21 APP_ABI=arm64-v8a 
	$ EXPORT_DIR=`pwd`/out make CROSS_COMPILE=aarch64-linux-gnu-
	
Copy generated "libteec.so" to /dev/sdx5/lib & /dev/sdx5/lib64

Copy generated "tee-supplicant" to /dev/sdx5/bin

# Make optee_examples (ICAP_write)

1. cd to the dir: `cd optee/optee_examples/ICAP_write`.
2. Build using the containers (assuming you have *android* container (`./docker/build.sh`)): `./build-test-docker.sh`.



Copy generated .ta file to /dev/sdx5/vendor/lib/optee_armtz

Copy generated .ta file to /dev/sdx2/data/vendor/tee/optee_armtz (changed..)

Copy generated host binary(tee_embassy) to /dev/sdx5/bin

# For tee-supplicant & save configure register values
Add following lines under "service surfaceflinger" in /dev/sdx2/init.rc file

	service tee-supplicant /system/bin/tee-supplicant
    		class core
    		user root
    		group shell
    		oneshot
	
	service embassy /system/bin/tee_embassy disconnected
    		class core
    		user root
    		group shell
    		oneshot


# Loading/unloading an Ambata onto FPGA

    $ tee_embassy load/unload safeLogin/storage/SSO

# IMPORTANT
You should connect the DisplayPort after the system is completely booted up.



