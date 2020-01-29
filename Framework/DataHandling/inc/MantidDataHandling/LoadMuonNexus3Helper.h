// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2007 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +
#ifndef MANTID_DATAHANDLING_LOADMUONNEXUS3HELPER_H_
#define MANTID_DATAHANDLING_LOADMUONNEXUS3HELPER_H_

#include "MantidNexus/NexusClasses.h"

namespace Mantid {
namespace DataHandling {
namespace LoadMuonNexus3Helper {

NeXus::NXInt loadGoodFramesData(const NeXus::NXEntry &entry,
                                bool isFileMultiPeriod);
}
} // namespace DataHandling
} // namespace Mantid

#endif
