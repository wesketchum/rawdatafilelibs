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

RawDataFileReader::RawDataFileReader(std::unique_ptr<ReaderBackend> backend, const std::string& path)
  : m_backend(std::move(backend))
  , m_path(path)
{
  if (m_backend == nullptr) {
    throw FileOpenFailed(ERS_HERE, m_path, "no backend was provided");
  }
  try {
    m_backend->open(m_path);
    m_metadata = m_backend->read_metadata();
    m_geo_id_map = m_backend->read_geo_id_map();
    m_record_ids = m_backend->read_record_ids();
  } catch (std::exception const& excpt) {
    try {
      m_backend->close();
    } catch (...) { // NOLINT(runtime/exceptions)
      // the open failure is what gets reported
    }
    throw FileOpenFailed(ERS_HERE, m_path, "the backend could not open or read the file", excpt);
  }
}

RawDataFileReader::~RawDataFileReader()
{
  try {
    close();
  } catch (ers::Issue const& issue) {
    ers::error(issue);
  } catch (std::exception const& excpt) {
    ers::error(FileCloseFailed(ERS_HERE, m_path, excpt.what()));
  } catch (...) { // NOLINT(runtime/exceptions) a backend may throw anything; a destructor must not
    ers::error(FileCloseFailed(ERS_HERE, m_path, "unknown exception"));
  }
}

void
RawDataFileReader::close()
{
  if (m_backend == nullptr) {
    return;
  }
  auto backend = std::move(m_backend); // released even if closing throws
  m_index_cache.clear();
  try {
    backend->close();
  } catch (std::exception const& excpt) {
    throw FileCloseFailed(ERS_HERE, m_path, "the backend could not close the file", excpt);
  } catch (...) { // NOLINT(runtime/exceptions)
    throw FileCloseFailed(ERS_HERE, m_path, "the backend could not close the file (unknown exception)");
  }
}

void
RawDataFileReader::check_open() const
{
  if (m_backend == nullptr) {
    throw FileClosed(ERS_HERE, m_path);
  }
}

void
RawDataFileReader::check_record_type(RecordType requested) const
{
  // files that don't say what they hold are given the benefit of the doubt
  if (m_metadata.record_type != RecordType::kUnknown && m_metadata.record_type != requested) {
    throw RecordTypeMismatch(
      ERS_HERE, m_path, record_type_to_string(m_metadata.record_type), record_type_to_string(requested));
  }
}

ReaderBackend&
RawDataFileReader::get_backend() const
{
  check_open();
  return *m_backend;
}

const daqdataformats::RecordIndex&
RawDataFileReader::get_index(const RecordID& record_id) const
{
  check_open();
  if (!contains(record_id)) {
    throw RecordIDNotFound(ERS_HERE, record_id.to_string(), m_path);
  }
  auto iter = m_index_cache.find(record_id);
  if (iter != m_index_cache.end()) {
    return iter->second;
  }
  try {
    return m_index_cache.emplace(record_id, m_backend->read_index(record_id)).first->second;
  } catch (std::exception const& excpt) {
    throw RecordReadFailed(ERS_HERE, record_id.to_string(), m_path, excpt.what(), excpt);
  }
}

std::set<RawDataFileReader::SourceID>
RawDataFileReader::get_fragment_source_ids(const RecordID& record_id, const SourceIDSelector& selector) const
{
  return get_index(record_id).get_fragment_source_ids(selector);
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

Buffer
RawDataFileReader::read_blob(const RecordID& record_id, const SourceID& source_id) const
{
  try {
    return m_backend->read_blob(record_id, source_id);
  } catch (std::exception const& excpt) {
    throw FragmentMissing(ERS_HERE, source_id.to_string(), record_id.to_string(), m_path, excpt.what(), excpt);
  }
}

std::unique_ptr<daqdataformats::Fragment>
RawDataFileReader::get_fragment(const RecordID& record_id, const SourceID& source_id) const
{
  auto const& index = get_index(record_id);
  if (!index.contains_fragment(source_id)) {
    throw SourceIDNotFound(ERS_HERE, source_id.to_string(), record_id.to_string(), m_path);
  }

  Buffer buffer = read_blob(record_id, source_id);
  // the fragment's own size must cover its header and fit in the stored data
  if (buffer.size < sizeof(daqdataformats::FragmentHeader) ||
      reinterpret_cast<const daqdataformats::FragmentHeader*>(buffer.data.get())->size > buffer.size || // NOLINT
      reinterpret_cast<const daqdataformats::FragmentHeader*>(buffer.data.get())->size <                // NOLINT
        sizeof(daqdataformats::FragmentHeader)) {
    throw FragmentMissing(ERS_HERE,
                          source_id.to_string(),
                          record_id.to_string(),
                          m_path,
                          "stored data (" + std::to_string(buffer.size) + " bytes) is not a complete fragment");
  }
  return std::make_unique<daqdataformats::Fragment>(buffer.data.release(),
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
  auto const& index = get_index(record_id);
  Buffer buffer = read_blob(record_id, index.get_header_source_id());
  // the header is followed by its component requests, which must all be in the stored data
  // (the copying constructor below reads as many as the header claims)
  if (buffer.size < sizeof(daqdataformats::TriggerRecordHeaderData) ||
      reinterpret_cast<const daqdataformats::TriggerRecordHeaderData*>(buffer.data.get()) // NOLINT
          ->num_requested_components >
        (buffer.size - sizeof(daqdataformats::TriggerRecordHeaderData)) / sizeof(daqdataformats::ComponentRequest)) {
    throw FragmentMissing(ERS_HERE,
                          index.get_header_source_id().to_string(),
                          record_id.to_string(),
                          m_path,
                          "stored header (" + std::to_string(buffer.size) +
                            " bytes) is not a complete TriggerRecordHeader");
  }
  // copies the header out of the buffer, which is then freed
  return std::make_unique<daqdataformats::TriggerRecordHeader>(buffer.data.get(), true);
}

std::unique_ptr<daqdataformats::TimeSliceHeader>
RawDataFileReader::get_timeslice_header(const RecordID& record_id) const
{
  check_record_type(RecordType::kTimeSlice);
  auto const& index = get_index(record_id);
  Buffer buffer = read_blob(record_id, index.get_header_source_id());
  if (buffer.size < sizeof(daqdataformats::TimeSliceHeader)) {
    throw FragmentMissing(ERS_HERE,
                          index.get_header_source_id().to_string(),
                          record_id.to_string(),
                          m_path,
                          "stored header is too small to be a TimeSliceHeader");
  }
  auto header = std::make_unique<daqdataformats::TimeSliceHeader>();
  std::memcpy(header.get(), buffer.data.get(), sizeof(daqdataformats::TimeSliceHeader));
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
