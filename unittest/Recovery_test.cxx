/**
 * @file Recovery_test.cxx Unit tests of recover_raw_data_file(), using the in-memory backend plugin
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/BackendFactory.hpp"
#include "rawdatafilelibs/InMemoryBackend.hpp"
#include "rawdatafilelibs/Issues.hpp"
#include "rawdatafilelibs/Recovery.hpp"

#include "daqdataformats/Fragment.hpp"
#include "daqdataformats/TriggerRecord.hpp"

#define BOOST_TEST_MODULE Recovery_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace dunedaq::rawdatafilelibs;
using dunedaq::daqdataformats::ComponentRequest;
using dunedaq::daqdataformats::Fragment;
using dunedaq::daqdataformats::FragmentHeader;
using dunedaq::daqdataformats::RecordID;
using dunedaq::daqdataformats::SourceID;
using dunedaq::daqdataformats::TriggerRecord;
using dunedaq::daqdataformats::TriggerRecordHeader;

namespace {

const SourceID trb{ SourceID::Subsystem::kTRBuilder, 0 };
const SourceID tpc_0{ SourceID::Subsystem::kDetectorReadout, 0 };
const SourceID tpc_1{ SourceID::Subsystem::kDetectorReadout, 1 };

TriggerRecord
make_trigger_record(uint64_t trigger_number) // NOLINT(build/unsigned)
{
  TriggerRecordHeader header(std::vector<ComponentRequest>(2));
  header.set_trigger_number(trigger_number);
  header.set_sequence_number(0);
  header.set_element_id(trb);
  TriggerRecord record(header);
  for (auto const& source_id : { tpc_0, tpc_1 }) {
    std::vector<char> payload(100, 'x');
    auto fragment = std::make_unique<Fragment>(payload.data(), payload.size());
    FragmentHeader fragment_header = fragment->get_header();
    fragment_header.trigger_number = trigger_number;
    fragment_header.sequence_number = 0;
    fragment_header.element_id = source_id;
    fragment->set_header_fields(fragment_header);
    record.add_fragment(std::move(fragment));
  }
  return record;
}

// A file as a crashed writer leaves it: records written, then neither closed nor renamed.
// Returns the stats a normal close would have stored.
FileStats
make_unfinished_file(const std::string& name, int record_count = 2)
{
  FileMetadata metadata;
  metadata.run_number = 7;
  metadata.record_type = RecordType::kTriggerRecord;
  FileStats stats;
  {
    auto writer = create_raw_data_file(name, metadata, {}, "rdfmem");
    for (int number = 1; number <= record_count; ++number) {
      writer->write(make_trigger_record(number));
    }
    writer->close();
    stats = writer->get_metadata().stats;
  }
  auto& store = InMemoryStore::instance();
  store.rename(name, name + ".writing");
  auto file = store.find(name + ".writing");
  file->metadata.closing_timestamp_ms = 0;
  file->metadata.stats = FileStats{};
  file->closed = false;
  return stats;
}

bool
mentions(const RecoveryReport& report, const std::string& text)
{
  return std::any_of(report.changes.begin(), report.changes.end(), [&](const std::string& change) {
    return change.find(text) != std::string::npos;
  });
}

struct StoreCleaner
{
  StoreCleaner() { InMemoryStore::instance().clear(); }
  ~StoreCleaner() { InMemoryStore::instance().clear(); }
};

} // namespace ""

BOOST_FIXTURE_TEST_SUITE(Recovery_test, StoreCleaner)

BOOST_AUTO_TEST_CASE(DryRunChangesNothing)
{
  make_unfinished_file("dry.rdfmem");
  auto report = recover_raw_data_file("dry.rdfmem.writing", false);

  BOOST_REQUIRE_EQUAL(report.final_path, "dry.rdfmem");
  BOOST_REQUIRE(!report.applied && !report.renamed);
  BOOST_REQUIRE(!report.scan.has_closing_timestamp && !report.scan.has_stats && !report.scan.has_recovery_timestamp);
  BOOST_REQUIRE_EQUAL(report.scan.readable_records, 2);
  BOOST_REQUIRE(mentions(report, "closing_timestamp_ms") && mentions(report, "stats") &&
                mentions(report, "recovery_timestamp_ms") && mentions(report, "rename to dry.rdfmem"));

  auto& store = InMemoryStore::instance();
  BOOST_REQUIRE(store.exists("dry.rdfmem.writing") && !store.exists("dry.rdfmem"));
  BOOST_REQUIRE_EQUAL(store.find("dry.rdfmem.writing")->metadata.closing_timestamp_ms, 0);
}

BOOST_AUTO_TEST_CASE(ApplyStoresTheMissingValuesAndRenames)
{
  auto expected_stats = make_unfinished_file("apply.rdfmem");
  auto report = recover_raw_data_file("apply.rdfmem.writing", true);
  BOOST_REQUIRE(report.applied && report.renamed);

  auto reader = open_raw_data_file("apply.rdfmem");
  auto const& metadata = reader->get_metadata();
  BOOST_REQUIRE(metadata.closing_timestamp_ms > 0); // no modification time in memory: "now"
  BOOST_REQUIRE(metadata.stats == expected_stats);  // as a normal close would have stored them
  BOOST_REQUIRE(metadata.recovery_timestamp_ms > 0);
  BOOST_REQUIRE_EQUAL(metadata.run_number, 7);
  BOOST_REQUIRE_EQUAL(reader->get_record_ids().size(), 2);
  reader->close();

  // recovering it again finds nothing to do
  auto again = recover_raw_data_file("apply.rdfmem", true);
  BOOST_REQUIRE(again.changes.empty() && !again.applied);
}

BOOST_AUTO_TEST_CASE(StoredValuesAreKept)
{
  make_unfinished_file("keep.rdfmem");
  InMemoryStore::instance().find("keep.rdfmem.writing")->metadata.closing_timestamp_ms = 1234;

  auto report = recover_raw_data_file("keep.rdfmem.writing", true);
  BOOST_REQUIRE(report.scan.has_closing_timestamp);
  BOOST_REQUIRE(!mentions(report, "closing_timestamp_ms"));
  BOOST_REQUIRE_EQUAL(InMemoryStore::instance().find("keep.rdfmem")->metadata.closing_timestamp_ms, 1234);
}

BOOST_AUTO_TEST_CASE(TakenFinalNameLeavesTheFileUntouched)
{
  make_unfinished_file("taken.rdfmem");
  auto& store = InMemoryStore::instance();
  store.create("taken.rdfmem");

  BOOST_REQUIRE_THROW(recover_raw_data_file("taken.rdfmem.writing", true), RecoveryFailed);
  auto file = store.find("taken.rdfmem.writing");
  BOOST_REQUIRE(file != nullptr);
  BOOST_REQUIRE_EQUAL(file->metadata.closing_timestamp_ms, 0);
  BOOST_REQUIRE_EQUAL(file->metadata.recovery_timestamp_ms, 0);
}

BOOST_AUTO_TEST_CASE(UnreadablePartsAreReportedAndNotCounted)
{
  auto full_stats = make_unfinished_file("partial.rdfmem");
  auto file = InMemoryStore::instance().find("partial.rdfmem.writing");
  const size_t lost = file->blobs.at(std::make_pair(RecordID{ 2 }, tpc_1)).size();
  file->blobs.erase(std::make_pair(RecordID{ 2 }, tpc_1));

  auto report = recover_raw_data_file("partial.rdfmem.writing", false);
  BOOST_REQUIRE_EQUAL(report.scan.readable_records, 1);
  BOOST_REQUIRE_EQUAL(report.scan.unreadable_records, 1);
  BOOST_REQUIRE_EQUAL(report.scan.calculated_stats.recorded_size, full_stats.recorded_size - lost);
  BOOST_REQUIRE(to_string(report).find("1 with unreadable parts") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(UnknownFileIsAnError)
{
  BOOST_REQUIRE_THROW(recover_raw_data_file("no_such_file.rdfmem.writing", false), RecoveryFailed);
}

BOOST_AUTO_TEST_SUITE_END()
