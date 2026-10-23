/**
 * @file BackendFactory_test.cxx Unit tests of finding backends by plugin, using the "rdfmem" plugin
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/BackendFactory.hpp"
#include "rawdatafilelibs/InMemoryBackend.hpp"
#include "rawdatafilelibs/Issues.hpp"

#define BOOST_TEST_MODULE BackendFactory_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <string>
#include <vector>

using namespace dunedaq::rawdatafilelibs;
using dunedaq::daqdataformats::ComponentRequest;
using dunedaq::daqdataformats::RecordID;
using dunedaq::daqdataformats::SourceID;
using dunedaq::daqdataformats::SourceIDGeoIDMap;
using dunedaq::daqdataformats::TriggerRecord;
using dunedaq::daqdataformats::TriggerRecordHeader;

BOOST_AUTO_TEST_SUITE(BackendFactory_test)

BOOST_AUTO_TEST_CASE(BackendNameFromPath)
{
  BOOST_REQUIRE_EQUAL(backend_name_for_path("/data/run1_0000_app.hdf5"), "hdf5");
  BOOST_REQUIRE_EQUAL(backend_name_for_path("/data/run1_0000_app.hdf5.writing"), "hdf5");
  BOOST_REQUIRE_EQUAL(backend_name_for_path("run1.v2.rdfmem"), "rdfmem");
  BOOST_REQUIRE_EQUAL(backend_name_for_path("/data/no_extension"), "");
  BOOST_REQUIRE_EQUAL(backend_name_for_path("/data.dir/no_extension"), "");
  BOOST_REQUIRE_EQUAL(backend_name_for_path("/data/.hidden"), "");
}

BOOST_AUTO_TEST_CASE(WriteAndReadThroughPlugin)
{
  InMemoryStore::instance().clear();

  FileMetadata metadata;
  metadata.run_number = 7;
  metadata.record_type = RecordType::kTriggerRecord;

  TriggerRecordHeader header(std::vector<ComponentRequest>(1));
  header.set_trigger_number(5);
  header.set_sequence_number(0);
  header.set_element_id(SourceID(SourceID::Subsystem::kTRBuilder, 0));
  TriggerRecord record(header);

  auto writer = create_raw_data_file("plugin_test.rdfmem", metadata, SourceIDGeoIDMap());
  writer->write(record);
  writer->close();

  auto reader = open_raw_data_file("plugin_test.rdfmem");
  BOOST_REQUIRE_EQUAL(reader->get_metadata().run_number, 7);
  BOOST_REQUIRE(reader->contains(RecordID{ 5 }));
  BOOST_REQUIRE_EQUAL(reader->get_trigger_record(RecordID{ 5 }).get_header_ref().get_trigger_number(), 5);

  InMemoryStore::instance().clear();
}

BOOST_AUTO_TEST_CASE(UnknownBackend)
{
  BOOST_REQUIRE_THROW(open_raw_data_file("file.no_such_format"), dunedaq::rawdatafilelibs::BackendNotAvailable);
  BOOST_REQUIRE_THROW(open_raw_data_file("/data/no_extension"), dunedaq::rawdatafilelibs::BackendNotAvailable);
  BOOST_REQUIRE_THROW(make_writer_backend("no_such_format"), dunedaq::rawdatafilelibs::BackendNotAvailable);
}

BOOST_AUTO_TEST_SUITE_END()
