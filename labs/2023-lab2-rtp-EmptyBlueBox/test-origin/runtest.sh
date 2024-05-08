#!/bin/bash

if [ ! -f ".built" ]; then
    touch .built &&
        rm -rf build &&
        cp test/test.cpp src/ &&
        cmake -B build/ &&
        cd build/ &&
        make -j &&
        cd ..
fi

./build/rtp_test_all --gtest_filter="RTP.$1"
