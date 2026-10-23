"""Format-independent reading and writing of DUNE DAQ raw data files.

Two layers:
  - the C++-mirror bindings: RawDataFileReader, RawDataFileWriter, open_raw_data_file(),
    create_raw_data_file(), FileMetadata, ... (same names and arguments as in C++);
  - a Pythonic layer: open() and create(), RawDataFile and Record (see rawdatafile.py).

The format is chosen from the file's extension (e.g. ".hdf5" loads the hdf5libs backend plugin).
"""

# daqdataformats and detdataformats types appear in this module's bindings, so register them first
import detdataformats  # noqa: F401
import daqdataformats  # noqa: F401

from . import _daq_rawdatafilelibs_py
from ._daq_rawdatafilelibs_py import *  # noqa: F401,F403
from .rawdatafile import RawDataFile, Record, open, create, as_record_id, make_selector  # noqa: F401

# "from rawdatafilelibs import *" gives the C++-mirror names and the Pythonic classes, but not
# open() and create(), which would shadow the builtin open; use rawdatafilelibs.open().
__all__ = [name for name in dir(_daq_rawdatafilelibs_py) if not name.startswith("_")] + [  # noqa: F405
    "RawDataFile",
    "Record",
    "as_record_id",
    "make_selector",
]
