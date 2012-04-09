#ifndef SLICEVIEWER_H
#define SLICEVIEWER_H

#include "ColorBarWidget.h"
#include "DimensionSliceWidget.h"
#include "DllOption.h"
#include "MantidAPI/IMDIterator.h"
#include "MantidAPI/IMDWorkspace.h"
#include "MantidGeometry/MDGeometry/MDHistoDimension.h"
#include "MantidKernel/VMD.h"
#include "MantidQtAPI/MantidColorMap.h"
#include "MantidQtMantidWidgets/SafeQwtPlot.h"
#include "MantidQtAPI/SyncedCheckboxes.h"
#include "MantidQtSliceViewer/LineOverlay.h"
#include "QwtRasterDataMD.h"
#include "ui_SliceViewer.h"
#include <QtCore/QtCore>
#include <QtGui/qdialog.h>
#include <QtGui/QWidget>
#include <qwt_color_map.h>
#include <qwt_plot_spectrogram.h>
#include <qwt_plot.h>
#include <qwt_raster_data.h>
#include <qwt_scale_widget.h>
#include <vector>
#include <Poco/NObserver.h>
#include "MantidAPI/Algorithm.h"

namespace MantidQt
{
namespace SliceViewer
{

/** GUI for viewing a 2D slice out of a multi-dimensional workspace.
 * You can select which dimension to plot as X,Y, and the cut point
 * along the other dimension(s).
 *
 */
class EXPORT_OPT_MANTIDQT_SLICEVIEWER SliceViewer : public QWidget
{
  friend class SliceViewerWindow;

  Q_OBJECT

public:
  SliceViewer(QWidget *parent = 0);
  ~SliceViewer();

  void setWorkspace(const QString & wsName);
  void setWorkspace(Mantid::API::IMDWorkspace_sptr ws);
  Mantid::API::IMDWorkspace_sptr getWorkspace();
  void showControls(bool visible);
  void zoomBy(double factor);
  void loadColorMap(QString filename = QString() );
  LineOverlay * getLineOverlay() { return m_lineOverlay; }
  Mantid::Kernel::VMD getSlicePoint() const { return m_slicePoint; }
  int getDimX() const;
  int getDimY() const;

  /// Methods for Python bindings
  QString getWorkspaceName() const;
  void setXYDim(int indexX, int indexY);
  void setXYDim(const QString & dimX, const QString & dimY);
  void setSlicePoint(int dim, double value);
  void setSlicePoint(const QString & dim, double value);
  double getSlicePoint(int dim) const;
  double getSlicePoint(const QString & dim) const;
  void setColorScaleMin(double min);
  void setColorScaleMax(double max);
  void setColorScaleLog(bool log);
  void setColorScale(double min, double max, bool log);
  void setColorMapBackground(int r, int g, int b);
  double getColorScaleMin() const;
  double getColorScaleMax() const;
  bool getColorScaleLog() const;
  bool getFastRender() const;
  void setXYLimits(double xleft, double xright, double ybottom, double ytop);
  QwtDoubleInterval getXLimits() const;
  QwtDoubleInterval getYLimits() const;
  void setXYCenter(double x, double y);
  void openFromXML(const QString & xml);
  void toggleLineMode(bool);
  void setNormalization(Mantid::API::MDNormalization norm);
  Mantid::API::MDNormalization getNormalization() const;

signals:
  /// Signal emitted when the X/Y index of the shown dimensions is changed
  void changedShownDim(size_t dimX, size_t dimY);
  /// Signal emitted when the slice point moves
  void changedSlicePoint(Mantid::Kernel::VMD slicePoint);
  /// Signal emitted when the LineViewer should be shown/hidden.
  void showLineViewer(bool);
  /// Signal emitted when someone uses setWorkspace() on SliceViewer
  void workspaceChanged();
  /// Signal emitted when the dynamic rebinning call is complete.
  void dynamicRebinComplete();

public slots:
  void changedShownDim(int index, int dim, int oldDim);
  void resetZoom();
  void setXYLimitsDialog();
  void showInfoAt(double, double);
  void colorRangeChanged();
  void zoomInSlot();
  void zoomOutSlot();
  void updateDisplaySlot(int index, double value);
  void loadColorMapSlot();
  void helpSliceViewer();
  void helpLineViewer();
  void setColorScaleAutoFull();
  void setColorScaleAutoSlice();
  void setTransparentZeros(bool transparent);
  void setFastRender(bool fast);
  void changeNormalization();
  // Slots that will be automatically connected via QMetaObject.connectSlotsByName
  void on_btnClearLine_clicked();
  QPixmap getImage();
  void saveImage(const QString & filename = QString());
  void copyImageToClipboard();

  // Synced checkboxes
  void LineMode_toggled(bool);
  void SnapToGrid_toggled(bool);
  void RebinMode_toggled(bool);

