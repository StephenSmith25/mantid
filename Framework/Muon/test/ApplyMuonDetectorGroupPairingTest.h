#ifndef MANTID_MUON_APPLYMUONDETECTORGROUPPAIRINGTEST_H_
#define MANTID_MUON_APPLYMUONDETECTORGROUPPAIRINGTEST_H_

#include <cxxtest/TestSuite.h>

#include "MantidAPI/FrameworkManager.h"
#include "MantidAPI/MatrixWorkspace.h"
#include "MantidAPI/ScopedWorkspace.h"
#include "MantidAPI/TableRow.h"
#include "MantidAPI/Workspace.h"
#include "MantidAPI/WorkspaceGroup.h"
#include "MantidAlgorithms/CompareWorkspaces.h"
#include "MantidDataHandling/LoadMuonNexus2.h"
#include "MantidDataObjects/TableWorkspace.h"
#include "MantidKernel/PhysicalConstants.h"
#include "MantidMuon/ApplyMuonDetectorGroupPairing.h"
#include "MantidTestHelpers/ComponentCreationHelper.h"
#include "MantidTestHelpers/WorkspaceCreationHelper.h"

using namespace Mantid;
using namespace Mantid::Kernel;
using namespace Mantid::API;
using namespace Mantid::Algorithms;
using namespace Mantid::DataObjects;
using namespace Mantid::Muon;

