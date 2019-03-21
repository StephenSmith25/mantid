"""
    Checks that the running version of Python is compatible with the version of
    Python that Mantid was built with.
"""
import sys

# Define the target major.minor version (i.e. the one mantid was built against)
TARGET_VERSION="2.7"


def check_python_version():
    vers_info = sys.version_info
    running_vers = "%d.%d" % (vers_info[0], vers_info[1])
    if running_vers != TARGET_VERSION:
        message = \
    """Python version mismatch, cannot continue.
    Mantid was built against version '%s' but you are running version '%s'. These versions
    must match.
    """
        raise ImportError(message % (TARGET_VERSION, running_vers))

