/**
 * @file RawDataFile_test.cxx Unit tests of RawDataFileReader and RawDataFileWriter, using the in-memory backend
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/InMemoryBackend.hpp"
#include "rawdatafilelibs/Issues.hpp"
#include "rawdatafilelibs/RawDataFileReader.hpp"
#include "rawdatafilelibs/RawDataFileWriter.hpp"

#define BOOST_TEST_MODULE RawDataFile_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace dunedaq::rawdatafilelibs;
using dunedaq::daqdataformats::ComponentRequest;
using dunedaq::daqdataformats::Fragment;
using dunedaq::daqdataformats::FragmentHeader;
using dunedaq::daqdataformats::FragmentType;
using dunedaq::daqdataformats::RecordID;
using dunedaq::daqdataformats::SourceID;
using dunedaq::daqdataformats::SourceIDGeoIDMap;
using dunedaq::daqdataformats::SourceIDSelector;
using dunedaq::daqdataformats::TimeSlice;
using dunedaq::daqdataformats::TriggerRecord;
using dunedaq::daqdataformats::TriggerRecordHeader;
using dunedaq::detdataformats::GeoID;
using Subdetector = dunedaq::detdataformats::DetID::Subdetector;

namespace {

const SourceID tpc_0{ SourceID::Subsystem::kDetectorReadout, 0 };
const SourceID tpc_1{ SourceID::Subsystem::kDetectorReadout, 1 };
const SourceID pds_0{ SourceID::Subsystem::kDetectorReadout, 10 };
const SourceID trb{ SourceID::Subsystem::kTRBuilder, 0 };

std::unique_ptr<Fragment>
make_fragment(const SourceID& source_id, FragmentType fragment_type, Subdetector subdetector, char fill)
{
  std::vector<char> payload(32, fill);
  auto frag = std::make_unique<Fragment>(payload.data(), payload.size());
  FragmentHeader fh = frag->get_header();
  fh.element_id = source_id;
  fh.fragment_type = static_cast<dunedaq::daqdataformats::fragment_type_t>(fragment_type);
  fh.detector_id = static_cast<uint16_t>(subdetector); // NOLINT(build/unsigned)
  frag->set_header_fields(fh);
  return frag;
}

TriggerRecord
make_trigger_record(uint64_t trigger_number, bool include_tpc_1 = true) // NOLINT(build/unsigned)
{
  TriggerRecordHeader header(std::vector<ComponentRequest>(3));
  header.set_trigger_number(trigger_number);
  header.set_sequence_number(0);
  header.set_element_id(trb);
  TriggerRecord record(header);
  record.add_fragment(make_fragment(tpc_0, FragmentType::kWIBEth, Subdetector::kHD_TPC, 'a'));
  if (include_tpc_1) {
    record.add_fragment(make_fragment(tpc_1, FragmentType::kWIBEth, Subdetector::kHD_TPC, 'b'));
  }
  record.add_fragment(make_fragment(pds_0, FragmentType::kDAPHNE, Subdetector::kHD_PDS, 'c'));
  return record;
}

SourceIDGeoIDMap
make_geo_id_map()
{
  SourceIDGeoIDMap the_map;
  the_map.add(tpc_0, GeoID(Subdetector::kHD_TPC, 1, 0, 0));
  the_map.add(tpc_1, GeoID(Subdetector::kHD_TPC, 1, 0, 1));
  the_map.add(pds_0, GeoID(Subdetector::kHD_PDS, 2, 0, 0));
  return the_map;
}

FileMetadata
make_metadata(RecordType record_type = RecordType::kTriggerRecord)
{
  FileMetadata metadata;
  metadata.run_number = 42;
  metadata.file_index = 3;
  metadata.application_name = "RawDataFile_test";
  metadata.record_type = record_type;
  metadata.operational_environment = "unit_test";
  metadata.offline_data_stream = "cosmics";
  metadata.run_is_for_test_purposes = true;
  return metadata;
}

std::unique_ptr<RawDataFileWriter>
make_writer(const std::string& name, RecordType record_type = RecordType::kTriggerRecord)
{
  return std::make_unique<RawDataFileWriter>(
    std::make_unique<InMemoryWriterBackend>(), name, make_metadata(record_type), make_geo_id_map());
}

std::unique_ptr<RawDataFileReader>
make_reader(const std::string& name)
{
  return std::make_unique<RawDataFileReader>(std::make_unique<InMemoryReaderBackend>(), name);
}

bool
same_bytes(const Fragment& lhs, const Fragment& rhs)
{
  return lhs.get_size() == rhs.get_size() &&
         std::memcmp(lhs.get_storage_location(), rhs.get_storage_location(), lhs.get_size()) == 0;
}

// An in-memory backend that can fail while finalizing the file (after releasing it, as the
// interface requires) or while renaming it, to exercise the writer's close() failure paths
class FailingWriterBackend : public InMemoryWriterBackend
{
public:
  FailingWriterBackend(bool fail_close, bool fail_rename)
    : m_fail_close(fail_close)
    , m_fail_rename(fail_rename)
  {
  }

  void close(const FileMetadata& final_metadata) override
  {
    InMemoryWriterBackend::close(final_metadata);
    if (m_fail_close) {
      throw std::runtime_error("simulated failure while finalizing");
    }
  }

  void rename_path(const std::string& from, const std::string& to) override
  {
    if (m_fail_rename) {
      throw std::runtime_error("simulated failure while renaming");
    }
    InMemoryWriterBackend::rename_path(from, to);
  }

private:
  bool m_fail_close;
  bool m_fail_rename;
};

std::unique_ptr<RawDataFileWriter>
make_failing_writer(const std::string& name, bool fail_close, bool fail_rename)
{
  return std::make_unique<RawDataFileWriter>(std::make_unique<FailingWriterBackend>(fail_close, fail_rename),
                                             name,
                                             make_metadata(RecordType::kTriggerRecord),
                                             make_geo_id_map());
}

struct StoreCleaner
{
  StoreCleaner() { InMemoryStore::instance().clear(); }
  ~StoreCleaner() { InMemoryStore::instance().clear(); }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(RawDataFile_test, StoreCleaner)

BOOST_AUTO_TEST_CASE(WriteReadTriggerRecords)
{
  auto writer = make_writer("trs.rdfmem");
  writer->write(make_trigger_record(1));
  writer->write(make_trigger_record(2));
  writer->close();

  auto reader = make_reader("trs.rdfmem");
  BOOST_REQUIRE((reader->get_record_ids() == std::set<RecordID>{ RecordID{ 1 }, RecordID{ 2 } }));

  auto original = make_trigger_record(2);
  auto read_back = reader->get_trigger_record(RecordID{ 2 });
  BOOST_REQUIRE_EQUAL(read_back.get_header_ref().get_trigger_number(), 2);
  BOOST_REQUIRE_EQUAL(read_back.get_header_ref().get_total_size_bytes(),
                      original.get_header_ref().get_total_size_bytes());
  BOOST_REQUIRE_EQUAL(read_back.get_fragments_ref().size(), 3);

  auto fragment = reader->get_fragment(RecordID{ 2 }, pds_0);
  BOOST_REQUIRE(same_bytes(*fragment, *original.get_fragments_ref().back()));
}

BOOST_AUTO_TEST_CASE(Metadata)
{
  auto writer = make_writer("meta.rdfmem");
  writer->write(make_trigger_record(1));
  BOOST_REQUIRE(writer->get_metadata().creation_timestamp_ms > 0); // filled in by the writer
  writer->close();

  auto reader = make_reader("meta.rdfmem");
  auto const& metadata = reader->get_metadata();
  BOOST_REQUIRE_EQUAL(metadata.run_number, 42);
  BOOST_REQUIRE_EQUAL(metadata.file_index, 3);
  BOOST_REQUIRE_EQUAL(metadata.application_name, "RawDataFile_test");
  BOOST_REQUIRE(metadata.record_type == RecordType::kTriggerRecord);
  BOOST_REQUIRE(metadata.run_is_for_test_purposes);
  BOOST_REQUIRE(metadata.closing_timestamp_ms >= metadata.creation_timestamp_ms);
  BOOST_REQUIRE(metadata.stats.recorded_size > 0);
  BOOST_REQUIRE(metadata.stats == writer->get_stats());
  BOOST_REQUIRE(reader->get_geo_id_map() == make_geo_id_map());
}

BOOST_AUTO_TEST_CASE(SelectionAndGeoIDs)
{
  auto writer = make_writer("select.rdfmem");
  writer->write(make_trigger_record(1, false)); // tpc_1 missing from this record
  writer->close();

  auto reader = make_reader("select.rdfmem");
  SourceIDSelector tpc_only;
  tpc_only.subdetector = Subdetector::kHD_TPC;
  BOOST_REQUIRE((reader->get_fragment_source_ids(RecordID{ 1 }, tpc_only) == std::set<SourceID>{ tpc_0 }));
  BOOST_REQUIRE_EQUAL(reader->get_fragments(RecordID{ 1 }, tpc_only).size(), 1);

  // only GeoIDs of fragments present in the record
  auto geo_ids = reader->get_geo_ids(RecordID{ 1 });
  BOOST_REQUIRE_EQUAL(geo_ids.size(), 2);
  BOOST_REQUIRE(geo_ids.count(GeoID(Subdetector::kHD_TPC, 1, 0, 1)) == 0);
  BOOST_REQUIRE_EQUAL(reader->get_geo_ids(RecordID{ 1 }, tpc_only).size(), 1);
}

BOOST_AUTO_TEST_CASE(LookupErrors)
{
  auto writer = make_writer("errors.rdfmem");
  writer->write(make_trigger_record(1, false));
  writer->close();

  auto reader = make_reader("errors.rdfmem");
  BOOST_REQUIRE_THROW(reader->get_index(RecordID{ 99 }), dunedaq::rawdatafilelibs::RecordIDNotFound);
  BOOST_REQUIRE_THROW(reader->get_fragment(RecordID{ 1 }, tpc_1), dunedaq::rawdatafilelibs::SourceIDNotFound);
  BOOST_REQUIRE_THROW(reader->get_timeslice(RecordID{ 1 }), dunedaq::rawdatafilelibs::RecordTypeMismatch);

  // a SourceID listed in the index whose data is missing
  auto file = InMemoryStore::instance().find("errors.rdfmem");
  file->blobs.erase(std::make_pair(RecordID{ 1 }, pds_0));
  BOOST_REQUIRE_THROW(reader->get_fragment(RecordID{ 1 }, pds_0), dunedaq::rawdatafilelibs::FragmentMissing);

  BOOST_REQUIRE_THROW(make_reader("does_not_exist.rdfmem"), dunedaq::rawdatafilelibs::FileOpenFailed);

  reader->close();
  BOOST_REQUIRE_THROW(reader->get_fragment(RecordID{ 1 }, tpc_0), dunedaq::rawdatafilelibs::FileClosed);
  BOOST_REQUIRE_NO_THROW(reader->close());
}

BOOST_AUTO_TEST_CASE(WriteErrors)
{
  auto writer = make_writer("write_errors.rdfmem");
  writer->write(make_trigger_record(1));
  BOOST_REQUIRE(writer->record_exists(RecordID{ 1 }));
  BOOST_REQUIRE_THROW(writer->write(make_trigger_record(1)), dunedaq::rawdatafilelibs::RecordAlreadyExists);
  BOOST_REQUIRE_THROW(writer->write(TimeSlice(5, 42)), dunedaq::rawdatafilelibs::RecordTypeMismatch);
  writer->close();
  BOOST_REQUIRE_THROW(writer->write(make_trigger_record(2)), dunedaq::rawdatafilelibs::FileClosed);

  FileMetadata no_type = make_metadata(RecordType::kUnknown);
  BOOST_REQUIRE_THROW(
    RawDataFileWriter(std::make_unique<InMemoryWriterBackend>(), "no_type.rdfmem", no_type, SourceIDGeoIDMap()),
    dunedaq::rawdatafilelibs::FileOpenFailed);
}

BOOST_AUTO_TEST_CASE(InProgressName)
{
  auto& store = InMemoryStore::instance();
  auto writer = make_writer("progress.rdfmem");
  BOOST_REQUIRE_EQUAL(writer->get_current_file_name(), "progress.rdfmem.writing");
  BOOST_REQUIRE(store.exists("progress.rdfmem.writing"));
  BOOST_REQUIRE(!store.exists("progress.rdfmem"));

  writer->close();
  BOOST_REQUIRE_EQUAL(writer->get_current_file_name(), "progress.rdfmem");
  BOOST_REQUIRE(store.exists("progress.rdfmem"));
  BOOST_REQUIRE(!store.exists("progress.rdfmem.writing"));
  BOOST_REQUIRE_NO_THROW(writer->close()); // second close is a no-op
}

BOOST_AUTO_TEST_CASE(ExistingFilesAreNeverReused)
{
  auto& store = InMemoryStore::instance();

  // a stale in-progress file, e.g. left by a crash
  auto stale = store.create("stale.rdfmem.writing");
  stale->metadata.application_name = "crashed writer";
  BOOST_REQUIRE_THROW(make_writer("stale.rdfmem"), dunedaq::rawdatafilelibs::FileOpenFailed);
  BOOST_REQUIRE_EQUAL(store.find("stale.rdfmem.writing")->metadata.application_name, "crashed writer");

  // an existing complete file
  make_writer("done.rdfmem")->close();
  BOOST_REQUIRE_THROW(make_writer("done.rdfmem"), dunedaq::rawdatafilelibs::FileOpenFailed);

  // the final name appears while the file is being written: close fails and keeps the in-progress name
  auto writer = make_writer("race.rdfmem");
  store.create("race.rdfmem");
  BOOST_REQUIRE_THROW(writer->close(), dunedaq::rawdatafilelibs::FileCloseFailed);
  BOOST_REQUIRE(store.exists("race.rdfmem.writing"));
  BOOST_REQUIRE_NO_THROW(writer->close()); // not retried
}

// A close() failure is reported once as FileCloseFailed, is not retried, and leaves the file
// under its in-progress name for recovery, whether finalizing or renaming fails
BOOST_AUTO_TEST_CASE(CloseFailuresKeepTheInProgressName)
{
  auto& store = InMemoryStore::instance();
  for (auto [fail_close, fail_rename] : { std::pair{ true, false }, std::pair{ false, true } }) {
    BOOST_TEST_INFO("fail_close " << fail_close << ", fail_rename " << fail_rename);
    store.clear();
    auto writer = make_failing_writer("failing.rdfmem", fail_close, fail_rename);
    writer->write(make_trigger_record(1));

    BOOST_REQUIRE_THROW(writer->close(), dunedaq::rawdatafilelibs::FileCloseFailed);
    BOOST_REQUIRE(!writer->is_open());
    BOOST_REQUIRE_EQUAL(writer->get_current_file_name(), "failing.rdfmem.writing");
    BOOST_REQUIRE(store.exists("failing.rdfmem.writing"));
    BOOST_REQUIRE(!store.exists("failing.rdfmem"));
    BOOST_REQUIRE_NO_THROW(writer->close()); // not retried

    // the in-progress file holds the record
    auto reader = make_reader("failing.rdfmem.writing");
    BOOST_REQUIRE_EQUAL(reader->get_record_ids().size(), 1);
    reader->close();
  }
}

// The default (filesystem) rename_path never replaces an existing file
BOOST_AUTO_TEST_CASE(DefaultRenamePathNeverReplaces)
{
  const std::string dir = std::filesystem::temp_directory_path().string();
  const std::string from = dir + "/rdf_rename_path_from" + std::to_string(getpid());
  const std::string to = dir + "/rdf_rename_path_to" + std::to_string(getpid());
  {
    std::ofstream(from) << "new";
  }
  {
    std::ofstream(to) << "existing";
  }

  InMemoryWriterBackend backend;
  BOOST_REQUIRE_THROW(backend.WriterBackend::rename_path(from, to), std::system_error);
  BOOST_REQUIRE(std::filesystem::exists(from));
  BOOST_REQUIRE_EQUAL(std::filesystem::file_size(to), 8); // "existing"

  std::filesystem::remove(from);
  std::filesystem::remove(to);
}

BOOST_AUTO_TEST_CASE(WriteReadTimeSlices)
{
  auto writer = make_writer("ts.rdfmem", RecordType::kTimeSlice);
  TimeSlice timeslice(17, 42);
  timeslice.set_element_id(trb);
  timeslice.add_fragment(make_fragment(tpc_0, FragmentType::kTriggerPrimitive, Subdetector::kDAQ, 't'));
  writer->write(timeslice);
  BOOST_REQUIRE_THROW(writer->write(make_trigger_record(1)), dunedaq::rawdatafilelibs::RecordTypeMismatch);
  writer->close();

  auto reader = make_reader("ts.rdfmem");
  BOOST_REQUIRE(reader->contains(RecordID{ 17 }));
  auto read_back = reader->get_timeslice(RecordID{ 17 });
  BOOST_REQUIRE_EQUAL(read_back.get_header().timeslice_number, 17);
  BOOST_REQUIRE(read_back.get_header().element_id == trb);
  BOOST_REQUIRE_EQUAL(read_back.get_fragments_ref().size(), 1);
  BOOST_REQUIRE(same_bytes(*read_back.get_fragments_ref().front(), *timeslice.get_fragments_ref().front()));
}

BOOST_AUTO_TEST_SUITE_END()
