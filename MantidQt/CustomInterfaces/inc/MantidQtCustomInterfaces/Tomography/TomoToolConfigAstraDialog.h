#ifndef MANTIDQTCUSTOMINTERFACES_TOMOTOOLCONFIGASTRADIALOG_H_
#define MANTIDQTCUSTOMINTERFACES_TOMOTOOLCONFIGASTRADIALOG_H_

#include "ui_TomoToolConfigAstra.h"
#include "MantidQtCustomInterfaces/Tomography/TomoToolConfigDialogBase.h"

namespace MantidQt {
namespace CustomInterfaces {
class TomoToolConfigAstraDialog : public QDialog,
                                  public TomoToolConfigDialogBase {
  Q_OBJECT
public:
  TomoToolConfigAstraDialog(QWidget *parent = 0)
      : QDialog(parent),
        TomoToolConfigDialogBase(DEFAULT_TOOL_NAME, DEFAULT_TOOL_METHOD) {}

private:
  void setupToolConfig() override;
  void setupDialogUi() override;
  int executeQt() override;

  // initialised in .cpp file
  static const std::string DEFAULT_TOOL_NAME;
  static const std::string DEFAULT_TOOL_METHOD;

  Ui::TomoToolConfigAstra m_astraUi;
};

} // CustomInterfaces
} // MantidQt
#endif // MANTIDQTCUSTOMINTERFACES_TOMOTOOLCONFIGASTRADIALOG_H_
