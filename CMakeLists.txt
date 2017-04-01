cmake_minimum_required(VERSION 3.6)
project(p2p)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)


set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        main.cc
        main.hh
        p2papp.pro
)
# Added
find_package(Qt4 4.8 REQUIRED QtCore QtGui QtNetwork)

add_executable(p2p ${SOURCE_FILES})

# Added
target_link_libraries(p2p Qt4::QtCore Qt4::QtGui Qt4::QtNetwork)