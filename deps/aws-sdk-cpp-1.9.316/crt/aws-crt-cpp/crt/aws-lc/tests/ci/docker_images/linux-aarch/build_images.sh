#!/bin/bash -ex
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

# Log Docker hub limit https://docs.docker.com/docker-hub/download-rate-limit/#how-can-i-check-my-current-rate
TOKEN=$(curl "https://auth.docker.io/token?service=registry.docker.io&scope=repository:ratelimitpreview/test:pull" | jq -r .token)
curl --head -H "Authorization: Bearer $TOKEN" https://registry-1.docker.io/v2/ratelimitpreview/test/manifests/latest

docker build -t amazonlinux-2-aarch:base amazonlinux-2_base
docker build -t amazonlinux-2-aarch:gcc-7x amazonlinux-2_gcc-7x
docker build -t amazonlinux-2-aarch:clang-7x amazonlinux-2_clang-7x
docker build -t ubuntu-20.04-aarch:base ubuntu-20.04_base
docker build -t ubuntu-20.04-aarch:gcc-7x ubuntu-20.04_gcc-7x
docker build -t ubuntu-20.04-aarch:gcc-8x ubuntu-20.04_gcc-8x
docker build -t ubuntu-20.04-aarch:gcc-11x ubuntu-20.04_gcc-11x
docker build -t ubuntu-20.04-aarch:clang-7x ubuntu-20.04_clang-7x
docker build -t ubuntu-20.04-aarch:clang-8x ubuntu-20.04_clang-8x
docker build -t ubuntu-20.04-aarch:clang-9x ubuntu-20.04_clang-9x
docker build -t ubuntu-20.04-aarch:clang-10x ubuntu-20.04_clang-10x
docker build -t ubuntu-20.04-aarch:clang-7x-bm-framework ubuntu-20.04_clang-7x-bm-framework
# This passes in the Dockerfile in the folder but uses the parent directory for the context so it has access to cryptofuzz_data.zip
docker build -t ubuntu-20.04-aarch:cryptofuzz -f ubuntu-20.04_cryptofuzz/Dockerfile ../
