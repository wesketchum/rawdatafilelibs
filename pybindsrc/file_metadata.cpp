/**
 * @file file_metadata.cpp Python bindings for FileMetadata, FileStats and RecordType
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "registrators.hpp"

#include "rawdatafilelibs/FileMetadata.hpp"

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>

namespace py = pybind11;

namespace dunedaq::rawdatafilelibs::python {

void
register_file_metadata(py::module& m)
{
  py::enum_<RecordType>(m, "RecordType")
    .value("kUnknown", RecordType::kUnknown)
    .value("kTriggerRecord", RecordType::kTriggerRecord)
    .value("kTimeSlice", RecordType::kTimeSlice);

  m.def("record_type_to_string", &record_type_to_string, py::arg("record_type"));
  m.def("string_to_record_type", &string_to_record_type, py::arg("name"));

  py::class_<FileStats>(m, "FileStats")
    .def(py::init())
    .def_readwrite("recorded_size", &FileStats::recorded_size)
    .def_readwrite("uncompressed_raw_data_size", &FileStats::uncompressed_raw_data_size)
    .def_readwrite("total_file_size", &FileStats::total_file_size)
    .def(py::self == py::self)
    .def("__repr__", [](const FileStats& stats) {
      return "<rawdatafilelibs::FileStats recorded_size=" + std::to_string(stats.recorded_size) +
             " uncompressed_raw_data_size=" + std::to_string(stats.uncompressed_raw_data_size) +
             " total_file_size=" + std::to_string(stats.total_file_size) + ">";
    });

  py::class_<FileMetadata>(m, "FileMetadata")
    .def(py::init())
    .def_readwrite("run_number", &FileMetadata::run_number)
    .def_readwrite("file_index", &FileMetadata::file_index)
    .def_readwrite("application_name", &FileMetadata::application_name)
    .def_readwrite("record_type", &FileMetadata::record_type)
    .def_readwrite("creation_timestamp_ms", &FileMetadata::creation_timestamp_ms)
    .def_readwrite("closing_timestamp_ms", &FileMetadata::closing_timestamp_ms)
    .def_readwrite("operational_environment", &FileMetadata::operational_environment)
    .def_readwrite("offline_data_stream", &FileMetadata::offline_data_stream)
    .def_readwrite("run_is_for_test_purposes", &FileMetadata::run_is_for_test_purposes)
    .def_readwrite("compression_level", &FileMetadata::compression_level)
    .def_readwrite("layout_version", &FileMetadata::layout_version)
    .def_readwrite("stats", &FileMetadata::stats)
    .def(py::self == py::self)
    .def("__repr__", [](const FileMetadata& metadata) {
      return "<rawdatafilelibs::FileMetadata run_number=" + std::to_string(metadata.run_number) +
             " file_index=" + std::to_string(metadata.file_index) +
             " record_type=" + record_type_to_string(metadata.record_type) + ">";
    });
}

} // namespace dunedaq::rawdatafilelibs::python
