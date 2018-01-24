#ifndef MANTID_ISISREFLECTOMETRY_INSTRUMENTPARAMETERS_H
#define MANTID_ISISREFLECTOMETRY_INSTRUMENTPARAMETERS_H
#include <vector>
#include <boost/optional.hpp>
#include "GetInstrumentParameter.h"
#include "First.h"

namespace MantidQt {
namespace CustomInterfaces {

template <typename T>
boost::optional<T>
firstFromParameterFile(Mantid::Geometry::Instrument_const_sptr instrument,
                       std::string const &parameterName) {
  return first(getInstrumentParameter<T>(instrument, parameterName));
}

class MissingInstrumentParameterValue {
public:
  explicit MissingInstrumentParameterValue(std::string const &parameterName)
      : m_parameterName(parameterName) {}

  std::string const &parameterName() const;

private:
  std::string m_parameterName;
};

class InstrumentParameters {
public:
  explicit InstrumentParameters(
      Mantid::Geometry::Instrument_const_sptr instrument);

  template <typename T> T valueOrEmpty(std::string const &parameterName) {
    static_assert(!std::is_arithmetic<T>::value, "Use valueOrZero instead.");
    return valueFromFileOrDefaultConstruct<T>(parameterName);
  }

  template <typename T> T valueOrZero(std::string const &parameterName) {
    static_assert(std::is_arithmetic<T>::value, "Use valueOrEmpty instead.");
    return valueFromFileOrDefaultConstruct<T>(parameterName);
  }

  template <typename T>
  boost::optional<T> optional(std::string const &parameterName) {
    return valueFromFile<T>(parameterName);
  }

  template <typename T> T mandatory(std::string const &parameterName) {
    try {
      if (auto value = firstFromParameterFile<T>(m_instrument, parameterName)) {
        return value.get();
      } else {
        m_missingValueErrors.emplace_back(parameterName);
        return T();
      }
    } catch (InstrumentParameterTypeMissmatch const& ex) {
      m_typeErrors.emplace_back(ex);
      return T();
    }
  }

  std::vector<InstrumentParameterTypeMissmatch> const &typeErrors() const;
  bool hasTypeErrors() const;
  std::vector<MissingInstrumentParameterValue> const &missingValues() const;
  bool hasMissingValues() const;

private:
  template <typename T>
  T valueFromFileOrDefaultConstruct(std::string const &parameterName) {
    return valueFromFile<T>(parameterName).value_or(T());
  }

  template <typename T>
  boost::optional<T> valueFromFile(std::string const &parameterName) {
    try {
      return firstFromParameterFile<T>(m_instrument, parameterName);
    } catch (InstrumentParameterTypeMissmatch const &ex) {
      m_typeErrors.emplace_back(ex);
      return boost::none;
    }
  }

  Mantid::Geometry::Instrument_const_sptr m_instrument;
  std::vector<InstrumentParameterTypeMissmatch> m_typeErrors;
  std::vector<MissingInstrumentParameterValue> m_missingValueErrors;
};
}
}
#endif // MANTID_ISISREFLECTOMETRY_INSTRUMENTPARAMETERS_H
