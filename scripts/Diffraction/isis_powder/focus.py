from __future__ import (absolute_import, division, print_function)

import mantid.simpleapi as mantid
import isis_powder.common as common


def focus(number, instrument, attenuate=True, van_norm=True):
    return _run_focus(run_number=number, perform_attenuation=attenuate,
                      perform_vanadium_norm=van_norm, instrument=instrument)


def _run_focus(instrument, run_number, perform_attenuation, perform_vanadium_norm):
    # Read
    read_ws = common._load_current_normalised_ws(number=run_number, instrument=instrument)
    input_workspace = instrument._do_tof_rebinning_focus(read_ws)  # Rebins for PEARL

    cycle_information = instrument._get_cycle_information(run_number=run_number)
    calibration_file_paths = instrument._get_calibration_full_paths(cycle=cycle_information["cycle"])

    # Compensate for empty sample if specified
    input_workspace = instrument._subtract_sample_empty(input_workspace)

    # Align / Focus
    input_workspace = mantid.AlignDetectors(InputWorkspace=input_workspace,
                                            CalibrationFile=calibration_file_paths["calibration"])

    # TODO fix this - maybe have it save solid angle corrections and just load/apply
    input_workspace = instrument._apply_solid_angle_efficiency_corr(ws_to_correct=input_workspace,
                                                                    vanadium_path=calibration_file_paths["vanadium"])

    input_workspace = mantid.DiffractionFocussing(InputWorkspace=input_workspace,
                                                  GroupingFileName=calibration_file_paths["grouping"])

    # Process
    calibrated_spectra = _divide_sample_by_vanadium(instrument=instrument, run_number=run_number,
                                                    input_workspace=input_workspace,
                                                    perform_vanadium_norm=perform_vanadium_norm)

    common.remove_intermediate_workspace(read_ws)
    common.remove_intermediate_workspace(input_workspace)

    # Output
    processed_nexus_files = instrument._process_focus_output(calibrated_spectra, run_number, attenuate=perform_attenuation)

    for ws in calibrated_spectra:
        common.remove_intermediate_workspace(ws)

    return processed_nexus_files


def _divide_sample_by_vanadium(instrument, run_number, input_workspace, perform_vanadium_norm):
    processed_spectra = []

    cycle_information = instrument._get_cycle_information(run_number=run_number)
    input_file_paths = instrument._get_calibration_full_paths(cycle=cycle_information["cycle"])

    alg_range, save_range = instrument._get_instrument_alg_save_ranges(cycle_information["instrument_version"])

    for index in range(0, alg_range):
        if perform_vanadium_norm:
            vanadium_ws = mantid.LoadNexus(Filename=input_file_paths["calibrated_vanadium"], EntryNumber=index + 1)

            processed_spectra.append(
                instrument.correct_sample_vanadium(focused_ws=input_workspace, index=index, vanadium_ws=vanadium_ws))

            common.remove_intermediate_workspace(vanadium_ws)
        else:
            processed_spectra.append(
                instrument.correct_sample_vanadium(focused_ws=input_workspace, index=index))

    return processed_spectra
