// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +
//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------
#include "MantidKernel/MersenneTwister.h"

#include "MantidKernel/uniform_int_distribution.h"

#include <Poco/Timestamp.h>

namespace Mantid {
namespace Kernel {

//------------------------------------------------------------------------------
// Public member functions
//------------------------------------------------------------------------------

/**
 * Constructor taking a seed value. Sets the range to [0.0,1.0]
 * @param seedValue :: The initial seed
 */
MersenneTwister::MersenneTwister(const size_t seedValue)
    : MersenneTwister(seedValue, 0.0, 1.0) {}

/**
 * Construct the generator time stamp for the initial seed.
 * The range is set to [0.0, 1.0]
 */
MersenneTwister::MersenneTwister() : MersenneTwister(0.0, 1.0) {}

/**
 * Constructor taking a range
 * @param start :: The minimum value a generated number should take
 * @param end :: The maximum value a generated number should take
 */
MersenneTwister::MersenneTwister(const double start, const double end)
    : MersenneTwister(Poco::Timestamp().epochMicroseconds(), start, end) {}

/**
 * Constructor taking a seed value and a range
 * @param seedValue :: The initial seed
 * @param start :: The minimum value a generated number should take
 * @param end :: The maximum value a generated number should take
 */
MersenneTwister::MersenneTwister(const size_t seedValue, const double start,
                                 const double end)
    : m_engine(), uniformRealDistribution(start, end), m_start(start),
      m_end(end), m_seed(), m_savedEngine() {
  setSeed(seedValue);
}

/**
 * (Re-)seed the generator. This resets the current saved state
 * @param seedValue :: A seed for the generator
 */
void MersenneTwister::setSeed(const size_t seedValue) {
  m_seed = static_cast<std::mt19937::result_type>(seedValue);
  m_engine.seed(m_seed);
  m_savedEngine = std::make_unique<std::mt19937>(m_engine);
}

/**
 * Sets the range of the subsequent calls to nextValue()
 * @param start :: The lowest value a call to nextValue() will produce
 * @param end :: The largest value a call to nextValue() will produce
 */
void MersenneTwister::setRange(const double start, const double end) {
  m_start = start;
  m_end = end;
  uniformRealDistribution = std::uniform_real_distribution<double>(start, end);
}

/**
 * Returns the next integer in the pseudo-random sequence generated by
 * the Mersenne Twister 19937 algorithm.
 * @param start Start of the requested range
 * @param end End of the requested range
 * @return An integer in the defined range
 */
int MersenneTwister::nextInt(int start, int end) {
  return Mantid::Kernel::uniform_int_distribution<int>(start, end)(m_engine);
}

/**
 * Resets the generator using the value given at the last call to setSeed
 */
void MersenneTwister::restart() { setSeed(m_seed); }

/// Saves the current state of the generator
void MersenneTwister::save() {
  m_savedEngine = std::make_unique<std::mt19937>(m_engine);
}

/// Restores the generator to the last saved point, or the beginning if nothing
/// has been saved
void MersenneTwister::restore() {
  // Copy saved to current, still distinct objects so that another restore still
  // brings us
  // back to the originally saved point
  if (m_savedEngine) {
    m_engine = std::mt19937(*m_savedEngine);
  } else {
    restart();
  }
}
} // namespace Kernel
} // namespace Mantid
