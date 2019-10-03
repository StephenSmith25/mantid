# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +

from __future__ import (absolute_import, division, print_function)

from os import path, makedirs
from ntpath import basename

from mantid.api import AnalysisDataService as Ads
from mantid.kernel import logger
from mantid.simpleapi import Load, EnggVanadiumCorrections, EnggCalibrate, DeleteWorkspace, CloneWorkspace, \
    CreateWorkspace, AppendSpectra
from mantidqt.plotting.functions import plot
from Engineering.EnggUtils import write_ENGINX_GSAS_iparam_file


class CalibrationModel(object):
    def __init__(self):
        self.VANADIUM_INPUT_WORKSPACE_NAME = "engggui_vanadium_ws"
        self.CURVES_WORKSPACE_NAME = "engggui_vanadium_curves"
        self.INTEGRATED_WORKSPACE_NAME = "engggui_vanadium_integration"
        self.out_files_root_dir = path.join(path.expanduser("~"), "Engineering_Mantid/")

    def create_new_calibration(self,
                               vanadium_path,
                               ceria_path,
                               plot_output,
                               instrument,
                               rb_num=None):
        vanadium_corrections = self.calculate_vanadium_correction(vanadium_path)
        van_integration = vanadium_corrections[0]
        van_curves = vanadium_corrections[1]
        ceria_workspace = self.load_ceria(ceria_path)
        output = self.run_calibration(ceria_workspace, van_integration, van_curves)
        if plot_output:
            self._plot_vanadium_curves()
            for i in range(2):
                difc = [output[i].DIFC]
                tzero = [output[i].TZERO]
                self._plot_difc_zero(difc, tzero)
        difc = [output[0].DIFC, output[1].DIFC]
        tzero = [output[0].TZERO, output[1].TZERO]

        calibration_dir = self.out_files_root_dir + "Calibration/"
        self.create_output_files(calibration_dir, difc, tzero, ceria_path, vanadium_path,
                                 instrument)
        if rb_num:
            calibration_dir = self.out_files_root_dir + "User/" + rb_num + "/Calibration/"
            self.create_output_files(calibration_dir, difc, tzero, ceria_path, vanadium_path,
                                     instrument)

    @staticmethod
    def _plot_vanadium_curves():
        van_curve_twin_ws = "__engggui_vanadium_curves_twin_ws"

        if Ads.doesExist(van_curve_twin_ws):
            DeleteWorkspace(van_curve_twin_ws)
        CloneWorkspace(InputWorkspace="engggui_vanadium_curves", OutputWorkspace=van_curve_twin_ws)
        van_curves_ws = Ads.retrieve(van_curve_twin_ws)
        for i in range(1, 3):
            if i == 1:
                curve_plot_bank_1 = plot([van_curves_ws], [0, 1, 2])
                curve_plot_bank_1.gca().set_title("Engg GUI Vanadium Curves Bank 1")
                curve_plot_bank_1.gca().legend(["Data", "Calc", "Diff"])
            if i == 2:
                curve_plot_bank_2 = plot([van_curves_ws], [3, 4, 5])
                curve_plot_bank_2.gca().set_title("Engg GUI Vanadium Curves Bank 2")
                curve_plot_bank_2.gca().legend(["Data", "Calc", "Diff"])

    @staticmethod
    def _plot_difc_zero(difc, tzero):
        for i in range(1, 3):
            bank_ws = Ads.retrieve("engggui_calibration_bank_" + str(i))

            x_val = []
            y_val = []
            y2_val = []

            difc_to_plot = difc[0]
            tzero_to_plot = tzero[0]

            for irow in range(0, bank_ws.rowCount()):
                x_val.append(bank_ws.cell(irow, 0))
                y_val.append(bank_ws.cell(irow, 5))
                y2_val.append(x_val[irow] * difc_to_plot + tzero_to_plot)

            ws1 = CreateWorkspace(DataX=x_val,
                                  DataY=y_val,
                                  UnitX="Expected Peaks Centre (dSpacing A)",
                                  YUnitLabel="Fitted Peaks Centre(TOF, us)")
            ws2 = CreateWorkspace(DataX=x_val, DataY=y2_val)

            output_ws = "engggui_difc_zero_peaks_bank_" + str(i)
            if Ads.doesExist(output_ws):
                DeleteWorkspace(output_ws)

            AppendSpectra(ws1, ws2, OutputWorkspace=output_ws)
            DeleteWorkspace(ws1)
            DeleteWorkspace(ws2)

            difc_zero_ws = Ads.retrieve(output_ws)
            # Create plot
            difc_zero_plot = plot([difc_zero_ws], [0, 1],
                                  plot_kwargs={
                                      "linestyle": "--",
                                      "marker": "o",
                                      "markersize": "3"
                                  })
            difc_zero_plot.gca().set_title("Engg Gui Difc Zero Peaks Bank " + str(i))
            difc_zero_plot.gca().legend(("Peaks Fitted", "DifC/TZero Fitted Straight Line"))
            difc_zero_plot.gca().set_xlabel("Expected Peaks Centre(dSpacing, A)")

    def load_ceria(self, ceria_run_no):
        try:
            return Load(Filename=ceria_run_no, OutputWorkspace="engggui_calibration_sample_ws")
        except Exception as e:
            logger.error("Error while loading calibration sample data. "
                         "Could not run the algorithm Load succesfully for the calibration sample "
                         "(run number: " + str(ceria_run_no) + "). Error description: " + str(e) +
                         " Please check also the previous log messages for details.")
            raise RuntimeError

    def run_calibration(self, ceria_ws, van_integration, van_curves):
        output = [None] * 2
        for i in range(2):
            table_name = self._generate_table_workspace_name(i)
            output[i] = EnggCalibrate(InputWorkspace=ceria_ws,
                                      VanIntegrationWorkspace=van_integration,
                                      VanCurvesWorkspace=van_curves,
                                      Bank=str(i + 1),
                                      FittedPeaks=table_name,
                                      OutputParametersTableName=table_name)
        return output

    def calculate_vanadium_correction(self, vanadium_run_no):
        try:
            Load(Filename=vanadium_run_no, OutputWorkspace=self.VANADIUM_INPUT_WORKSPACE_NAME)
        except Exception as e:
            logger.error("Error when loading vanadium sample data. "
                         "Could not run Load algorithm with vanadium run number: " +
                         str(vanadium_run_no) + ". Error description: " + str(e))
            raise RuntimeError
        EnggVanadiumCorrections(VanadiumWorkspace=self.VANADIUM_INPUT_WORKSPACE_NAME,
                                OutIntegrationWorkspace=self.INTEGRATED_WORKSPACE_NAME,
                                OutCurvesWorkspace=self.CURVES_WORKSPACE_NAME)
        Ads.remove(self.VANADIUM_INPUT_WORKSPACE_NAME)
        integrated_workspace = Ads.Instance().retrieve(self.INTEGRATED_WORKSPACE_NAME)
        curves_workspace = Ads.Instance().retrieve(self.CURVES_WORKSPACE_NAME)
        return integrated_workspace, curves_workspace

    def create_output_files(self, calibration_dir, difc, tzero, ceria_path, vanadium_path,
                            instrument):
        if not path.exists(calibration_dir):
            makedirs(calibration_dir)
        filename, vanadium_no, ceria_no = self._generate_output_file_name(vanadium_path,
                                                                          ceria_path,
                                                                          instrument,
                                                                          bank="all")
        # Both Banks
        file_path = calibration_dir + filename
        write_ENGINX_GSAS_iparam_file(file_path,
                                      difc,
                                      tzero,
                                      ceria_run=ceria_no,
                                      vanadium_run=vanadium_no)
        # North Bank
        file_path = calibration_dir + self._generate_output_file_name(
            vanadium_path, ceria_path, instrument, bank="north")[0]
        write_ENGINX_GSAS_iparam_file(file_path, [difc[0]], [tzero[0]],
                                      ceria_run=ceria_no,
                                      vanadium_run=vanadium_no,
                                      template_file="template_ENGINX_241391_236516_North_bank.prm",
                                      bank_names=["North"])
        # South Bank
        file_path = calibration_dir + self._generate_output_file_name(
            vanadium_path, ceria_path, instrument, bank="south")[0]
        write_ENGINX_GSAS_iparam_file(file_path, [difc[1]], [tzero[1]],
                                      ceria_run=ceria_no,
                                      vanadium_run=vanadium_no,
                                      template_file="template_ENGINX_241391_236516_South_bank.prm",
                                      bank_names=["South"])

    @staticmethod
    def _generate_table_workspace_name(bank_num):
        return "engggui_calibration_bank_" + str(bank_num + 1)

    @staticmethod
    def _generate_output_file_name(vanadium_path, ceria_path, instrument, bank):
        vanadium_no = path.splitext(basename(vanadium_path))[0].replace(instrument, '').lstrip('0')
        ceria_no = path.splitext(basename(ceria_path))[0].replace(instrument, '').lstrip('0')
        filename = instrument + "_" + vanadium_no + "_" + ceria_no + "_"
        if bank == "all":
            filename = filename + "all_banks.prm"
        elif bank == "north":
            filename = filename + "bank_North.prm"
        elif bank == "south":
            filename = filename + "bank_South.prm"
        else:
            raise ValueError("Invalid bank name entered")
        return filename, vanadium_no, ceria_no