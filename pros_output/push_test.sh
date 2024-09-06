#!/bin/bash

# mount data partition
sudo mount /dev/sdd4 mnt
sudo cp /home/jwseo/workspace/PrOS_ver6/optee/hello_world/output/bin/* mnt/tee/bin
sudo cp /home/jwseo/workspace/PrOS_ver6/optee/hello_world/output/ta/* mnt/tee/optee_armtz
sudo cp /home/jwseo/workspace/PrOS_ver6/optee/optee_client/libs/arm64-v8a/tee-supplicant mnt/tee/bin
sudo ls -al mnt/tee/bin
sudo ls -al mnt/tee/optee_armtz
sudo umount mnt/

#mount system partition
sudo mount /dev/sdd5 mnt
sudo cp /home/jwseo/workspace/PrOS_ver6/optee/optee_client/libs/arm64-v8a/libteec.so mnt/lib64
sudo cp /home/jwseo/workspace/PrOS_ver6/optee/optee_client/libs/arm64-v8a/libteec.so mnt/lib
sudo cp /home/jwseo/workspace/PrOS_ver6/optee/optee_client/libs/arm64-v8a/tee-supplicant mnt/bin
sudo cp /home/jwseo/ta/* mnt/lib/optee_armtz
sudo cp /home/jwseo/workspace/PrOS_ver6/optee/hello_world/output/ta/* mnt/lib/optee_armtz
sudo ls -al mnt/lib/libteec.so
sudo ls -al mnt/lib64/libteec.so
sudo ls -al mnt/lib/optee_armtz
sudo umount mnt

