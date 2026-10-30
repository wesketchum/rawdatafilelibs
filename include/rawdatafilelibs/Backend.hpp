/**
 * @file Backend.hpp The interfaces that each raw data file format implements
 *
 * A format provides a ReaderBackend and a WriterBackend, and makes them available as a
 * cetlib plugin of type "duneRawDataFileBackend" with DEFINE_DUNE_RAW_DATA_FILE_BACKEND.
 * The plugin's name is the file extension it handles (e.g. "hdf5"), so that
 * open_raw_data_file() can find it from the file name.
 *
 * Backends only store and retrieve bytes, record indices and metadata. Everything else
 * (record assembly, caching, error reporting, the in-progress file name) is done once,
 * for all formats, by RawDataFileReader and RawDataFileWriter.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_BACKEND_HPP_
#define RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_BACKEND_HPP_

#include "rawdatafilelibs/FileMetadata.hpp"

#include "daqdataformats/RecordID.hpp"
#include "daqdataformats/RecordIndex.hpp"
#include "daqdataformats/SourceID.hpp"
#include "daqdataformats/SourceIDGeoIDMap.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <vector>

#ifndef EXTERN_C_FUNC_DECLARE_START
// NOLINTNEXTLINE(build/define_used)
#define EXTERN_C_FUNC_DECLARE_START                                                                                    \
  extern "C"                                                                                                           \
  {
#endif

/**
 * @brief Declare the functions that the plugin loader calls to create a format's backends
 * @param reader_klass Class implementing ReaderBackend
 * @param writer_klass Class implementing WriterBackend
 */
// NOLINTNEXTLINE(build/define_used)
#define DEFINE_DUNE_RAW_DATA_FILE_BACKEND(reader_klass, writer_klass)                                                  \
  EXTERN_C_FUNC_DECLARE_START                                                                                          \
  std::unique_ptr<dunedaq::rawdatafilelibs::ReaderBackend> make_reader()                                               \
  {                                                                                                                    \
    return std::make_unique<reader_klass>();                                                                           \
  }                                                                                                                    \
  std::unique_ptr<dunedaq::rawdatafilelibs::WriterBackend> make_writer()                                               \
  {                                                                                                                    \
    return std::make_unique<writer_klass>();                                                                           \
  }                                                                                                                    \
  }

namespace dunedaq::rawdatafilelibs {

/**
 * @brief Blobs returned by backends are allocated with std::malloc, so that a Fragment
 * (which frees its buffer with std::free) can take one over without copying it.
 */
struct FreeDeleter
{
  void operator()(void* ptr) const noexcept { std::free(ptr); } // NOLINT(cppcoreguidelines-no-malloc)
};
using blob_ptr_t = std::unique_ptr<char[], FreeDeleter>; // NOLINT(modernize-avoid-c-arrays)

/**
 * @brief The stored bytes of one part of a record (its header or a fragment), owned
 */
struct Blob
{
  blob_ptr_t data;
  size_t size{ 0 };
};

/// Allocates a Blob of the given size with std::malloc; throws std::bad_alloc on failure
Blob
allocate_blob(size_t size);

/**
 * @brief A view of bytes to be written, and the SourceID they belong to
 */
struct BlobView
{
  daqdataformats::SourceID source_id;
  const void* data{ nullptr };
  size_t size{ 0 };
};

/**
 * @brief Format-specific reading. Created unopened by the plugin; open() is called once.
 * Any method may throw; RawDataFileReader turns failures into the issues in Issues.hpp.
 */
class ReaderBackend
{
public:
  ReaderBackend() = default;

  virtual void open(const std::string& path) = 0;

  virtual FileMetadata read_metadata() = 0;
  virtual daqdataformats::SourceIDGeoIDMap read_geo_id_map() = 0;
  virtual std::set<daqdataformats::RecordID> read_record_ids() = 0;
  virtual daqdataformats::RecordIndex read_record_index(const daqdataformats::RecordID& record_id) = 0;

  /// The stored bytes of the record header (when source_id is the header's SourceID) or of a fragment
  virtual Blob read_blob(const daqdataformats::RecordID& record_id, const daqdataformats::SourceID& source_id) = 0;