namespace {

// Create fake muon data with exponential decay
struct yDataAsymmetry {
  double operator()(const double time, size_t specNum) {
    double amplitude = (static_cast<double>(specNum) + 1) * 10.;
    double tau = Mantid::PhysicalConstants::MuonLifetime *
                 1e6; // Muon life time in microseconds
    return (20. * (1.0 + amplitude * exp(-time / tau)));
  }
};

struct yDataCounts {
  yDataCounts() : m_count(-1) {}
  int m_count;
  double operator()(const double, size_t) {
    m_count++;
    return static_cast<double>(m_count);
  }
};

struct eData {
  double operator()(const double, size_t) { return 0.005; }
};

/**
 * Create a matrix workspace appropriate for Group Asymmetry. One detector per
 * spectra, numbers starting from 1. The detector ID and spectrum number are
 * equal.
 * @param nspec :: The number of spectra
 * @param maxt ::  The number of histogram bin edges (between 0.0 and 1.0).
 * Number of bins = maxt - 1 .
 * @return Pointer to the workspace.
 */
MatrixWorkspace_sptr createAsymmetryWorkspace(size_t nspec, size_t maxt) {

  MatrixWorkspace_sptr ws =
      WorkspaceCreationHelper::create2DWorkspaceFromFunction(
          yDataAsymmetry(), static_cast<int>(nspec), 0.0, 1.0,
          (1.0 / static_cast<double>(maxt)), true, eData());

  ws->setInstrument(ComponentCreationHelper::createTestInstrumentCylindrical(
      static_cast<int>(nspec)));

  for (int g = 0; g < static_cast<int>(nspec); g++) {
    auto &spec = ws->getSpectrum(g);
    spec.addDetectorID(g + 1);
    spec.setSpectrumNo(g + 1);
  }

  // Add number of good frames (required for Asymmetry calculation)
  ws->mutableRun().addProperty("goodfrm", 10);

  return ws;
}

/**
 * Create a matrix workspace appropriate for Group Counts. One detector per
 * spectra, numbers starting from 1. The detector ID and spectrum number are
 * equal. Y values increase from 0 in integer steps.
 * @param nspec :: The number of spectra
 * @param maxt ::  The number of histogram bin edges (between 0.0 and 1.0).
 * Number of bins = maxt - 1 .
 * @param seed :: Number added to all y-values.
 * @return Pointer to the workspace.
 */
MatrixWorkspace_sptr createCountsWorkspace(size_t nspec, size_t maxt,
                                           double seed) {

  MatrixWorkspace_sptr ws =
      WorkspaceCreationHelper::create2DWorkspaceFromFunction(
          yDataCounts(), static_cast<int>(nspec), 0.0, 1.0,
          (1.0 / static_cast<double>(maxt)), true, eData());

  ws->setInstrument(ComponentCreationHelper::createTestInstrumentCylindrical(
      static_cast<int>(nspec)));

  for (int g = 0; g < static_cast<int>(nspec); g++) {
    auto &spec = ws->getSpectrum(g);
    spec.addDetectorID(g + 1);
    spec.setSpectrumNo(g + 1);
    ws->mutableY(g) += seed;
  }

  return ws;
}

/**
 * Create a WorkspaceGroup and add to the ADS, populate with MatrixWorkspaces
 * simulating periods as used in muon analysis. Workspace for period i has a
 * name ending _i.
 * @param nPeriods :: The number of periods (independent workspaces)
 * @param maxt ::  The number of histogram bin edges (between 0.0 and 1.0).
 * Number of bins = maxt - 1 .
 * @param wsGroupName :: Name of the workspace group containing the period
 * workspaces.
 * @return Pointer to the workspace group.
 */
WorkspaceGroup_sptr
createMultiPeriodWorkspaceGroup(const int &nPeriods, size_t nspec, size_t maxt,
                                const std::string &wsGroupName) {

  WorkspaceGroup_sptr wsGroup = boost::make_shared<WorkspaceGroup>();
  AnalysisDataService::Instance().addOrReplace(wsGroupName, wsGroup);

  std::string wsNameStem = "MuonDataPeriod_";
  std::string wsName;

  for (int period = 1; period < nPeriods + 1; period++) {
    // Period 1 yvalues : 1,2,3,4,5,6,7,8,9,10
    // Period 2 yvalues : 2,3,4,5,6,7,8,9,10,11 etc..
    MatrixWorkspace_sptr ws = createCountsWorkspace(nspec, maxt, period);
    wsGroup->addWorkspace(ws);
    wsName = wsNameStem + std::to_string(period);
    AnalysisDataService::Instance().addOrReplace(wsName, ws);
  }

  return wsGroup;
}

ITableWorkspace_sptr createDeadTimeTable(const size_t &nspec,
                                         std::vector<double> &deadTimes) {

  auto deadTimeTable = boost::dynamic_pointer_cast<ITableWorkspace>(
      WorkspaceFactory::Instance().createTable("TableWorkspace"));

  deadTimeTable->addColumn("int", "Spectrum Number");
  deadTimeTable->addColumn("double", "Dead Time");

  if (deadTimes.size() != nspec) {
    return deadTimeTable;
  }

  for (size_t spec = 0; spec < deadTimes.size(); spec++) {
    TableRow newRow = deadTimeTable->appendRow();
    newRow << static_cast<int>(spec) + 1;
    newRow << deadTimes[spec];
  }

  return deadTimeTable;
}

void setPairAlgorithmProperties(ApplyMuonDetectorGroupPairing &alg,
                                std::string inputWSName,
                                std::string wsGroupName) {
  alg.setProperty("SpecifyGroupsManually", true);
  alg.setProperty("PairName", "test");
  alg.setProperty("Alpha", 1.0);
  alg.setProperty("InputWorkspace", inputWSName);
  alg.setProperty("InputWorkspaceGroup", wsGroupName);
  alg.setProperty("Group1", "1-5");
  alg.setProperty("Group2", "5-10");
  alg.setProperty("TimeMin", 0.0);
  alg.setProperty("TimeMax", 30.0);
  alg.setProperty("RebinArgs", "");
  alg.setProperty("TimeOffset", 0.0);
  alg.setProperty("SummedPeriods", std::to_string(1));
  alg.setProperty("SubtractedPeriods", "");
  alg.setProperty("ApplyDeadTimeCorrection", false);
  alg.setLogging(false);
}

// Simple class to set up the ADS with the configuration required by the
// algorithm (a matrixWorkspace and an empty group).
class setUpADSWithWorkspace {
public:
  setUpADSWithWorkspace(Workspace_sptr ws) {
    AnalysisDataService::Instance().addOrReplace(inputWSName, ws);
    wsGroup = boost::make_shared<WorkspaceGroup>();
    AnalysisDataService::Instance().addOrReplace(groupWSName, wsGroup);
  };
  ~setUpADSWithWorkspace() { AnalysisDataService::Instance().clear(); };
  WorkspaceGroup_sptr wsGroup;

