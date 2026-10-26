/**
 * @file Backend.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/Backend.hpp"
#include "rawdatafilelibs/FileUtils.hpp"

#include <cstdlib>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>

namespace dunedaq::rawdatafilelibs {

Buffer
allocate_buffer(size_t size)
{
  // malloc(0) may return nullptr; always allocate at least one byte so that data is non-null
  void* memory = std::malloc(size > 0 ? size : 1); // NOLINT(cppcoreguidelines-no-malloc)
  if (memory == nullptr) {
    throw std::bad_alloc();
  }
  return Buffer{ buffer_ptr_t(static_cast<char*>(memory)), size };
}

bool
WriterBackend::path_exists(const std::string& path) const
{
  return std::filesystem::exists(path);
}

void
WriterBackend::rename_path(const std::string& from, const std::string& to)
{
  rename_without_replacing(from, to);
}

RecoveryScan
WriterBackend::scan_for_recovery(const std::string& path)
{
  throw std::runtime_error("this file format's backend can't recover unfinished files (" + path + ")");
}

void
WriterBackend::store_recovery_metadata(const std::string& path, const RecoveryScan&, const FileMetadata&)
{
  throw std::runtime_error("this file format's backend can't recover unfinished files (" + path + ")");
}

} // namespace dunedaq::rawdatafilelibs