  void rebinParamsChanged();
  void dynamicRebinCompleteSlot();


private:
  void loadSettings();
  void saveSettings();
  void initMenus();
  void initZoomer();

  void updateDisplay(bool resetAxes = false);
  void updateDimensionSliceWidgets();
  void resetAxis(int axis, Mantid::Geometry::IMDDimension_const_sptr dim);
  QwtDoubleInterval getRange(Mantid::API::IMDIterator * it);
  QwtDoubleInterval getRange(std::vector<Mantid::API::IMDIterator *> iterators);

  void findRangeFull();
  void findRangeSlice();

protected:

  /// Algorithm notification handlers
  void handleAlgorithmFinishedNotification(const Poco::AutoPtr<Mantid::API::Algorithm::FinishedNotification>& pNf);
  Poco::NObserver<SliceViewer, Mantid::API::Algorithm::FinishedNotification> m_finishedObserver;

  void handleAlgorithmProgressNotification(const Poco::AutoPtr<Mantid::API::Algorithm::ProgressNotification>& pNf);
  Poco::NObserver<SliceViewer, Mantid::API::Algorithm::ProgressNotification> m_progressObserver;

  void handleAlgorithmErrorNotification(const Poco::AutoPtr<Mantid::API::Algorithm::ErrorNotification>& pNf);
  Poco::NObserver<SliceViewer, Mantid::API::Algorithm::ErrorNotification> m_errorObserver;

private:
  // -------------------------- Widgets ----------------------------

  /// Auto-generated UI controls.
  Ui::SliceViewerClass ui;

  /// Main plot object
  MantidQt::MantidWidgets::SafeQwtPlot * m_plot;

  /// Spectrogram plot
  QwtPlotSpectrogram * m_spect;

  /// Layout containing the spectrogram
  QHBoxLayout * m_spectLayout;

  /// Color bar indicating the color scale
  ColorBarWidget * m_colorBar;

  /// Vector of the widgets for slicing dimensions
  std::vector<DimensionSliceWidget *> m_dimWidgets;

  /// The LineOverlay widget for drawing line cross-sections (hidden at startup)
  LineOverlay * m_lineOverlay;



  // -------------------------- Data Members ----------------------------

  /// Workspace being shown
  Mantid::API::IMDWorkspace_sptr m_ws;

  /// Workspace overlaid on top of original (optional) for dynamic rebinning
  Mantid::API::IMDWorkspace_sptr m_overlayWS;

  /// Set to true once the first workspace has been loaded in it
  bool m_firstWorkspaceOpen;

  /// File of the last loaded color map.
  QString m_currentColorMapFile;

  /// Vector of the dimensions to show.
  std::vector<Mantid::Geometry::MDHistoDimension_sptr> m_dimensions;

  /// Data presenter
  QwtRasterDataMD * m_data;

  /// The X and Y dimensions being plotted
  Mantid::Geometry::IMDDimension_const_sptr m_X;
  Mantid::Geometry::IMDDimension_const_sptr m_Y;
  size_t m_dimX;
  size_t m_dimY;

  /// The point of slicing in the other dimensions
  Mantid::Kernel::VMD m_slicePoint;

  /// The range of values to fit in the color map.
  QwtDoubleInterval m_colorRange;

  /// The calculated range of values in the FULL data set
  QwtDoubleInterval m_colorRangeFull;

  /// The calculated range of values ONLY in the currently viewed part of the slice
  QwtDoubleInterval m_colorRangeSlice;

  /// Use the log of the value for the color scale
  bool m_logColor;

  /// Menus
  QMenu *m_menuColorOptions, *m_menuView, *m_menuHelp, *m_menuLine, *m_menuFile;
  QAction *m_actionFileClose;
  QAction *m_actionTransparentZeros;
  QAction *m_actionNormalizeNone;
  QAction *m_actionNormalizeVolume;
  QAction *m_actionNormalizeNumEvents;

  /// Synced menu/buttons
  MantidQt::API::SyncedCheckboxes *m_syncLineMode, *m_syncSnapToGrid, *m_syncRebinMode;

  /// Cached double for infinity
  double m_inf;

  /// "Fast" rendering mode
  bool m_fastRender;

  /// Last path that was saved using saveImage()
  QString m_lastSavedFile;

  /// For the asynchronous call in dynamic rebinning. Holds the result of the async BinMD call
  Poco::ActiveResult<bool> * m_asyncRebinResult;

  /// Reference to the BinMD algorithm executing asynchronously.
  Mantid::API::IAlgorithm_sptr m_asyncRebinAlg;

  /// Name of the workspace generated by the dynamic rebinning BinMD call
  std::string m_overlayWSName;
};

} // namespace SliceViewer
} // namespace Mantid

#endif // SLICEVIEWER_H
