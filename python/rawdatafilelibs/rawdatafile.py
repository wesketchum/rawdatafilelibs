"""Pythonic access to DUNE DAQ raw data files, of any format.

    import rawdatafilelibs

    with rawdatafilelibs.open("run001234_0000_dataflow0.hdf5") as f:
        print(f.metadata.run_number, f.record_type, len(f))
        for record in f:                                   # records in order
            print(record.id, record.header.get_trigger_timestamp())
            for frag in record.fragments(subdetector="HD_TPC", fragment_type="WIBEth"):
                ...
        record = f[(17, 0)]                                # or f[17], or f[RecordID(17, 0)]

Records are read lazily: a Record holds only its index until fragments or the header are asked for.

RawDataFile also has the method names of hdf5libs.HDF5RawDataFile (get_all_record_ids, get_frag,
get_trh, get_source_ids, ...), so scripts can move to this API one call at a time; see "Compatibility".
"""

import warnings

import daqdataformats
import detdataformats

from . import _daq_rawdatafilelibs_py as _cpp

__all__ = ["RawDataFile", "Record", "open", "create", "as_record_id", "make_selector"]


def _deprecated(old, new):
    warnings.warn(f"rawdatafilelibs RawDataFile.{old} is deprecated; use {new}", DeprecationWarning, stacklevel=3)


def as_record_id(record_id):
    """A daqdataformats.RecordID from a RecordID, a record number, or a (number, sequence) pair."""
    if isinstance(record_id, daqdataformats.RecordID):
        return record_id
    if isinstance(record_id, int):
        return daqdataformats.RecordID(record_id, 0)
    if isinstance(record_id, (tuple, list)) and len(record_id) == 2:
        return daqdataformats.RecordID(int(record_id[0]), int(record_id[1]))
    raise TypeError(f"can't make a RecordID from {record_id!r}")


def _subsystem(value):
    if value is None or isinstance(value, daqdataformats.SourceID.Subsystem):
        return value
    subsystem = daqdataformats.SourceID.string_to_subsystem(value)
    if subsystem == daqdataformats.SourceID.Subsystem.kUnknown:
        raise ValueError(f"unknown subsystem name {value!r}")
    return subsystem


def _fragment_type(value):
    if value is None or isinstance(value, daqdataformats.FragmentType):
        return value
    fragment_type = daqdataformats.string_to_fragment_type(value)
    if fragment_type == daqdataformats.FragmentType.kUnknown:
        raise ValueError(f"unknown fragment type name {value!r}")
    return fragment_type


def _subdetector(value):
    if value is None or isinstance(value, detdataformats.DetID.Subdetector):
        return value
    subdetector = detdataformats.DetID.string_to_subdetector(value)
    if subdetector == detdataformats.DetID.Subdetector.kUnknown:
        raise ValueError(f"unknown subdetector name {value!r}")
    return subdetector


def make_selector(subsystem=None, fragment_type=None, subdetector=None):
    """A daqdataformats.SourceIDSelector; each criterion may be an enum value or its name
    (e.g. subsystem="Detector_Readout", fragment_type="WIBEth", subdetector="HD_TPC")."""
    return daqdataformats.SourceIDSelector(
        subsystem=_subsystem(subsystem),
        fragment_type=_fragment_type(fragment_type),
        subdetector=_subdetector(subdetector),
    )


