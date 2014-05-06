#ifndef MANTID_ALGORITHMS_SPECULARREFLECTIONPOSITIONCORRECTTEST_H_
#define MANTID_ALGORITHMS_SPECULARREFLECTIONPOSITIONCORRECTTEST_H_

#include <cxxtest/TestSuite.h>

#include "MantidAlgorithms/SpecularReflectionPositionCorrect.h"
#include "MantidTestHelpers/WorkspaceCreationHelper.h"
#include "MantidAPI/AlgorithmManager.h"
#include "MantidAPI/FrameworkManager.h"
#include "MantidKernel/Exception.h"
#include "MantidKernel/V3D.h"
#include "MantidGeometry/Instrument/ReferenceFrame.h"
#include <cmath>
#include <boost/tuple/tuple.hpp>

using Mantid::Algorithms::SpecularReflectionPositionCorrect;
using namespace Mantid::API;
using namespace Mantid::Kernel;
using namespace Mantid::Geometry;

namespace
{
  typedef boost::tuple<double, double> VerticalHorizontalOffsetType;
}

class SpecularReflectionPositionCorrectTest: public CxxTest::TestSuite
{
public:
  // This pair of boilerplate methods prevent the suite being created statically
  // This means the constructor isn't called when running other tests
  static SpecularReflectionPositionCorrectTest *createSuite()
  {
    return new SpecularReflectionPositionCorrectTest();
  }
  static void destroySuite(SpecularReflectionPositionCorrectTest *suite)
  {
    delete suite;
  }

  SpecularReflectionPositionCorrectTest()
  {
    FrameworkManager::Instance();
  }

  void test_init()
  {
    SpecularReflectionPositionCorrect alg;
    TS_ASSERT_THROWS_NOTHING( alg.initialize())
    TS_ASSERT( alg.isInitialized())
  }

  void test_theta_is_mandatory()
  {
    SpecularReflectionPositionCorrect alg;
    alg.setRethrows(true);
    alg.initialize();
    alg.setProperty("InputWorkspace", WorkspaceCreationHelper::Create1DWorkspaceConstant(1, 1, 1));
    alg.setPropertyValue("OutputWorkspace", "test_out");
    TS_ASSERT_THROWS(alg.execute(), std::runtime_error&);
  }

  void test_theta_is_greater_than_zero_else_throws()
  {
    SpecularReflectionPositionCorrect alg;
    alg.setRethrows(true);
    alg.initialize();
    alg.setProperty("InputWorkspace", WorkspaceCreationHelper::Create1DWorkspaceConstant(1, 1, 1));
    alg.setPropertyValue("OutputWorkspace", "test_out");
    TS_ASSERT_THROWS(alg.setProperty("ThetaIn", 0.0), std::invalid_argument&);
  }

  void test_theta_is_less_than_ninety_else_throws()
  {
    SpecularReflectionPositionCorrect alg;
    alg.setRethrows(true);
    alg.initialize();
    alg.setProperty("InputWorkspace", WorkspaceCreationHelper::Create1DWorkspaceConstant(1, 1, 1));
    alg.setPropertyValue("OutputWorkspace", "test_out");
    TS_ASSERT_THROWS(alg.setProperty("ThetaIn", 90.0), std::invalid_argument&);
  }

  void test_throws_if_SpectrumNumbersOfDetectors_less_than_zero()
  {
    SpecularReflectionPositionCorrect alg;
    alg.setRethrows(true);
    alg.initialize();
    alg.setProperty("InputWorkspace", WorkspaceCreationHelper::Create1DWorkspaceConstant(1, 1, 1));
    alg.setPropertyValue("OutputWorkspace", "test_out");
    alg.setProperty("ThetaIn", 10.0);
    std::vector<int> invalid(1, -1);
    TS_ASSERT_THROWS(alg.setProperty("SpectrumNumbersOfDetectors", invalid), std::invalid_argument&);
  }

