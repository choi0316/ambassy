#!/bin/bash
set -e

export TA_NAME=ICAP_write
pushd optee/optee_examples/$TA_NAME
./build.sh
popd
export TA_NAME=TA_call
pushd optee/optee_examples/$TA_NAME
./build.sh
popd
export TA_NAME=sha256
pushd optee/optee_examples/$TA_NAME
./build.sh
popd
export TA_NAME=secure_storage
pushd optee/optee_examples/$TA_NAME
./build.sh
popd
export TA_NAME=aes
pushd optee/optee_examples/$TA_NAME
./build.sh
popd
