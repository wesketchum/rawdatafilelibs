/**
 * @file module.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "registrators.hpp"

#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace dunedaq::rawdatafilelibs::python {

PYBIND11_MODULE(_daq_rawdatafilelibs_py, m)
{
  m.doc() = "C++ implementation of the rawdatafilelibs modules: format-independent raw data file reading and writing";

  register_file_metadata(m);
  register_raw_data_file(m);
  register_file_utils(m);
  register_recovery(m);
}

} // namespace dunedaq::rawdatafilelibs::python
