/**
 * @file Recovery.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/Recovery.hpp"
#include "rawdatafilelibs/BackendFactory.hpp"
#include "rawdatafilelibs/FileUtils.hpp"
#include "rawdatafilelibs/Issues.hpp"

#include <cstdint>
#include <exception>
#include <memory>
#include <sstream>
#include <string>

namespace dunedaq::rawdatafilelibs {

namespace {
std::string
stats_string(const FileStats& stats)
{
  std::ostringstream oss;
  oss << "recorded_size " << stats.recorded_size << ", uncompressed_raw_data_size " << stats.uncompressed_raw_data_size
      << ", total_file_size " << stats.total_file_size;
  return oss.str();
}
} // namespace ""

RecoveryReport
recover_raw_data_file(const std::string& path,
                      bool apply,
                      const std::string& backend_name,
                      const std::string& inprogress_suffix)
{
  RecoveryReport report;
  report.path = path;
  report.final_path = path;
  if (!inprogress_suffix.empty() && path.size() > inprogress_suffix.size() &&
      path.compare(path.size() - inprogress_suffix.size(), inprogress_suffix.size(), inprogress_suffix) == 0) {
    report.final_path = path.substr(0, path.size() - inprogress_suffix.size());
  }

  auto backend =
    make_writer_backend(backend_name.empty() ? backend_name_from_path(path, inprogress_suffix) : backend_name, path);
  try {
    report.scan = backend->scan_for_recovery(path);
  } catch (std::exception const& excpt) {
    throw RecoveryFailed(ERS_HERE, path, "the file could not be scanned", excpt);
  }

  // work out the closing values the file lacks; values it already has are kept
  const auto now = current_time_ms();
  report.final_metadata = report.scan.stored;
  if (!report.scan.has_closing_timestamp) {
    report.final_metadata.closing_timestamp_ms =
      (report.scan.last_modified_ms != 0) ? report.scan.last_modified_ms : now;
    report.changes.push_back("closing_timestamp_ms = " + std::to_string(report.final_metadata.closing_timestamp_ms) +
                             (report.scan.last_modified_ms != 0 ? " (the file's last modification time)" : " (now)"));
  }
  if (!report.scan.has_stats) {
    report.final_metadata.stats = report.scan.calculated_stats;
    report.changes.push_back("stats = " + stats_string(report.final_metadata.stats) + " (recalculated)");
  }
  // a complete file under its final name needs nothing, not even a recovery timestamp
  const bool rename_needed = (report.final_path != path);
  if ((!report.changes.empty() || rename_needed) && !report.scan.has_recovery_timestamp) {
    report.final_metadata.recovery_timestamp_ms = now;
    report.changes.push_back("recovery_timestamp_ms = " + std::to_string(now));
  }
  if (rename_needed) {
    report.changes.push_back("rename to " + report.final_path);
  }

  if (!apply || report.changes.empty()) {
    return report;
  }

  // never replace an existing file; checked before anything is stored, so the file is then untouched
  if (report.final_path != path && backend->path_exists(report.final_path)) {
    throw RecoveryFailed(ERS_HERE, path, "a file named " + report.final_path + " already exists; nothing was changed");
  }
  try {
    backend->store_recovery_metadata(path, report.scan, report.final_metadata);
  } catch (std::exception const& excpt) {
    throw RecoveryFailed(ERS_HERE, path, "the missing closing values could not be stored", excpt);
  }
  report.applied = true;

  if (report.final_path != path) {
    try {
      backend->rename_path(path, report.final_path);
    } catch (std::exception const& excpt) {
      throw RecoveryFailed(ERS_HERE,
                           path,
                           "the closing values were stored, but the file could not be renamed to " + report.final_path,
                           excpt);
    }
    report.renamed = true;
  }
  return report;
}

std::string
to_string(const RecoveryReport& report)
{
  std::ostringstream oss;
  oss << "File: " << report.path << "\n"
      << "Records: " << report.scan.readable_records << " readable, " << report.scan.unreadable_records
      << " with unreadable parts (not counted in the recalculated stats)\n"
      << "Stored: closing_timestamp_ms "
      << (report.scan.has_closing_timestamp ? std::to_string(report.scan.stored.closing_timestamp_ms) : "missing")
      << ", stats " << (report.scan.has_stats ? stats_string(report.scan.stored.stats) : "missing")
      << ", recovery_timestamp_ms "
      << (report.scan.has_recovery_timestamp ? std::to_string(report.scan.stored.recovery_timestamp_ms) : "missing")
      << "\n"
      << "Recalculated stats: " << stats_string(report.scan.calculated_stats) << "\n";
  if (report.changes.empty()) {
    oss << "Nothing to do: the file is complete.\n";
    return oss.str();
  }
  oss << (report.applied ? "Done:\n" : "Would do (run with --apply to do it):\n");
  for (auto const& change : report.changes) {
    oss << "  " << change << "\n";
  }
  return oss.str();
}

} // namespace dunedaq::rawdatafilelibs
