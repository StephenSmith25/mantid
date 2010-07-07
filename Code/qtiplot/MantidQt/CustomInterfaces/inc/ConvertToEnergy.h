#ifndef MANTIDQTCUSTOMINTERFACES_CONVERTTOENERGY_H_
#define MANTIDQTCUSTOMINTERFACES_CONVERTTOENERGY_H_

//----------------------
// Includes
//----------------------
#include "MantidQtCustomInterfaces/ui_ConvertToEnergy.h"
#include "MantidQtAPI/UserSubWindow.h"


namespace MantidQt
{
namespace CustomInterfaces
{
  class Homer;

class ConvertToEnergy : public MantidQt::API::UserSubWindow
{
  Q_OBJECT

public:
  /// Default Constructor
  ConvertToEnergy(QWidget *parent = 0);
  ///Interface name
  static std::string name() { return "ConvertToEnergy"; }
  /// Aliases for this interface
  static std::set<std::string> aliases()
  { 
    std::set<std::string> aliasList;
    aliasList.insert("Homer");
    return aliasList;
  }

private:
  /// Initialize the layout
  virtual void initLayout();
  ///Init python
  virtual void initLocalPython();

private:
  //The form generated by Qt Designer
  Ui::ConvertToEnergy m_uiForm;
  /// Direct instruments
  Homer *m_directInstruments;
};

}
}

#endif //MANTIDQTCUSTOMINTERFACES_CONVERTTOENERGY_H_
