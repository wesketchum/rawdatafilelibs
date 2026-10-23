/**
 * @file InMemoryBackend.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/InMemoryBackend.hpp"
#include "rawdatafilelibs/Issues.hpp"

#include <cstring>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dunedaq::rawdatafilelibs {

InMemoryStore&
InMemoryStore::instance()
{
  static InMemoryStore the_store;
  return the_store;
}

std::shared_ptr<InMemoryFile>
InMemoryStore::create(const std::string& path)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_files.count(path) != 0) {
    throw FileOpenFailed(ERS_HERE, path, "an in-memory file with this name already exists");
  }
  auto file = std::make_shared<InMemoryFile>();
  m_files[path] = file;
  return file;
}

std::shared_ptr<InMemoryFile>
InMemoryStore::find(const std::string& path) const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  auto iter = m_files.find(path);
  return (iter == m_files.end()) ? nullptr : iter->second;
}

bool
InMemoryStore::exists(const std::string& path) const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_files.count(path) != 0;
}

void
InMemoryStore::rename(const std::string& from, const std::string& to)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  auto iter = m_files.find(from);
  if (iter == m_files.end()) {
    throw std::runtime_error("no in-memory file named " + from);
  }
  if (m_files.count(to) != 0) {
    throw std::runtime_error("an in-memory file named " + to + " already exists");
  }
  m_files[to] = iter->second;
  m_files.erase(from);
}

void
InMemoryStore::remove(const std::string& path)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_files.erase(path);
}

void
InMemoryStore::clear()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_files.clear();
}

// ---------------------------------------------------------------------------

void
InMemoryReaderBackend::open(const std::string& path)
{
  m_file = InMemoryStore::instance().find(path);
  if (m_file == nullptr) {
    throw std::runtime_error("no in-memory file named " + path);
  }
}

FileMetadata
InMemoryReaderBackend::read_metadata()
{
  return m_file->metadata;
}

daqdataformats::SourceIDGeoIDMap
InMemoryReaderBackend::read_geo_id_map()
{
  return m_file->geo_id_map;
}

std::set<daqdataformats::RecordID>
InMemoryReaderBackend::read_record_ids()
{
  std::set<daqdataformats::RecordID> record_ids;
  for (auto const& [record_id, index] : m_file->indices) {
    record_ids.insert(record_id);
  }
  return record_ids;
}

daqdataformats::RecordIndex
InMemoryReaderBackend::read_index(const daqdataformats::RecordID& record_id)
{
  auto iter = m_file->indices.find(record_id);
  if (iter == m_file->indices.end()) {
    throw std::runtime_error("no index stored for record " + record_id.to_string());
  }
  return iter->second;
}

Buffer
InMemoryReaderBackend::read_blob(const daqdataformats::RecordID& record_id, const daqdataformats::SourceID& source_id)
{
  auto iter = m_file->blobs.find(std::make_pair(record_id, source_id));
  if (iter == m_file->blobs.end()) {
    throw std::runtime_error("no data stored for " + source_id.to_string() + " in record " + record_id.to_string());
  }
  Buffer buffer = allocate_buffer(iter->second.size());
  std::memcpy(buffer.data.get(), iter->second.data(), iter->second.size());
  return buffer;
}

void
InMemoryReaderBackend::close()
{
  m_file.reset();
}

// ---------------------------------------------------------------------------

void
InMemoryWriterBackend::open(const std::string& path,
                            const FileMetadata& metadata,
                            const daqdataformats::SourceIDGeoIDMap& geo_id_map)
{
  m_file = InMemoryStore::instance().create(path);
  m_file->metadata = metadata;
  m_file->geo_id_map = geo_id_map;
}

bool
InMemoryWriterBackend::record_exists(const daqdataformats::RecordID& record_id)
{
  return m_file->indices.count(record_id) != 0;
}

void
InMemoryWriterBackend::write_record(const daqdataformats::RecordIndex& index,
                                    const BlobView& header,
                                    const std::vector<BlobView>& fragments)
{
  auto const& record_id = index.get_record_id();
  auto store_blob = [&](const BlobView& blob) {
    auto const* bytes = static_cast<const char*>(blob.data);
    m_file->blobs[std::make_pair(record_id, blob.source_id)] = std::vector<char>(bytes, bytes + blob.size);
    m_stats.recorded_size += blob.size;
    m_stats.uncompressed_raw_data_size += blob.size;
    m_stats.total_file_size += blob.size;
  };
  store_blob(header);
  for (auto const& fragment : fragments) {
    store_blob(fragment);
  }
  m_file->indices[record_id] = index;
}

FileStats
InMemoryWriterBackend::get_stats() const
{
  return m_stats;
}

void
InMemoryWriterBackend::close(const FileMetadata& final_metadata)
{
  if (m_file == nullptr) {
    return;
  }
  m_file->metadata = final_metadata;
  m_file->closed = true;
  m_file.reset();
}

bool
InMemoryWriterBackend::path_exists(const std::string& path) const
{
  return InMemoryStore::instance().exists(path);
}

void
InMemoryWriterBackend::rename_path(const std::string& from, const std::string& to)
{
  InMemoryStore::instance().rename(from, to);
}

} // namespace dunedaq::rawdatafilelibs
