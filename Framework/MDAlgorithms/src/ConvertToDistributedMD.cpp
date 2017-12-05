#include <MantidAPI/IEventWorkspace.h>
#include "MantidMDAlgorithms/ConvertToDistributedMD.h"
#include "MantidMDAlgorithms/EventToMDEventConverter.h"
#include "MantidParallel/Communicator.h"
#include "MantidParallel/Collectives.h"
#include "MantidKernel/UnitLabelTypes.h"

#include "MantidKernel/Strings.h"
#include "MantidKernel/ArrayProperty.h"
#include "MantidKernel/BoundedValidator.h"
#include "MantidKernel/StringTokenizer.h"
#include "MantidKernel/ListValidator.h"

#include <boost/mpi/request.hpp>

namespace Mantid {
namespace MDAlgorithms {

using Mantid::Kernel::Direction;
using Mantid::API::WorkspaceProperty;
using namespace Mantid::Kernel;
using namespace Mantid::Geometry;

const std::string ConvertToDistributedMD::boxSplittingGroupName =
    "Box Splitting Settings";

// Register the algorithm into the AlgorithmFactory
DECLARE_ALGORITHM(ConvertToDistributedMD)

//----------------------------------------------------------------------------------------------

/// Algorithms name for identification. @see Algorithm::name
const std::string ConvertToDistributedMD::name() const {
  return "ConvertToDistributedMD";
}

/// Algorithm's version for identification. @see Algorithm::version
int ConvertToDistributedMD::version() const { return 1; }

/// Algorithm's category for identification. @see Algorithm::category
const std::string ConvertToDistributedMD::category() const {
  return "TODO: FILL IN A CATEGORY";
}

/// Algorithm's summary for use in the GUI and help. @see Algorithm::summary
const std::string ConvertToDistributedMD::summary() const {
  return "TODO: FILL IN A SUMMARY";
}

//----------------------------------------------------------------------------------------------
/** Initialize the algorithm's properties.
 */
void ConvertToDistributedMD::init() {
  declareProperty(
      Kernel::make_unique<WorkspaceProperty<DataObjects::EventWorkspace>>(
          "InputWorkspace", "", Direction::Input),
      "An input workspace.");

  declareProperty(make_unique<Mantid::Kernel::PropertyWithValue<bool>>(
                      "LorentzCorrection", false, Direction::Input),
                  "Correct the weights of events by multiplying by the Lorentz "
                  "formula: sin(theta)^2 / lambda^4");

  declareProperty(make_unique<Mantid::Kernel::PropertyWithValue<double>>("Fraction", 0.01f),
                  "Fraction of pulse time that should be used to build the "
                  "initial data set.\n");


  std::vector<std::string> propOptions{"Q (lab frame)", "Q (sample frame)",
                                       "HKL"};
  declareProperty(
    "OutputDimensions", "Q (lab frame)",
    boost::make_shared<Mantid::Kernel::StringListValidator>(propOptions),
    "What will be the dimensions of the output workspace?\n"
      "  Q (lab frame): Wave-vector change of the lattice in the lab frame.\n"
      "  Q (sample frame): Wave-vector change of the lattice in the frame of "
      "the sample (taking out goniometer rotation).\n"
      "  HKL: Use the sample's UB matrix to convert to crystal's HKL indices.");

  // ---------------------------------
  // Box Controller settings
  // ---------------------------------
  this->initBoxControllerProps("2" /*SplitInto*/, 1500 /*SplitThreshold*/,
                               20 /*MaxRecursionDepth*/);

  declareProperty(
      make_unique<PropertyWithValue<int>>("MinRecursionDepth", 0),
      "Optional. If specified, then all the boxes will be split to this "
      "minimum recursion depth. 1 = one level of splitting, etc.\n"
      "Be careful using this since it can quickly create a huge number of "
      "boxes = (SplitInto ^ (MinRercursionDepth x NumDimensions)).\n"
      "But setting this property equal to MaxRecursionDepth property is "
      "necessary if one wants to generate multiple file based workspaces in "
      "order to merge them later\n");
  setPropertyGroup("MinRecursionDepth", boxSplittingGroupName);

  std::vector<double> extents{-50., 50., -50., 50., -50., 50.};
  declareProperty(
      Kernel::make_unique<ArrayProperty<double>>("Extents", extents),
      "A comma separated list of min, max for each dimension,\n"
      "specifying the extents of each dimension. Optional, default "
      "+-50 in each dimension.");
  setPropertyGroup("Extents", boxSplittingGroupName);
}

/**
 * TODO: Find a good way to share with BoxSettingsAlgorithm
 */
void ConvertToDistributedMD::initBoxControllerProps(
    const std::string &SplitInto, int SplitThreshold, int MaxRecursionDepth) {
  auto mustBePositive = boost::make_shared<BoundedValidator<int>>();
  mustBePositive->setLower(0);
  auto mustBeMoreThen1 = boost::make_shared<BoundedValidator<int>>();
  mustBeMoreThen1->setLower(1);

  // Split up comma-separated properties
  typedef Mantid::Kernel::StringTokenizer tokenizer;
  tokenizer values(SplitInto, ",",
                   tokenizer::TOK_IGNORE_EMPTY | tokenizer::TOK_TRIM);
  std::vector<int> valueVec;
  valueVec.reserve(values.count());
  for (const auto &value : values)
    valueVec.push_back(boost::lexical_cast<int>(value));

  declareProperty(
      Kernel::make_unique<ArrayProperty<int>>("SplitInto", valueVec),
      "A comma separated list of into how many sub-grid elements each "
      "dimension should split; "
      "or just one to split into the same number for all dimensions. Default " +
          SplitInto + ".");

  declareProperty(
      make_unique<PropertyWithValue<int>>("SplitThreshold", SplitThreshold,
                                          mustBePositive),
      "How many events in a box before it should be split. Default " +
          Kernel::Strings::toString(SplitThreshold) + ".");

  declareProperty(make_unique<PropertyWithValue<int>>(
                      "MaxRecursionDepth", MaxRecursionDepth, mustBeMoreThen1),
                  "How many levels of box splitting recursion are allowed. "
                  "The smallest box will have each side length :math:`l = "
                  "(extents) / (SplitInto^{MaxRecursionDepth}).` "
                  "Default " +
                      Kernel::Strings::toString(MaxRecursionDepth) + ".");
  setPropertyGroup("SplitInto", boxSplittingGroupName);
  setPropertyGroup("SplitThreshold", boxSplittingGroupName);
  setPropertyGroup("MaxRecursionDepth", boxSplittingGroupName);
}

//----------------------------------------------------------------------------------------------
/** Execute the algorithm.
 */
void ConvertToDistributedMD::exec() {
  // The generation of the distributed MD data requires the following steps:
  // 1. Convert the data which corresponds to the first n percent of the total
  // measurement
  // 2. Build a preliminary box structure on the master rank
  // 3. Determine how the data will be split based on the preliminary box
  // structure and share this information with all
  //    ranks
  // 4. Share the preliminary box structure with all ranks
  // 5. Convert all events
  // 6. Disable the box controller and add events to the local box structure
  // 7. Send data from the each rank to the correct rank
  // 8. Enable the box controller and start splitting the data

  // Get the users's inputs
  Mantid::DataObjects::EventWorkspace_sptr inputWorkspace =
      getProperty("InputWorkspace");

  // ----------------------------------------------------------
  // 1. Get a n-percent fraction
  // ----------------------------------------------------------
  auto nPercentEvents = getNPercentEvents(*inputWorkspace);

  // -----------------------------------------------------------------
  // 2. + 3. = 4.  Get the preliminary box structure and the partition behaviour
  // -----------------------------------------------------------------
  auto *boxes = getPreliminaryBoxStructure(nPercentEvents);

  // ----------------------------------------------------------
  // 5. Convert all events
  // ----------------------------------------------------------

  // ----------------------------------------------------------
  // 6. Add the local data to the preliminary box structure
  // ----------------------------------------------------------

  // ----------------------------------------------------------
  // 7. Redistribute data
  // ----------------------------------------------------------

  // ----------------------------------------------------------
  // 8. Continue to split locally
  // ----------------------------------------------------------
}

ConvertToDistributedMD::MDEventList ConvertToDistributedMD::getNPercentEvents(const Mantid::DataObjects::EventWorkspace& workspace) const {
  // Get user inputs
  double fraction = getProperty("Fraction");
  auto extents = getWorkspaceExtents();

  EventToMDEventConverter converter;
  return converter.getEvents(workspace, extents, fraction, QFrame::QLab);
}

std::vector<Mantid::coord_t> ConvertToDistributedMD::getWorkspaceExtents() const {
  std::vector<double> extentsInput = getProperty("Extents");

  // Replicate a single min,max into several
  if (extentsInput.size() != DIM_DISTRIBUTED_TEST * 2)
    throw std::invalid_argument(
      "You must specify multiple of 2 extents, ie two for each dimension.");

  // Convert input to coordinate type
  std::vector<coord_t> extents;
  std::transform(extentsInput.begin(), extentsInput.end(), std::back_inserter(extents), [](double value) {
    return static_cast<coord_t>(value);
  });

  return extents;
}

BoxStructure *
ConvertToDistributedMD::getPreliminaryBoxStructure(
    ConvertToDistributedMD::MDEventList &mdEvents) const {
  BoxStructure *boxStructure = nullptr;

  const auto &communicator = this->communicator();
  if (communicator.rank() == 0) {
    // 1.b Receive the data from all the other ranks
    auto allEvents = receiveMDEventsOnMaster(communicator, mdEvents);

    // 2. Build the box structure on the master rank
    auto boxStructureInformation = generatePreliminaryBoxStructure(allEvents);
;
    // 4. Serialize the box structure
    //serializeBoxStructure(boxStructureInformation);

    // 5.a Broadcast serialized box structure to all other ranks

  } else {
    // 1.a Send the event data to the master rank
    sendMDEventsToMaster(communicator, mdEvents);

    // 5.b Receive the box structure from master

    // 6. Deserialize on all ranks (except for master)
  }

  return boxStructure;
}

void ConvertToDistributedMD::sendMDEventsToMaster(
    const Mantid::Parallel::Communicator &communicator,
    const MDEventList &mdEvents) const {
  // Send the totalNumberOfEvents
  auto totalNumberEvents = mdEvents.size();
  gather(communicator, totalNumberEvents, 0);

  // Send the vector of md events
  communicator.send(0, 2, mdEvents.data(), static_cast<int>(mdEvents.size()));
}

ConvertToDistributedMD::MDEventList
ConvertToDistributedMD::receiveMDEventsOnMaster(
    const Mantid::Parallel::Communicator &communicator,
    const MDEventList &mdEvents) const {
  // In order to get all relevant md events onto the master rank we need to:
  // 1. Inform the master rank about the number of events on all ranks (gather)
  // 2. Create a sufficiently large buff on master
  // 3. Determine the stride that each rank requires, ie which offset each rank
  // requires
  // 4. Send the data to master. This would be a natural operation for gatherv,
  // however this is not available in
  //    boost 1.58 (in 1.59+ it is).

  // 1. Get the totalNumberOfEvents for each rank
  std::vector<size_t> numberOfEventsPerRank;
  auto masterNumberOfEvents = mdEvents.size();
  gather(communicator, masterNumberOfEvents, numberOfEventsPerRank, 0);

  // 2. Create a buffer
  auto totalNumberOfEvents = std::accumulate(numberOfEventsPerRank.begin(),
                                             numberOfEventsPerRank.end(), 0ul);
  MDEventList totalEvents(totalNumberOfEvents);
  std::copy(mdEvents.begin(), mdEvents.end(), totalEvents.begin());

  // 3. Determine the strides
  std::vector<int> strides;
  strides.reserve(numberOfEventsPerRank.size());
  strides.push_back(0);
  {
    std::vector<int> tempStrides(numberOfEventsPerRank.size() - 1);
    std::partial_sum(numberOfEventsPerRank.begin(),
                     numberOfEventsPerRank.end() - 1, tempStrides.begin());
    std::move(tempStrides.begin(), tempStrides.end(),
              std::back_inserter(strides));
  }

  // 4. Send the data from all ranks to the master rank
  const auto &boostCommunicator = communicator.getBoostCommunicator();
  const auto numberOfRanks = communicator.size();
  std::vector<boost::mpi::request> requests;
  requests.reserve(static_cast<size_t>(numberOfRanks) - 1);

  for (int rank = 1; rank < numberOfRanks; ++rank) {
    // Determine where to insert the array
    auto start = totalEvents.data() + strides[rank];
    auto length = numberOfEventsPerRank[rank];
    requests.emplace_back(
        boostCommunicator.irecv(rank, 2, start, static_cast<int>(length)));
  }
  boost::mpi::wait_all(requests.begin(), requests.end());
  return totalEvents;
}

BoxStructureInformation ConvertToDistributedMD::generatePreliminaryBoxStructure(
    const MDEventList &allEvents) const {
  // To build the box structure we need to:
  // 1. Generate a temporary MDEventWorkspace locally on the master rank
  // 2. Populate the workspace with the MDEvents
  // 3. Extract the box structure and the box controller from the workspace, ie
  //    remove ownership from the underlying data structures

  // 1. Create temporary MDEventWorkspace
  auto tempWorkspace = createTemporaryWorkspace();

  // 2. Populate workspace with MDEvents
  addMDEventsToMDEventWorkspace(*tempWorkspace, allEvents);

  // 3. Extract box structure and box controller
  return extractBoxStructure(*tempWorkspace);
}


void ConvertToDistributedMD::addMDEventsToMDEventWorkspace(Mantid::DataObjects::MDEventWorkspace3Lean& workspace,
                                                           const MDEventList & allEvents) const {
  // We could call the addEvents method, but it seems to behave slightly differently
  // to addEvent. TODO: Investigate if addEvents could be used.
  size_t eventsAddedSinceLastSplit = 0;
  size_t eventsAddedTotal = 0;
  auto boxController = workspace.getBoxController();
  size_t lastNumBoxes = boxController->getTotalNumMDBoxes();

  for (const auto& event : allEvents) {
    // We have a different situation here compared to ConvertToDiffractionMD, since we already have
    // all events converted
    if (boxController->shouldSplitBoxes(eventsAddedTotal, eventsAddedSinceLastSplit, lastNumBoxes)) {
      workspace.splitAllIfNeeded(nullptr);
      eventsAddedSinceLastSplit = 0;
      lastNumBoxes = boxController->getTotalNumMDBoxes();
    }
    ++eventsAddedSinceLastSplit;
    ++eventsAddedTotal;
    workspace.addEvent(event);
  }
  workspace.splitAllIfNeeded(nullptr);
  workspace.refreshCache();
}


BoxStructureInformation ConvertToDistributedMD::extractBoxStructure(Mantid::DataObjects::MDEventWorkspace3Lean& workspace) const {
  // Extract the box controller
  BoxStructureInformation boxStructureInformation;
  workspace.transferInternals(boxStructureInformation.boxController, boxStructureInformation.boxStructure);
  return boxStructureInformation;
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
// Methods below are from ConvertToDiffractionMD and BoxControllerSettingsAlgorithm. In actual implementation
// we need to re-implement ConvertToMD
// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

Mantid::DataObjects::MDEventWorkspace3Lean::sptr ConvertToDistributedMD::createTemporaryWorkspace() const {
  auto imdWorkspace = DataObjects::MDEventFactory::CreateMDWorkspace(DIM_DISTRIBUTED_TEST, "MDLeanEvent");
  auto workspace = boost::dynamic_pointer_cast<DataObjects::MDEventWorkspace3Lean>(imdWorkspace);
  auto frameInformation = createFrame();
  auto extents = getWorkspaceExtents();
  for (size_t d = 0; d < DIM_DISTRIBUTED_TEST; d++) {
    MDHistoDimension *dim =
      new MDHistoDimension(frameInformation.dimensionNames[d], frameInformation.dimensionNames[d], *frameInformation.frame,
                           static_cast<coord_t>(extents[d * 2]),
                           static_cast<coord_t>(extents[d * 2 + 1]), 10);
    workspace->addDimension(MDHistoDimension_sptr(dim));
  }
  workspace->initialize();

  // Build up the box controller
  auto bc = workspace->getBoxController();
  this->setBoxController(bc);
  workspace->splitBox();

  // Perform minimum recursion depth splitting
  int minDepth = this->getProperty("MinRecursionDepth");
  int maxDepth = this->getProperty("MaxRecursionDepth");
  if (minDepth > maxDepth)
    throw std::invalid_argument(
      "MinRecursionDepth must be <= MaxRecursionDepth ");
  workspace->setMinRecursionDepth(size_t(minDepth));

  workspace->setCoordinateSystem(frameInformation.specialCoordinateSystem);
  return workspace;
}


FrameInformation ConvertToDistributedMD::createFrame() const {
  using Mantid::Kernel::SpecialCoordinateSystem;
  FrameInformation information;
  std::string outputDimensions = getProperty("OutputDimensions");
  auto frameFactory = makeMDFrameFactoryChain();

  if (outputDimensions == "Q (sample frame)") {
    // Names
    information.dimensionNames[0] = "Q_sample_x";
    information.dimensionNames[1] = "Q_sample_y";
    information.dimensionNames[2] = "Q_sample_z";
    information.specialCoordinateSystem = Mantid::Kernel::QSample;
    // Frame
    MDFrameArgument frameArgQSample(QSample::QSampleName, "");
    information.frame = frameFactory->create(frameArgQSample);
  } else if (outputDimensions == "HKL") {
    information.dimensionNames[0] = "H";
    information.dimensionNames[1] = "K";
    information.dimensionNames[2] = "L";
    information.specialCoordinateSystem = Mantid::Kernel::HKL;
    MDFrameArgument frameArgQLab(HKL::HKLName, Mantid::Kernel::Units::Symbol::RLU.ascii());
    information.frame = frameFactory->create(frameArgQLab);
  } else {
    information.dimensionNames[0] = "Q_lab_x";
    information.dimensionNames[1] = "Q_lab_y";
    information.dimensionNames[2] = "Q_lab_z";
    information.specialCoordinateSystem = Mantid::Kernel::QLab;
    MDFrameArgument frameArgQLab(QLab::QLabName, "");
    information.frame = frameFactory->create(frameArgQLab);
  }

  return information;
}


void ConvertToDistributedMD::setBoxController(Mantid::API::BoxController_sptr bc) const {
  size_t numberOfDimensions = bc->getNDims();

  int splitThreshold = this->getProperty("SplitThreshold");
  bc->setSplitThreshold(splitThreshold);
  int maxRecursionDepth = this->getProperty("MaxRecursionDepth");
  bc->setMaxDepth(maxRecursionDepth);

  std::vector<int> splits = getProperty("SplitInto");
  if (splits.size() == 1) {
    bc->setSplitInto(splits[0]);
  } else if (splits.size() == numberOfDimensions) {
    for (size_t d = 0; d < numberOfDimensions; ++d)
      bc->setSplitInto(d, splits[d]);
  } else
    throw std::invalid_argument("SplitInto parameter has " +
                                Strings::toString(splits.size()) +
                                " arguments. It should have either 1, or the "
                                  "same as the number of dimensions.");
  bc->resetNumBoxes();
}

} // namespace MDAlgorithms
} // namespace Mantid