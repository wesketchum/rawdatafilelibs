/**
 * @file BackendFactory.hpp Creating readers and writers for a file, by loading its format's backend plugin
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_BACKENDFACTORY_HPP_
#define RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_BACKENDFACTORY_HPP_

#include "rawdatafilelibs/Backend.hpp"
#include "rawdatafilelibs/RawDataFileReader.hpp"
#include "rawdatafilelibs/RawDataFileWriter.hpp"

#include <memory>
#include <string>

namespace dunedaq::rawdatafilelibs {

// The backend (plugin) name for a file: its extension, ignoring an in-progress suffix and the ".copied"
// suffix that the file transfer system adds, e.g. "run1.hdf5", "run1.hdf5.writing" and "run1.hdf5.copied"
// all give "hdf5". Empty if there is no extension.
std::string
backend_name_for_path(const std::string& path,
                      const std::string& inprogress_suffix = RawDataFileWriter::s_default_inprogress_suffix);

// Load the named backend plugin (cetlib plugin type "duneRawDataFileBackend"); throw BackendNotAvailable
std::unique_ptr<ReaderBackend>
make_reader_backend(const std::string& backend_name, const std::string& path_for_messages = "");
std::unique_ptr<WriterBackend>
make_writer_backend(const std::string& backend_name, const std::string& path_for_messages = "");

// Opens an existing file; the backend defaults to the one named by the file's extension
std::unique_ptr<RawDataFileReader>
open_raw_data_file(const std::string& path, const std::string& backend_name = "");

// Creates a new file; the backend defaults to the one named by the file's extension
std::unique_ptr<RawDataFileWriter>
create_raw_data_file(const std::string& file_name,
                     const FileMetadata& metadata,
                     const daqdataformats::SourceIDGeoIDMap& geo_id_map,
                     const std::string& backend_name = "",
                     const std::string& inprogress_suffix = RawDataFileWriter::s_default_inprogress_suffix);

} // namespace dunedaq::rawdatafilelibs

#endif // RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_BACKENDFACTORY_HPP_