class Record:
    """One TriggerRecord or TimeSlice in a RawDataFile, read lazily."""

    def __init__(self, raw_data_file, record_id):
        self._file = raw_data_file
        self.id = record_id
        self.index = raw_data_file.reader.get_index(record_id)

    @property
    def number(self):
        return self.id.number

    @property
    def sequence(self):
        return self.id.sequence

    @property
    def header_source_id(self):
        return self.index.get_header_source_id()

    @property
    def header(self):
        """The TriggerRecordHeader or TimeSliceHeader."""
        if self._file.record_type == "TimeSlice":
            return self._file.reader.get_timeslice_header(self.id)
        return self._file.reader.get_trigger_record_header(self.id)

    def source_ids(self, subsystem=None, fragment_type=None, subdetector=None):
        """SourceIDs of the selected fragments (not the header), sorted."""
        selector = make_selector(subsystem, fragment_type, subdetector)
        return sorted(self.index.get_fragment_source_ids(selector))

    def geo_ids(self, subsystem=None, fragment_type=None, subdetector=None):
        """GeoIDs of the selected fragments present in the record, sorted."""
        selector = make_selector(subsystem, fragment_type, subdetector)
        return sorted(self._file.reader.get_geo_ids(self.id, selector))

    def fragment(self, source_id):
        return self._file.reader.get_fragment(self.id, source_id)

    def fragments(self, subsystem=None, fragment_type=None, subdetector=None):
        """The selected fragments, read one at a time, in SourceID order."""
        for source_id in self.source_ids(subsystem, fragment_type, subdetector):
            yield self.fragment(source_id)

    def assemble(self):
        """The whole record, as a daqdataformats.TriggerRecord or TimeSlice."""
        if self._file.record_type == "TimeSlice":
            return self._file.reader.get_timeslice(self.id)
        return self._file.reader.get_trigger_record(self.id)

    def __iter__(self):
        return self.fragments()

    def __len__(self):
        return len(self.index.get_fragments())

    def __repr__(self):
        return f"<Record {self._file.record_type} {self.id.to_string()} ({len(self)} fragments)>"


class RawDataFile:
    """A raw data file opened for reading. Use rawdatafilelibs.open()."""

    def __init__(self, path, backend=""):
        self.reader = _cpp.open_raw_data_file(str(path), backend)

    # ---- the file ----

    @property
    def path(self):
        return self.reader.get_path()

    @property
    def metadata(self):
        return self.reader.get_metadata()

    @property
    def record_type(self):
        """"TriggerRecord", "TimeSlice" or "Unknown"."""
        return _cpp.record_type_to_string(self.reader.get_record_type())

    @property
    def geo_id_map(self):
        return self.reader.get_geo_id_map()

    @property
    def backend(self):
        """The format backend, for format-specific extras (e.g. hdf5libs.HDF5ReaderBackend)."""
        return self.reader.get_backend()

    @property
    def record_ids(self):
        """All RecordIDs, sorted."""
        return sorted(self.reader.get_record_ids())

    def close(self):
        self.reader.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        # A close failure is raised as usual, unless the with-block itself raised: then that
        # exception is kept and the close failure is attached to it as a note instead.
        if exc_type is None:
            self.close()
            return False
        try:
            self.close()
        except Exception as close_error:  # noqa: BLE001 - reported on the block's exception
            exc_value.add_note(f"closing the file also failed: {close_error}")
        return False

    # ---- records ----

    def __len__(self):
        return len(self.reader.get_record_ids())

    def __contains__(self, record_id):
        return self.reader.contains(as_record_id(record_id))

    def __getitem__(self, record_id):
        record_id = as_record_id(record_id)
        if not self.reader.contains(record_id):
            raise KeyError(record_id.to_string())
        return Record(self, record_id)

    def __iter__(self):
        for record_id in self.record_ids:
            yield Record(self, record_id)

    def records(self):
        return iter(self)

    def __repr__(self):
        return f"<RawDataFile {self.path} ({self.record_type}, {len(self)} records)>"

    # ---- Compatibility: the hdf5libs.HDF5RawDataFile Python names ----
    # Record IDs may be given as (number, sequence) pairs, numbers or RecordIDs, and are
    # returned as (number, sequence) pairs, as HDF5RawDataFile did.

    def get_file_name(self):
        return self.path

    def get_record_type(self):
        return self.record_type

    def is_trigger_record_type(self):
        return self.record_type == "TriggerRecord"

    def is_timeslice_type(self):
        return self.record_type == "TimeSlice"

    def get_recorded_size(self):
        return self.metadata.stats.recorded_size

    def get_all_record_ids(self):
        return [(rid.number, rid.sequence) for rid in self.record_ids]

    def get_all_trigger_record_ids(self):
        _deprecated("get_all_trigger_record_ids()", "get_all_record_ids() or record_ids")
        self._check_type("TriggerRecord")
        return self.get_all_record_ids()

    def get_all_timeslice_ids(self):
        _deprecated("get_all_timeslice_ids()", "get_all_record_ids() or record_ids")
        self._check_type("TimeSlice")
        return self.get_all_record_ids()

    def get_source_ids(self, record_id):
        """The fragments' SourceIDs and the record header's, as HDF5RawDataFile returned them."""
        _deprecated("get_source_ids()", "get_fragment_source_ids() (which leaves out the record header's SourceID)")
        record = self[record_id]
        return set(record.source_ids()) | {record.header_source_id}

    def get_record_header_source_id(self, record_id):
        return self[record_id].header_source_id

    def get_fragment_source_ids(self, record_id):
        return set(self[record_id].source_ids())

    def get_source_ids_for_fragment_type(self, record_id, fragment_type):
        return set(self[record_id].source_ids(fragment_type=fragment_type))

    def get_source_ids_for_subdetector(self, record_id, subdetector):
        return set(self[record_id].source_ids(subdetector=subdetector))

    def get_source_ids_for_fragtype_and_subdetector(self, record_id, fragment_type, subdetector):
        _deprecated("get_source_ids_for_fragtype_and_subdetector()",
                    "record.source_ids(fragment_type=..., subdetector=...)")
        return set(self[record_id].source_ids(fragment_type=fragment_type, subdetector=subdetector))

    def get_geo_ids(self, record_id):
        """Encoded GeoIDs of the fragments present in the record."""
        return {geo_id.encode() for geo_id in self[record_id].geo_ids()}

    def get_geo_ids_for_subdetector(self, record_id, subdetector):
        return {geo_id.encode() for geo_id in self[record_id].geo_ids(subdetector=subdetector)}

    def get_frag(self, record_id, source_id):
        """The fragment with this SourceID, or (deprecated, as HDF5RawDataFile allowed) this encoded GeoID."""
        record_id = as_record_id(record_id)
        if isinstance(source_id, int):
            _deprecated("get_frag(record, geo_id)", "the GeoID map, geo_id_map.get_source_ids(GeoID.decode(geo_id))")
            # as HDF5RawDataFile did: the first SourceID, even for a GeoID listed under several
            found = self.geo_id_map.get_source_ids(detdataformats.GeoID.decode(source_id))
            if not found:
                raise KeyError(f"no SourceID for GeoID {source_id}")
            source_id = found[0]
        return self.reader.get_fragment(record_id, source_id)

    def get_trh(self, record_id):
        return self.reader.get_trigger_record_header(as_record_id(record_id))

    def get_tsh(self, record_id):
        return self.reader.get_timeslice_header(as_record_id(record_id))

    def get_trigger_record(self, record_id):
        return self.reader.get_trigger_record(as_record_id(record_id))

    def get_timeslice(self, record_id):
        return self.reader.get_timeslice(as_record_id(record_id))

    def _check_type(self, wanted):
        if self.record_type not in (wanted, "Unknown"):
            raise TypeError(f"{self.path} holds {self.record_type} records, not {wanted}")


