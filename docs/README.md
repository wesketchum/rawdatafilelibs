# rawdatafilelibs

Format-independent reading and writing of DUNE DAQ raw data files (files of TriggerRecords or TimeSlices).

The on-disk format is provided by a **backend** plugin. HDF5 (from `hdf5libs`) is the main format. This package also contains an in-memory backend for tests. Code that reads or writes raw data files uses the classes here and never needs to know which format a file is in.

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

- `open_raw_data_file()` picks the backend from the file extension: `.hdf5` loads the `hdf5` backend plugin. An in-progress `.writing` suffix, and the `.copied` suffix that the file transfer system adds, are ignored when choosing (so `x.hdf5.copied` is read as HDF5). For any other name, pass the backend name explicitly.
- Each record's `daqdataformats::RecordIndex` (header SourceID, plus the fragment SourceIDs with their fragment types and subdetectors) is the source of truth for what the record contains. Fragments are found by SourceID, never by format-specific locations.
- Errors are ERS issues from `rawdatafilelibs/Issues.hpp`:
  - `RecordIDNotFound`: the record isn't in the file.
  - `SourceIDNotFound`: the SourceID isn't in the record.
  - `FragmentMissing`: the SourceID is listed in the record but its data can't be read.
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

## Writing a backend

Implement `ReaderBackend` and `WriterBackend` from `rawdatafilelibs/Backend.hpp`, then register them as a plugin named after the file extension:

```cpp
// plugins/myformat.cpp, built with: daq_add_plugin(myformat duneRawDataFileBackend LINK_LIBRARIES ...)
DEFINE_DUNE_RAW_DATA_FILE_BACKEND(MyReaderBackend, MyWriterBackend)
```

Backends only store and retrieve bytes, record indices and metadata. Record assembly, caching, error reporting and the in-progress name are handled once, for every format, by `RawDataFileReader` and `RawDataFileWriter`.

## Utilities

`rawdatafilelibs/FileUtils.hpp` contains:
- `make_file_name()`: the standard DAQ output file name.
- `add_timestamp_suffix()`: the unique timestamp suffix.
- `get_free_space()`: free disk space for a path.

## Testing with the in-memory backend

`rawdatafilelibs/InMemoryBackend.hpp` keeps files in process memory. Use it directly (`InMemoryReaderBackend` / `InMemoryWriterBackend`), or through the plugin with files named `*.rdfmem`. `InMemoryStore::instance()` gives access to the stored files.
