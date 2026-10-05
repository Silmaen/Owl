#
# Consumer of the installed OwlEngine package (audit G-03): no Owl build setting may leak into this project.
#
cmake_minimum_required(VERSION 3.24)
project(OwlTestPackage LANGUAGES CXX)

find_package(OwlEngine CONFIG REQUIRED)

add_executable(owl_test_package main.cpp)
target_link_libraries(owl_test_package PRIVATE Owl::OwlEngine)