  void test_throws_if_SpectrumNumbersOfDetectors_outside_range()
  {
    SpecularReflectionPositionCorrect alg;
    alg.setRethrows(true);
    alg.initialize();
    alg.setProperty("InputWorkspace",
        WorkspaceCreationHelper::create2DWorkspaceWithRectangularInstrument(1, 1, 1));
    alg.setPropertyValue("OutputWorkspace", "test_out");
    alg.setProperty("ThetaIn", 10.0);
    std::vector<int> invalid(1, 1e7);  // Well outside range
    alg.setProperty("SpectrumNumbersOfDetectors", invalid);  // Well outside range
    TS_ASSERT_THROWS(alg.execute(), std::invalid_argument&);
  }

  void test_throws_if_DetectorComponentName_unknown()
  {
    SpecularReflectionPositionCorrect alg;
    alg.setRethrows(true);
    alg.initialize();
    alg.setProperty("InputWorkspace",
        WorkspaceCreationHelper::create2DWorkspaceWithRectangularInstrument(1, 1, 1));
    alg.setPropertyValue("OutputWorkspace", "test_out");
    alg.setProperty("ThetaIn", 10.0);
    std::vector<int> invalid(1, 1e7);
    alg.setProperty("DetectorComponentName", "junk_value");  // Well outside range
    TS_ASSERT_THROWS(alg.execute(), std::invalid_argument&);
  }

  VerticalHorizontalOffsetType determine_vertical_and_horizontal_offsets(MatrixWorkspace_sptr ws,
      std::string detectorName = "point-detector")
  {
    auto instrument = ws->getInstrument();
    const V3D pointDetector = instrument->getComponentByName(detectorName)->getPos();
    const V3D surfaceHolder = instrument->getComponentByName("some-surface-holder")->getPos();
    const auto referenceFrame = instrument->getReferenceFrame();
    const V3D sampleToDetector = pointDetector - surfaceHolder;

    const double sampleToDetectorVerticalOffset = sampleToDetector.scalar_prod(
        referenceFrame->vecPointingUp());
    const double sampleToDetectorHorizontalOffset = sampleToDetector.scalar_prod(
        referenceFrame->vecPointingAlongBeam());

    return VerticalHorizontalOffsetType(sampleToDetectorVerticalOffset, sampleToDetectorHorizontalOffset);
  }

  void test_correct_point_detector_to_current_position()
  {
    auto loadAlg = AlgorithmManager::Instance().create("Load");
    loadAlg->initialize();
    loadAlg->setChild(true);
    loadAlg->setProperty("Filename", "INTER00013460.nxs");
    loadAlg->setPropertyValue("OutputWorkspace", "demo");
    loadAlg->execute();
    Workspace_sptr temp = loadAlg->getProperty("OutputWorkspace");
    MatrixWorkspace_sptr toConvert = boost::dynamic_pointer_cast<MatrixWorkspace>(temp);

    VerticalHorizontalOffsetType offsetTuple = determine_vertical_and_horizontal_offsets(toConvert); // Offsets before correction
    const double sampleToDetectorVerticalOffset = offsetTuple.get<0>();
    const double sampleToDetectorBeamOffset = offsetTuple.get<1>();

    // Based on the current positions, calculate the current incident theta.
    const double currentThetaInRad = std::atan(
        sampleToDetectorVerticalOffset / sampleToDetectorBeamOffset);
    const double currentThetaInDeg = currentThetaInRad * (180.0 / M_PI);

    SpecularReflectionPositionCorrect alg;
    alg.setRethrows(true);
    alg.setChild(true);
    alg.initialize();
    alg.setProperty("InputWorkspace", toConvert);
    alg.setPropertyValue("OutputWorkspace", "test_out");
    alg.setProperty("ThetaIn", currentThetaInDeg);
    alg.execute();
    MatrixWorkspace_sptr corrected = alg.getProperty("OutputWorkspace");

    VerticalHorizontalOffsetType offsetTupleCorrected = determine_vertical_and_horizontal_offsets(
        corrected); // Positions after correction
    const double sampleToDetectorVerticalOffsetCorrected = offsetTupleCorrected.get<0>();
    const double sampleToDetectorBeamOffsetCorrected = offsetTupleCorrected.get<1>();

    // Positions should be identical to original. No correction
    TSM_ASSERT_DELTA("Vertical position should be unchanged", sampleToDetectorVerticalOffsetCorrected,
        sampleToDetectorVerticalOffset, 1e-6);
    TSM_ASSERT_DELTA("Beam position should be unchanged", sampleToDetectorBeamOffsetCorrected,
        sampleToDetectorBeamOffset, 1e-6);
  }