  static constexpr const char *inputWSName = "inputData";
  static constexpr const char *groupWSName = "inputGroup";
};

} // namespace

class ApplyMuonDetectorGroupPairingTest : public CxxTest::TestSuite {
public:
  // WorkflowAlgorithms do not appear in the FrameworkManager without this line
  ApplyMuonDetectorGroupPairingTest() {
    Mantid::API::FrameworkManager::Instance();
  }
  // This pair of boilerplate methods prevent the suite being created statically
  // This means the constructor isn't called when running other tests
  static ApplyMuonDetectorGroupPairingTest *createSuite() {
    return new ApplyMuonDetectorGroupPairingTest();
  }
  static void destroySuite(ApplyMuonDetectorGroupPairingTest *suite) {
    delete suite;
  }

  void test_Init() {
    ApplyMuonDetectorGroupPairing alg;
    TS_ASSERT_THROWS_NOTHING(alg.initialize());
    TS_ASSERT(alg.isInitialized());
  }

  void test_checkNonAlphanumericPairNamesNotAllowed() {
    MatrixWorkspace_sptr ws = createAsymmetryWorkspace(10, 10);
    setUpADSWithWorkspace setup(ws);

    ApplyMuonDetectorGroupPairing alg;
    alg.initialize();
    setPairAlgorithmProperties(alg, setup.inputWSName, setup.groupWSName);

    std::vector<std::string> badPairNames = {"",  "!", ";name;", ".",
                                             ",", ";", ":"};
    for (auto badName : badPairNames) {
      alg.setProperty("PairName", badName);
      TS_ASSERT_THROWS(alg.execute(), std::runtime_error);
      TS_ASSERT(!alg.isExecuted());
    }
  }

  void test_checkZeroOrNegativeAlphaNotAllowed() {
    MatrixWorkspace_sptr ws = createAsymmetryWorkspace(10, 10);
    setUpADSWithWorkspace setup(ws);

    ApplyMuonDetectorGroupPairing alg;
    alg.initialize();
    setPairAlgorithmProperties(alg, setup.inputWSName, setup.groupWSName);

    std::vector<double> badAlphas = {0.0, -1.0};
    for (auto badAlpha : badAlphas) {
      alg.setProperty("Alpha", badAlpha);
      TS_ASSERT_THROWS(alg.execute(), std::runtime_error);
      TS_ASSERT(!alg.isExecuted());
    }
  }

  void test_checkThrowsIfTwoGroupsAreIdentical() {
    MatrixWorkspace_sptr ws = createAsymmetryWorkspace(10, 10);
    setUpADSWithWorkspace setup(ws);

    ApplyMuonDetectorGroupPairing alg;
    alg.initialize();
    setPairAlgorithmProperties(alg, setup.inputWSName, setup.groupWSName);

    std::vector<std::string> badGroup1 = {"1-5", "1-5", "1-5", "1-5"};
    std::vector<std::string> badGroup2 = {"1-5", "1,2,3,4,5", "5,4,3,2,1",
                                          "1,2,2,3,4,5,5,5"};
    for (auto i = 0; i < badGroup1.size(); i++) {
      alg.setProperty("Group1", badGroup1[i]);
      alg.setProperty("Group2", badGroup2[i]);
      TS_ASSERT_THROWS(alg.execute(), std::runtime_error);
      TS_ASSERT(!alg.isExecuted());
    }
  }

