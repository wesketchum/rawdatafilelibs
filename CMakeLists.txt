cmake_minimum_required(VERSION 3.12)
project(rawdatafilelibs VERSION 0.1.0)

find_package(daq-cmake REQUIRED)

daq_setup_environment()

find_package(ers REQUIRED)
find_package(cetlib REQUIRED) # Uses the daq-buildtools/cmake/Findcetlib.cmake
find_package(daqdataformats REQUIRED)
find_package(detdataformats REQUIRED)
find_package(Boost COMPONENTS unit_test_framework REQUIRED)

set(RAWDATAFILELIBS_DEPENDENCIES
    ${CETLIB}
    ${CETLIB_EXCEPT}
    ers::ers
    daqdataformats::daqdataformats
    detdataformats::detdataformats)

##############################################################################
# Main library

daq_add_library(Backend.cpp
                BackendFactory.cpp
                FileMetadata.cpp
                FileUtils.cpp
                InMemoryBackend.cpp
                RawDataFileReader.cpp
                RawDataFileWriter.cpp
                Recovery.cpp
                LINK_LIBRARIES ${RAWDATAFILELIBS_DEPENDENCIES})

##############################################################################
# Test plugin: the in-memory test double, loadable as "rdfmem" in a work area. It is installed
# under test/lib64, which dbt adds to CET_PLUGIN_PATH, not with the package's plugins.

daq_add_plugin(rdfmem duneRawDataFileBackend TEST LINK_LIBRARIES rawdatafilelibs)

##############################################################################
# Applications

daq_add_application(rawdatafile_recover rawdatafile_recover.cxx LINK_LIBRARIES rawdatafilelibs)

##############################################################################
# Python bindings (the C++-mirror module _daq_rawdatafilelibs_py; the Pythonic layer is in python/)

daq_add_python_bindings(*.cpp LINK_LIBRARIES rawdatafilelibs)

##############################################################################
# Unit Tests (the Python tests are in test/python, run with pytest)

daq_add_unit_test(FileUtils_test LINK_LIBRARIES rawdatafilelibs)
daq_add_unit_test(RawDataFile_test LINK_LIBRARIES rawdatafilelibs)
daq_add_unit_test(BackendFactory_test LINK_LIBRARIES rawdatafilelibs)
add_dependencies(BackendFactory_test rawdatafilelibs_rdfmem_duneRawDataFileBackend)
daq_add_unit_test(Recovery_test LINK_LIBRARIES rawdatafilelibs)
add_dependencies(Recovery_test rawdatafilelibs_rdfmem_duneRawDataFileBackend)

##############################################################################

daq_install()
