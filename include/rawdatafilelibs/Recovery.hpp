/**
 * @file Recovery.hpp Recovering raw data files left unfinished, e.g. by a crash
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_RECOVERY_HPP_
#define RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_RECOVERY_HPP_

#include "rawdatafilelibs/Backend.hpp"
#include "rawdatafilelibs/FileMetadata.hpp"
#include "rawdatafilelibs/RawDataFileWriter.hpp"

#include <string>
#include <vector>

namespace dunedaq::rawdatafilelibs {

/**
 * @brief What recover_raw_data_file() found and did (or, without apply, would do)
 */
struct RecoveryReport
{
  std::string path;       ///< The file as given, normally "<name>.writing"
  std::string final_path; ///< Its name once recovered: the in-progress suffix removed (same as path if it has none)
  RecoveryScan scan;      ///< What the backend found in the file
  FileMetadata final_metadata;      ///< The metadata the file holds after recovery
  std::vector<std::string> changes; ///< One line per value that was (or would be) set, or the rename
  bool applied{ false };            ///< Whether the missing values were stored
  bool renamed{ false };            ///< Whether the file was renamed to final_path
};

/**
 * @brief Recovers a file left unfinished (e.g. by a crash), in any format with a backend that supports it.
 *
 * The backend scans the file; the closing values it lacks are then worked out: the closing
 * timestamp (the file's modification time, or now if unknown) and the statistics (as the backend
 * recalculates them from the parts it can read). If anything is missing, or the file still has
 * its in-progress name, the recovery timestamp (now) is set too; a complete file under its final
 * name needs nothing. Values the file already has are never changed. With apply, those values are stored and the file
 * is renamed to its final name, never replacing an existing file. Without apply, nothing is changed and the report says
 * what would be done.
 *
 * Throws RecoveryFailed if the backend can't scan or update the file, or if the final name is
 * already taken (checked before anything is stored, so the file is then left untouched). The
 * backend defaults to the one named by the file's extension.
 */
RecoveryReport
recover_raw_data_file(const std::string& path,
                      bool apply,
                      const std::string& backend_name = "",
                      const std::string& inprogress_suffix = RawDataFileWriter::s_default_inprogress_suffix);

// A readable multi-line summary of a report, as printed by rawdatafile_recover
std::string
to_string(const RecoveryReport& report);

} // namespace dunedaq::rawdatafilelibs

#endif // RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_RECOVERY_HPP_
