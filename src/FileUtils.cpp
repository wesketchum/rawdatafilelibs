/**
 * @file FileUtils.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/FileUtils.hpp"

#include <sys/statvfs.h>

#include <iomanip>
#include <sstream>
#include <string>

namespace dunedaq::rawdatafilelibs {

std::optional<size_t>
get_free_space(const std::string& path)
{
  struct statvfs vfs_results;
  if (statvfs(path.c_str(), &vfs_results) != 0) {
    return std::nullopt;
  }
  return static_cast<size_t>(vfs_results.f_bsize) * vfs_results.f_bavail;
}

std::string
make_file_name(const FileNameParams& params, daqdataformats::run_number_t run_number, size_t file_index)
{
  std::ostringstream oss;
  if (!params.directory_path.empty()) {
    oss << params.directory_path << "/";
  }
  oss << params.operational_environment << "_" << params.file_type_prefix << "_";
  oss << params.run_number_prefix << std::setw(params.digits_for_run_number) << std::setfill('0') << run_number << "_";
  oss << params.file_index_prefix << std::setw(params.digits_for_file_index) << std::setfill('0') << file_index;
  oss << "_" << params.writer_identifier << "." << params.extension;
  return oss.str();
}

std::string
make_timestamp_string(std::time_t time)
{
  std::tm utc_time{};
  gmtime_r(&time, &utc_time);
  std::ostringstream oss;
  oss << std::put_time(&utc_time, "%Y%m%dT%H%M%S");
  return oss.str();
}

std::string
add_timestamp_suffix(const std::string& file_name, const std::string& extension, std::time_t time)
{
  std::string unique_name = file_name;
  const size_t extension_length = extension.size() + 1; // + 1 for the "." before the extension
  // only when there is at least one character before the extension
  if (unique_name.length() > extension_length + 1) {
    unique_name.insert(unique_name.length() - extension_length, "_" + make_timestamp_string(time));
  }
  return unique_name;
}

} // namespace dunedaq::rawdatafilelibs
