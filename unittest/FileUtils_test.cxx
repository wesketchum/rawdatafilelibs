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

#include <unistd.h>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

using namespace dunedaq::rawdatafilelibs;

namespace {
std::string
temp_file_name(const std::string& stem)
{
  return (std::filesystem::temp_directory_path() / (stem + std::to_string(getpid()))).string();
}

void
write_text_file(const std::string& name, const std::string& contents)
{
  std::ofstream file(name);
  file << contents;
}

std::string
read_text_file(const std::string& name)
{
  std::ifstream file(name);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}
} // namespace ""

BOOST_AUTO_TEST_SUITE(FileUtils_test)

BOOST_AUTO_TEST_CASE(FileNameMatchesDAQConvention)
{
  FilenameParams params;
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

// Called directly, with no prior existence check, the rename still never replaces a file
BOOST_AUTO_TEST_CASE(RenameWithoutReplacing)
{
  const std::string from = temp_file_name("rdf_rename_from");
  const std::string to = temp_file_name("rdf_rename_to");
  write_text_file(from, "new");
  write_text_file(to, "existing");

  try {
    rename_without_replacing(from, to);
    BOOST_FAIL("rename_without_replacing replaced an existing file");
  } catch (std::system_error const& excpt) {
    BOOST_REQUIRE(excpt.code() == std::errc::file_exists);
  }
  BOOST_REQUIRE_EQUAL(read_text_file(from), "new");
  BOOST_REQUIRE_EQUAL(read_text_file(to), "existing");

  std::filesystem::remove(to);
  BOOST_REQUIRE_NO_THROW(rename_without_replacing(from, to));
  BOOST_REQUIRE(!std::filesystem::exists(from));
  BOOST_REQUIRE_EQUAL(read_text_file(to), "new");
  std::filesystem::remove(to);

  BOOST_REQUIRE_THROW(rename_without_replacing(from, to), std::system_error); // from no longer exists
}

BOOST_AUTO_TEST_SUITE_END()