def open(path, backend=""):  # noqa: A001 - like gzip.open; not exported by "import *"
    """Open an existing raw data file for reading; the backend defaults to the one for its extension."""
    return RawDataFile(path, backend)


def create(path, record_type, geo_id_map=None, backend="", inprogress_suffix=None, **metadata):
    """Create a new raw data file and return its RawDataFileWriter (a context manager).

    record_type is "TriggerRecord" or "TimeSlice" (or a RecordType); the other FileMetadata fields
    (run_number, file_index, application_name, compression_level, operational_environment,
    offline_data_stream, run_is_for_test_purposes, creation_timestamp_ms) can be given as keywords.
    The file is written as <path>.writing and renamed to <path> when closed; existing files are never
    reused or overwritten.
    """
    file_metadata = _cpp.FileMetadata()
    if isinstance(record_type, str):
        record_type = _cpp.string_to_record_type(record_type)
    file_metadata.record_type = record_type
    for name, value in metadata.items():
        if not hasattr(file_metadata, name) or name in ("stats", "closing_timestamp_ms", "layout_version"):
            raise TypeError(f"create() got an unexpected metadata field {name!r}")
        setattr(file_metadata, name, value)
    if geo_id_map is None:
        geo_id_map = daqdataformats.SourceIDGeoIDMap()
    if inprogress_suffix is None:
        inprogress_suffix = _cpp.RawDataFileWriter.s_default_inprogress_suffix
    return _cpp.create_raw_data_file(str(path), file_metadata, geo_id_map, backend, inprogress_suffix)
