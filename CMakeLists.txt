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
# Plugins: the in-memory backend, for files named "*.rdfmem"

daq_add_plugin(rdfmem duneRawDataFileBackend LINK_LIBRARIES rawdatafilelibs)

##############################################################################
# Applications

daq_add_application(rawdatafile_recover rawdatafile_recover.cxx LINK_LIBRARIES rawdatafilelibs)

##############################################################################
# Unit Tests

daq_add_unit_test(FileUtils_test LINK_LIBRARIES rawdatafilelibs)
daq_add_unit_test(RawDataFile_test LINK_LIBRARIES rawdatafilelibs)
daq_add_unit_test(BackendFactory_test LINK_LIBRARIES rawdatafilelibs)
add_dependencies(BackendFactory_test rawdatafilelibs_rdfmem_duneRawDataFileBackend)
daq_add_unit_test(Recovery_test LINK_LIBRARIES rawdatafilelibs)
add_dependencies(Recovery_test rawdatafilelibs_rdfmem_duneRawDataFileBackend)

##############################################################################

daq_install()
