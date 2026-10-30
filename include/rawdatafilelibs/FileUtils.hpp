/**
 * @file FileUtils.hpp Format-independent helpers for writing data files: names, disk space and time
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_FILEUTILS_HPP_
#define RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_FILEUTILS_HPP_

#include "daqdataformats/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>

namespace dunedaq::rawdatafilelibs {

/// Renames a file without ever replacing an existing file at the new name, atomically (so a file
/// that appears at the new name concurrently is not replaced either). Uses renameat2 with
/// RENAME_NOREPLACE, or link() + unlink() on filesystems without it (e.g. NFS). Throws
/// std::system_error (code EEXIST if the new name exists); on failure the file keeps its old name.
/// (With link() + unlink(), if neither the old nor the new name can then be removed, the file is
/// left under both names, and the exception says so.)
void
rename_without_replacing(const std::string& from, const std::string& to);

/// Bytes available to unprivileged users on the filesystem holding path; nullopt if path is invalid
std::optional<size_t>
get_free_space(const std::string& path);

/// The current time in milliseconds since the epoch, as stored in FileMetadata timestamps
uint64_t // NOLINT(build/unsigned)
current_time_ms();

/**
 * @brief The pieces of a DAQ output file name (the fields of appmodel FilenameParams plus context)
 */
struct FilenameParams
{
  std::string directory_path;
  std::string operational_environment;
  std::string file_type_prefix; ///< e.g. "raw", "tp"
  std::string run_number_prefix{ "run" };
  int digits_for_run_number{ 6 };
  std::string file_index_prefix;
  int digits_for_file_index{ 4 };
  std::string writer_identifier;
  std::string extension; ///< without the "."
};

/// "<dir>/<openv>_<type>_<runprefix><run>_<indexprefix><index>_<writer>.<extension>", as DAQ files have always been
/// named
std::string
make_file_name(const FilenameParams& params, daqdataformats::run_number_t run_number, size_t file_index);

/// UTC time as "YYYYMMDDTHHMMSS"
std::string
make_timestamp_string(std::time_t time);

/// Inserts "_<timestamp>" before ".<extension>" to make a file name unique. Names too short to have
/// anything before the extension are returned unchanged.
std::string
add_timestamp_suffix(const std::string& file_name, const std::string& extension, std::time_t time);

} // namespace dunedaq::rawdatafilelibs

#endif // RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_FILEUTILS_HPP_
