/**
 * @file FileMetadata.hpp File-level metadata common to all raw data file formats
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_FILEMETADATA_HPP_
#define RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_FILEMETADATA_HPP_

#include "daqdataformats/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace dunedaq::rawdatafilelibs {

enum class RecordType
{
  kUnknown,
  kTriggerRecord,
  kTimeSlice
};

// "TriggerRecord", "TimeSlice" or "Unknown", matching the names stored in existing files
std::string
record_type_to_string(RecordType record_type);
RecordType
string_to_record_type(const std::string& name);

/**
 * @brief Sizes describing a file's contents
 */
struct FileStats
{
  size_t recorded_size{ 0 };              ///< Bytes of record data as stored (after any compression)
  size_t uncompressed_raw_data_size{ 0 }; ///< Bytes of record data before any compression
  size_t total_file_size{ 0 };            ///< Size of the whole file, including format overhead

  bool operator==(const FileStats&) const = default;
};

/**
 * @brief Metadata describing one raw data file. Writers receive it when the file is created
 * (closing_timestamp_ms and stats are filled in at close); readers return what the file stores.
 */
struct FileMetadata
{
  daqdataformats::run_number_t run_number{ 0 };
  size_t file_index{ 0 };
  std::string application_name;
  RecordType record_type{ RecordType::kUnknown };
  uint64_t creation_timestamp_ms{ 0 }; ///< Milliseconds since the epoch; the writer sets it if left at 0 // NOLINT
  uint64_t closing_timestamp_ms{ 0 };  ///< Milliseconds since the epoch; 0 while the file is open // NOLINT
  std::string operational_environment;
  std::string offline_data_stream;
  bool run_is_for_test_purposes{ false };
  unsigned compression_level{ 0 }; ///< 0 means no compression // NOLINT(build/unsigned)
  uint32_t layout_version{ 0 };    ///< Format-specific layout version; 0 if not applicable // NOLINT(build/unsigned)
  FileStats stats;

  bool operator==(const FileMetadata&) const = default;
};

} // namespace dunedaq::rawdatafilelibs

#endif // RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_FILEMETADATA_HPP_
