/**
 * @file RawDataFileWriter.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/RawDataFileWriter.hpp"
#include "rawdatafilelibs/Issues.hpp"

#include "ers/ers.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace dunedaq::rawdatafilelibs {

namespace {
uint64_t // NOLINT(build/unsigned)
now_ms()
{
  return static_cast<uint64_t>( // NOLINT(build/unsigned)
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}
} // namespace

RawDataFileWriter::RawDataFileWriter(std::unique_ptr<WriterBackend> backend,
                                     const std::string& file_name,
                                     const FileMetadata& metadata,
                                     const daqdataformats::SourceIDGeoIDMap& geo_id_map,
                                     const std::string& inprogress_suffix)
  : m_backend(std::move(backend))
  , m_file_name(file_name)
  , m_current_file_name(file_name + inprogress_suffix)
  , m_metadata(metadata)
{
  if (m_backend == nullptr) {
    throw FileOpenFailed(ERS_HERE, m_current_file_name, "no backend was provided");
  }
  if (m_metadata.record_type == RecordType::kUnknown) {
    throw FileOpenFailed(ERS_HERE, m_current_file_name, "the record type (TriggerRecord or TimeSlice) must be set");
  }
  if (m_metadata.creation_timestamp_ms == 0) {
    m_metadata.creation_timestamp_ms = now_ms();
  }

  // never reuse or overwrite an existing file, e.g. one left behind by a crash
  for (auto const& name : { m_current_file_name, m_file_name }) {
    if (m_backend->path_exists(name)) {
      throw FileOpenFailed(ERS_HERE, name, "a file with this name already exists, and is left untouched");
    }
  }

  try {
    m_backend->open(m_current_file_name, m_metadata, geo_id_map);
  } catch (std::exception const& excpt) {
    throw FileOpenFailed(ERS_HERE, m_current_file_name, "the backend could not create the file", excpt);
  }
  m_open = true;
}

RawDataFileWriter::~RawDataFileWriter()
{
  try {
    close();
  } catch (ers::Issue const& issue) {
    ers::error(issue);
  } catch (std::exception const& excpt) {
    ers::error(FileCloseFailed(ERS_HERE, m_current_file_name, excpt.what()));
  }
}

WriterBackend&
RawDataFileWriter::get_backend() const
{
  return *m_backend;
}

bool
RawDataFileWriter::record_exists(const daqdataformats::RecordID& record_id) const
{
  if (!is_open()) {
    throw FileClosed(ERS_HERE, m_current_file_name);
  }
  return m_backend->record_exists(record_id);
}

FileStats
RawDataFileWriter::get_stats() const
{
  return is_open() ? m_backend->get_stats() : m_metadata.stats;
}

void
RawDataFileWriter::write(const daqdataformats::TriggerRecord& trigger_record)
{
  auto const& header = trigger_record.get_header_ref();
  BlobView header_view{ header.get_element_id(), header.get_storage_location(), header.get_total_size_bytes() };

  std::vector<BlobView> fragment_views;
  fragment_views.reserve(trigger_record.get_fragments_ref().size());
  for (auto const& fragment : trigger_record.get_fragments_ref()) {
    fragment_views.push_back(
      BlobView{ fragment->get_element_id(), fragment->get_storage_location(), fragment->get_size() });
  }

  write_record(
    daqdataformats::RecordIndex::from(trigger_record), RecordType::kTriggerRecord, header_view, fragment_views);
}

void
RawDataFileWriter::write(const daqdataformats::TimeSlice& timeslice)
{
  const daqdataformats::TimeSliceHeader header = timeslice.get_header();
  BlobView header_view{ header.element_id, &header, sizeof(daqdataformats::TimeSliceHeader) };

  std::vector<BlobView> fragment_views;
  fragment_views.reserve(timeslice.get_fragments_ref().size());
  for (auto const& fragment : timeslice.get_fragments_ref()) {
    fragment_views.push_back(
      BlobView{ fragment->get_element_id(), fragment->get_storage_location(), fragment->get_size() });
  }

  write_record(daqdataformats::RecordIndex::from(timeslice), RecordType::kTimeSlice, header_view, fragment_views);
}

void
RawDataFileWriter::write_record(const daqdataformats::RecordIndex& index,
                                RecordType record_type,
                                const BlobView& header,
                                const std::vector<BlobView>& fragments)
{
  if (!is_open()) {
    throw FileClosed(ERS_HERE, m_current_file_name);
  }
  if (record_type != m_metadata.record_type) {
    throw RecordTypeMismatch(
      ERS_HERE, m_current_file_name, record_type_to_string(m_metadata.record_type), record_type_to_string(record_type));
  }
  auto const& record_id = index.get_record_id();
  if (m_backend->record_exists(record_id)) {
    throw RecordAlreadyExists(ERS_HERE, record_id.to_string(), m_current_file_name);
  }

  try {
    m_backend->write_record(index, header, fragments);
  } catch (std::exception const& excpt) {
    throw FileWriteFailed(ERS_HERE, record_id.to_string(), m_current_file_name, excpt);
  }
}

void
RawDataFileWriter::close()
{
  if (!is_open()) {
    return;
  }
  m_open = false; // a failed close is not retried

  FileMetadata final_metadata = m_metadata;
  final_metadata.closing_timestamp_ms = now_ms();
  try {
    final_metadata.stats = m_backend->get_stats();
    m_backend->close(final_metadata);
  } catch (std::exception const& excpt) {
    throw FileCloseFailed(ERS_HERE,
                          m_current_file_name,
                          "the backend could not finalize the file, which keeps its in-progress name",
                          excpt);
  }
  m_metadata = final_metadata;

  if (m_current_file_name != m_file_name) {
    if (m_backend->path_exists(m_file_name)) {
      throw FileCloseFailed(ERS_HERE,
                            m_current_file_name,
                            "the final name " + m_file_name +
                              " already exists, so the file keeps its in-progress name");
    }
    try {
      m_backend->rename_path(m_current_file_name, m_file_name);
    } catch (std::exception const& excpt) {
      throw FileCloseFailed(ERS_HERE, m_current_file_name, "could not rename the file to " + m_file_name, excpt);
    }
    m_current_file_name = m_file_name;
  }
}

} // namespace dunedaq::rawdatafilelibs
