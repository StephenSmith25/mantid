"""
    ===== AUTO-GENERATED MODULE ====

    Handles set up of aspects related to extra Python packages e.g. adding
    any required paths to sys.path, setting environment variables etc.
"""
from __future__ import (absolute_import, division,
                        print_function)
import os as _os
import sys as _sys


def update_sys_paths(paths, recursive=False):
    """
        Add the required script directories to the path
        @param paths A list of path strings or a string using a semi-colon
                     separator
        @param recursive If true then all directories below the given paths
                         are added as well
    """
    if paths == "":
        return

    if type(paths) == str:
        paths = paths.split(";")

    def _append_to_path(path):
        # sys.path doesn't like trailing slashes
        _sys.path.append(path.rstrip("\\").rstrip("/"))

    for path in paths:
        _append_to_path(path)
        if recursive:
            for dirpath, dirnames, filenames in _os.walk(path):
                for dirname in dirnames:
                    _append_to_path(_os.path.join(dirpath, dirname))


def set_NEXUSLIB_var():
    """
        Sets the NEXUSLIB environment variable to point to the NeXus shared library
    """
    import os as _os
    _nexuslib = r"C:/Users/qbr77747/dev/m/source/external/src/ThirdParty/bin/libNeXus-0.dll"
    if _nexuslib == "":
        return

    if not _os.path.isabs(_nexuslib):
        # Assume relative to this file
        thisdir = _os.path.dirname(__file__)
        _nexuslib = _os.path.normpath(_os.path.join(thisdir, _nexuslib))

    _os.environ['NEXUSLIB'] = _nexuslib
