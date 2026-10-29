/**
 * @file Issues.hpp ERS issues shared by all raw data file formats
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_ISSUES_HPP_
#define RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_ISSUES_HPP_

#include "ers/Issue.hpp"

#include <string>

namespace dunedaq {

// Disable coverage checking LCOV_EXCL_START

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  FileOpenFailed,
                  "Unable to open file " << file << ": " << message,
                  ((std::string)file)((std::string)message))

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  FileCloseFailed,
                  "Problem closing file " << file << ": " << message,
                  ((std::string)file)((std::string)message))

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  FileWriteFailed,
                  "Problem writing record " << record_id << " to file " << file << ": " << message,
                  ((std::string)record_id)((std::string)file)((std::string)message))

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  DuplicateSourceID,
                  "Record " << record_id << " for file " << file << " has more than one part (header or fragment) with SourceID "
                            << source_id << "; nothing of it was written",
                  ((std::string)source_id)((std::string)record_id)((std::string)file))

ERS_DECLARE_ISSUE(rawdatafilelibs, FileClosed, "File " << file << " has already been closed", ((std::string)file))

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  RecordIDNotFound,
                  "Record " << record_id << " not found in file " << file,
                  ((std::string)record_id)((std::string)file))

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  SourceIDNotFound,
                  "SourceID " << source_id << " not found in record " << record_id << " of file " << file,
                  ((std::string)source_id)((std::string)record_id)((std::string)file))

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  RecordReadFailed,
                  "Unable to read the index of record " << record_id << " from file " << file << ": " << message,
                  ((std::string)record_id)((std::string)file)((std::string)message))

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  FragmentMissing,
                  "Record " << record_id << " of file " << file << " lists SourceID " << source_id
                            << ", but its data could not be read: " << message,
                  ((std::string)source_id)((std::string)record_id)((std::string)file)((std::string)message))

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  RecordAlreadyExists,
                  "Record " << record_id << " already exists in file " << file,
                  ((std::string)record_id)((std::string)file))

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  RecordTypeMismatch,
                  "File " << file << " holds records of type " << file_type << ", not " << requested_type,
                  ((std::string)file)((std::string)file_type)((std::string)requested_type))

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  BackendNotAvailable,
                  "No raw data file backend \"" << backend << "\" could be loaded (for file " << file
                                                 << "): no duneRawDataFileBackend plugin of that name was found on"
                                                 << " CET_PLUGIN_PATH. Is the package that provides it set up?",
                  ((std::string)backend)((std::string)file))

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  RecoveryFailed,
                  "Unable to recover file " << file << ": " << message,
                  ((std::string)file)((std::string)message))

ERS_DECLARE_ISSUE(rawdatafilelibs,
                  SharedGeoIDs,
                  "File " << file << " lists " << count << " GeoID(s) under more than one SourceID (e.g. " << example
                          << "); looking up the SourceID of such a GeoID gives no answer",
                  ((std::string)file)((size_t)count)((std::string)example))

// Re-enable coverage checking LCOV_EXCL_STOP

} // namespace dunedaq

#endif // RAWDATAFILELIBS_INCLUDE_RAWDATAFILELIBS_ISSUES_HPP_
