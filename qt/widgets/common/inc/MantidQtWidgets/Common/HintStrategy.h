// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2014 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +
#ifndef MANTID_MANTIDWIDGETS_HINTSTRATEGY_H
#define MANTID_MANTIDWIDGETS_HINTSTRATEGY_H

#include "DllOption.h"
#include "MantidQtWidgets/Common/Hint.h"
#include <string>
#include <vector>

namespace MantidQt {
namespace MantidWidgets {
/** HintStrategy : Provides an interface for generating hints to be used by a
HintingLineEdit.
*/
class EXPORT_OPT_MANTIDQT_COMMON HintStrategy {
public:
  HintStrategy(){};
  virtual ~HintStrategy() = default;

  /** Create a list of hints for auto completion.
    * This is overwritten on the python side.
    * However, leaving this as abstract method causes SIP issues
    * in _common.sip

      @returns A map of keywords to short descriptions for the keyword.
   */
  virtual std::vector<Hint> createHints() { return std::vector<Hint>(); };
};
} // namespace MantidWidgets
} // namespace MantidQt

#endif /* MANTID_MANTIDWIDGETS_HINTSTRATEGY_H */
