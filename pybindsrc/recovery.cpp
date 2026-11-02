/**
 * @file recovery.cpp Python bindings for recovering unfinished raw data files
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "registrators.hpp"

#include "rawdatafilelibs/Recovery.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>

namespace py = pybind11;

namespace dunedaq::rawdatafilelibs::python {

void
register_recovery(py::module& m)
{
  py::class_<RecoveryScan>(m, "RecoveryScan")
    .def_readonly("stored", &RecoveryScan::stored)
    .def_readonly("has_closing_timestamp", &RecoveryScan::has_closing_timestamp)
    .def_readonly("has_stats", &RecoveryScan::has_stats)
    .def_readonly("has_recovery_timestamp", &RecoveryScan::has_recovery_timestamp)
    .def_readonly("calculated_stats", &RecoveryScan::calculated_stats)
    .def_readonly("readable_records", &RecoveryScan::readable_records)
    .def_readonly("unreadable_records", &RecoveryScan::unreadable_records)
    .def_readonly("last_modified_ms", &RecoveryScan::last_modified_ms);

  py::class_<RecoveryReport>(m, "RecoveryReport")
    .def_readonly("path", &RecoveryReport::path)
    .def_readonly("final_path", &RecoveryReport::final_path)
    .def_readonly("scan", &RecoveryReport::scan)
    .def_readonly("final_metadata", &RecoveryReport::final_metadata)
    .def_readonly("changes", &RecoveryReport::changes)
    .def_readonly("applied", &RecoveryReport::applied)
    .def_readonly("renamed", &RecoveryReport::renamed)
    .def("__str__", [](const RecoveryReport& report) { return to_string(report); });

  m.def("recover_raw_data_file",
        &recover_raw_data_file,
        py::arg("path"),
        py::arg("apply") = false,
        py::arg("backend_name") = "",
        py::arg("inprogress_suffix") = std::string(RawDataFileWriter::s_default_inprogress_suffix),
        "Recover a file left unfinished (e.g. by a crash); without apply, only report what would be done");
}

} // namespace dunedaq::rawdatafilelibs::python
