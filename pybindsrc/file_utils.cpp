/**
 * @file file_utils.cpp Python bindings for the generic file utilities
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "registrators.hpp"

#include "rawdatafilelibs/FileUtils.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <ctime>

namespace py = pybind11;

namespace dunedaq::rawdatafilelibs::python {

void
register_file_utils(py::module& m)
{
  py::class_<FileNameParams>(m, "FileNameParams")
    .def(py::init())
    .def_readwrite("directory_path", &FileNameParams::directory_path)
    .def_readwrite("operational_environment", &FileNameParams::operational_environment)
    .def_readwrite("file_type_prefix", &FileNameParams::file_type_prefix)
    .def_readwrite("run_number_prefix", &FileNameParams::run_number_prefix)
    .def_readwrite("digits_for_run_number", &FileNameParams::digits_for_run_number)
    .def_readwrite("file_index_prefix", &FileNameParams::file_index_prefix)
    .def_readwrite("digits_for_file_index", &FileNameParams::digits_for_file_index)
    .def_readwrite("writer_identifier", &FileNameParams::writer_identifier)
    .def_readwrite("extension", &FileNameParams::extension);

  m.def("make_file_name", &make_file_name, py::arg("params"), py::arg("run_number"), py::arg("file_index"));
  m.def(
    "make_timestamp_string",
    [](int64_t seconds_since_epoch) { return make_timestamp_string(static_cast<std::time_t>(seconds_since_epoch)); },
    py::arg("seconds_since_epoch"),
    "UTC time as YYYYMMDDTHHMMSS");
  m.def(
    "add_timestamp_suffix",
    [](const std::string& file_name, const std::string& extension, int64_t seconds_since_epoch) {
      return add_timestamp_suffix(file_name, extension, static_cast<std::time_t>(seconds_since_epoch));
    },
    py::arg("file_name"),
    py::arg("extension"),
    py::arg("seconds_since_epoch"));
  m.def("get_free_space", &get_free_space, py::arg("path"), "Bytes available on the filesystem holding path, or None");
}

} // namespace dunedaq::rawdatafilelibs::python
