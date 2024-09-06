#!/bin/bash


set -e
echo "===== 1. Building with gcc(android) container"
docker run \
  -v $(pwd)/../../../:/home/android/work \
  -it --rm \
  --net host \
  --workdir=/home/android/work/optee/optee_examples/ICAP_write \
  --entrypoint= \
  android bash build-test-android.sh



echo "===== 2. Building with ndk container"
docker run \
  -v $(pwd)/../../../:/bitrise/src \
  -it --rm \
  --net host \
  --workdir=/bitrise/src/optee/optee_examples/ICAP_write \
  --entrypoint= \
  bitriseio/android-ndk ./build-test-ndk.sh
