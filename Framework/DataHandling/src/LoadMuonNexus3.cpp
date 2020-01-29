// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2020 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +
#include "MantidDataHandling/LoadMuonNexus3.h"
#include "MantidDataHandling/LoadISISNexus2.h"
#include "MantidDataHandling/LoadMuonNexus3Helper.h"

#include "MantidAPI/FileProperty.h"
#include "MantidAPI/RegisterFileLoader.h"
#include "MantidAPI/WorkspaceFactory.h"
#include "MantidAPI/WorkspaceGroup.h"
#include "MantidDataHandling/ISISRunLogs.h"
#include "MantidDataObjects/Workspace2D.h"

#include "MantidKernel/ArrayProperty.h"
#include "MantidKernel/BoundedValidator.h"
#include "MantidKernel/ConfigService.h"

// clang-format off
#include <nexus/NeXusException.hpp>
// clang-format on

#include <functional>
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
using namespace DataObjects;

/// Empty default constructor
LoadMuonNexus3::LoadMuonNexus3()
    : m_filename(), m_instrumentName(), m_sampleName(),
      m_isFileMultiPeriod(false), m_multiPeriodsLoaded(false) {}

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
  Workspace_sptr outWS = getProperty("OutputWorkspace");

  // Open the nexus entry
  m_entrynumber = getProperty("EntryNumber");
  m_filename = getPropertyValue("Filename");
  // Create the root Nexus class
  NXRoot root(m_filename);
  // Open the raw data group 'raw_data_1'
  NXEntry entry = root.openEntry("raw_data_1");
  // What do we have to do next? At this point we have all the workspaces loaded
  // If its multi period we have a workspace group
  isEntryMultiPeriod(entry);

  // DO lots of stuff to this workspace
  if (m_multiPeriodsLoaded) {
    WorkspaceGroup_sptr wksp_grp =
        boost::dynamic_pointer_cast<WorkspaceGroup>(outWS);
    addGoodFrames(wksp_grp, entry);
    LoadMuonNexus3Helper::loadGoodFramesData(entry, true);
  } else {
    // we just have a single workspace
    Workspace2D_sptr workspace2D =
        boost::dynamic_pointer_cast<DataObjects::Workspace2D>(outWS);
    addGoodFrames(workspace2D, entry);
  }
}
// Determine whether file is multi period, and whether we have more than 1
// period loaded
void LoadMuonNexus3::isEntryMultiPeriod(const NXEntry &entry) {
  NXClass periodClass = entry.openNXGroup("periods");
  int numberOfPeriods = periodClass.getInt("number");
  if (numberOfPeriods > 1) {
    m_isFileMultiPeriod = true;
    if (m_entrynumber == 0) {
      m_multiPeriodsLoaded = true;
    }
  } else {
    m_isFileMultiPeriod = false;
    m_multiPeriodsLoaded = false;
  }
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
 * Loads the good frames assuming we have just one workspace loaded
 * for a workspace 2d object
 */
void LoadMuonNexus3::addGoodFrames(
    DataObjects::Workspace2D_sptr &localWorkspace, const NXEntry &entry) {

  auto &run = localWorkspace->mutableRun();
  run.removeProperty("goodfrm");
  NXInt goodframes =
      LoadMuonNexus3Helper::loadGoodFramesData(entry, m_isFileMultiPeriod);
  if (m_isFileMultiPeriod) {
    run.addProperty("goodfrm", goodframes[static_cast<int>(m_entrynumber - 1)]);
  } else {
    run.addProperty("goodfrm", goodframes[0]);
  }
}
// Overload #2 Takes in a workspace_group if we have loaded multiple periods
void LoadMuonNexus3::addGoodFrames(WorkspaceGroup_sptr &workspaceGroup,
                                   const NXEntry &entry) {

  NXInt goodframes =
      LoadMuonNexus3Helper::loadGoodFramesData(entry, m_isFileMultiPeriod);

  // check there is a good frames entry for each period
  if (goodframes.dim0() < workspaceGroup->getNumberOfEntries()) {
    g_log.warning("Good frames data not available for each period loaded!\n");
    return;
  }

  auto workspaceList = workspaceGroup->getAllItems();
  for (auto it = workspaceList.begin(); it != workspaceList.end(); ++it) {
    auto index = std::distance(workspaceList.begin(), it);
    Workspace2D_sptr ws = boost::dynamic_pointer_cast<Workspace2D>(*it);
    auto &run = ws->mutableRun();
    run.removeProperty("goodfrm");
    run.addProperty("goodfrm", goodframes[static_cast<int>(index)]);
  }
}

} // namespace DataHandling
} // namespace Mantid