  void test_checkThrowsIfTimeMinGreaterThanTimeMax() {
    MatrixWorkspace_sptr ws = createAsymmetryWorkspace(10, 10);
    setUpADSWithWorkspace setup(ws);

    ApplyMuonDetectorGroupPairing alg;
    alg.initialize();
    setPairAlgorithmProperties(alg, setup.inputWSName, setup.groupWSName);

    alg.setProperty("TimeMin", 10.0);
    alg.setProperty("TimeMax", 5.0);
    TS_ASSERT_THROWS(alg.execute(), std::runtime_error);
    TS_ASSERT(!alg.isExecuted());
  }

  void test_checkThrowsIfPeriodOutOfRange() {
    // If inputWS is a matrixWorkspace then the summed/subtracted
    // periods are set to "1" and "" and so no checks are needed.
    int nPeriods = 2;
    WorkspaceGroup_sptr ws =
        createMultiPeriodWorkspaceGroup(nPeriods, 10, 10, "MuonAnalysis");
    ;
    setUpADSWithWorkspace setup(ws);

    ApplyMuonDetectorGroupPairing alg;
    alg.initialize();
    setPairAlgorithmProperties(alg, setup.inputWSName, setup.groupWSName);

    std::vector<std::string> badPeriods = {"3", "1,2,3,4", "-1"};

    for (auto &&badPeriod : badPeriods) {
      alg.setProperty("SummedPeriods", badPeriod);
      // This throw comes from MuonProcess
      TS_ASSERT_THROWS(alg.execute(), std::runtime_error);
      TS_ASSERT(!alg.isExecuted());
    }

    for (auto &&badPeriod : badPeriods) {
      alg.setProperty("SubtractedPeriods", badPeriod);
      // This throw comes from MuonProcess
      TS_ASSERT_THROWS(alg.execute(), std::runtime_error);
      TS_ASSERT(!alg.isExecuted());
    }
  }

  void test_producesOutputWorkspacesInWorkspaceGroup() {
    MatrixWorkspace_sptr ws = createAsymmetryWorkspace(10, 5);
    setUpADSWithWorkspace setup(ws);

    ApplyMuonDetectorGroupPairing alg;
    alg.initialize();
    setPairAlgorithmProperties(alg, setup.inputWSName, setup.groupWSName);
    TS_ASSERT_THROWS_NOTHING(alg.execute());
    TS_ASSERT(alg.isExecuted());
    TS_ASSERT(setup.wsGroup);
    if (setup.wsGroup) {
      TS_ASSERT_EQUALS(setup.wsGroup->getNumberOfEntries(), 2);
    }
  }

  void test_outputWorkspacesHaveCorrectName() {
    MatrixWorkspace_sptr ws = createAsymmetryWorkspace(10, 5);
    setUpADSWithWorkspace setup(ws);

    ApplyMuonDetectorGroupPairing alg;
    alg.initialize();
    setPairAlgorithmProperties(alg, setup.inputWSName, setup.groupWSName);
    alg.execute();

    TS_ASSERT(setup.wsGroup->getItem("inputGroup; Pair; test; Asym; #1"));
    TS_ASSERT(setup.wsGroup->getItem("inputGroup; Pair; test; Asym; #1_Raw"));
  }

