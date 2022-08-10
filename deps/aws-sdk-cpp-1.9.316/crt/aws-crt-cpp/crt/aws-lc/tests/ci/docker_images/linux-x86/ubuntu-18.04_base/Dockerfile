# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

FROM ubuntu:18.04

SHELL ["/bin/bash", "-c"]

RUN set -ex && \
    apt-get update && \
    apt-get -y --no-install-recommends upgrade && \
    apt-get -y --no-install-recommends install \
    cmake \
    python3 \
    ninja-build \
    perl \
    ca-certificates \
    wget && \
    cd /tmp && \
    wget https://dl.google.com/go/go1.13.12.linux-amd64.tar.gz && \
    tar -xvf go1.13.12.linux-amd64.tar.gz && \
    mv go /usr/local && \
    apt-get autoremove --purge -y && \
    apt-get clean && \
    apt-get autoclean && \
    rm -rf /var/lib/apt/lists/* && \
    rm -rf /tmp/*

ENV GOROOT=/usr/local/go
ENV PATH="$GOROOT/bin:$PATH"
