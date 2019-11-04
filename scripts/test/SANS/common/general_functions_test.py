# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +
from __future__ import (absolute_import, division, print_function)

import unittest

from mantid.api import AnalysisDataService, FrameworkManager
from mantid.kernel import (V3D, Quat)
from mantid.py3compat import mock
from sans.common.constants import (SANS2D, LOQ, LARMOR)
from sans.common.enums import (ISISReductionMode, ReductionDimensionality, OutputParts,
                               SANSInstrument, DetectorType, SANSFacility, DataType)
from sans.common.general_functions import (quaternion_to_angle_and_axis, create_managed_non_child_algorithm,
                                           create_unmanaged_algorithm, add_to_sample_log,
                                           get_standard_output_workspace_name, sanitise_instrument_name,
                                           get_reduced_can_workspace_from_ads, write_hash_into_reduced_can_workspace,
                                           convert_instrument_and_detector_type_to_bank_name,
                                           convert_bank_name_to_detector_type_isis,
                                           get_facility, parse_diagnostic_settings, get_transmission_output_name,
                                           get_output_name, parse_event_slice_setting)
from sans.state.data import StateData
from sans.test_helper.test_director import TestDirector


class SANSFunctionsTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        FrameworkManager.Instance()

    @staticmethod
    def _prepare_workspaces(number_of_workspaces, tagged_workspace_names=None, state=None, reduction_mode=None):
        create_name = "CreateSampleWorkspace"
        create_options = {"OutputWorkspace": "test",
                          "NumBanks": 1,
                          "BankPixelWidth": 2,
                          "XMin": 1,
                          "XMax": 10,
                          "BinWidth": 2}
        create_alg = create_unmanaged_algorithm(create_name, **create_options)

        for index in range(number_of_workspaces):
            create_alg.execute()
            workspace = create_alg.getProperty("OutputWorkspace").value
            workspace_name = "test" + "_" + str(index)
            AnalysisDataService.addOrReplace(workspace_name, workspace)

        if tagged_workspace_names is not None:
            for key, value in list(tagged_workspace_names.items()):
                create_alg.execute()
                workspace = create_alg.getProperty("OutputWorkspace").value
                AnalysisDataService.addOrReplace(value, workspace)
                write_hash_into_reduced_can_workspace(state, workspace, reduction_mode, key)

    @staticmethod
    def _create_sample_workspace():
        sample_name = "CreateSampleWorkspace"
        sample_options = {"OutputWorkspace": "dummy"}
        sample_alg = create_unmanaged_algorithm(sample_name, **sample_options)
        sample_alg.execute()
        return sample_alg.getProperty("OutputWorkspace").value

    def _do_test_quaternion(self, angle, axis, expected_axis=None):
        # Act
        quaternion = Quat(angle, axis)
        converted_angle, converted_axis = quaternion_to_angle_and_axis(quaternion)

        # Assert
        if expected_axis is not None:
            axis = expected_axis
        self.assertAlmostEqual(angle, converted_angle)
        self.assertAlmostEqual(axis[0], converted_axis[0])
        self.assertAlmostEqual(axis[1], converted_axis[1])
        self.assertAlmostEqual(axis[2], converted_axis[2])

    @staticmethod
    def _get_state():
        test_director = TestDirector()
        state = test_director.construct()

        state.data.sample_scatter_run_number = 12345
        state.data.sample_scatter_period = StateData.ALL_PERIODS

        state.reduction.dimensionality = ReductionDimensionality.OneDim

        state.wavelength.wavelength_low = [12.0]
        state.wavelength.wavelength_high = [34.0]

        state.mask.phi_min = 12.0
        state.mask.phi_max = 56.0

        state.slice.start_time = [4.56778]
        state.slice.end_time = [12.373938]
        return state

    def test_that_quaternion_can_be_converted_to_axis_and_angle_for_regular(self):
        # Arrange
        angle = 23.0
        axis = V3D(0.0, 1.0, 0.0)
        self._do_test_quaternion(angle, axis)

    def test_that_quaternion_can_be_converted_to_axis_and_angle_for_0_degree(self):
        # Arrange
        angle = 0.0
        axis = V3D(1.0, 0.0, 0.0)
        # There shouldn't be an axis for angle 0
        expected_axis = V3D(0.0, 0.0, 0.0)
        self._do_test_quaternion(angle, axis, expected_axis)

    def test_that_quaternion_can_be_converted_to_axis_and_angle_for_180_degree(self):
        # Arrange
        angle = 180.0
        axis = V3D(0.0, 1.0, 0.0)
        # There shouldn't be an axis for angle 0
        self._do_test_quaternion(angle, axis)

    def test_that_sample_log_is_added(self):
        # Arrange
        workspace = SANSFunctionsTest._create_sample_workspace()
        log_name = "TestName"
        log_value = "TestValue"
        log_type = "String"

        # Act
        add_to_sample_log(workspace, log_name, log_value, log_type)

        # Assert
        run = workspace.run()
        self.assertTrue(run.hasProperty(log_name))
        self.assertEqual(run.getProperty(log_name).value,  log_value)

    def test_that_sample_log_raises_for_non_string_type_arguments(self):
        # Arrange
        workspace = SANSFunctionsTest._create_sample_workspace()
        log_name = "TestName"
        log_value = 123
        log_type = "String"

        # Act + Assert
        try:
            add_to_sample_log(workspace, log_name, log_value, log_type)
            did_raise = False
        except TypeError:
            did_raise = True
        self.assertTrue(did_raise)

    def test_that_sample_log_raises_for_wrong_type_selection(self):
        # Arrange
        workspace = SANSFunctionsTest._create_sample_workspace()
        log_name = "TestName"
        log_value = "test"
        log_type = "sdfsdfsdf"

        # Act + Assert
        try:
            add_to_sample_log(workspace, log_name, log_value, log_type)
            did_raise = False
        except ValueError:
            did_raise = True
        self.assertTrue(did_raise)

    def test_that_unknown_reduction_mode_raises(self):
        # Arrange
        state = SANSFunctionsTest._get_state()

        # Act + Assert
        try:
            get_standard_output_workspace_name(state, ISISReductionMode.All)
            did_raise = False
        except RuntimeError:
            did_raise = True
        self.assertTrue(did_raise)

    def test_that_creates_correct_workspace_name_for_1D(self):
        # Arrange
        state = SANSFunctionsTest._get_state()
        # Act
        output_workspace, _ = get_standard_output_workspace_name(state, ISISReductionMode.LAB)
        # Assert
        self.assertEqual("12345rear_1D_12.0_34.0Phi12.0_56.0_t4.57_T12.37",  output_workspace)

    def test_that_can_switch_off_including_slice_limits_in_standard_output_workspace_name(self):
        # Arrange
        state = SANSFunctionsTest._get_state()
        # Act
        output_workspace, _ = get_standard_output_workspace_name(state, ISISReductionMode.LAB,
                                                                 include_slice_limits=False)
        # Assert
        self.assertTrue("12345rear_1D_12.0_34.0Phi12.0_56.0" == output_workspace)

    def test_that_get_transmission_output_name_returns_correct_name_for_user_specified_workspace(self):
        # Arrange
        state = SANSFunctionsTest._get_state()
        state.save.user_specified_output_name = "test_output"
        # Act
        output_workspace, group_output_name = get_transmission_output_name(state)
        # Assert
        self.assertEqual(output_workspace, "test_output_trans_Sample")
        self.assertEqual(group_output_name, 'test_output_trans')

    def test_that_get_transmission_output_name_returns_correct_name(self):
        # Arrange
        state = SANSFunctionsTest._get_state()
        state.save.user_specified_output_name = ''
        # Act
        output_workspace, group_output_name = get_transmission_output_name(state)
        # Assert
        self.assertEqual(output_workspace, "12345_trans_Sample_1.0_10.0")
        self.assertEqual(group_output_name, "12345_trans_1.0_10.0")

    def test_that_get_transmission_output_name_returns_correct_name_for_wavelength_slices_with_user_specified(self):
        state = self._get_state()
        state.save.user_specified_output_name = 'user_output_name'
        state.reduction.reduction_mode = ISISReductionMode.Merged
        multi_reduction_type = {"period": False,
                                "event_slice": False,
                                "wavelength_range": True}

        output_name, group_output_name = get_transmission_output_name(state, multi_reduction_type=multi_reduction_type)

        self.assertEqual(output_name, 'user_output_name_trans_Sample_12.0_34.0')
        self.assertEqual(group_output_name, 'user_output_name_trans')

    def test_that_get_transmission_output_name_returns_correct_name_for_wavelength_slices(self):
        state = self._get_state()
        state.save.user_specified_output_name = ''
        state.reduction.reduction_mode = ISISReductionMode.Merged
        multi_reduction_type = {"period": False,
                                "event_slice": False,
                                "wavelength_range": True}

        output_name, group_output_name = get_transmission_output_name(state, multi_reduction_type=multi_reduction_type)

        self.assertEqual(output_name, '12345_trans_Sample_1.0_10.0_12.0_34.0')
        self.assertEqual(group_output_name, '12345_trans_1.0_10.0')

    def test_that_get_transmission_output_name_returns_correct_name_for_user_specified_workspace_for_CAN(self):
        # Arrange
        state = SANSFunctionsTest._get_state()
        state.save.user_specified_output_name = "test_output"
        # Act
        output_workspace, group_output_name = get_transmission_output_name(state, data_type=DataType.Can)
        # Assert
        self.assertEqual(output_workspace, "test_output_trans_Can")
        self.assertEqual(group_output_name, 'test_output_trans')


    def test_that_get_transmission_output_name_returns_correct_name_for_CAN(self):
        # Arrange
        state = SANSFunctionsTest._get_state()
        state.save.user_specified_output_name = ''
        # Act
        output_workspace, group_output_name = get_transmission_output_name(state, data_type=DataType.Can)
        # Assert
        self.assertEqual(output_workspace, "12345_trans_Can_1.0_10.0")
        self.assertEqual(group_output_name, '12345_trans_1.0_10.0')


    def test_that_get_transmission_output_name_returns_correct_name_for_wavelength_slices_for_CAN(self):
        state = self._get_state()
        state.save.user_specified_output_name = ''
        state.reduction.reduction_mode = ISISReductionMode.Merged
        multi_reduction_type = {"period": False,
                                "event_slice": False,
                                "wavelength_range": True}

        output_name, group_output_name = get_transmission_output_name(state,
                                                                      multi_reduction_type=multi_reduction_type,
                                                                      data_type=DataType.Can)

        self.assertEqual(output_name, '12345_trans_Can_1.0_10.0_12.0_34.0')
        self.assertEqual(group_output_name, '12345_trans_1.0_10.0')

    def test_that_get_transmission_output_name_returns_correct_name_for_wavelength_slices_for_CAN_unfitted(self):
        state = self._get_state()
        state.save.user_specified_output_name = ''
        state.reduction.reduction_mode = ISISReductionMode.Merged
        multi_reduction_type = {"period": False,
                                "event_slice": False,
                                "wavelength_range": True}

        output_name, group_output_name = get_transmission_output_name(state,
                                                                      multi_reduction_type=multi_reduction_type,
                                                                      data_type=DataType.Can, fitted=False)

        self.assertEqual(output_name, '12345_trans_Can_unfitted_1.0_10.0')
        self.assertEqual(group_output_name, '12345_trans_1.0_10.0')

    def test_that_sanitises_instrument_names(self):
        name1 = sanitise_instrument_name("LOQ_trans")
        self.assertEqual(name1, LOQ)

        name2 = sanitise_instrument_name("sans2d")
        self.assertEqual(name2, SANS2D)

        name3 = sanitise_instrument_name("__LArMOR_")
        self.assertEqual(name3, LARMOR)

        name4 = sanitise_instrument_name("OThER")
        self.assertEqual(name4, "OThER")

    def test_that_can_find_can_reduction_if_it_exists(self):
        # Arrange
        test_director = TestDirector()
        state = test_director.construct()
        tagged_workspace_names = {None: "test_ws",
                                  OutputParts.Count: "test_ws_count",
                                  OutputParts.Norm: "test_ws_norm"}
        SANSFunctionsTest._prepare_workspaces(number_of_workspaces=4,
                                              tagged_workspace_names=tagged_workspace_names,
                                              state=state,
                                              reduction_mode=ISISReductionMode.LAB)
        # Act
        workspace, workspace_count, workspace_norm = get_reduced_can_workspace_from_ads(state, output_parts=True,
                                                                              reduction_mode=ISISReductionMode.LAB)  # noqa

        # Assert
        self.assertNotEqual(workspace, None)
        self.assertEqual(workspace.name(),  AnalysisDataService.retrieve("test_ws").name())
        self.assertNotEqual(workspace_count, None)
        self.assertEqual(workspace_count.name(),  AnalysisDataService.retrieve("test_ws_count").name())
        self.assertNotEqual(workspace_norm, None)
        self.assertEqual(workspace_norm.name(),  AnalysisDataService.retrieve("test_ws_norm").name())

        # Clean up
        SANSFunctionsTest._remove_workspaces()

    @staticmethod
    def _remove_workspaces():
        for element in AnalysisDataService.getObjectNames():
            AnalysisDataService.remove(element)

    def test_that_returns_none_if_it_does_not_exist(self):
        # Arrange
        test_director = TestDirector()
        state = test_director.construct()
        SANSFunctionsTest._prepare_workspaces(number_of_workspaces=4, tagged_workspace_names=None,
                                              state=state, reduction_mode=ISISReductionMode.LAB)

        # Act
        workspace, workspace_count, workspace_norm = \
            get_reduced_can_workspace_from_ads(state, output_parts=False, reduction_mode=ISISReductionMode.LAB)

        # Assert
        self.assertEqual(workspace, None)
        self.assertEqual(workspace_count, None)
        self.assertEqual(workspace_norm, None)

        # Clean up
        SANSFunctionsTest._remove_workspaces()

    def test_that_convert_instrument_and_detector_type_to_bank_name_produces_expected_results(self):
        self.assertTrue("front-detector" == convert_instrument_and_detector_type_to_bank_name(SANSInstrument.SANS2D,
                                                                                              DetectorType.HAB))
        self.assertTrue("rear-detector" == convert_instrument_and_detector_type_to_bank_name(SANSInstrument.SANS2D,
                                                                                             DetectorType.LAB))
        self.assertTrue("HAB" == convert_instrument_and_detector_type_to_bank_name(SANSInstrument.LOQ,
                                                                                   DetectorType.HAB))
        self.assertTrue("main-detector-bank" == convert_instrument_and_detector_type_to_bank_name(SANSInstrument.LOQ,
                                                                                                  DetectorType.LAB))
        self.assertTrue("DetectorBench" == convert_instrument_and_detector_type_to_bank_name(SANSInstrument.LARMOR,
                                                                                             DetectorType.LAB))
        self.assertTrue("rear-detector" == convert_instrument_and_detector_type_to_bank_name(SANSInstrument.ZOOM,
                                                                                             DetectorType.LAB))

    def test_that_converts_detector_name_to_type(self):
        self.assertEqual(convert_bank_name_to_detector_type_isis("rEar-detector"), DetectorType.LAB)
        self.assertEqual(convert_bank_name_to_detector_type_isis("MAIN-detector-BANK"), DetectorType.LAB)
        self.assertEqual(convert_bank_name_to_detector_type_isis("DeTectorBench"), DetectorType.LAB)
        self.assertEqual(convert_bank_name_to_detector_type_isis("rear"), DetectorType.LAB)
        self.assertEqual(convert_bank_name_to_detector_type_isis("MAIN"), DetectorType.LAB)
        self.assertEqual(convert_bank_name_to_detector_type_isis("FRoNT-DETECTOR"), DetectorType.HAB)
        self.assertEqual(convert_bank_name_to_detector_type_isis("HaB"), DetectorType.HAB)
        self.assertEqual(convert_bank_name_to_detector_type_isis("front"), DetectorType.HAB)
        self.assertRaises(RuntimeError, convert_bank_name_to_detector_type_isis, "test")

    def test_that_gets_facility(self):
        self.assertEqual(get_facility(SANSInstrument.SANS2D), SANSFacility.ISIS)
        self.assertEqual(get_facility(SANSInstrument.LOQ), SANSFacility.ISIS)
        self.assertEqual(get_facility(SANSInstrument.LARMOR), SANSFacility.ISIS)
        self.assertEqual(get_facility(SANSInstrument.ZOOM), SANSFacility.ISIS)
        self.assertEqual(get_facility(SANSInstrument.NoInstrument), SANSFacility.NoFacility)

    def test_that_diagnostic_parser_produces_correct_list(self):
        string_to_parse = '8-11, 12:15, 5, 7:9'
        expected_result = [[8, 11], [12, 12], [13, 13], [14, 14], [15, 15], [5, 5], [7,7], [8,8], [9,9]]

        result = parse_diagnostic_settings(string_to_parse)

        self.assertEqual(result, expected_result)

    def test_get_output_name_should_return_standard_name_for_non_sliced_non_user_specified_name_reduction(self):
        state = self._get_state()
        state.save.user_specified_output_name = ''

        output_name, group_output_name = get_output_name(state, ISISReductionMode.LAB, False)

        self.assertEqual(output_name, '12345rear_1D_12.0_34.0Phi12.0_56.0_t4.57_T12.37')
        self.assertEqual(group_output_name, '12345rear_1D_12.0_34.0Phi12.0_56.0_t4.57_T12.37')

    def test_get_output_name_should_replace_name_with_user_specified_name_for_LAB_reduction(self):
        state = self._get_state()
        state.save.user_specified_output_name = 'user_output_name'
        state.reduction.reduction_mode = ISISReductionMode.LAB

        output_name, group_output_name = get_output_name(state, ISISReductionMode.LAB, False)

        self.assertEqual(output_name, 'user_output_name')
        self.assertEqual(group_output_name, 'user_output_name')

    def test_get_output_name_should_replace_name_with_user_specified_name_for_HAB_reduction(self):
        state = self._get_state()
        state.save.user_specified_output_name = 'user_output_name'
        state.reduction.reduction_mode = ISISReductionMode.LAB

        output_name, group_output_name = get_output_name(state, ISISReductionMode.HAB, False)

        self.assertEqual(output_name, 'user_output_name')
        self.assertEqual(group_output_name, 'user_output_name')

    def test_get_output_name_replaces_name_with_user_specified_name_with_appended_detector_for_All_reduction(self):
        state = self._get_state()
        state.save.user_specified_output_name = 'user_output_name'
        state.reduction.reduction_mode = ISISReductionMode.All

        output_name, group_output_name = get_output_name(state, ISISReductionMode.LAB, False)

        self.assertEqual(output_name, 'user_output_name_rear')
        self.assertEqual(group_output_name, 'user_output_name_rear')

    def test_get_output_name_replaces_name_with_user_specified_name_with_appended_detector_for_Merged_reduction(self):
        state = self._get_state()
        state.save.user_specified_output_name = 'user_output_name'
        state.reduction.reduction_mode = ISISReductionMode.Merged

        output_name, group_output_name = get_output_name(state, ISISReductionMode.Merged, False)

        self.assertEqual(output_name, 'user_output_name_merged')
        self.assertEqual(group_output_name, 'user_output_name_merged')

    def test_returned_name_for_time_sliced_merged_reduction_with_user_specified_name_correct(self):
        state = self._get_state()
        state.save.user_specified_output_name = 'user_output_name'
        state.reduction.reduction_mode = ISISReductionMode.Merged
        multi_reduction_type = {"period": False,
                                "event_slice": True,
                                "wavelength_range": False}

        output_name, group_output_name = get_output_name(state, ISISReductionMode.Merged, True,
                                                         multi_reduction_type=multi_reduction_type)

        self.assertEqual(output_name, 'user_output_name_merged_t4.57_T12.37')
        self.assertEqual(group_output_name, 'user_output_name_merged')

    def test_returned_name_for_wavelength_sliced_merged_reduction_with_user_specified_name_correct(self):
        state = self._get_state()
        state.save.user_specified_output_name = 'user_output_name'
        state.reduction.reduction_mode = ISISReductionMode.Merged
        multi_reduction_type = {"period": False,
                                "event_slice": False,
                                "wavelength_range": True}

        output_name, group_output_name = get_output_name(state, ISISReductionMode.Merged, True,
                                                         multi_reduction_type=multi_reduction_type)

        self.assertEqual(output_name, 'user_output_name_merged_12.0_34.0')
        self.assertEqual(group_output_name, 'user_output_name_merged')

    def test_returned_name_for_period_reduction_with_user_specified_name_correct(self):
        state = self._get_state()
        state.save.user_specified_output_name = 'user_output_name'
        state.reduction.reduction_mode = ISISReductionMode.Merged
        multi_reduction_type = {"period": True,
                                "event_slice": False,
                                "wavelength_range": False}

        output_name, group_output_name = get_output_name(state, ISISReductionMode.Merged, True,
                                                         multi_reduction_type=multi_reduction_type)

        self.assertEqual(output_name, 'user_output_name_merged_p0')
        self.assertEqual(group_output_name, 'user_output_name_merged')

    def test_returned_name_for_all_multi_reduction_with_user_specified_name_correct(self):
        state = self._get_state()
        state.save.user_specified_output_name = 'user_output_name'
        state.reduction.reduction_mode = ISISReductionMode.Merged
        multi_reduction_type = {"period": True,
                                "event_slice": True,
                                "wavelength_range": True}

        output_name, group_output_name = get_output_name(state, ISISReductionMode.Merged, True,
                                                         multi_reduction_type=multi_reduction_type)

        self.assertEqual(output_name, 'user_output_name_merged_p0_t4.57_T12.37_12.0_34.0')
        self.assertEqual(group_output_name, 'user_output_name_merged')

    def test_returned_name_for_all_multi_reduction_with_user_specified_name_correct_LAB_reduction(self):
        state = self._get_state()
        state.save.user_specified_output_name = 'user_output_name'
        state.reduction.reduction_mode = ISISReductionMode.LAB
        multi_reduction_type = {"period": True,
                                "event_slice": True,
                                "wavelength_range": True}

        output_name, group_output_name = get_output_name(state, ISISReductionMode.LAB, True,
                                                         multi_reduction_type=multi_reduction_type)

        self.assertEqual(output_name, 'user_output_name_p0_t4.57_T12.37_12.0_34.0')
        self.assertEqual(group_output_name, 'user_output_name')

    def test_returned_name_for_time_sliced_merged_reduction_without_user_specified_name_correct(self):
        state = self._get_state()
        state.save.user_specified_output_name = ''
        state.reduction.reduction_mode = ISISReductionMode.Merged
        multi_reduction_type = {"period": False,
                                "event_slice": True,
                                "wavelength_range": False}

        output_name, group_output_name = get_output_name(state, ISISReductionMode.Merged, True,
                                                         multi_reduction_type=multi_reduction_type)

        self.assertEqual(output_name, '12345merged_1D_12.0_34.0Phi12.0_56.0_t4.57_T12.37')
        self.assertEqual(group_output_name, '12345merged_1DPhi12.0_56.0')

    def test_returned_name_for_all_multi_reduction_without_user_specified_name_correct(self):
        state = self._get_state()
        state.save.user_specified_output_name = ''
        state.reduction.reduction_mode = ISISReductionMode.Merged
        multi_reduction_type = {"period": True,
                                "event_slice": True,
                                "wavelength_range": True}

        output_name, group_output_name = get_output_name(state, ISISReductionMode.Merged, True,
                                                         multi_reduction_type=multi_reduction_type)

        self.assertEqual(output_name, '12345merged_1D_12.0_34.0Phi12.0_56.0_t4.57_T12.37')
        self.assertEqual(group_output_name, '12345merged_1DPhi12.0_56.0')

    def test_returned_name_for_all_multi_reduction_without_user_specified_name_correct_LAB_reduction(self):
        state = self._get_state()
        state.save.user_specified_output_name = ''
        state.reduction.reduction_mode = ISISReductionMode.LAB
        multi_reduction_type = {"period": True,
                                "event_slice": True,
                                "wavelength_range": True}

        output_name, group_output_name = get_output_name(state, ISISReductionMode.LAB, True,
                                                         multi_reduction_type=multi_reduction_type)

        self.assertEqual(output_name, '12345rear_1D_12.0_34.0Phi12.0_56.0_t4.57_T12.37')
        self.assertEqual(group_output_name, '12345rear_1DPhi12.0_56.0')

    @mock.patch("sans.common.general_functions.AlgorithmManager")
    def test_that_can_create_versioned_managed_non_child_algorithms(self, alg_manager_mock):
        create_managed_non_child_algorithm("TestAlg", version=2, **{"test_val": 5})
        alg_manager_mock.create.assert_called_once_with("TestAlg", 2)

        alg_manager_mock.reset_mock()
        create_managed_non_child_algorithm("TestAlg", **{"test_val": 5})
        alg_manager_mock.create.assert_called_once_with("TestAlg")


