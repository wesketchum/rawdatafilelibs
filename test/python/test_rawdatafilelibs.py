"""Tests of the rawdatafilelibs Python bindings and Pythonic layer that need no recorded data.
Run with:  pytest -v sourcecode/rawdatafilelibs/test/python
(Reading and copying real records is tested in hdf5libs/test/python.)"""

import pytest

import daqdataformats
import detdataformats
import rawdatafilelibs


@pytest.fixture(autouse=True)
def empty_in_memory_store():
    rawdatafilelibs.clear_in_memory_files()
    yield
    rawdatafilelibs.clear_in_memory_files()


def test_star_import_does_not_shadow_open():
    namespace = {}
    exec("from rawdatafilelibs import *", namespace)
    assert "RawDataFileReader" in namespace and "RawDataFile" in namespace
    assert "open" not in namespace and "create" not in namespace


def test_metadata_and_record_type_names():
    metadata = rawdatafilelibs.FileMetadata()
    metadata.run_number = 12
    metadata.record_type = rawdatafilelibs.RecordType.kTimeSlice
    assert rawdatafilelibs.record_type_to_string(metadata.record_type) == "TimeSlice"
    assert rawdatafilelibs.string_to_record_type("TriggerRecord") == rawdatafilelibs.RecordType.kTriggerRecord
    assert metadata.stats == rawdatafilelibs.FileStats()


def test_file_utils():
    params = rawdatafilelibs.FileNameParams()
    params.directory_path = "/data"
    params.operational_environment = "np04hd"
    params.file_type_prefix = "raw"
    params.writer_identifier = "dataflow0_datawriter_0"
    params.extension = "hdf5"
    name = rawdatafilelibs.make_file_name(params, 1234, 5)
    assert name == "/data/np04hd_raw_run001234_0005_dataflow0_datawriter_0.hdf5"
    assert rawdatafilelibs.make_timestamp_string(0) == "19700101T000000"
    assert rawdatafilelibs.add_timestamp_suffix("a/b.hdf5", "hdf5", 0) == "a/b_19700101T000000.hdf5"
    assert rawdatafilelibs.backend_name_for_path("x/run1.hdf5.writing") == "hdf5"
    assert rawdatafilelibs.get_free_space("/") is not None


def test_create_and_open_empty_file_both_layers():
    geo_id_map = daqdataformats.SourceIDGeoIDMap()
    source_id = daqdataformats.SourceID(daqdataformats.SourceID.Subsystem.kDetectorReadout, 3)
    geo_id_map.add(source_id, detdataformats.GeoID(detdataformats.DetID.Subdetector.kHD_TPC, 1, 2, 3))

    with rawdatafilelibs.create("empty.rdfmem", "TriggerRecord", geo_id_map=geo_id_map, run_number=77,
                                application_name="pytest", creation_timestamp_ms=1234) as writer:
        assert writer.get_current_file_name() == "empty.rdfmem.writing"
        assert rawdatafilelibs.in_memory_file_exists("empty.rdfmem.writing")
    assert rawdatafilelibs.in_memory_file_exists("empty.rdfmem")

    # the C++-mirror layer
    reader = rawdatafilelibs.open_raw_data_file("empty.rdfmem")
    assert reader.get_metadata().run_number == 77
    assert reader.get_metadata().creation_timestamp_ms == 1234
    assert reader.get_metadata().closing_timestamp_ms >= 1234
    assert reader.get_record_ids() == set()
    assert reader.get_geo_id_map() == geo_id_map
    reader.close()

    # the Pythonic layer
    with rawdatafilelibs.open("empty.rdfmem") as f:
        assert len(f) == 0 and list(f) == [] and f.record_ids == []
        assert f.record_type == "TriggerRecord" and f.is_trigger_record_type()
        assert f.metadata.application_name == "pytest"
        assert (1, 0) not in f
        with pytest.raises(KeyError):
            f[(1, 0)]
        assert f.get_all_record_ids() == []


def test_existing_files_are_never_reused():
    rawdatafilelibs.create("taken.rdfmem", "TimeSlice").close()
    with pytest.raises(RuntimeError, match="already exists"):
        rawdatafilelibs.create("taken.rdfmem", "TimeSlice")


def test_close_failure_does_not_hide_the_with_block_exception():
    # a file that appears at the final name while writing makes close() fail
    with pytest.raises(ValueError, match="from the with block") as caught:
        with rawdatafilelibs.create("hidden.rdfmem", "TimeSlice"):
            rawdatafilelibs.create("hidden.rdfmem", "TimeSlice", inprogress_suffix="").close()
            raise ValueError("from the with block")
    assert any("closing the file also failed" in note for note in caught.value.__notes__)
    assert rawdatafilelibs.in_memory_file_exists("hidden.rdfmem.writing")

    # with no exception in the block, the close failure itself is raised
    with pytest.raises(RuntimeError, match="already exists"):
        with rawdatafilelibs.create("plain.rdfmem", "TimeSlice"):
            rawdatafilelibs.create("plain.rdfmem", "TimeSlice", inprogress_suffix="").close()


def test_pythonic_reader_close_failure_does_not_hide_the_with_block_exception():
    rawdatafilelibs.create("reader_exit.rdfmem", "TimeSlice").close()

    class FailingClose:
        """The real reader, except that close() fails after closing it."""

        def __init__(self, reader):
            self.reader = reader

        def __getattr__(self, name):
            return getattr(self.reader, name)

        def close(self):
            self.reader.close()
            raise RuntimeError("simulated close failure")

    with pytest.raises(KeyError) as caught:
        with rawdatafilelibs.open("reader_exit.rdfmem") as f:
            f.reader = FailingClose(f.reader)
            f[(1, 0)]
    assert any("simulated close failure" in note for note in caught.value.__notes__)

    with pytest.raises(RuntimeError, match="simulated close failure"):
        with rawdatafilelibs.open("reader_exit.rdfmem") as f:
            f.reader = FailingClose(f.reader)


def test_bad_arguments():
    with pytest.raises(TypeError):
        rawdatafilelibs.create("bad.rdfmem", "TriggerRecord", no_such_field=1)
    with pytest.raises(ValueError):
        rawdatafilelibs.make_selector(fragment_type="NotAFragmentType")
    with pytest.raises(TypeError):
        rawdatafilelibs.as_record_id("17")
    with pytest.raises(RuntimeError):
        rawdatafilelibs.open("no_such_file.rdfmem")
    with pytest.raises(RuntimeError, match="no_such_backend"):
        rawdatafilelibs.open("file.no_such_backend")


def test_selector_and_record_id_helpers():
    selector = rawdatafilelibs.make_selector(subsystem="Detector_Readout", fragment_type="WIBEth", subdetector="HD_TPC")
    assert selector.subsystem == daqdataformats.SourceID.Subsystem.kDetectorReadout
    assert selector.fragment_type == daqdataformats.FragmentType.kWIBEth
    assert selector.subdetector == detdataformats.DetID.Subdetector.kHD_TPC
    assert rawdatafilelibs.make_selector().subsystem is None
    assert rawdatafilelibs.as_record_id(5) == daqdataformats.RecordID(5, 0)
    assert rawdatafilelibs.as_record_id((5, 2)) == daqdataformats.RecordID(5, 2)
