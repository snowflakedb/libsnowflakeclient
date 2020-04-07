#!/bin/bash -e
#
# start the local dev container and setup
#

set -o pipefail

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $THIS_DIR/../_init.sh
source $THIS_DIR/../scripts/login_internal_docker.sh

CONTAINER_NAME=clion_libsnowflakeclient_dev
IMAGE_NAME=${CONTAINER_NAME}_image
SSH_PORT=7786
GDB_PORT=7787

cd $THIS_DIR

docker pull ${BUILD_IMAGE_NAMES[@]}

IP_ADDR=$(ip -4 addr show scope global dev eth0 | grep inet | awk '{print $2}' | cut -d / -f 1)
#rebuild the image
[[ $(docker ps | grep "$CONTAINER_NAME") ]] && docker container stop "$CONTAINER_NAME"
docker image build -t $IMAGE_NAME \
 --build-arg BASE_IMAGE_NAME=${BUILD_IMAGE_NAMES[@]} \
 --build-arg LOCAL_USER_ID=$(id -u $USER) .

# -v $(cd $THIS_DIR/../.. && pwd):/mnt/host 

docker container run \
 --rm -d \
 -p $SSH_PORT:22 \
 -p $GDB_PORT:7777 \
 -e AWS_ACCESS_KEY_ID \
 -e AWS_SECRET_ACCESS_KEY \
 --security-opt seccomp=unconfined \
 --add-host=snowflake.reg.local:${IP_ADDR} \
 --name=$CONTAINER_NAME \
 $IMAGE_NAME


# clean up known host since we have started a new container
[[ $(ssh-keygen -F [dev.docker]:$SSH_PORT) != "" ]] && ssh-keygen -R [dev.docker]:$SSH_PORT
ssh-copy-id -p $SSH_PORT debugger@dev.docker
EP_COMMAND_KEY="export AWS_ACCESS_KEY_ID=${AWS_ACCESS_KEY_ID}"
EP_COMMAND_PWD="export AWS_SECRET_ACCESS_KEY=${AWS_SECRET_ACCESS_KEY}"
ssh debugger@dev.docker -p $SSH_PORT \
 -t "echo 'export PATH=/usr/lib64/ccache/:\$PATH' >> /home/debugger/.bashrc &&
     echo $EP_COMMAND_KEY >> /home/debugger/.bashrc &&
     echo $EP_COMMAND_PWD >> /home/debugger/.bashrc"