class SANSEventSliceParsing(unittest.TestCase):
    def test_simple_range(self):
        input_range = "10-20"
        expected = [[10.0, 20.0]]
        returned = parse_event_slice_setting(input_range)
        self.assertEqual(returned, expected)

    def test_multiple_simple_ranges(self):
        input_range = "10-20, 20-30, 40-45"
        expected = [[10.0, 20.0], [20.0, 30.0], [40.0, 45.0]]
        returned = parse_event_slice_setting(input_range)
        self.assertEqual(returned, expected)

    def test_unbounded_range(self):
        input_range = "10-20 , >25"
        expected = [[10.0, 20.0], [25.0, -1]]
        returned = parse_event_slice_setting(input_range)
        self.assertEqual(returned, expected)

    def test_bounded_start(self):
        input_range = "< 15, 20-21"
        expected = [[-1, 15.0], [20.0, 21.0]]
        returned = parse_event_slice_setting(input_range)
        self.assertEqual(returned, expected)

    def test_steps_range(self):
        input_range = "20:2:26"
        expected = [[20.0, 22.0], [22.0, 24.0], [24.0, 26.0]]
        returned = parse_event_slice_setting(input_range)
        self.assertEqual(returned, expected)

    def test_comma_separated_steps(self):
        input_range = "1,2, 3, 5, 7"
        expected = [[1.0, 2.0], [2.0, 3.0], [3.0, 5.0], [5.0, 7.0]]
        returned = parse_event_slice_setting(input_range)
        self.assertEqual(returned, expected)

    def test_comma_bad_input_rejected(self):
        input_range = "1,2,a,3"
        with self.assertRaises(ValueError):
            parse_event_slice_setting(input_range)


if __name__ == '__main__':
    unittest.main()
