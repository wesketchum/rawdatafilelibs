/**
 * @file BackendFactory.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/BackendFactory.hpp"
#include "rawdatafilelibs/Issues.hpp"

#include "cetlib/BasicPluginFactory.h"
#include "cetlib_except/exception.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq::rawdatafilelibs {

namespace {
const std::string s_plugin_type = "duneRawDataFileBackend"; // NOLINT(cert-err58-cpp)

// Suffixes that other tools add to finished files, ignored when choosing the backend
const std::vector<std::string> s_ignored_suffixes = { ".copied" }; // NOLINT(cert-err58-cpp)

// Removes suffix from the end of name, if name ends with it and has something before it
bool
remove_suffix(std::string& name, const std::string& suffix)
{
  if (suffix.empty() || name.size() <= suffix.size() ||
      name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
    return false;
  }
  name.resize(name.size() - suffix.size());
  return true;
}

cet::BasicPluginFactory&
reader_factory()
{
  static cet::BasicPluginFactory bpf(s_plugin_type, "make_reader"); // NOLINT
  return bpf;
}

cet::BasicPluginFactory&
writer_factory()
{
  static cet::BasicPluginFactory bpf(s_plugin_type, "make_writer"); // NOLINT
  return bpf;
}
} // namespace

std::string
backend_name_for_path(const std::string& path, const std::string& inprogress_suffix)
{
  std::string name = std::filesystem::path(path).filename().string();
  remove_suffix(name, inprogress_suffix);
  for (auto const& suffix : s_ignored_suffixes) {
    if (remove_suffix(name, suffix)) {
      break;
    }
  }
  auto dot_pos = name.rfind('.');
  if (dot_pos == std::string::npos || dot_pos == 0 || dot_pos + 1 == name.size()) {
    return "";
  }
  return name.substr(dot_pos + 1);
}

std::unique_ptr<ReaderBackend>
make_reader_backend(const std::string& backend_name, const std::string& path_for_messages)
{
  std::unique_ptr<ReaderBackend> backend;
  try {
    if (!backend_name.empty()) {
      backend = reader_factory().makePlugin<std::unique_ptr<ReaderBackend>>(backend_name);
    }
  } catch (cet::exception const& excpt) {
    throw BackendNotAvailable(ERS_HERE, backend_name, path_for_messages, excpt);
  }
  if (backend == nullptr) {
    throw BackendNotAvailable(ERS_HERE, backend_name, path_for_messages);
  }
  return backend;
}

std::unique_ptr<WriterBackend>
make_writer_backend(const std::string& backend_name, const std::string& path_for_messages)
{
  std::unique_ptr<WriterBackend> backend;
  try {
    if (!backend_name.empty()) {
      backend = writer_factory().makePlugin<std::unique_ptr<WriterBackend>>(backend_name);
    }
  } catch (cet::exception const& excpt) {
    throw BackendNotAvailable(ERS_HERE, backend_name, path_for_messages, excpt);
  }
  if (backend == nullptr) {
    throw BackendNotAvailable(ERS_HERE, backend_name, path_for_messages);
  }
  return backend;
}

std::unique_ptr<RawDataFileReader>
open_raw_data_file(const std::string& path, const std::string& backend_name)
{
  const std::string name = backend_name.empty() ? backend_name_for_path(path) : backend_name;
  return std::make_unique<RawDataFileReader>(make_reader_backend(name, path), path);
}

std::unique_ptr<RawDataFileWriter>
create_raw_data_file(const std::string& file_name,
                     const FileMetadata& metadata,
                     const daqdataformats::SourceIDGeoIDMap& geo_id_map,
                     const std::string& backend_name,
                     const std::string& inprogress_suffix)
{
  const std::string name = backend_name.empty() ? backend_name_for_path(file_name) : backend_name;
  return std::make_unique<RawDataFileWriter>(
    make_writer_backend(name, file_name), file_name, metadata, geo_id_map, inprogress_suffix);
}

} // namespace dunedaq::rawdatafilelibs