  void do_test_correct_point_detector_position(std::string detectorFindProperty = "",
      std::string stringValue = "")
  {
    auto loadAlg = AlgorithmManager::Instance().create("Load");
    loadAlg->initialize();
    loadAlg->setChild(true);
    loadAlg->setProperty("Filename", "INTER00013460.nxs");
    loadAlg->setPropertyValue("OutputWorkspace", "demo");
    loadAlg->execute();
    Workspace_sptr temp = loadAlg->getProperty("OutputWorkspace");
    MatrixWorkspace_sptr toConvert = boost::dynamic_pointer_cast<MatrixWorkspace>(temp);

    const double thetaInDegrees = 10.0; //Desired theta in degrees.
    const double thetaInRad = thetaInDegrees * (M_PI / 180);
    VerticalHorizontalOffsetType offsetTuple = determine_vertical_and_horizontal_offsets(toConvert); // Offsets before correction
    const double sampleToDetectorBeamOffsetExpected = offsetTuple.get<1>();
    const double sampleToDetectorVerticalOffsetExpected = std::tan(thetaInRad)
        * sampleToDetectorBeamOffsetExpected;

    SpecularReflectionPositionCorrect alg;
    alg.setChild(true);
    alg.initialize();
    alg.setProperty("InputWorkspace", toConvert);
    alg.setPropertyValue("OutputWorkspace", "test_out");
    if (!detectorFindProperty.empty())
      alg.setProperty(detectorFindProperty, stringValue);
    alg.setProperty("ThetaIn", thetaInDegrees);
    alg.execute();
    MatrixWorkspace_sptr corrected = alg.getProperty("OutputWorkspace");

    VerticalHorizontalOffsetType offsetTupleCorrected = determine_vertical_and_horizontal_offsets(
        corrected); // Positions after correction
    const double sampleToDetectorVerticalOffsetCorrected = offsetTupleCorrected.get<0>();
    const double sampleToDetectorBeamOffsetCorrected = offsetTupleCorrected.get<1>();

    // Positions should be identical to original. No correction
    TSM_ASSERT_DELTA("Vertical position should be unchanged", sampleToDetectorVerticalOffsetCorrected,
        sampleToDetectorVerticalOffsetExpected, 1e-6);
    TSM_ASSERT_DELTA("Beam position should be unchanged", sampleToDetectorBeamOffsetCorrected,
        sampleToDetectorBeamOffsetExpected, 1e-6);
  }

  void test_correct_point_detector_position_using_defaults_for_specifying_detector()
  {
    do_test_correct_point_detector_position();
  }

  void test_correct_point_detector_position_using_name_for_specifying_detector()
  {
    do_test_correct_point_detector_position("DetectorComponentName", "point-detector");
  }

  void test_correct_point_detector_position_using_spectrum_number_for_specifying_detector()
  {
    do_test_correct_point_detector_position("SpectrumNumbersOfDetectors", "4");
  }

