# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +
from __future__ import (absolute_import, division, print_function)

import mantid.simpleapi as mantid
import os


def split_into_tof_d_spacing_groups(run_details, processed_spectra):
    """
    Splits a processed list containing all focused banks into TOF and
    d-spacing groups. It also sets the names of the output workspaces
    to the run number(s) - Result<Unit>-<Bank Number> e.g.
    123-130-ResultTOF-3
    :param run_details: The run details associated with this run
    :param processed_spectra: A list containing workspaces, one entry per focused bank.
    :return: A workspace group for dSpacing and TOF in that order
    """
    d_spacing_output = []
    tof_output = []
    run_number = str(run_details.output_run_string)
    ext = run_details.file_extension if run_details.file_extension else ""

    for name_index, ws in enumerate(processed_spectra, start=1):
        d_spacing_out_name = run_number + ext + "-ResultD_" + str(name_index)
        tof_out_name = run_number + ext + "-ResultTOF_" + str(name_index)

        d_spacing_output.append(
            mantid.ConvertUnits(InputWorkspace=ws,
                                OutputWorkspace=d_spacing_out_name,
                                Target="dSpacing"))
        tof_output.append(
            mantid.ConvertUnits(InputWorkspace=ws, OutputWorkspace=tof_out_name, Target="TOF"))

    # Group the outputs
    d_spacing_group_name = run_number + ext + "-ResultD"
    d_spacing_group = mantid.GroupWorkspaces(InputWorkspaces=d_spacing_output,
                                             OutputWorkspace=d_spacing_group_name)
    tof_group_name = run_number + ext + "-ResultTOF"
    tof_group = mantid.GroupWorkspaces(InputWorkspaces=tof_output, OutputWorkspace=tof_group_name)

    return d_spacing_group, tof_group


def save_focused_data(d_spacing_group, tof_group, output_paths):
    """
    Saves out focused data into nxs, GSAS and .dat formats. Requires the grouped workspace
    in TOF and dSpacing and the dictionary of output paths generated by abstract_inst.
    :param d_spacing_group: The focused workspace group in dSpacing
    :param tof_group: The focused workspace group in TOF
    :param output_paths: A dictionary containing the full paths to save to
    :return: None
    """
    def ensure_dir_exists(filename):
        dirname = os.path.dirname(filename)
        if not os.path.exists(dirname):
            os.makedirs(dirname)
        return filename

    mantid.SaveGSS(InputWorkspace=tof_group,
                   Filename=ensure_dir_exists(output_paths["gss_filename"]),
                   SplitFiles=False,
                   Append=False)
    mantid.SaveNexusProcessed(InputWorkspace=tof_group,
                              Filename=ensure_dir_exists(output_paths["nxs_filename"]),
                              Append=False)

    _save_xye(ws_group=d_spacing_group,
              filename_template=ensure_dir_exists(output_paths["tof_xye_filename"]))
    _save_xye(ws_group=tof_group,
              filename_template=ensure_dir_exists(output_paths["dspacing_xye_filename"]))


def _save_xye(ws_group, filename_template):
    """
    Saves XYE data into .dat files. This expects the .dat folder to be created and passed.
    It saves the specified group with the specified units into a file whose name contains that
    information.
    :param ws_group: The workspace group to save out to .dat files
    :param filename_template: A string containing a fullpath with a string format template {bankno} to
    denote where the bank number should be inserted
    """
    for bank_index, ws in enumerate(ws_group):
        bank_index += 1  # Ensure we start a 1 when saving out
        mantid.SaveFocusedXYE(InputWorkspace=ws,
                              Filename=filename_template.format(bankno=bank_index),
                              SplitFiles=False,
                              IncludeHeader=False)
