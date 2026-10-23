/**
 * @file registrators.hpp
 *
 * Declaration of functions to register Python bindings to C++ objects
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef RAWDATAFILELIBS_PYBINDSRC_REGISTRATORS_HPP_
#define RAWDATAFILELIBS_PYBINDSRC_REGISTRATORS_HPP_

#include <pybind11/pybind11.h>

namespace dunedaq::rawdatafilelibs::python {

void
register_renameme(pybind11::module&);

}

#endif // RAWDATAFILELIBS_PYBINDSRC_REGISTRATORS_HPP_
