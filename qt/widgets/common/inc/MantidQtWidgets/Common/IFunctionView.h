// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +
#ifndef MANTIDWIDGETS_IFUNCTIONVIEW_H_
#define MANTIDWIDGETS_IFUNCTIONVIEW_H_

#include "DllOption.h"

#include "MantidAPI/IFunction_fwd.h"

#include <QString>
#include <QWidget>

namespace MantidQt {
namespace MantidWidgets {

using namespace Mantid::API;

/**
 * The interface to a function view.
 */
class EXPORT_OPT_MANTIDQT_COMMON IFunctionView: public QWidget {
  Q_OBJECT
public:
  IFunctionView(QWidget *parent) : QWidget(parent) {}
  virtual ~IFunctionView() {}
  virtual void clear() = 0;
  virtual void setFunction(IFunction_sptr fun) = 0;
  virtual bool hasFunction() const = 0;
  virtual void setParameter(const QString &funcIndex, const QString &paramName, double value) = 0;
  virtual void setParamError(const QString &funcIndex, const QString &paramName, double error) = 0;
  virtual double getParameter(const QString &funcIndex, const QString &paramName) const = 0;
  virtual void setErrorsEnabled(bool enabled) = 0;
  virtual void clearErrors() = 0;

signals:
  /// User selects a different function (or one of it's sub-properties)
  void currentFunctionChanged();
  /// Function parameter gets changed
  void parameterChanged(const QString &funcIndex, const QString &paramName);
  /// In multi-dataset context a button value editor was clicked
  void localParameterButtonClicked(const QString &parName);
  void functionStructureChanged();
  void globalsChanged();
  void constraintsChanged();
  void tiesChanged();
};

} // namespace MantidWidgets
} // namespace MantidQt

#endif /*MANTIDWIDGETS_IFUNCTIONVIEW_H_*/