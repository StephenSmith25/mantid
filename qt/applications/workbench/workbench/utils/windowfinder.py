# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +

from __future__ import (absolute_import, division, print_function, unicode_literals)

from qtpy.QtWidgets import QApplication

from mantidqt.project.encoderfactory import EncoderFactory
from workbench.app import MAIN_WINDOW_OBJECT_NAME
from workbench.plotting.globalfiguremanager import GlobalFigureManager


def get_main_window_widget():
    """
    Return the Workbench's MainWindow widget. If the widget cannot be found
    return None.
    :return workbench.app.mainwindow.MainWindow: Workbench's main window
    """
    for widget in QApplication.instance().topLevelWidgets():
        if widget.objectName() == MAIN_WINDOW_OBJECT_NAME:
            return widget


def find_plot_windows():
    """
    Finds the currently open plot windows and returns them in a dict
    :return: ObservableDict; of currently open plots
    """
    return GlobalFigureManager.figs


def find_all_windows_that_are_savable():
    """
    Finds all windows and then checks if they have an encoder, if they do return them in a list of windows and encoders.
    :return: List of Lists of Windows and Encoders; Window at index 0 and Encoder at index 1 in each sub-list.
    """
    # Get all top level widgets and check if they have an encoder
    list_of_windows_and_encoder = []

    windows = QApplication.topLevelWidgets()
    for window in windows:
        encoder = EncoderFactory.find_encoder(window)
        if encoder is not None:
            list_of_windows_and_encoder.append((window, encoder))
    return list_of_windows_and_encoder
