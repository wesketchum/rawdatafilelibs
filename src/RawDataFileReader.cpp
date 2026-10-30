/**
 * @file RawDataFileReader.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/RawDataFileReader.hpp"
#include "rawdatafilelibs/Issues.hpp"

#include "ers/ers.hpp"

#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace dunedaq::rawdatafilelibs {

RawDataFileReader::RawDataFileReader(std::unique_ptr<ReaderBackend> backend, const std::string& file_name)
  : m_backend(std::move(backend))
  , m_file_name(file_name)
{
  if (m_backend == nullptr) {
    throw FileOpenFailed(ERS_HERE, m_file_name, "no backend was provided");
  }
  try {
    m_backend->open(m_file_name);
    m_metadata = m_backend->read_metadata();
    m_geo_id_map = m_backend->read_geo_id_map();
    m_record_ids = m_backend->read_record_ids();
  } catch (std::exception const& excpt) {
    try {
      m_backend->close();
    } catch (...) { // NOLINT(runtime/exceptions)
      // the open failure is what gets reported
    }
    throw FileOpenFailed(ERS_HERE, m_file_name, "the backend could not open or read the file", excpt);
  }

  // a stored map may break the one-SourceID-per-GeoID rule (e.g. from a faulty configuration);
  // the file is still read, with a warning
  if (auto shared = m_geo_id_map.get_shared_geo_ids(); !shared.empty()) {
    ers::warning(SharedGeoIDs(ERS_HERE, m_file_name, shared.size(), shared.begin()->to_string()));
  }
}

RawDataFileReader::~RawDataFileReader()
{
  try {
    close();
  } catch (ers::Issue const& issue) {
    ers::error(issue);
  } catch (std::exception const& excpt) {
    ers::error(FileCloseFailed(ERS_HERE, m_file_name, excpt.what()));
  } catch (...) { // NOLINT(runtime/exceptions) a backend may throw anything; a destructor must not
    ers::error(FileCloseFailed(ERS_HERE, m_file_name, "unknown exception"));
  }
}

void
RawDataFileReader::close()
{
  if (m_backend == nullptr) {
    return;
  }
  auto backend = std::move(m_backend); // released even if closing throws
  m_record_index_cache.clear();
  try {
    backend->close();
  } catch (std::exception const& excpt) {
    throw FileCloseFailed(ERS_HERE, m_file_name, "the backend could not close the file", excpt);
  } catch (...) { // NOLINT(runtime/exceptions)
    throw FileCloseFailed(ERS_HERE, m_file_name, "the backend could not close the file (unknown exception)");
  }
}

void
RawDataFileReader::check_open() const
{
  if (m_backend == nullptr) {
    throw FileClosed(ERS_HERE, m_file_name);
  }
}

void
RawDataFileReader::check_record_type(RecordType requested) const
{
  // files that don't say what they hold are given the benefit of the doubt
  if (m_metadata.record_type != RecordType::kUnknown && m_metadata.record_type != requested) {
    throw RecordTypeMismatch(
      ERS_HERE, m_file_name, record_type_to_string(m_metadata.record_type), record_type_to_string(requested));
  }
}

ReaderBackend&
RawDataFileReader::get_backend() const
{
  check_open();
  return *m_backend;
}

const daqdataformats::RecordIndex&
RawDataFileReader::get_record_index(const RecordID& record_id) const
{
  check_open();
  if (!contains(record_id)) {
    throw RecordIDNotFound(ERS_HERE, record_id.to_string(), m_file_name);
  }
  auto iter = m_record_index_cache.find(record_id);
  if (iter != m_record_index_cache.end()) {
    return iter->second;
  }
  try {
    return m_record_index_cache.emplace(record_id, m_backend->read_record_index(record_id)).first->second;
  } catch (std::exception const& excpt) {
    throw RecordIndexReadFailed(ERS_HERE, record_id.to_string(), m_file_name, excpt.what(), excpt);
  }
}

std::set<RawDataFileReader::SourceID>
RawDataFileReader::get_fragment_source_ids(const RecordID& record_id, const SourceIDSelector& selector) const
{
  return get_record_index(record_id).get_fragment_source_ids(selector);
}

std::set<detdataformats::GeoID>
RawDataFileReader::get_geo_ids(const RecordID& record_id, const SourceIDSelector& selector) const
{
  std::set<detdataformats::GeoID> geo_ids;
  for (auto const& source_id : get_fragment_source_ids(record_id, selector)) {
    for (auto const& geo_id : m_geo_id_map.get_geo_ids(source_id)) {
      geo_ids.insert(geo_id);
    }
  }
  return geo_ids;
}

Blob
RawDataFileReader::read_blob(const RecordID& record_id, const SourceID& source_id) const
{
  try {
    return m_backend->read_blob(record_id, source_id);
  } catch (std::exception const& excpt) {
    throw RecordPartUnreadable(
      ERS_HERE, source_id.to_string(), record_id.to_string(), m_file_name, excpt.what(), excpt);
  }
}

std::unique_ptr<daqdataformats::Fragment>
RawDataFileReader::get_fragment(const RecordID& record_id, const SourceID& source_id) const
{
  auto const& index = get_record_index(record_id);
  if (!index.contains_fragment(source_id)) {
    throw SourceIDNotFound(ERS_HERE, source_id.to_string(), record_id.to_string(), m_file_name);
  }

  Blob blob = read_blob(record_id, source_id);
  // the fragment's own size must cover its header and fit in the stored data
  auto const* header =
    reinterpret_cast<const daqdataformats::FragmentHeader*>(blob.data.get()); // NOLINT
  if (blob.size < sizeof(daqdataformats::FragmentHeader) || header->size > blob.size ||
      header->size < sizeof(daqdataformats::FragmentHeader)) {
    throw RecordPartUnreadable(ERS_HERE,
                               source_id.to_string(),
                               record_id.to_string(),
                               m_file_name,
                               "stored data (" + std::to_string(blob.size) + " bytes) is not a complete fragment");
  }
  return std::make_unique<daqdataformats::Fragment>(blob.data.release(),
                                                    daqdataformats::Fragment::BufferAdoptionMode::kTakeOverBuffer);
}

std::vector<std::unique_ptr<daqdataformats::Fragment>>
RawDataFileReader::get_fragments(const RecordID& record_id, const SourceIDSelector& selector) const
{
  std::vector<std::unique_ptr<daqdataformats::Fragment>> fragments;
  for (auto const& source_id : get_fragment_source_ids(record_id, selector)) {
    fragments.push_back(get_fragment(record_id, source_id));
  }
  return fragments;
}

std::unique_ptr<daqdataformats::TriggerRecordHeader>
RawDataFileReader::get_trigger_record_header(const RecordID& record_id) const
{
  check_record_type(RecordType::kTriggerRecord);
  auto const& index = get_record_index(record_id);
  Blob blob = read_blob(record_id, index.get_header_source_id());
  // the header is followed by its component requests, which must all be in the stored data
  // (the copying constructor below reads as many as the header claims)
  auto const* header_data =
    reinterpret_cast<const daqdataformats::TriggerRecordHeaderData*>(blob.data.get()); // NOLINT
  if (blob.size < sizeof(daqdataformats::TriggerRecordHeaderData) ||
      header_data->num_requested_components >
        (blob.size - sizeof(daqdataformats::TriggerRecordHeaderData)) / sizeof(daqdataformats::ComponentRequest)) {
    throw RecordPartUnreadable(ERS_HERE,
                               index.get_header_source_id().to_string(),
                               record_id.to_string(),
                               m_file_name,
                               "stored header (" + std::to_string(blob.size) +
                                 " bytes) is not a complete TriggerRecordHeader");
  }
  // copies the header out of the blob, which is then freed
  return std::make_unique<daqdataformats::TriggerRecordHeader>(blob.data.get(), true);
}

std::unique_ptr<daqdataformats::TimeSliceHeader>
RawDataFileReader::get_timeslice_header(const RecordID& record_id) const
{
  check_record_type(RecordType::kTimeSlice);
  auto const& index = get_record_index(record_id);
  Blob blob = read_blob(record_id, index.get_header_source_id());
  if (blob.size < sizeof(daqdataformats::TimeSliceHeader)) {
    throw RecordPartUnreadable(ERS_HERE,
                               index.get_header_source_id().to_string(),
                               record_id.to_string(),
                               m_file_name,
                               "stored header is too small to be a TimeSliceHeader");
  }
  auto header = std::make_unique<daqdataformats::TimeSliceHeader>();
  std::memcpy(header.get(), blob.data.get(), sizeof(daqdataformats::TimeSliceHeader));
  return header;
}

daqdataformats::TriggerRecord
RawDataFileReader::get_trigger_record(const RecordID& record_id) const
{
  daqdataformats::TriggerRecord trigger_record(*get_trigger_record_header(record_id));
  for (auto& fragment : get_fragments(record_id)) {
    trigger_record.add_fragment(std::move(fragment));
  }
  return trigger_record;
}

daqdataformats::TimeSlice
RawDataFileReader::get_timeslice(const RecordID& record_id) const
{
  daqdataformats::TimeSlice timeslice(*get_timeslice_header(record_id));
  for (auto& fragment : get_fragments(record_id)) {
    timeslice.add_fragment(std::move(fragment));
  }
  return timeslice;
}

} // namespace dunedaq::rawdatafilelibs
