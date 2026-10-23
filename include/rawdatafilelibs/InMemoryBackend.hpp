/**
 * @file InMemoryBackend.hpp A raw data file "format" that keeps files in process memory
 *
 * Intended for unit tests of code that reads or writes raw data files (no disk, no HDF5).
 * Files live in a process-wide InMemoryStore, keyed by path. It is also available as the
 * backend plugin "rdfmem", so files named "*.rdfmem" use it through open_raw_data_file()
 * and create_raw_data_file().
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_INMEMORYBACKEND_HPP_
#define RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_INMEMORYBACKEND_HPP_

#include "rawdatafilelibs/Backend.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace dunedaq::rawdatafilelibs {

struct InMemoryFile
{
  FileMetadata metadata;
  daqdataformats::SourceIDGeoIDMap geo_id_map;
  std::map<daqdataformats::RecordID, daqdataformats::RecordIndex> indices;
  std::map<std::pair<daqdataformats::RecordID, daqdataformats::SourceID>, std::vector<char>> blobs;
  bool closed{ false };
};

class InMemoryStore
{
public:
  static InMemoryStore& instance();

  // Throws FileOpenFailed if path already exists
  std::shared_ptr<InMemoryFile> create(const std::string& path);
  // nullptr if path doesn't exist
  std::shared_ptr<InMemoryFile> find(const std::string& path) const;

  bool exists(const std::string& path) const;
  // Throws std::runtime_error if from doesn't exist or to does
  void rename(const std::string& from, const std::string& to);
  void remove(const std::string& path);
  void clear();

private:
  mutable std::mutex m_mutex;
  std::map<std::string, std::shared_ptr<InMemoryFile>> m_files;
};

class InMemoryReaderBackend : public ReaderBackend
{
public:
  void open(const std::string& path) override;
  FileMetadata read_metadata() override;
  daqdataformats::SourceIDGeoIDMap read_geo_id_map() override;
  std::set<daqdataformats::RecordID> read_record_ids() override;
  daqdataformats::RecordIndex read_index(const daqdataformats::RecordID& record_id) override;
  Buffer read_blob(const daqdataformats::RecordID& record_id, const daqdataformats::SourceID& source_id) override;
  void close() override;

private:
  std::shared_ptr<InMemoryFile> m_file;
};

class InMemoryWriterBackend : public WriterBackend
{
public:
  void open(const std::string& path,
            const FileMetadata& metadata,
            const daqdataformats::SourceIDGeoIDMap& geo_id_map) override;
  bool record_exists(const daqdataformats::RecordID& record_id) override;
  void write_record(const daqdataformats::RecordIndex& index,
                    const BlobView& header,
                    const std::vector<BlobView>& fragments) override;
  FileStats get_stats() const override;
  void close(const FileMetadata& final_metadata) override;
  bool path_exists(const std::string& path) const override;
  void rename_path(const std::string& from, const std::string& to) override;

private:
  std::shared_ptr<InMemoryFile> m_file;
  FileStats m_stats;
};

} // namespace dunedaq::rawdatafilelibs

#endif // RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_INMEMORYBACKEND_HPP_
