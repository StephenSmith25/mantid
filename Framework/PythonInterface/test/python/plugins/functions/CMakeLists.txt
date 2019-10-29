# Tests for Fit functions

set(TEST_PY_FILES
    AFM_LFTest.py
    AFM_ZFTest.py
    AttributeTest.py
    BesselTest.py
    CombGaussLorentzKTTest.py
    CompositePCRmagnetTest.py
    DSFinterp1DFitTest.py
    EISFDiffSphereTest.py
    EISFDiffCylinderTest.py
    EISFDiffSphereAlkylTest.py
    Example1DFunctionTest.py
    ExamplePeakFunctionTest.py
    FmuFTest.py
    GauBroadGauKTTest.py
    GaussBesselTest.py
    HighTFMuoniumTest.py
    LowTFMuoniumTest.py
    MeierTest.py
    ModOscTest.py
    MsdGaussTest.py
    MsdPetersTest.py
    MsdYiTest.py
    MuHTest.py
    MuMinusExpTFTest.py
    PCRmagnetTest.py
    PCRmagnetZFKTTest.py
    PCRmagnetRedfieldTest.py
    PCRmagnetfnormTest.py
    RFresonanceTest.py
    SilverBaselineTest.py
    StandardSCTest.py
    StaticLorentzianKTTest.py
    StretchedExpFTTest.py
    SCgapAnisoSwaveTest.py
    SCgapDwaveTest.py
    SCgapPwaveTest.py
    SCgapSwaveTest.py
    TeixeiraWaterTest.py
    TriangleOscTest.py
    TFMuoniumTest.py
    ZFdipoleTest.py
    ZFprotonDipoleTest.py
    ZFelectronDipoleTest.py)

check_tests_valid(${CMAKE_CURRENT_SOURCE_DIR} ${TEST_PY_FILES})

# Prefix for test name=PythonFunctions
pyunittest_add_test(${CMAKE_CURRENT_SOURCE_DIR} python.functions
                    ${TEST_PY_FILES})
