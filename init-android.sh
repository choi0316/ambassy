mkdir -p android
cd android
repo init -u git://github.com/MentorEmbedded/mpsoc-manifest.git -b zynqmp-android_6.0.1
repo sync -c
