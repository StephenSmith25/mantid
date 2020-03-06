// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +
//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include "MantidDataHandling/LoadMuonNexus3.h"
#include "MantidDataHandling/DataBlockGenerator.h"
#include "MantidDataHandling/LoadEventNexus.h"
#include "MantidDataHandling/LoadISISNexus2.h"
#include "MantidDataHandling/LoadISISNexusHelper.h"
#include "MantidDataHandling/LoadRawHelper.h"

#include "MantidAPI/Axis.h"
#include "MantidAPI/FileProperty.h"
#include "MantidAPI/RegisterFileLoader.h"
#include "MantidAPI/WorkspaceFactory.h"
#include "MantidGeometry/Instrument.h"
#include "MantidGeometry/Instrument/Detector.h"
#include "MantidKernel/ArrayProperty.h"
#include "MantidKernel/BoundedValidator.h"
#include "MantidKernel/ConfigService.h"
#include "MantidKernel/ListValidator.h"
//#include "MantidKernel/LogParser.h"
#include "MantidKernel/LogFilter.h"
#include "MantidKernel/TimeSeriesProperty.h"
#include "MantidKernel/UnitFactory.h"

// clang-format off
#include <nexus/NeXusFile.hpp>
#include <nexus/NeXusException.hpp>
// clang-format on

#include <algorithm>
#include <cmath>
#include <cctype>
#include <climits>
#include <functional>
#include <sstream>
#include <vector>

