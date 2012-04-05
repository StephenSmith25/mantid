/*WIKI* 
Calculate the detector sensitivity and patch the pixels that are masked in
a second workspace.
*WIKI*/
//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include "MantidWorkflowAlgorithms/EQSANSPatchSensitivity.h"

namespace Mantid
{
namespace WorkflowAlgorithms
{

// Register the algorithm into the AlgorithmFactory
DECLARE_ALGORITHM(EQSANSPatchSensitivity)

/// Sets documentation strings for this algorithm
void EQSANSPatchSensitivity::initDocs()
{
  this->setWikiSummary("Patch EQSANS sensitivity correction.");
  this->setOptionalMessage("Patch EQSANS sensitivity correction.");
}

using namespace Kernel;
using namespace API;
using namespace Geometry;

void EQSANSPatchSensitivity::init()
{
  declareProperty(new WorkspaceProperty<>("Workspace","",Direction::InOut));
  declareProperty(new WorkspaceProperty<>("PatchWorkspace","",Direction::Input));
  declareProperty("OutputMessage","",Direction::Output);
}

void EQSANSPatchSensitivity::exec()
{
  MatrixWorkspace_sptr inputWS = getProperty("Workspace");
  MatrixWorkspace_sptr patchWS = getProperty("PatchWorkspace");
  const int nx_pixels = (int)(inputWS->getInstrument()->getNumberParameter("number-of-x-pixels")[0]);
  const int ny_pixels = (int)(inputWS->getInstrument()->getNumberParameter("number-of-y-pixels")[0]);

  const int numberOfSpectra = static_cast<int>(inputWS->getNumberHistograms());
  // Need to get hold of the parameter map
  Geometry::ParameterMap& pmap = inputWS->instrumentParameters();

  // Loop over all tubes and patch as necessary
  for (int i=0; i<nx_pixels; i++)
  {
    std::vector<int> patched_ids;
    int nUnmasked = 0;
    double totalUnmasked = 0.0;
    double errorUnmasked = 0.0;
    progress(0.9*i/nx_pixels, "Processing patch");

    for (int j=0; j<ny_pixels; j++)
    {
      // EQSANS-specific: get detector ID from pixel coordinates
      int iDet = ny_pixels*i + j;
      if (iDet>numberOfSpectra)
      {
        g_log.notice() << "Got an invalid detector ID " << iDet << std::endl;
        continue;
      }

      IDetector_const_sptr det = patchWS->getDetector(iDet);
      // If this detector is a monitor, skip to the next one
      if ( det->isMonitor() ) continue;

      const MantidVec& YValues = inputWS->readY(iDet);
      const MantidVec& YErrors = inputWS->readE(iDet);

      // If this detector is masked, skip to the next one
      if ( det->isMasked() ) patched_ids.push_back(iDet);
      else
      {
        totalUnmasked += YErrors[0]*YErrors[0]*YValues[0];
        errorUnmasked += YErrors[0]*YErrors[0];
        nUnmasked++;
      }
    }

    if (nUnmasked>0 && errorUnmasked>0)
    {
      double error = sqrt(errorUnmasked)/nUnmasked;
      double average = totalUnmasked / errorUnmasked;

      // Apply patch
      progress(0.91, "Applying patch");
      for (size_t k=0; k<patched_ids.size(); k++)
      {
        MantidVec& YValues = inputWS->dataY(patched_ids[k]);
        MantidVec& YErrors = inputWS->dataE(patched_ids[k]);
        YValues[0] = average;
        YErrors[0] = error;
        try
        {
          if ( const Geometry::ComponentID det = inputWS->getDetector(patched_ids[k])->getComponentID() )
          {
             pmap.addBool(det,"masked",false);
          }
        }
        catch(Kernel::Exception::NotFoundError &e)
        {
          g_log.warning() << e.what() << " Found while setting mask bit" << std::endl;
        }

      }
    }
  }
  /*
  This rebuild request call, gives the workspace the opportunity to rebuild the nearest neighbours map
  and therefore pick up any detectors newly masked with this algorithm.
  */
  inputWS->rebuildNearestNeighbours();

  // Call Calculate efficiency to renormalize
  progress(0.91, "Renormalizing");
  IAlgorithm_sptr effAlg = createSubAlgorithm("CalculateEfficiency");
  effAlg->setProperty("InputWorkspace", inputWS);
  effAlg->setProperty("OutputWorkspace", inputWS);
  effAlg->execute();
  inputWS = effAlg->getProperty("OutputWorkspace");
  setProperty("Workspace", inputWS);

  setProperty("OutputMessage", "Applied wavelength-dependent sensitivity correction");
}

} // namespace WorkflowAlgorithms
} // namespace Mantid

