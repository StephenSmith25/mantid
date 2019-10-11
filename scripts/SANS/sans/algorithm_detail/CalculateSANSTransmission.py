# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2019 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +
from __future__ import (absolute_import, division, print_function)

from sans.algorithm_detail.calculate_transmission_helper import (get_detector_id_for_spectrum_number,
                                                                 get_workspace_indices_for_monitors,
                                                                 apply_flat_background_correction_to_monitors,
                                                                 apply_flat_background_correction_to_detectors,
                                                                 get_region_of_interest)
from sans.common.constants import EMPTY_NAME
from sans.common.enums import (RangeStepType, RebinType, FitType, DataType)
from sans.common.general_functions import create_unmanaged_algorithm


class CalculateSANSTransmission(object):
    def __init__(self, state_adjustment_calculate_transmission, data_type_str):
        """
        Calculates the transmission for a SANS reduction.
        :param data_type_str:  	The component of the instrument which is to be reduced. Allowed values: ['Sample', 'Can']
        """
        self._state = state_adjustment_calculate_transmission
        self._data_type_str = data_type_str

    def calculate_transmission(self, transmission_ws, direct_ws):
        """
        Calculates the transmission for a SANS reduction.
        :param transmission_ws: The transmission workspace in time-of-flight units.
        :param direct_ws: The direct workspace in time-of-flight units.
        :return: Tuple of: Output Workspace and Unfitted Data - Both in wavelength
        """
        calculate_transmission_state = self._state
        # The calculation of the transmission has the following steps:
        # 1. Get all spectrum numbers which take part in the transmission calculation
        # 2. Clean up the transmission and direct workspaces, ie peak prompt correction, flat background calculation,
        #    wavelength conversion and rebinning of the data.
        # 3. Run the CalculateTransmission algorithm
        incident_monitor_spectrum_number = calculate_transmission_state.incident_monitor
        if incident_monitor_spectrum_number is None:
            incident_monitor_spectrum_number = calculate_transmission_state.default_incident_monitor

        # 1. Get relevant spectra
        detector_id_incident_monitor = get_detector_id_for_spectrum_number(transmission_ws,
                                                                           incident_monitor_spectrum_number)
        detector_ids_roi, detector_id_transmission_monitor, detector_id_default_transmission_monitor = \
            self._get_detector_ids_for_transmission_calculation(transmission_ws, calculate_transmission_state)
        all_detector_ids = [detector_id_incident_monitor]

        if len(detector_ids_roi) > 0:
            all_detector_ids.extend(detector_ids_roi)
        elif detector_id_transmission_monitor is not None:
            all_detector_ids.append(detector_id_transmission_monitor)
        elif detector_id_default_transmission_monitor is not None:
            all_detector_ids.append(detector_id_default_transmission_monitor)
        else:
            raise RuntimeError("SANSCalculateTransmission: No region of interest or transmission monitor selected.")

        # 2. Clean transmission data

        transmission_ws = self._get_corrected_wavelength_workspace(transmission_ws, all_detector_ids,
                                                                   calculate_transmission_state)
        direct_ws = self._get_corrected_wavelength_workspace(direct_ws, all_detector_ids,
                                                             calculate_transmission_state)

        data_type = DataType.from_string(self._data_type_str)

        # 3. Fit
        output_workspace, unfitted_transmission_workspace = \
            self._perform_fit(transmission_ws, direct_ws, detector_ids_roi,
                              detector_id_transmission_monitor, detector_id_default_transmission_monitor,
                              detector_id_incident_monitor, calculate_transmission_state, data_type)

        return output_workspace, unfitted_transmission_workspace

    @staticmethod
    def _perform_fit(transmission_workspace, direct_workspace,
                     transmission_roi_detector_ids, transmission_monitor_detector_id,
                     transmission_monitor_detector_id_default, incident_monitor_detector_id,
                     calculate_transmission_state, data_type):
        """
        This performs the actual transmission calculation.

        :param transmission_workspace: the corrected transmission workspace
        :param direct_workspace: the corrected direct workspace
        :param transmission_roi_detector_ids: the roi detector ids
        :param transmission_monitor_detector_id: the transmission monitor detector id
        :param transmission_monitor_detector_id_default: the default transmission monitor id
        :param incident_monitor_detector_id: the incident monitor id
        :param calculate_transmission_state: the state for the transmission calculation
        :param data_type: the data type which is currently being investigated, ie if it is a sample or a can run.
        :return: a fitted workspace and an unfitted workspace
        """

        wavelength_low = calculate_transmission_state.wavelength_low[0]
        wavelength_high = calculate_transmission_state.wavelength_high[0]
        wavelength_step = calculate_transmission_state.wavelength_step
        wavelength_step_type = calculate_transmission_state.wavelength_step_type
        prefix = 1.0 if wavelength_step_type is RangeStepType.Lin else -1.0
        wavelength_step *= prefix
        rebin_params = str(wavelength_low) + "," + str(wavelength_step) + "," + str(wavelength_high)

        trans_name = "CalculateTransmission"
        trans_options = {"SampleRunWorkspace": transmission_workspace,
                         "DirectRunWorkspace": direct_workspace,
                         "OutputWorkspace": EMPTY_NAME,
                         "IncidentBeamMonitor": incident_monitor_detector_id,
                         "RebinParams": rebin_params,
                         "OutputUnfittedData": True}

        # If we have a region of interest we use it else we use the transmission monitor

        if len(transmission_roi_detector_ids) > 0:
            trans_options.update({"TransmissionROI": transmission_roi_detector_ids})
        elif transmission_monitor_detector_id is not None:
            trans_options.update({"TransmissionMonitor": transmission_monitor_detector_id})
        elif transmission_monitor_detector_id_default:
            trans_options.update({"TransmissionMonitor": transmission_monitor_detector_id_default})
        else:
            raise RuntimeError("No transmission monitor has been provided.")

        # Get the fit setting for the correct data type, ie either for the Sample of the Can
        fit_type = calculate_transmission_state.fit[DataType.to_string(data_type)].fit_type
        if fit_type is FitType.Logarithmic:
            fit_string = "Log"
        elif fit_type is FitType.Polynomial:
            fit_string = "Polynomial"
        else:
            fit_string = "Linear"

        trans_options.update({"FitMethod": fit_string})
        if fit_type is FitType.Polynomial:
            polynomial_order = calculate_transmission_state.fit[DataType.to_string(data_type)].polynomial_order
            trans_options.update({"PolynomialOrder": polynomial_order})

        trans_alg = create_unmanaged_algorithm(trans_name, **trans_options)
        trans_alg.execute()

        fitted_transmission_workspace = trans_alg.getProperty("OutputWorkspace").value
        try:
            unfitted_transmission_workspace = trans_alg.getProperty("UnfittedData").value
        except RuntimeError:
            unfitted_transmission_workspace = None

        # Set the y label correctly for the fitted and unfitted transmission workspaces
        y_unit_label_transmission_ratio = "Transmission"
        if fitted_transmission_workspace:
            fitted_transmission_workspace.setYUnitLabel(y_unit_label_transmission_ratio)
        if unfitted_transmission_workspace:
            unfitted_transmission_workspace.setYUnitLabel(y_unit_label_transmission_ratio)

        if fit_type is FitType.NoFit:
            output_workspace = unfitted_transmission_workspace
        else:
            output_workspace = fitted_transmission_workspace
        return output_workspace, unfitted_transmission_workspace

    @staticmethod
    def _get_detector_ids_for_transmission_calculation(transmission_workspace, calculate_transmission_state):
        """
        Get the detector ids which participate in the transmission calculation.

        This can come either from a ROI/MASK/RADIUS selection or from a transmission monitor, not both.
        :param transmission_workspace: the transmission workspace.
        :param calculate_transmission_state: a SANSStateCalculateTransmission object.
        :return: a list of detector ids for ROI and a detector id for the transmission monitor, either can be None
        """
        # Get the potential ROI detector ids
        transmission_radius = calculate_transmission_state.transmission_radius_on_detector
        transmission_roi = calculate_transmission_state.transmission_roi_files
        transmission_mask = calculate_transmission_state.transmission_mask_files
        detector_ids_roi = get_region_of_interest(transmission_workspace, transmission_radius, transmission_roi,
                                                  transmission_mask)

        # Get the potential transmission monitor detector id
        transmission_monitor_spectrum_number = calculate_transmission_state.transmission_monitor
        detector_id_transmission_monitor = None
        if transmission_monitor_spectrum_number is not None:
            detector_id_transmission_monitor = get_detector_id_for_spectrum_number(transmission_workspace,
                                                                                   transmission_monitor_spectrum_number)

        # Get the default transmission monitor detector id. This is our fallback if nothing else was specified.
        default_transmission_monitor = calculate_transmission_state.default_transmission_monitor

        detector_id_default_transmission_monitor = get_detector_id_for_spectrum_number(transmission_workspace,
                                                                                       default_transmission_monitor)

        return detector_ids_roi, detector_id_transmission_monitor, detector_id_default_transmission_monitor

    def _get_corrected_wavelength_workspace(self, workspace, detector_ids, calculate_transmission_state):
        """
        Performs a prompt peak correction, a background correction, converts to wavelength and rebins.

        :param workspace: the workspace which is being corrected.
        :param detector_ids: a list of relevant detector ids
        :param calculate_transmission_state: a SANSStateCalculateTransmission state
        :return:  a corrected workspace.
        """
        # Extract the relevant spectra. These include
        # 1. The incident monitor spectrum
        # 2. The transmission spectra, be it monitor or ROI based.
        # A previous implementation of this code had a comment which suggested
        # that we have to exclude unused spectra as the interpolation runs into
        # problems if we don't.
        extract_name = "ExtractSpectra"
        extract_options = {"InputWorkspace": workspace,
                           "OutputWorkspace": EMPTY_NAME,
                           "DetectorList": detector_ids}
        extract_alg = create_unmanaged_algorithm(extract_name, **extract_options)
        extract_alg.execute()
        workspace = extract_alg.getProperty("OutputWorkspace").value

        # Make sure that we still have spectra in the workspace
        if workspace.getNumberHistograms() == 0:
            raise RuntimeError("SANSCalculateTransmissionCorrection: The transmission workspace does "
                               "not seem to have any spectra.")

        # ----------------------------------
        # Perform the prompt peak correction
        # ----------------------------------
        prompt_peak_correction_min = calculate_transmission_state.prompt_peak_correction_min
        prompt_peak_correction_max = calculate_transmission_state.prompt_peak_correction_max
        prompt_peak_correction_enabled = calculate_transmission_state.prompt_peak_correction_enabled
        workspace = self._perform_prompt_peak_correction(workspace, prompt_peak_correction_min,
                                                         prompt_peak_correction_max, prompt_peak_correction_enabled)

        # ---------------------------------------
        # Perform the flat background correction
        # ---------------------------------------
        # The flat background correction has two parts:
        # 1. Corrections on monitors
        # 2. Corrections on regular detectors

        # Monitor flat background correction
        workspace_indices_of_monitors = list(get_workspace_indices_for_monitors(workspace))
        background_tof_monitor_start = calculate_transmission_state.background_TOF_monitor_start
        background_tof_monitor_stop = calculate_transmission_state.background_TOF_monitor_stop
        background_tof_general_start = calculate_transmission_state.background_TOF_general_start
        background_tof_general_stop = calculate_transmission_state.background_TOF_general_stop
        workspace = apply_flat_background_correction_to_monitors(workspace,
                                                                 workspace_indices_of_monitors,
                                                                 background_tof_monitor_start,
                                                                 background_tof_monitor_stop,
                                                                 background_tof_general_start,
                                                                 background_tof_general_stop)

        # Detector flat background correction
        flat_background_correction_start = calculate_transmission_state.background_TOF_roi_start
        flat_background_correction_stop = calculate_transmission_state.background_TOF_roi_stop
        workspace = apply_flat_background_correction_to_detectors(workspace, flat_background_correction_start,
                                                                  flat_background_correction_stop)

        # ---------------------------------------
        # Convert to wavelength and rebin
        # ---------------------------------------
        # The wavelength setting is reasonably complex.
        # 1. Use full wavelength range
        # 2. Use standard settings
        if calculate_transmission_state.use_full_wavelength_range:
            wavelength_low = calculate_transmission_state.wavelength_full_range_low
            wavelength_high = calculate_transmission_state.wavelength_full_range_high
        else:
            fit_state = calculate_transmission_state.fit[self._data_type_str]
            wavelength_low = fit_state.wavelength_low if fit_state.wavelength_low \
                else calculate_transmission_state.wavelength_low[0]
            wavelength_high = fit_state.wavelength_high if fit_state.wavelength_high \
                else calculate_transmission_state.wavelength_high[0]

        wavelength_step = calculate_transmission_state.wavelength_step
        rebin_type = calculate_transmission_state.rebin_type
        wavelength_step_type = calculate_transmission_state.wavelength_step_type

        convert_name = "SANSConvertToWavelengthAndRebin"
        convert_options = {"InputWorkspace": workspace,
                           "WavelengthLow": wavelength_low,
                           "WavelengthHigh": wavelength_high,
                           "WavelengthStep": wavelength_step,
                           "WavelengthStepType": RangeStepType.to_string(wavelength_step_type),
                           "RebinMode": RebinType.to_string(rebin_type)}
        convert_alg = create_unmanaged_algorithm(convert_name, **convert_options)
        convert_alg.setPropertyValue("OutputWorkspace", EMPTY_NAME)
        convert_alg.setProperty("OutputWorkspace", workspace)
        convert_alg.execute()
        return convert_alg.getProperty("OutputWorkspace").value

    @staticmethod
    def _perform_prompt_peak_correction(workspace, prompt_peak_correction_min, prompt_peak_correction_max,
                                        prompt_peak_correction_enabled):
        """
        Prompt peak correction is performed if it is explicitly set by the user.

        :param workspace: the workspace to correct.
        :param prompt_peak_correction_min: the start time for the prompt peak correction.
        :param prompt_peak_correction_max: the stop time for the prompt peak correction.
        :prompt_peak_correction_enabled: flag if prompt peak correction should be enabled
        :return: a corrected workspace.
        """
        # We perform only a prompt peak correction if the start and stop values of the bins we want to remove,
        # were explicitly set. Some instruments require it, others don't.
        if prompt_peak_correction_enabled and prompt_peak_correction_min is not None and \
                prompt_peak_correction_max is not None:  # noqa
            remove_name = "RemoveBins"
            remove_options = {"InputWorkspace": workspace,
                              "XMin": prompt_peak_correction_min,
                              "XMax": prompt_peak_correction_max,
                              "Interpolation": "Linear"}
            remove_alg = create_unmanaged_algorithm(remove_name, **remove_options)
            remove_alg.setPropertyValue("OutputWorkspace", EMPTY_NAME)
            remove_alg.setProperty("OutputWorkspace", workspace)
            remove_alg.execute()
            workspace = remove_alg.getProperty("OutputWorkspace").value
        return workspace