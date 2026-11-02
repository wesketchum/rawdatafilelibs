/**
 * @file registrators.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef RAWDATAFILELIBS_PYBINDSRC_REGISTRATORS_HPP_
#define RAWDATAFILELIBS_PYBINDSRC_REGISTRATORS_HPP_

#include <pybind11/pybind11.h>

namespace dunedaq::rawdatafilelibs::python {

void
register_file_metadata(pybind11::module& m);
void
register_raw_data_file(pybind11::module& m);
void
register_file_utils(pybind11::module& m);
void
register_recovery(pybind11::module& m);

} // namespace dunedaq::rawdatafilelibs::python

#endif // RAWDATAFILELIBS_PYBINDSRC_REGISTRATORS_HPP_
