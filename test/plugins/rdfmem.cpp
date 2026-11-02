/**
 * @file rdfmem.cpp The in-memory test double as the test plugin "rdfmem", for files named "*.rdfmem" in tests
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/InMemoryBackend.hpp"

DEFINE_DUNE_RAW_DATA_FILE_BACKEND(dunedaq::rawdatafilelibs::InMemoryReaderBackend,
                                  dunedaq::rawdatafilelibs::InMemoryWriterBackend)
