// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2007 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +
#ifndef MANTID_DATAHANDLING_LoadISISNexus22_H_
#define MANTID_DATAHANDLING_LoadISISNexus22_H_

#include "MantidAPI/Algorithm.h"
#include "MantidAPI/IFileLoader.h"
#include "MantidAPI/SpectrumDetectorMapping.h"
#include "MantidDataHandling/DataBlockComposite.h"
#include "MantidDataHandling/ISISRunLogs.h"
#include "MantidDataObjects/TableWorkspace.h"
#include "MantidDataObjects/Workspace2D.h"
#include "MantidNexus/NexusClasses.h"
#include <nexus/NeXusFile.hpp>

#include <boost/scoped_ptr.hpp>

#include <climits>

namespace Mantid {
namespace DataHandling {
/**

Loads a file in a NeXus format and stores it in a 2D workspace. LoadISISNexus2
is an algorithm and
as such inherits  from the Algorithm class, via DataHandlingCommand, and
overrides
the init() & exec() methods.

Required Properties:
<UL>
<LI> Filename - The name of and path to the input NeXus file </LI>
<LI> OutputWorkspace - The name of the workspace in which to store the imported
data
(a multi-period file will store higher periods in workspaces called
OutputWorkspace_PeriodNo)</LI>
</UL>

Optional Properties: (note that these options are not available if reading a
multi-period file)
<UL>
<LI> SpectrumMin  - The  starting spectrum number</LI>
<LI> SpectrumMax  - The  final spectrum number (inclusive)</LI>
<LI> SpectrumList - An ArrayProperty of spectra to load</LI>
</UL>

@author Stephen Smith, ISIS
*/
class DLLExport LoadMuonNexus3
    : public API::IFileLoader<Kernel::NexusDescriptor> {
public:
  // Default constructor
  LoadMuonNexus3();
  /// Algorithm's name for identification overriding a virtual method
  const std::string name() const override { return "LoadMuonNexus3"; }
  /// Summary of algorithms purpose
  const std::string summary() const override {
    return "Loads a Muon Nexus V2 data file and stores it in a 2D "
           "workspace (Workspace2D class).";
  }
  /// Returns a confidence value that this algorithm can load a file
  int confidence(Kernel::NexusDescriptor &descriptor) const override;
  // Version
  int version() const override { return 3; }

  /// Algorithm's category for identification overriding a virtual method
  const std::string category() const override { return "DataHandling\\Nexus"; }

private:
  /// Overwrites Algorithm method.
  void init() override;
  /// Overwrites Algorithm method
  void exec() override;
  void checkOptionalProperties();
  // Run child algorithm LoadISISNexus3
  void runLoadISISNexus();
  /// Loads dead time table for the detector
  void loadDeadTimes(Mantid::NeXus::NXRoot &root) const;
  // create the dead time table
  DataObjects::TableWorkspace_sptr
  createDeadTimeTable(const std::vector<int> &specToLoad,
                      const std::vector<double> &deadTimes);
  // Load the detector grouping
  API::Workspace_sptr
  loadDetectorGrouping(Mantid::NeXus::NXRoot &root,
                       Mantid::Geometry::Instrument_const_sptr inst) const;

  /// The name and path of the input file
  std::string m_filename;
  /// The instrument name from Nexus
  std::string m_instrumentName;
  /// The sample name read from Nexus
  std::string m_sampleName;
  /// The number of the input entry
  int64_t m_entrynumber;
  /// The number of spectra in the raw file
  int64_t m_numberOfSpectra;
  /// The number of periods in the raw file
  int64_t m_numberOfPeriods;
  /// Has the spectrum_list property been set?
  bool m_list;
  /// Have the spectrum_min/max properties been set?
  bool m_interval;
  /// The value of the spectrum_list property
  std::vector<specnum_t> m_specList;
  /// The value of the spectrum_min property
  int64_t m_specMin;
  /// The value of the spectrum_max property
  int64_t m_specMax;
  /// The group which each detector belongs to in order
  std::vector<specnum_t> m_groupings;
};

} // namespace DataHandling
} // namespace Mantid

#endif