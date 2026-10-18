#
# Consumer of the installed OwlEngine package (audit G-03): no Owl build setting may leak into this project.
#
cmake_minimum_required(VERSION 3.24)
project(OwlTestPackage LANGUAGES CXX)

# OWL_TEST_GUI: the package was built with the Gui module (owlengine option `gui`).
option(OWL_TEST_GUI "Check the Owl::Gui component too" ON)
if (OWL_TEST_GUI)
    find_package(OwlEngine CONFIG REQUIRED COMPONENTS Gui)
else ()
    find_package(OwlEngine CONFIG REQUIRED)
endif ()

# <owl.h> with Owl::OwlEngine alone: EnTT is its only public dependency (no imgui, no yaml-cpp).
add_executable(owl_test_package main.cpp)
target_link_libraries(owl_test_package PRIVATE Owl::OwlEngine)

# <owlgui.h> with the optional Owl::Gui target, which brings imgui.
if (OWL_TEST_GUI)
    add_executable(owl_test_package_gui gui_main.cpp)
    target_link_libraries(owl_test_package_gui PRIVATE Owl::Gui)
endif ()
