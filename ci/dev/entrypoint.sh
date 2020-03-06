#!/bin/bash
source scl_source enable devtoolset-4
source scl_source enable python27

exec "$@"