  /// Must release the file, even when it throws, and be harmless if open() failed or wasn't called
  virtual void close() = 0;

  ReaderBackend(const ReaderBackend&) = delete;
  ReaderBackend& operator=(const ReaderBackend&) = delete;
  ReaderBackend(ReaderBackend&&) = delete;
  ReaderBackend& operator=(ReaderBackend&&) = delete;
  virtual ~ReaderBackend() = default;
};

/**
 * @brief What a backend finds in a file left unfinished (e.g. by a crash), for recover_raw_data_file()
 */
struct RecoveryScan
{
  FileMetadata stored; ///< The metadata as stored; closing values are 0 where missing
  bool has_closing_timestamp{ false };
  bool has_stats{ false };
  bool has_recovery_timestamp{ false };
  FileStats calculated_stats; ///< Recalculated from the stored data, in the format's own terms (see below)
  size_t readable_records{ 0 };
  size_t unreadable_records{ 0 }; ///< Records with at least one part (header or fragment) that can't be read
  uint64_t last_modified_ms{ 0 }; ///< The file's last modification time; 0 if unknown // NOLINT(build/unsigned)
};

/**
 * @brief Format-specific writing. Created unopened by the plugin; open() is called once.
 */
class WriterBackend
{
public:
  WriterBackend() = default;

  /// Creates a new file at path. Must fail, leaving any existing file untouched, if path exists.
  virtual void open(const std::string& path,
                    const FileMetadata& metadata,
                    const daqdataformats::SourceIDGeoIDMap& geo_id_map) = 0;

  virtual bool record_exists(const daqdataformats::RecordID& record_id) = 0;

  /// Writes one complete record: its header bytes, its fragments' bytes, and its index
  virtual void write_record(const daqdataformats::RecordIndex& index,
                            const BlobView& header,
                            const std::vector<BlobView>& fragments) = 0;

  /// Called only after write_record() threw for this record: removes whatever part of the record
  /// it stored, so that record_exists() is false again and get_stats() no longer counts it (the
  /// file size may stay as it is). The default throws std::runtime_error ("not supported"); the
  /// generic writer then reports that the record may remain partly stored.
  virtual void discard_record(const daqdataformats::RecordID& record_id);

  virtual FileStats get_stats() const = 0;

  /// Stores the final metadata and closes the file. Must release the file, even when it throws.
  virtual void close(const FileMetadata& final_metadata) = 0;

  /// Storage operations that the generic writer uses for the in-progress file name.
  /// The defaults act on the filesystem; backends that don't store files override them.
  /// rename_path must never replace an existing file at `to`, even one that appears concurrently:
  /// it fails instead, leaving both names as they were (the default uses rename_without_replacing).
  virtual bool path_exists(const std::string& path) const;
  virtual void rename_path(const std::string& from, const std::string& to);

  /// Recovery of a file left unfinished (used by recover_raw_data_file(); not with open()).
  /// scan_for_recovery reads the file at path without changing it. calculated_stats counts every
  /// part (record header or fragment) that can still be read, as close() would have counted it
  /// (recorded_size as stored, e.g. after compression), and total_file_size is the file's size now;
  /// a record with any unreadable part also counts in unreadable_records. It must throw if it can't
  /// tell which records the file holds, rather than report a file with no records: recovery then
  /// fails and the file is left untouched.
  /// store_recovery_metadata stores, in the file at path, the closing values of final_metadata that
  /// the scan found missing (closing timestamp, stats, recovery timestamp), and nothing else.
  /// Backends that can't recover files keep these defaults, which throw std::runtime_error.
  virtual RecoveryScan scan_for_recovery(const std::string& path);
  virtual void store_recovery_metadata(const std::string& path,
                                       const RecoveryScan& scan,
                                       const FileMetadata& final_metadata);

  WriterBackend(const WriterBackend&) = delete;
  WriterBackend& operator=(const WriterBackend&) = delete;
  WriterBackend(WriterBackend&&) = delete;
  WriterBackend& operator=(WriterBackend&&) = delete;
  virtual ~WriterBackend() = default;
};

} // namespace dunedaq::rawdatafilelibs

#endif // RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_BACKEND_HPP_
