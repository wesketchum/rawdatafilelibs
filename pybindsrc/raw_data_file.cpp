/**
 * @file raw_data_file.cpp Python bindings for RawDataFileReader, RawDataFileWriter and the backend factory.
 * These mirror the C++ API; the Pythonic layer (rawdatafilelibs.open / rawdatafilelibs.create) is in Python.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "registrators.hpp"

#include "rawdatafilelibs/Backend.hpp"
#include "rawdatafilelibs/BackendFactory.hpp"
#include "rawdatafilelibs/InMemoryBackend.hpp"
#include "rawdatafilelibs/RawDataFileReader.hpp"
#include "rawdatafilelibs/RawDataFileWriter.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <string>

namespace py = pybind11;

namespace dunedaq::rawdatafilelibs::python {

void
register_raw_data_file(py::module& m)
{
  using daqdataformats::SourceIDSelector;

  // The backend base classes, so that format packages can bind their backends (with their
  // format-specific extras) as subclasses, and get_backend() returns the most derived type
  py::class_<ReaderBackend>(m, "ReaderBackend");
  py::class_<WriterBackend>(m, "WriterBackend");

  py::class_<RawDataFileReader>(m, "RawDataFileReader")
    .def("get_path", &RawDataFileReader::get_path)
    .def("get_metadata", &RawDataFileReader::get_metadata, py::return_value_policy::copy)
    .def("get_record_type", &RawDataFileReader::get_record_type)
    .def("get_geo_id_map", &RawDataFileReader::get_geo_id_map, py::return_value_policy::copy)
    .def("get_record_ids", &RawDataFileReader::get_record_ids, py::return_value_policy::copy)
    .def("contains", &RawDataFileReader::contains, py::arg("record_id"))
    // a copy, since the reader's cached index goes away when it is closed
    .def("get_index", &RawDataFileReader::get_index, py::arg("record_id"), py::return_value_policy::copy)
    .def("get_fragment_source_ids",
         &RawDataFileReader::get_fragment_source_ids,
         py::arg("record_id"),
         py::arg("selector") = SourceIDSelector{})
    .def("get_geo_ids",
         &RawDataFileReader::get_geo_ids,
         py::arg("record_id"),
         py::arg("selector") = SourceIDSelector{},
         "GeoIDs of the selected fragments actually present in the record")
    .def("get_fragment", &RawDataFileReader::get_fragment, py::arg("record_id"), py::arg("source_id"))
    .def("get_fragments",
         &RawDataFileReader::get_fragments,
         py::arg("record_id"),
         py::arg("selector") = SourceIDSelector{})
    .def("get_trigger_record_header", &RawDataFileReader::get_trigger_record_header, py::arg("record_id"))
    .def("get_timeslice_header", &RawDataFileReader::get_timeslice_header, py::arg("record_id"))
    .def("get_trigger_record", &RawDataFileReader::get_trigger_record, py::arg("record_id"))
    .def("get_timeslice", &RawDataFileReader::get_timeslice, py::arg("record_id"))
    .def("get_backend",
         &RawDataFileReader::get_backend,
         py::return_value_policy::reference_internal,
         "The format backend, for format-specific extras (e.g. hdf5libs.HDF5ReaderBackend)")
    .def("close", &RawDataFileReader::close)
    .def("is_open", &RawDataFileReader::is_open)
    .def("__enter__", [](RawDataFileReader& self) -> RawDataFileReader& { return self; })
    .def("__exit__", [](RawDataFileReader& self, const py::args&) { self.close(); })
    .def("__repr__",
         [](const RawDataFileReader& self) { return "<rawdatafilelibs::RawDataFileReader " + self.get_path() + ">"; });

  py::class_<RawDataFileWriter>(m, "RawDataFileWriter")
    .def_property_readonly_static(
      "s_default_inprogress_suffix",
      [](const py::object&) { return std::string(RawDataFileWriter::s_default_inprogress_suffix); })
    .def("write", py::overload_cast<const daqdataformats::TriggerRecord&>(&RawDataFileWriter::write), py::arg("record"))
    .def("write", py::overload_cast<const daqdataformats::TimeSlice&>(&RawDataFileWriter::write), py::arg("record"))
    .def("record_exists", &RawDataFileWriter::record_exists, py::arg("record_id"))
    .def("get_stats", &RawDataFileWriter::get_stats)
    .def("get_metadata", &RawDataFileWriter::get_metadata, py::return_value_policy::copy)
    .def("get_file_name", &RawDataFileWriter::get_file_name)
    .def("get_current_file_name", &RawDataFileWriter::get_current_file_name)
    .def("get_backend", &RawDataFileWriter::get_backend, py::return_value_policy::reference_internal)
    .def("close", &RawDataFileWriter::close)
    .def("is_open", &RawDataFileWriter::is_open)
    .def("__enter__", [](RawDataFileWriter& self) -> RawDataFileWriter& { return self; })
    .def("__exit__", [](RawDataFileWriter& self, const py::args&) { self.close(); })
    .def("__repr__", [](const RawDataFileWriter& self) {
      return "<rawdatafilelibs::RawDataFileWriter " + self.get_current_file_name() + ">";
    });

  m.def("backend_name_for_path",
        &backend_name_for_path,
        py::arg("path"),
        py::arg("inprogress_suffix") = std::string(RawDataFileWriter::s_default_inprogress_suffix));
  m.def("open_raw_data_file",
        &open_raw_data_file,
        py::arg("path"),
        py::arg("backend_name") = "",
        "Open an existing file; the backend defaults to the one named by the file's extension");
  m.def("create_raw_data_file",
        &create_raw_data_file,
        py::arg("file_name"),
        py::arg("metadata"),
        py::arg("geo_id_map") = daqdataformats::SourceIDGeoIDMap{},
        py::arg("backend_name") = "",
        py::arg("inprogress_suffix") = std::string(RawDataFileWriter::s_default_inprogress_suffix),
        "Create a new file; the backend defaults to the one named by the file's extension");

  // The in-memory store behind "*.rdfmem" files, which lives as long as the process
  m.def(
    "in_memory_file_exists",
    [](const std::string& path) { return InMemoryStore::instance().exists(path); },
    py::arg("path"));
  m.def(
    "remove_in_memory_file", [](const std::string& path) { InMemoryStore::instance().remove(path); }, py::arg("path"));
  m.def("clear_in_memory_files", []() { InMemoryStore::instance().clear(); });
}

} // namespace dunedaq::rawdatafilelibs::python
