#ifndef MUONFITPROPERTYBROWSER_H_
#define MUONFITPROPERTYBROWSER_H_

#include "MantidQtMantidWidgets/FitPropertyBrowser.h"

/* Forward declarations */

class QtTreePropertyBrowser;
class QtGroupPropertyManager;
class QtDoublePropertyManager;
class QtIntPropertyManager;
class QtBoolPropertyManager;
class QtStringPropertyManager;
class QtEnumPropertyManager;
class QtProperty;
class QtBrowserItem;

namespace Mantid
{
  namespace API
  {
    class IFitFunction;
    class IPeakFunction;
    class CompositeFunction;
  }
}

namespace MantidQt
{
namespace MantidWidgets
{
class PropertyHandler;

class EXPORT_OPT_MANTIDQT_MANTIDWIDGETS MuonFitPropertyBrowser: public MantidQt::MantidWidgets::FitPropertyBrowser
{  
  Q_OBJECT

public:
  /// Constructor.
  MuonFitPropertyBrowser(QWidget *parent = NULL, QObject* mantidui = NULL);
  /// Set the input workspace name
  virtual void setWorkspaceName(const QString& wsName);
  /// Initialise the layout.
  virtual void init();

public slots:
  /// Perform the fit algorithm
  virtual void fit();


protected:
  virtual void showEvent(QShowEvent* e);


private slots:
  virtual void doubleChanged(QtProperty* prop);


private:  
  /// Get the registered function names
  virtual void populateFunctionNames();
  /// Enable/disable the Fit button;
  virtual void setFitEnabled(bool yes);
  /// Sets the new workspace to the current one
  virtual void workspaceChange(const QString& wsName);

};

} // MantidQt
} // API


#endif /*MUONFITPROPERTYBROWSER_H_*/