# M5Atom Serial Switch Sensor - CMakeLists.txt
# ESP32-S3 (M5Stack AtomS3 Lite) USB sensor device. See docs/00-overview.md.

cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "0.1")
set(PROJECT_VER_NUMBER 1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(M5Atom-Serial-Switch-Sensor)
