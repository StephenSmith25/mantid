
// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2020 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +

// These functions handle the nexus operations needed to load
// the information from the Muon Nexus V2 file
#include "MantidDataHandling/LoadMuonNexus3Helper.h"
#include "MantidAPI/TableRow.h"
#include "MantidAPI/WorkspaceFactory.h"

#include <iostream>

namespace Mantid {
namespace DataHandling {
namespace LoadMuonNexus3Helper {

using namespace NeXus;
using namespace Kernel;
using namespace API;
using namespace NeXus;
using namespace HistogramData;
using std::size_t;
using namespace DataObjects;

// Loads the good frames from the Muon Nexus V2 entry
NXInt loadGoodFramesDataFromNexus(const NXEntry &entry,
                                  bool isFileMultiPeriod) {

  if (!isFileMultiPeriod) {
    try {
      NXInt goodFrames = entry.openNXInt("good_frames");
      goodFrames.load();
      return goodFrames;
    } catch (...) {
    }
  } else {
    try {
      NXClass periodClass = entry.openNXGroup("periods");
      // For multiperiod datasets, read raw_data_1/periods/good_frames
      NXInt goodFrames = periodClass.openNXInt("good_frames");
      goodFrames.load();
      return goodFrames;
    } catch (...) {
    }
  }
}
// Loads the detector grouping from the Muon Nexus V2 entry
std::tuple<std::vector<detid_t>, std::vector<detid_t>>
loadDetectorGroupingFromNexus(NXEntry &entry,
                              DataObjects::Workspace2D_sptr &localWorkspace,
                              bool isFileMultiPeriod) {

  int64_t numberOfSpectra =
      static_cast<int64_t>(localWorkspace->getNumberHistograms());

  // Open nexus entry
  NXClass detectorGroup = entry.openNXGroup("instrument/detector_1");

  if (detectorGroup.containsDataSet("grouping")) {
    NXInt groupingData = detectorGroup.openNXInt("grouping");
    groupingData.load();
    int numGroupingEntries = groupingData.dim0();

    std::vector<detid_t> detectorsLoaded;
    std::vector<detid_t> grouping;
    // Return the detectors which are loaded
    // then find the grouping ID for each detector
    for (int64_t spectraIndex = 0; spectraIndex < numberOfSpectra;
         spectraIndex++) {
      const auto detIdSet =
          localWorkspace->getSpectrum(spectraIndex).getDetectorIDs();
      for (auto detector : detIdSet) {
        detectorsLoaded.emplace_back(detector);
      }
    }
    if (!isFileMultiPeriod) {
      // Simplest case - one grouping entry per detector
      for (const auto &detectorNumber : detectorsLoaded) {
        grouping.emplace_back(groupingData[detectorNumber - 1]);
      }
    }

    return std::make_tuple(detectorsLoaded, grouping);
  }
}
std::string loadMainFieldDirectionFromNexus(const NeXus::NXEntry &entry) {
  std::string mainFieldDirection = "Longitudinal"; // default
  try {
    NXChar orientation =
        entry.openNXChar("run/instrument/detector/orientation");
    // some files have no data there
    orientation.load();
    if (orientation[0] == 't') {
      mainFieldDirection = "Transverse";
    }
  } catch (...) {
    // no data - assume main field was longitudinal
  }

  return mainFieldDirection;
}
std::tuple<std::vector<detid_t>, std::vector<double>>
loadDeadTimesFromNexus(const NeXus::NXEntry &entry,
                       const DataObjects::Workspace2D_sptr &localWorkspace,
                       const bool isFileMultiPeriod) {

  int64_t numberOfSpectra =
      static_cast<int64_t>(localWorkspace->getNumberHistograms());

  std::vector<detid_t> loadedDetectors;
  std::vector<double> deadTimes;
  // Open detector nexus entry
  NXClass detectorGroup = entry.openNXGroup("instrument/detector_1");

  if (detectorGroup.containsDataSet("dead_time")) {
    NXFloat deadTimesData = detectorGroup.openNXFloat("dead_time");
    deadTimesData.load();
    int numGroupingEntries = deadTimesData.dim0();

    // Return the detectors which are loaded
    // then find the grouping ID for each detector
    for (int64_t spectraIndex = 0; spectraIndex < numberOfSpectra;
         spectraIndex++) {
      const auto detIdSet =
          localWorkspace->getSpectrum(spectraIndex).getDetectorIDs();
      for (auto detector : detIdSet) {
        loadedDetectors.emplace_back(detector);
      }
    }
    if (!isFileMultiPeriod) {
      // Simplest case - one grouping entry per detector
      for (const auto &detectorNumber : loadedDetectors) {
        deadTimes.emplace_back(deadTimesData[detectorNumber - 1]);
      }
    }
  }
  return std::make_tuple(loadedDetectors, deadTimes);
}

double loadFirstGoodDataFromNexus(const NeXus::NXEntry &entry) {
  try {
    NXClass detectorEntry = entry.openNXGroup("instrument/detector_1");
    NXInfo infoResolution = detectorEntry.getDataSetInfo("resolution");
    NXInt counts = detectorEntry.openNXInt("counts");
    std::string firstGoodBin = counts.attributes("first_good_bin");
    if (!firstGoodBin.empty()) {
      double resolution;
      switch (infoResolution.type) {
      case NX_FLOAT32:
        resolution = static_cast<double>(detectorEntry.getFloat("resolution"));
        break;
      case NX_INT32:
        resolution = static_cast<double>(detectorEntry.getInt("resolution"));
        break;
      default:
        throw std::runtime_error("Unsupported data type for resolution");
      }
      double bin = static_cast<double>(boost::lexical_cast<int>(firstGoodBin));
      double bin_size = resolution / 1000000.0;
      return bin * bin_size;
    }
  } catch (std::exception &e) {
    throw e;
  }
}
} // namespace LoadMuonNexus3Helper
} // namespace DataHandling
} // namespace Mantid