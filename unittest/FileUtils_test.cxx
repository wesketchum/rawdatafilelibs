/**
 * @file FileUtils_test.cxx Unit tests of the file-name and disk-space helpers
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/FileUtils.hpp"

#define BOOST_TEST_MODULE FileUtils_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <ctime>
#include <filesystem>
#include <string>

using namespace dunedaq::rawdatafilelibs;

BOOST_AUTO_TEST_SUITE(FileUtils_test)

BOOST_AUTO_TEST_CASE(FileNameMatchesDAQConvention)
{
  FileNameParams params;
  params.directory_path = "/data";
  params.operational_environment = "np04hd";
  params.file_type_prefix = "raw";
  params.run_number_prefix = "run";
  params.digits_for_run_number = 6;
  params.file_index_prefix = "";
  params.digits_for_file_index = 4;
  params.writer_identifier = "dataflow0_datawriter_0";
  params.extension = "hdf5";

  BOOST_REQUIRE_EQUAL(make_file_name(params, 12345, 7), "/data/np04hd_raw_run012345_0007_dataflow0_datawriter_0.hdf5");

  params.directory_path = "";
  BOOST_REQUIRE_EQUAL(make_file_name(params, 1, 0), "np04hd_raw_run000001_0000_dataflow0_datawriter_0.hdf5");
}

BOOST_AUTO_TEST_CASE(TimestampSuffix)
{
  const std::time_t time = 1761225300; // 2025-10-23 13:15:00 UTC
  BOOST_REQUIRE_EQUAL(make_timestamp_string(time), "20251023T131500");
  BOOST_REQUIRE_EQUAL(add_timestamp_suffix("/data/run1_0000_app.hdf5", "hdf5", time),
                      "/data/run1_0000_app_20251023T131500.hdf5");
  // nothing before the extension: unchanged
  BOOST_REQUIRE_EQUAL(add_timestamp_suffix(".hdf5", "hdf5", time), ".hdf5");
}

BOOST_AUTO_TEST_CASE(FreeSpace)
{
  auto free_space = get_free_space(std::filesystem::temp_directory_path().string());
  BOOST_REQUIRE(free_space.has_value());
  BOOST_REQUIRE(*free_space > 0);
  BOOST_REQUIRE(!get_free_space("/this/path/does/not/exist/at/all").has_value());
}

BOOST_AUTO_TEST_SUITE_END()
