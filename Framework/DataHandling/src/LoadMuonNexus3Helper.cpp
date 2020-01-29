
// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2020 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +
#include "MantidDataHandling/LoadMuonNexus3Helper.h"
#include <iostream>

namespace Mantid {
namespace DataHandling {
namespace LoadMuonNexus3Helper {

using namespace NeXus;

// Loads the good frames from the Muon Nexus V2 entry
NXInt loadGoodFramesData(const NXEntry &entry, bool isFileMultiPeriod) {

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
} // namespace LoadMuonNexus3Helper
} // namespace DataHandling
} // namespace Mantid