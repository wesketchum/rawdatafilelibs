/**
 * @file rawdatafile_recover.cxx Recovers a raw data file left unfinished, e.g. by a crash
 *
 * Usage: rawdatafile_recover [--apply] [--backend <name>] <file>
 *
 * Without --apply, reports what the file lacks and what recovery would do, and changes nothing.
 * With --apply, stores the missing closing values (closing timestamp, statistics, recovery
 * timestamp) and renames "<name>.writing" to "<name>", never replacing an existing file.
 * Works for any file format whose backend supports recovery (e.g. "hdf5").
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/Recovery.hpp"

#include "ers/ers.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace {
void
print_usage(const char* program)
{
  std::cout << "Usage: " << program << " [--apply] [--backend <name>] <file>\n"
            << "  Reports what an unfinished raw data file lacks and what recovery would do.\n"
            << "  --apply           store the missing closing values and rename the file to its final name\n"
            << "  --backend <name>  the file format backend (default: from the file's extension)\n";
}
} // namespace

int
main(int argc, char** argv)
{
  bool apply = false;
  std::string backend_name;
  std::string path;
  for (int idx = 1; idx < argc; ++idx) {
    std::string arg = argv[idx];
    if (arg == "--apply") {
      apply = true;
    } else if (arg == "--backend" && idx + 1 < argc) {
      backend_name = argv[++idx];
    } else if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      return 0;
    } else if (path.empty() && !arg.empty() && arg[0] != '-') {
      path = arg;
    } else {
      print_usage(argv[0]);
      return 1;
    }
  }
  if (path.empty()) {
    print_usage(argv[0]);
    return 1;
  }

  try {
    auto report = dunedaq::rawdatafilelibs::recover_raw_data_file(path, apply, backend_name);
    std::cout << dunedaq::rawdatafilelibs::to_string(report);
  } catch (ers::Issue const& issue) {
    ers::error(issue);
    return 2;
  } catch (std::exception const& excpt) {
    std::cerr << "rawdatafile_recover: " << excpt.what() << std::endl;
    return 2;
  } catch (...) { // NOLINT(runtime/exceptions)
    std::cerr << "rawdatafile_recover: unknown exception" << std::endl;
    return 2;
  }
  return 0;
}
