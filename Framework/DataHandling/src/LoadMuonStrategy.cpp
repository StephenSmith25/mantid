
// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2020 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +

#include "MantidDataHandling/LoadMuonStrategy.h"
#include "MantidAPI/TableRow.h"
#include "MantidAPI/WorkspaceFactory.h"

#include <boost/scoped_ptr.hpp>

#include <vector>

namespace Mantid {
namespace DataHandling {

// Constructor
LoadMuonStrategy::LoadMuonStrategy(Kernel::Logger &g_log,
                                   const std::string &filename)
    : m_logger(g_log), m_filename(filename) {}


/**
 * Creates Detector Grouping Table .
 * @param detectorsLoaded :: Vector containing the list of detectorsLoaded
 * @param grouping :: Vector containing corresponding grouping
 * @return Detector Grouping Table create using the data
 */
DataObjects::TableWorkspace_sptr
LoadMuonStrategy::createDetectorGroupingTable(std::vector<detid_t> detectorsLoaded,
                            std::vector<detid_t> grouping) {
  auto detectorGroupingTable =
      boost::dynamic_pointer_cast<DataObjects::TableWorkspace>(
          API::WorkspaceFactory::Instance().createTable("TableWorkspace"));

  detectorGroupingTable->addColumn("vector_int", "Detectors");

  std::map<detid_t, std::vector<detid_t>> groupingMap;
  for (size_t i = 0; i < detectorsLoaded.size(); ++i) {
    // Add detector ID to the list of group detectors. Detector ID is always
    groupingMap[grouping[i]].emplace_back(detectorsLoaded[i]);
  }

  for (auto &group : groupingMap) {
    if (group.first != 0) { // Skip 0 group
      API::TableRow newRow = detectorGroupingTable->appendRow();
      newRow << group.second;
    }
  }
  return detectorGroupingTable;
}
/**
 * Creates Detector Deadtimes Table .
 * @param detectorsLoaded :: Vector containing the list of detectorsLoaded
 * @param deadTimes :: Vector containing corresponding grouping
 * @return Detector Grouping Table create using the data
 */
DataObjects::TableWorkspace_sptr
LoadMuonStrategy::createDeadTimeTable(std::vector<detid_t> detectorsLoaded,
                    std::vector<double> deadTimes) const{
  auto deadTimesTable =
      boost::dynamic_pointer_cast<DataObjects::TableWorkspace>(
          API::WorkspaceFactory::Instance().createTable("TableWorkspace"));

  deadTimesTable->addColumn("int", "spectrum");
  deadTimesTable->addColumn("double", "dead-time");

  for (size_t i = 0; i < detectorsLoaded.size(); i++) {
    API::TableRow row = deadTimesTable->appendRow();
    row << detectorsLoaded[i] << deadTimes[i];
  }

  return deadTimesTable;
}


} // namespace DataHandling
} // namespace Mantid
