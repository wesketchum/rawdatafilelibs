/**
 * @file FileMetadata.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "rawdatafilelibs/FileMetadata.hpp"

#include <string>

namespace dunedaq::rawdatafilelibs {

std::string
record_type_to_string(RecordType record_type)
{
  switch (record_type) {
    case RecordType::kTriggerRecord:
      return "TriggerRecord";
    case RecordType::kTimeSlice:
      return "TimeSlice";
    default:
      return "Unknown";
  }
}

RecordType
string_to_record_type(const std::string& name)
{
  if (name == "TriggerRecord") {
    return RecordType::kTriggerRecord;
  }
  if (name == "TimeSlice") {
    return RecordType::kTimeSlice;
  }
  return RecordType::kUnknown;
}

} // namespace dunedaq::rawdatafilelibs
