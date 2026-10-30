/**
 * @file RawDataFileReader.hpp Format-independent reading of raw data files
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_RAWDATAFILEREADER_HPP_
#define RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_RAWDATAFILEREADER_HPP_

#include "rawdatafilelibs/Backend.hpp"
#include "rawdatafilelibs/FileMetadata.hpp"

#include "daqdataformats/Fragment.hpp"
#include "daqdataformats/RecordID.hpp"
#include "daqdataformats/RecordIndex.hpp"
#include "daqdataformats/SourceIDGeoIDMap.hpp"
#include "daqdataformats/TimeSlice.hpp"
#include "daqdataformats/TriggerRecord.hpp"
#include "detdataformats/GeoID.hpp"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace dunedaq::rawdatafilelibs {

/**
 * @brief RawDataFileReader reads TriggerRecords or TimeSlices, and their parts, from a file of
 * any format, through that format's ReaderBackend. Usually created with open_raw_data_file().
 *
 * Lookups go through each record's RecordIndex, which is the source of truth for what a record
 * contains: asking for a SourceID that isn't in a record throws SourceIDNotFound, and a SourceID
 * that is listed but whose data can't be read throws RecordPartUnreadable.
 *
 * Indices are loaded on first use and cached. Not thread-safe.
 */
class RawDataFileReader
{
public:
  using RecordID = daqdataformats::RecordID;
  using SourceID = daqdataformats::SourceID;
  using SourceIDSelector = daqdataformats::SourceIDSelector;

  /// Opens file_name with the given backend; throws FileOpenFailed
  RawDataFileReader(std::unique_ptr<ReaderBackend> backend, const std::string& file_name);

  const std::string& get_file_name() const noexcept { return m_file_name; }
  const FileMetadata& get_metadata() const noexcept { return m_metadata; }
  RecordType get_record_type() const noexcept { return m_metadata.record_type; }
  const daqdataformats::SourceIDGeoIDMap& get_geo_id_map() const noexcept { return m_geo_id_map; }

  const std::set<RecordID>& get_record_ids() const noexcept { return m_record_ids; }
  bool contains(const RecordID& record_id) const { return m_record_ids.count(record_id) != 0; }

  /// Throws RecordIDNotFound
  const daqdataformats::RecordIndex& get_record_index(const RecordID& record_id) const;

  std::set<SourceID> get_fragment_source_ids(const RecordID& record_id, const SourceIDSelector& selector = {}) const;

  /// GeoIDs of the selected fragments actually present in the record
  std::set<detdataformats::GeoID> get_geo_ids(const RecordID& record_id, const SourceIDSelector& selector = {}) const;

  std::unique_ptr<daqdataformats::Fragment> get_fragment(const RecordID& record_id, const SourceID& source_id) const;
  std::vector<std::unique_ptr<daqdataformats::Fragment>> get_fragments(const RecordID& record_id,
                                                                       const SourceIDSelector& selector = {}) const;

  /// Throw RecordTypeMismatch if the file holds the other kind of record
  std::unique_ptr<daqdataformats::TriggerRecordHeader> get_trigger_record_header(const RecordID& record_id) const;
  std::unique_ptr<daqdataformats::TimeSliceHeader> get_timeslice_header(const RecordID& record_id) const;
  daqdataformats::TriggerRecord get_trigger_record(const RecordID& record_id) const;
  daqdataformats::TimeSlice get_timeslice(const RecordID& record_id) const;

  /// For format-specific extras, e.g. dynamic_cast<hdf5libs::HDF5ReaderBackend&>(reader.get_backend())
  ReaderBackend& get_backend() const;

  /// Releases the file. May throw FileCloseFailed; calling it again is a no-op. Afterwards only
  /// get_file_name(), get_metadata(), get_geo_id_map() and get_record_ids() may be used.
  void close();
  bool is_open() const noexcept { return m_backend != nullptr; }

  RawDataFileReader(const RawDataFileReader&) = delete;
  RawDataFileReader& operator=(const RawDataFileReader&) = delete;
  RawDataFileReader(RawDataFileReader&&) = delete;
  RawDataFileReader& operator=(RawDataFileReader&&) = delete;

  /// Calls close(); any failure is reported with ers::error
  ~RawDataFileReader();

private:
  void check_open() const;
  void check_record_type(RecordType requested) const;
  Blob read_blob(const RecordID& record_id, const SourceID& source_id) const;

  std::unique_ptr<ReaderBackend> m_backend;
  std::string m_file_name;
  FileMetadata m_metadata;
  daqdataformats::SourceIDGeoIDMap m_geo_id_map;
  std::set<RecordID> m_record_ids;
  mutable std::map<RecordID, daqdataformats::RecordIndex> m_record_index_cache;
};

} // namespace dunedaq::rawdatafilelibs

#endif // RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_RAWDATAFILEREADER_HPP_