  void test_workspacePairingHasCorrectAsymmetryValues() {
    MatrixWorkspace_sptr ws = createAsymmetryWorkspace(10, 10);
    setUpADSWithWorkspace setup(ws);

    ApplyMuonDetectorGroupPairing alg;
    alg.initialize();
    setPairAlgorithmProperties(alg, setup.inputWSName, setup.groupWSName);
    alg.execute();
    auto wsOut = boost::dynamic_pointer_cast<MatrixWorkspace>(
        setup.wsGroup->getItem("inputGroup; Pair; test; Asym; #1_Raw"));

    // Current behaviour is to convert bin edge x-values to bin centre x-values
    // (point data) so there is on fewer x-value now.
    TS_ASSERT_DELTA(wsOut->readX(0)[0], 0.050, 0.001);
    TS_ASSERT_DELTA(wsOut->readX(0)[4], 0.450, 0.001);
    TS_ASSERT_DELTA(wsOut->readX(0)[9], 0.950, 0.001);

    TS_ASSERT_DELTA(wsOut->readY(0)[0], -0.492635, 0.0001);
    TS_ASSERT_DELTA(wsOut->readY(0)[4], -0.491110, 0.0001);
    TS_ASSERT_DELTA(wsOut->readY(0)[9], -0.489006, 0.0001);

    // The error calculation as per Issue #5035
    TS_ASSERT_DELTA(wsOut->readE(0)[0], 0.01008430, 0.000001);
    TS_ASSERT_DELTA(wsOut->readE(0)[4], 0.01101931, 0.000001);
    TS_ASSERT_DELTA(wsOut->readE(0)[9], 0.01230289, 0.000001);
  }

  void test_timeOffsetShiftsTimeAxisCorrectly() {
    MatrixWorkspace_sptr ws = createAsymmetryWorkspace(10, 10);
    setUpADSWithWorkspace setup(ws);

    ApplyMuonDetectorGroupPairing alg;
    alg.initialize();
    setPairAlgorithmProperties(alg, setup.inputWSName, setup.groupWSName);
    alg.setProperty("TimeOffset", 0.2);
    alg.execute();
    auto wsOut = boost::dynamic_pointer_cast<MatrixWorkspace>(
        setup.wsGroup->getItem("inputGroup; Pair; test; Asym; #1_Raw"));

    TS_ASSERT_DELTA(wsOut->readX(0)[0], 0.250, 0.001);
    TS_ASSERT_DELTA(wsOut->readX(0)[4], 0.650, 0.001);
    TS_ASSERT_DELTA(wsOut->readX(0)[9], 1.150, 0.001);
  }

  void test_detectorIDsNotInWorkspaceFails() {
    // If the input detector IDs are not in the input workspace the algorithm
    // should not execute
  }

  void test_SummedPeriodsGivesCorrectAsymmetryValues() {
    WorkspaceGroup_sptr ws =
        createMultiPeriodWorkspaceGroup(4, 10, 10, "MuonAnalysis");
    setUpADSWithWorkspace setup(ws);

    ApplyMuonDetectorGroupPairing alg;
    alg.initialize();
    setPairAlgorithmProperties(alg, setup.inputWSName, setup.groupWSName);
    alg.setProperty("SummedPeriods", "1,2");
    alg.execute();
    auto wsOut = boost::dynamic_pointer_cast<MatrixWorkspace>(
        setup.wsGroup->getItem("inputGroup; Pair; test; Asym; #1_Raw"));

    // Summation of periods occurs before asymmetry calculation
    TS_ASSERT_DELTA(wsOut->readX(0)[0], 0.050, 0.001);
    TS_ASSERT_DELTA(wsOut->readX(0)[4], 0.450, 0.001);
    TS_ASSERT_DELTA(wsOut->readX(0)[9], 0.950, 0.001);

    TS_ASSERT_DELTA(wsOut->readY(0)[0], -0.5755, 0.0001);
    TS_ASSERT_DELTA(wsOut->readY(0)[4], -0.5368, 0.0001);
    TS_ASSERT_DELTA(wsOut->readY(0)[9], -0.4963, 0.0001);

    // The error calculation as per Issue #5035
    TS_ASSERT_DELTA(wsOut->readE(0)[0], 0.03625, 0.00001);
    TS_ASSERT_DELTA(wsOut->readE(0)[4], 0.03420, 0.00001);
    TS_ASSERT_DELTA(wsOut->readE(0)[9], 0.03208, 0.00001);
  }

