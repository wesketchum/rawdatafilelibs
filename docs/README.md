# rawdatafilelibs

Format-independent reading and writing of DUNE DAQ raw data files (files of TriggerRecords or TimeSlices).

The design, class structure and guarantees are described in detail in [DESIGN.md](DESIGN.md).

The on-disk format is provided by a **backend** plugin. The HDF5 backend comes with `hdf5libs`. This package also contains an in-memory backend for tests. Code that reads or writes raw data files uses the classes here and never needs to know which format a file is in.

## Reading

```cpp
#include "rawdatafilelibs/BackendFactory.hpp"
using namespace dunedaq;

auto reader = rawdatafilelibs::open_raw_data_file("/data/np04hd_raw_run012345_0000_df0.hdf5");
auto const& metadata = reader->get_metadata(); // run number, file index, application name, ...

for (auto const& record_id : reader->get_record_ids()) {
  daqdataformats::SourceIDSelector tpc;
  tpc.subdetector = detdataformats::DetID::Subdetector::kHD_TPC;
  for (auto const& fragment : reader->get_fragments(record_id, tpc)) {
    // ...
  }
  auto geo_ids = reader->get_geo_ids(record_id, tpc); // only fragments present in this record
}
```

- `open_raw_data_file()` picks the backend from the file extension: `.hdf5` loads the `hdf5` backend plugin. An in-progress `.writing` suffix, and the `.copied` suffix that the file transfer system adds, are ignored when choosing, in any order (so `x.hdf5.copied` and `x.hdf5.writing.copied` are read as HDF5). For any other name, pass the backend name explicitly.
- Each record's `daqdataformats::RecordIndex` (header SourceID, plus the fragment SourceIDs with their fragment types and subdetectors) is the source of truth for what the record contains. Fragments are found by SourceID, never by format-specific locations.
- Errors are ERS issues from `rawdatafilelibs/Issues.hpp`:
  - `RecordIDNotFound`: the record isn't in the file.
  - `SourceIDNotFound`: the SourceID isn't in the record.
  - `RecordPartUnreadable`: the SourceID is listed in the record but its data can't be read.
  - `RecordTypeMismatch`, `FileOpenFailed`, `FileClosed` and others.

## Writing

```cpp
rawdatafilelibs::FileMetadata metadata;
metadata.run_number = run_number;
metadata.record_type = rawdatafilelibs::RecordType::kTriggerRecord;
// ...
auto writer = rawdatafilelibs::create_raw_data_file(file_name, metadata, source_id_geo_id_map);
writer->write(trigger_record);
writer->close();
```

- **In-progress name.** The file is written as `<file_name>.writing` and renamed to `<file_name>` by `close()`.
- **Never overwritten.** Existing files are never reused or overwritten. If either name exists, creation fails with `FileOpenFailed` and the existing file is left untouched. If the final name appears before `close()`, the file keeps its in-progress name and `close()` throws `FileCloseFailed`.
- **Explicit close.** Call `close()`: it can report errors, whereas the destructor can only log them.
- **One part per SourceID.** A record whose header and fragments don't all have different SourceIDs is refused with `DuplicateSourceID`, and nothing of it is written.
- **Failed writes are removed.** If the backend fails in the middle of a record, the writer asks it to remove what it stored (`WriterBackend::discard_record`), so the record can be written again. `FileWriteFailed` says whether that worked; with a backend that can't remove records, the record may remain partly stored.

## Writing a backend

Implement `ReaderBackend` and `WriterBackend` from `rawdatafilelibs/Backend.hpp`, then register them as a plugin named after the file extension:

```cpp
// plugins/myformat.cpp, built with: daq_add_plugin(myformat duneRawDataFileBackend LINK_LIBRARIES ...)
DEFINE_DUNE_RAW_DATA_FILE_BACKEND(MyReaderBackend, MyWriterBackend)
```

Backends only store and retrieve bytes, record indices and metadata. Record assembly, caching, error reporting and the in-progress name are handled once, for every format, by `RawDataFileReader` and `RawDataFileWriter`.

Override `WriterBackend::discard_record()` if the format can remove a partly written record; the default reports that it can't.

## Known limitations

- The reader and writer constructors and `write()` turn only `std::exception`s from a backend into rawdatafilelibs issues; anything else a backend throws there passes through unchanged. (`close()` and the destructors catch everything.)
- The statistics stored at `close()` are taken before the backend writes its closing values, so for HDF5 `total_file_size` excludes the closing attributes. If the backend's `close()` fails, `get_stats()` afterwards returns the values from file creation (zeros).
- If `recover_raw_data_file()` stores the closing values but the rename to the final name then fails, it throws `RecoveryFailed` and the report (which would say `applied`) is not returned. The file has its closing values but keeps its in-progress name; running the recovery again renames it.
- Not thread-safe: use one reader or writer per thread.

## Utilities

`rawdatafilelibs/FileUtils.hpp` contains:
- `make_file_name()`: the standard DAQ output file name.
- `add_timestamp_suffix()`: the unique timestamp suffix.
- `get_free_space()`: free disk space for a path.

## Testing with the in-memory backend

`rawdatafilelibs/InMemoryBackend.hpp` keeps files in process memory. Use it directly (`InMemoryReaderBackend` / `InMemoryWriterBackend`), or through the plugin with files named `*.rdfmem`. `InMemoryStore::instance()` gives access to the stored files.