  double do_test_correct_line_detector_position(std::vector<int> specNumbers, double thetaInDegrees,
      std::string detectorName = "lineardetector")
  {
    auto loadAlg = AlgorithmManager::Instance().create("Load");
    loadAlg->initialize();
    loadAlg->setChild(true);
    loadAlg->setProperty("Filename", "POLREF0004699.nxs");
    loadAlg->setPropertyValue("OutputWorkspace", "demo");
    loadAlg->execute();
    Workspace_sptr temp = loadAlg->getProperty("OutputWorkspace");
    WorkspaceGroup_sptr temp1 = boost::dynamic_pointer_cast<WorkspaceGroup>(temp);
    MatrixWorkspace_sptr toConvert = boost::dynamic_pointer_cast<MatrixWorkspace>(temp1->getItem(0));

    const double thetaInRad = thetaInDegrees * (M_PI / 180);
    VerticalHorizontalOffsetType offsetTuple = determine_vertical_and_horizontal_offsets(toConvert,
        detectorName); // Offsets before correction
    const double sampleToDetectorBeamOffsetExpected = offsetTuple.get<1>();
    const double sampleToDetectorVerticalOffsetExpected = std::tan(thetaInRad)
        * sampleToDetectorBeamOffsetExpected;

    SpecularReflectionPositionCorrect alg;
    alg.setChild(true);
    alg.initialize();
    alg.setProperty("InputWorkspace", toConvert);
    alg.setPropertyValue("OutputWorkspace", "test_out");
    alg.setProperty("SpectrumNumbersOfDetectors", specNumbers);
    alg.setProperty("ThetaIn", thetaInDegrees);
    alg.execute();
    MatrixWorkspace_sptr corrected = alg.getProperty("OutputWorkspace");

    VerticalHorizontalOffsetType offsetTupleCorrected = determine_vertical_and_horizontal_offsets(
        corrected, detectorName); // Positions after correction
    const double sampleToDetectorVerticalOffsetCorrected = offsetTupleCorrected.get<0>();
    const double sampleToDetectorBeamOffsetCorrected = offsetTupleCorrected.get<1>();

    return sampleToDetectorVerticalOffsetCorrected;
  }

  void test_correct_line_detector_position_many_spec_numbers_equal_averaging()
  {
    std::vector<int> specNumbers;
    specNumbers.push_back(74);
    double offset1 = do_test_correct_line_detector_position(specNumbers, 1, "lineardetector");

    specNumbers.push_back(73); //Add spectra below
    specNumbers.push_back(75); //Add spectra above
    double offset2 = do_test_correct_line_detector_position(specNumbers, 1, "lineardetector");

    TSM_ASSERT_DELTA(
        "If grouping has worked correctly the group average position should be the same as for spectra 74",
        offset1, offset2, 1e-9);
  }

  void test_correct_line_detector_position_average_offset_by_one_pixel()
  {
    std::vector<int> specNumbers;
    specNumbers.push_back(100);
    double offset1 = do_test_correct_line_detector_position(specNumbers, 0.1, "lineardetector"); // Average spectrum number at 100

    specNumbers.push_back(101);
    specNumbers.push_back(102);
    double offset2 = do_test_correct_line_detector_position(specNumbers, 0.1, "lineardetector"); // Average spectrum number now at 101.

    double const width = 1.2e-3; //Pixel height

    TS_ASSERT_DELTA(
        offset1, offset2+width, 1e-9);
  }

  void test_correct_line_detector_position_average_offset_by_many_pixels()
  {
    std::vector<int> specNumbers;
    specNumbers.push_back(100);
    double offset1 = do_test_correct_line_detector_position(specNumbers, 0.1, "lineardetector");  // Average spectrum number at 100

    specNumbers.push_back(104);
    double offset2 = do_test_correct_line_detector_position(specNumbers, 0.1, "lineardetector");  // Average spectrum number at 102

    double const width = 1.2e-3; //Pixel height

    TS_ASSERT_DELTA(
        offset1, offset2+(2*width), 1e-9);
  }

};

#endif /* MANTID_ALGORITHMS_SPECULARREFLECTIONPOSITIONCORRECTTEST_H_ */
