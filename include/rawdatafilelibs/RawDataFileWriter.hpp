/**
 * @file RawDataFileWriter.hpp Format-independent writing of raw data files
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_RAWDATAFILEWRITER_HPP_
#define RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_RAWDATAFILEWRITER_HPP_

#include "rawdatafilelibs/Backend.hpp"
#include "rawdatafilelibs/FileMetadata.hpp"

#include "daqdataformats/RecordID.hpp"
#include "daqdataformats/RecordIndex.hpp"
#include "daqdataformats/SourceIDGeoIDMap.hpp"
#include "daqdataformats/TimeSlice.hpp"
#include "daqdataformats/TriggerRecord.hpp"

#include <memory>
#include <string>

namespace dunedaq::rawdatafilelibs {

/**
 * @brief RawDataFileWriter writes TriggerRecords or TimeSlices to a new file of any format,
 * through that format's WriterBackend. Usually created with create_raw_data_file().
 *
 * The file is written under an in-progress name (the final name plus ".writing" by default)
 * and renamed to the final name by close(), so that complete files are easy to tell apart.
 * Existing files are never reused or overwritten: if either name already exists, construction
 * fails with FileOpenFailed, and close() fails (keeping the in-progress name) if the final name
 * has appeared in the meantime.
 */
class RawDataFileWriter
{
public:
  static constexpr const char* s_default_inprogress_suffix = ".writing";

  // Creates the file; throws FileOpenFailed. A creation_timestamp_ms of 0 is replaced by the current time.
  RawDataFileWriter(std::unique_ptr<WriterBackend> backend,
                    const std::string& file_name,
                    const FileMetadata& metadata,
                    const daqdataformats::SourceIDGeoIDMap& geo_id_map,
                    const std::string& inprogress_suffix = s_default_inprogress_suffix);

  // Calls close(); any failure is reported with ers::error
  ~RawDataFileWriter();

  RawDataFileWriter(const RawDataFileWriter&) = delete;
  RawDataFileWriter& operator=(const RawDataFileWriter&) = delete;
  RawDataFileWriter(RawDataFileWriter&&) = delete;
  RawDataFileWriter& operator=(RawDataFileWriter&&) = delete;

  // Throw RecordTypeMismatch, RecordAlreadyExists, FileClosed or FileWriteFailed
  void write(const daqdataformats::TriggerRecord& trigger_record);
  void write(const daqdataformats::TimeSlice& timeslice);

  bool record_exists(const daqdataformats::RecordID& record_id) const;

  FileStats get_stats() const;

  // The metadata as given at creation; after close(), the final metadata as stored
  const FileMetadata& get_metadata() const noexcept { return m_metadata; }

  const std::string& get_file_name() const noexcept { return m_file_name; }
  // The name on storage: the in-progress name while open, the final name once closed successfully
  const std::string& get_current_file_name() const noexcept { return m_current_file_name; }

  // For format-specific extras
  WriterBackend& get_backend() const;

  // Finalizes the file and renames it to its final name. May throw FileCloseFailed, in which
  // case the file keeps its in-progress name. Calling it again is a no-op.
  void close();
  bool is_open() const noexcept { return m_backend != nullptr && m_open; }

private:
  void write_record(const daqdataformats::RecordIndex& index,
                    RecordType record_type,
                    const BlobView& header,
                    const std::vector<BlobView>& fragments);

  std::unique_ptr<WriterBackend> m_backend;
  std::string m_file_name;
  std::string m_current_file_name;
  FileMetadata m_metadata;
  bool m_open{ false };
};

} // namespace dunedaq::rawdatafilelibs

#endif // RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_RAWDATAFILEWRITER_HPP_