  void test_SubtractedPeriodsGivesCorrectAsymmetryValues() {
    WorkspaceGroup_sptr ws =
        createMultiPeriodWorkspaceGroup(4, 10, 10, "MuonAnalysis");
    setUpADSWithWorkspace setup(ws);

    ApplyMuonDetectorGroupPairing alg;
    alg.initialize();
    setPairAlgorithmProperties(alg, setup.inputWSName, setup.groupWSName);
    alg.setProperty("SummedPeriods", "1,2");
    alg.setProperty("SubtractedPeriods", "3");
    alg.execute();
    auto wsOut = boost::dynamic_pointer_cast<MatrixWorkspace>(
        setup.wsGroup->getItem("inputGroup; Pair; test; Asym; #1_Raw"));

    // Summation of periods occurs before asymmetry calculation
	// Subtraction of periods occurs AFTER asymmetry calculation
    TS_ASSERT_DELTA(wsOut->readX(0)[0], 0.050, 0.001);
    TS_ASSERT_DELTA(wsOut->readX(0)[4], 0.450, 0.001);
    TS_ASSERT_DELTA(wsOut->readX(0)[9], 0.950, 0.001);

    TS_ASSERT_DELTA(wsOut->readY(0)[0], -0.01528882, 0.0001);
    TS_ASSERT_DELTA(wsOut->readY(0)[4], -0.01297522, 0.0001);
    TS_ASSERT_DELTA(wsOut->readY(0)[9], -0.01075352, 0.0001);

    // The error calculation as per Issue #5035
    TS_ASSERT_DELTA(wsOut->readE(0)[0], 0.06185703, 0.00001);
    TS_ASSERT_DELTA(wsOut->readE(0)[4], 0.0584598, 0.00001);
    TS_ASSERT_DELTA(wsOut->readE(0)[9], 0.05491692, 0.00001);
  }

  void test_deadTimeCorrection() {

    MatrixWorkspace_sptr ws = createAsymmetryWorkspace(10, 10);
    setUpADSWithWorkspace setup(ws);

    ApplyMuonDetectorGroupPairing alg;
    alg.initialize();
    setPairAlgorithmProperties(alg, setup.inputWSName, setup.groupWSName);

    std::vector<double> deadTimes(10, 0.0025);
    ITableWorkspace_sptr deadTimeTable = createDeadTimeTable(10, deadTimes);

    TS_ASSERT_THROWS_NOTHING(alg.setProperty("ApplyDeadTimeCorrection", true));
    TS_ASSERT_THROWS_NOTHING(alg.setProperty("DeadTimeTable", deadTimeTable));

    alg.execute();
	

    auto wsOut = boost::dynamic_pointer_cast<MatrixWorkspace>(
        setup.wsGroup->getItem("inputGroup; Pair; test; Asym; #1_Raw"));

    TS_ASSERT_DELTA(wsOut->readX(0)[0], 0.050, 0.001);
    TS_ASSERT_DELTA(wsOut->readX(0)[4], 0.450, 0.001);
    TS_ASSERT_DELTA(wsOut->readX(0)[9], 0.950, 0.001);

    TS_ASSERT_DELTA(wsOut->readY(0)[0], 0.5152, 0.001);
    TS_ASSERT_DELTA(wsOut->readY(0)[4], -0.9689, 0.001);
    TS_ASSERT_DELTA(wsOut->readY(0)[9], 0.4172, 0.001);

    TS_ASSERT_DELTA(wsOut->readE(0)[0], 0.0050, 0.0001);
    TS_ASSERT_DELTA(wsOut->readE(0)[4], 0.0050, 0.0001);
    TS_ASSERT_DELTA(wsOut->readE(0)[9], 0.0050, 0.0001);
  }

  void test_inputWorkspaceWithMultipleSpectraFails() {
    // We expect the input workspaces to have a single spectra.
  }

  void test_inputWorkspaceWithDifferentTimeAxisFails() {
    // e.g. rebin with non-rebin should throw an error from this algorithm.
  }
};

#endif /* MANTID_MUON_APPLYMUONDETECTORGROUPPAIRINGTEST_H_ */