#include <iostream>
namespace Mantid {
namespace DataHandling {

DECLARE_NEXUS_FILELOADER_ALGORITHM(LoadMuonNexus3)

using namespace Kernel;
using namespace API;
using namespace NeXus;
using namespace HistogramData;
using std::size_t;

/// Empty default constructor
LoadMuonNexus3::LoadMuonNexus3()
    : m_filename(), m_instrumentName(), m_sampleName() {}

/**
 * Return the confidence criteria for this algorithm can load the file
 * @param descriptor A descriptor for the file
 * @returns An integer specifying the confidence level. 0 indicates it will not
 * be used
 */
int LoadMuonNexus3::confidence(Kernel::NexusDescriptor &descriptor) const {
  if (descriptor.pathOfTypeExists("/raw_data_1", "NXentry")) {
    // It also could be an Event Nexus file or a TOFRaw file,
    // so confidence is set to less than 80.
    return 75;
  }
  return 0;
}
/// Initialization method.
void LoadMuonNexus3::init() {
  declareProperty(std::make_unique<FileProperty>("Filename", "",
                                                 FileProperty::Load, ".nxs"),
                  "The name of the Nexus file to load");

  declareProperty(
      std::make_unique<WorkspaceProperty<Workspace>>("OutputWorkspace", "",
                                                     Direction::Output),
      "The name of the workspace to be created as the output of the\n"
      "algorithm. For multiperiod files, one workspace will be\n"
      "generated for each period");

  auto mustBePositive = boost::make_shared<BoundedValidator<int64_t>>();
  mustBePositive->setLower(0);
  declareProperty("SpectrumMin", static_cast<int64_t>(0), mustBePositive);
  declareProperty("SpectrumMax", static_cast<int64_t>(EMPTY_INT()),
                  mustBePositive);
  declareProperty(std::make_unique<ArrayProperty<int64_t>>("SpectrumList"));
  declareProperty("EntryNumber", static_cast<int64_t>(0), mustBePositive,
                  "0 indicates that every entry is loaded, into a separate "
                  "workspace within a group. "
                  "A positive number identifies one entry to be loaded, into "
                  "one worskspace");

  declareProperty(
      std::make_unique<WorkspaceProperty<Workspace>>(
          "DeadTimeTable", "", Direction::Output, PropertyMode::Optional),
      "Table or a group of tables containing detector dead times. Version 1 "
      "only.");

  declareProperty(
      std::make_unique<WorkspaceProperty<Workspace>>("DetectorGroupingTable",
                                                     "", Direction::Output,
                                                     PropertyMode::Optional),
      "Table or a group of tables with information about the "
      "detector grouping stored in the file (if any). Version 1 only.");
}

/// Validates the optional 'spectra to read' properties, if they have been set
void LoadMuonNexus3::checkOptionalProperties() {
  // read in the settings passed to the algorithm
  m_specList = getProperty("SpectrumList");
  m_specMax = getProperty("SpectrumMax");
  // Are we using a list of spectra or all the spectra in a range?
  m_list = !m_specList.empty();
  m_interval = (m_specMax != EMPTY_INT());
  if (m_specMax == EMPTY_INT())
    m_specMax = 0;

  // Check validity of spectra list property, if set
  if (m_list) {
    const specnum_t minlist =
        *min_element(m_specList.begin(), m_specList.end());
    const specnum_t maxlist =
        *max_element(m_specList.begin(), m_specList.end());
    if (maxlist > m_numberOfSpectra || minlist == 0) {
      g_log.error("Invalid list of spectra");
      throw std::invalid_argument("Inconsistent properties defined");
    }
  }

  // Check validity of spectra range, if set
  if (m_interval) {
    m_specMin = getProperty("SpectrumMin");
    if (m_specMax < m_specMin || m_specMax > m_numberOfSpectra) {
      g_log.error("Invalid Spectrum min/max properties");
      throw std::invalid_argument("Inconsistent properties defined");
    }
  }
}

void LoadMuonNexus3::exec() {

  // we need to execute the child algorithm LoadISISNexus2, as this
  // will do the majority of the loading for us
  runLoadISISNexus();

  // What do we have to do next? At this point we have all the workspaces loaded
  // If its multi period we have a workspace group
  API::Workspace_sptr outWS = getProperty("OutputWorkspace");
  outWS->setTitle("chaning the workspace title");
}
/**
 * Runs the child algorithm LoadISISNexus, which loads data into an output
 * workspace
 */
void LoadMuonNexus3::runLoadISISNexus() {
  IAlgorithm_sptr childAlg =
      createChildAlgorithm("LoadISISNexus", 0, 1, true, 2);
  declareProperty("LoadMonitors", "Exclude"); // we need to set this property

  auto ISISLoader = boost::dynamic_pointer_cast<API::Algorithm>(childAlg);
  ISISLoader->copyPropertiesFrom(*this);
  ISISLoader->executeAsChildAlg();
  this->copyPropertiesFrom(*ISISLoader);
}
/**
 * Loads dead time table for the detector.
 * @param root :: Root entry of the Nexus to read dead times from
 */
void LoadMuonNexus3::loadDeadTimes(NXRoot &root) const {
  // If dead times workspace name is empty - caller doesn't need dead times
  if (getPropertyValue("DeadTimeTable").empty())
    return;
  try {
    NXEntry deadTimes = root.openEntry("raw_data_1/detector_1/dead_time");
  } catch (...) {
  }
}
// Create dead time table
DataObjects::TableWorkspace_sptr
LoadMuonNexus3::createDeadTimeTable(const std::vector<int> &specToLoad,
                                    const std::vector<double> &deadTimes) {
  std::cout << "3" << std::endl;
}
/**
 * Loads detector grouping.
 * If no entry in NeXus file for grouping, load it from the IDF.
 * @param root :: Root entry of the Nexus file to read from
 * @param inst :: Pointer to instrument (to use if IDF needed)
 * @returns :: Grouping table - or tables, if per period
 */
Workspace_sptr LoadMuonNexus3::loadDetectorGrouping(
    NXRoot &root, Geometry::Instrument_const_sptr inst) const {

  NXEntry dataEntry = root.openEntry("run/histogram_data_1");
  NXInfo infoGrouping = dataEntry.getDataSetInfo("grouping");
}

} // namespace DataHandling
} // namespace Mantid
