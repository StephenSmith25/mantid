#ifndef MANTIDQTMANTIDWIDGETS_GENERICDATAPROCESSORPRESENTER_H
#define MANTIDQTMANTIDWIDGETS_GENERICDATAPROCESSORPRESENTER_H

#include "MantidAPI/AlgorithmManager.h"
#include "MantidAPI/IAlgorithm_fwd.h"
#include "MantidAPI/ITableWorkspace_fwd.h"
#include "MantidQtWidgets/Common/DataProcessorUI/Command.h"
#include "MantidQtWidgets/Common/DataProcessorUI/DataProcessorMainPresenter.h"
#include "MantidQtWidgets/Common/DataProcessorUI/DataProcessorPresenter.h"
#include "MantidQtWidgets/Common/DataProcessorUI/GenericDataProcessorPresenterThread.h"
#include "MantidQtWidgets/Common/DataProcessorUI/OneLevelTreeManager.h"
#include "MantidQtWidgets/Common/DataProcessorUI/PostprocessingAlgorithm.h"
#include "MantidQtWidgets/Common/DataProcessorUI/PreprocessMap.h"
#include "MantidQtWidgets/Common/DataProcessorUI/PreprocessingAlgorithm.h"
#include "MantidQtWidgets/Common/DataProcessorUI/ProcessingAlgorithm.h"
#include "MantidQtWidgets/Common/DataProcessorUI/TreeData.h"
#include "MantidQtWidgets/Common/DataProcessorUI/TwoLevelTreeManager.h"
#include "MantidQtWidgets/Common/DataProcessorUI/WhiteList.h"
#include "MantidQtWidgets/Common/ParseKeyValueString.h"
#include "MantidQtWidgets/Common/DllOption.h"
#include "MantidQtWidgets/Common/ProgressPresenter.h"
#include "MantidQtWidgets/Common/WorkspaceObserver.h"
#include "boost/optional.hpp"

#include <QSet>
#include <queue>

#include <QObject>

namespace MantidQt {
namespace MantidWidgets {
class ProgressableView;
namespace DataProcessor {
// Forward decs
class DataProcessorView;
class TreeManager;
class GenericDataProcessorPresenterThread;

using RowItem = std::pair<int, RowData>;
using RowQueue = std::queue<RowItem>;
using GroupQueue = std::queue<std::pair<int, RowQueue>>;

/** @class GenericDataProcessorPresenter

GenericDataProcessorPresenter is a presenter class for the Data Processor
Interface. It handles any interface functionality and model manipulation.

Copyright &copy; 2011-16 ISIS Rutherford Appleton Laboratory, NScD Oak Ridge
National Laboratory & European Spallation Source

This file is part of Mantid.

Mantid is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

Mantid is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

File change history is stored at: <https://github.com/mantidproject/mantid>.
Code Documentation is available at: <http://doxygen.mantidproject.org>
*/
struct PreprocessingAttributes {
  PreprocessingAttributes(const QString &options) : m_options(options) {}
  PreprocessingAttributes(const QString &options,
                          std::map<QString, PreprocessingAlgorithm> map)
      : m_options(options), m_map(map) {}
  QString m_options;
  std::map<QString, PreprocessingAlgorithm> m_map;
};

struct PostprocessingAttributes {
  PostprocessingAttributes(const QString &options) : m_options(options) {}
  PostprocessingAttributes(const QString &options,
                           PostprocessingAlgorithm algorithm,
                           std::map<QString, QString> map)
      : m_options(options), m_algorithm(algorithm), m_map(map) {}
  QString m_options;
  // Post-processing algorithm
  PostprocessingAlgorithm m_algorithm;
  // Post-processing map
  std::map<QString, QString> m_map;

  QString getPostprocessedWorkspaceName(const WhiteList &whitelist,
                                        const GroupData &groupData,
                                        const QString &prefix) {
    QStringList outputNames;
    for (const auto &data : groupData) {
      outputNames.append(
          getReducedWorkspaceName(whitelist, data.second, prefix));
    }
    return prefix + outputNames.join("_");
  }

  QString getReducedWorkspaceName(const WhiteList &whitelist,
                                  const QStringList &data,
                                  const QString &prefix) {

    if (static_cast<std::size_t>(data.size()) != whitelist.size())
      throw std::invalid_argument("Can't find reduced workspace name");

    /* This method calculates, for a given row, the name of the output
    * (processed)
    * workspace. This is done using the white list, which contains information
    * about the columns that should be included to create the ws name. In
    * Reflectometry for example, we want to include values in the 'Run(s)' and
    * 'Transmission Run(s)' columns. We may also use a prefix associated with
    * the column when specified. Finally, to construct the ws name we may also
    * use a 'global' prefix associated with the processing algorithm (for
    * instance 'IvsQ_' in Reflectometry) this is given by the second argument to
    * this method */

    // Temporary vector of strings to construct the name
    QStringList names;

    auto columnIt = whitelist.cbegin();
    auto runNumbersIt = data.constBegin();
    for (; columnIt != whitelist.cend(); ++columnIt, ++runNumbersIt) {
      auto column = *columnIt;
      // Do we want to use this column to generate the name of the output ws?
      if (column.isShown()) {
        auto const runNumbers = *runNumbersIt;

        if (!runNumbers.isEmpty()) {
          // But we may have things like '1+2' which we want to replace with
          // '1_2'
          auto value = runNumbers.split("+", QString::SkipEmptyParts);
          names.append(column.prefix() + value.join("_"));
        }
      }
    } // Columns

    auto wsname = prefix;
    wsname += names.join("_");
    return wsname;
  }

  void removeWorkspace(QString const &workspaceName) const {
    Mantid::API::AnalysisDataService::Instance().remove(
        workspaceName.toStdString());
  }

  bool workspaceExists(QString const &workspaceName) const {
    return Mantid::API::AnalysisDataService::Instance().doesExist(
        workspaceName.toStdString());
  }

  void removeIfExists(QString const &workspaceName) const {
    if (workspaceExists(workspaceName)) {
      removeWorkspace(workspaceName);
    }
  }

  void postprocessGroup(QString const &processorPrefix,
                        const GroupData &groupData,
                        WhiteList const &whitelist) {
    QStringList inputNames;

    auto const outputWSName = getPostprocessedWorkspaceName(
        whitelist, groupData, m_algorithm.prefix());

    for (auto const &row : groupData) {
      auto const inputWSName =
          getReducedWorkspaceName(whitelist, row.second, processorPrefix);

      if (workspaceExists(inputWSName)) {
        inputNames.append(inputWSName);
      }
    }

    auto const inputWSNames = inputNames.join(", ");

    // If the previous result is in the ADS already, we'll need to remove it.
    // If it's a group, we'll get an error for trying to group into a used group
    // name
    removeIfExists(outputWSName);

    auto alg =
        Mantid::API::AlgorithmManager::Instance().create(m_algorithm.name().toStdString());
    alg->initialize();
    alg->setProperty(m_algorithm.inputProperty().toStdString(),
                              inputWSNames.toStdString());
    alg->setProperty(m_algorithm.outputProperty().toStdString(),
                              outputWSName.toStdString());

    auto optionsMap = parseKeyValueString(m_options.toStdString());
    for (auto kvp = optionsMap.begin(); kvp != optionsMap.end(); ++kvp) {
      try {
        alg->setProperty(kvp->first, kvp->second);
      } catch (Mantid::Kernel::Exception::NotFoundError &) {
        throw std::runtime_error("Invalid property in options column: " +
                                 kvp->first);
      }
    }

    // Options specified via post-process map
    for (auto const &prop : m_map) {
      auto const propName = prop.second;
      auto const propValueStr =
          groupData.begin()->second[whitelist.indexFromName(prop.first)];
      if (!propValueStr.isEmpty()) {
        // Warning: we take minus the value of the properties because in
        // Reflectometry this property refers to the rebin step, and they want a
        // logarithmic binning. If other technique areas need to use a
        // post-process map we'll need to re-think how to do this.
        alg->setPropertyValue(propName.toStdString(),
                              ("-" + propValueStr).toStdString());
      }
    }

    alg->execute();

    if (!alg->isExecuted())
      throw std::runtime_error("Failed to post-process workspaces.");
  }
};

class EXPORT_OPT_MANTIDQT_COMMON GenericDataProcessorPresenter
    : public QObject,
      public DataProcessorPresenter,
      public MantidQt::API::WorkspaceObserver {
  // Q_OBJECT for 'connect' with thread/worker
  Q_OBJECT

  friend class GenericDataProcessorPresenterRowReducerWorker;
  friend class GenericDataProcessorPresenterGroupReducerWorker;

public:
  // Constructor: pre-processing and post-processing
  GenericDataProcessorPresenter(
      const WhiteList &whitelist,
      const std::map<QString, PreprocessingAlgorithm> &preprocessMap,
      const ProcessingAlgorithm &processor,
      const PostprocessingAlgorithm &postprocessor,
      const std::map<QString, QString> &postprocessMap =
          std::map<QString, QString>(),
      const QString &loader = "Load");
  // Constructor: no pre-processing, post-processing
  GenericDataProcessorPresenter(const WhiteList &whitelist,
                                const ProcessingAlgorithm &processor,
                                const PostprocessingAlgorithm &postprocessor);
  // Constructor: pre-processing, no post-processing
  GenericDataProcessorPresenter(
      const WhiteList &whitelist,
      const std::map<QString, PreprocessingAlgorithm> &preprocessMap,
      const ProcessingAlgorithm &processor);
  // Constructor: no pre-processing, no post-processing
  GenericDataProcessorPresenter(const WhiteList &whitelist,
                                const ProcessingAlgorithm &processor);
  // Constructor: only whitelist
  GenericDataProcessorPresenter(const WhiteList &whitelist);
  // Delegating constructor: pre-processing, no post-processing
  GenericDataProcessorPresenter(const WhiteList &whitelist,
                                const PreprocessMap &preprocessMap,
                                const ProcessingAlgorithm &processor);
  // Delegating Constructor: pre-processing and post-processing
  GenericDataProcessorPresenter(const WhiteList &whitelist,
                                const PreprocessMap &preprocessMap,
                                const ProcessingAlgorithm &processor,
                                const PostprocessingAlgorithm &postprocessor);
  virtual ~GenericDataProcessorPresenter() override;
  void notify(DataProcessorPresenter::Flag flag) override;
  const std::map<QString, QVariant> &options() const override;
  void setOptions(const std::map<QString, QVariant> &options) override;
  void transfer(const std::vector<std::map<QString, QString>> &runs) override;
  void setInstrumentList(const QStringList &instruments,
                         const QString &defaultInstrument) override;
  std::vector<std::unique_ptr<Command>> publishCommands() override;
  void acceptViews(DataProcessorView *tableView,
                   ProgressableView *progressView) override;
  void accept(DataProcessorMainPresenter *mainPresenter) override;
  void setModel(QString const &name) override;

  // The following methods are public only for testing purposes
  // Get the whitelist
  WhiteList getWhiteList() const { return m_whitelist; };
  // Get the name of the reduced workspace for a given row
  QString getReducedWorkspaceName(const QStringList &data,
                                  const QString &prefix = "");
  // Get the name of a post-processed workspace
  QString getPostprocessedWorkspaceName(const GroupData &groupData,
                                        const QString &prefix = "");

  ParentItems selectedParents() const override;
  ChildItems selectedChildren() const override;
  bool askUserYesNo(const QString &prompt, const QString &title) const override;
  void giveUserWarning(const QString &prompt,
                       const QString &title) const override;
  bool isProcessing() const override;
  void setForcedReProcessing(bool forceReProcessing) override;

protected:
  template <typename T> using QOrderedSet = QMap<T, std::nullptr_t>;
  // The table view we're managing
  DataProcessorView *m_view;
  // The progress view
  ProgressableView *m_progressView;
  // A workspace receiver we want to notify
  DataProcessorMainPresenter *m_mainPresenter;
  // The tree manager, a proxy class to retrieve data from the model
  std::unique_ptr<TreeManager> m_manager;
  // Loader
  QString m_loader;
  // The list of selected items to reduce
  TreeData m_selectedData;
  void setPreprocessingOptions(QString const &options) {
    m_preprocessing.m_options = options;
  }

  void setPostprocessingOptions(QString const &options) {
    m_postprocessing.get().m_options = options;
  }

  boost::optional<PostprocessingAttributes> m_postprocessing;

  // Pre-processing options
  PreprocessingAttributes m_preprocessing;
  // Data processor options
  QString m_processingOptions;
  void updateProcessedStatus(const std::pair<int, GroupData> &group);
  // Post-process some rows
  void postProcessGroup(const GroupData &data);
  // Reduce a row
  void reduceRow(RowData *data);
  // Finds a run in the AnalysisDataService
  QString findRunInADS(const QString &run, const QString &prefix,
                       bool &runFound);
  // Sets whether to prompt user when getting selected runs
  void setPromptUser(bool allowPrompt);

  // Process selected rows
  virtual void process();
  // Plotting
  virtual void plotRow();
  virtual void plotGroup();
  void plotWorkspaces(const QOrderedSet<QString> &workspaces);

protected slots:
  void reductionError(QString ex);
  void threadFinished(const int exitCode);
  void issueNotFoundWarning(QString const &granule,
                            QSet<QString> const &missingWorkspaces);

private:
  bool areOptionsUpdated();
  void applyDefaultOptions(std::map<QString, QVariant> &options);
  void setPropertiesFromKeyValueString(Mantid::API::IAlgorithm_sptr alg,
                                       const std::string &hiddenOptions,
                                       const std::string &columnName);
  Mantid::API::IAlgorithm_sptr createProcessingAlgorithm() const;
  // the name of the workspace/table/model in the ADS, blank if unsaved
  QString m_wsName;
  // The whitelist
  WhiteList m_whitelist;
  // The data processor algorithm
  ProcessingAlgorithm m_processor;

  // The current queue of groups to be reduced
  GroupQueue m_group_queue;
  // The current group we are reducing row data for
  GroupData m_groupData;
  // The current row item being reduced
  RowItem m_rowItem;
  // The progress reporter
  ProgressPresenter *m_progressReporter;
  // The number of columns
  int m_columns;
  // A boolean indicating whether to prompt the user when getting selected runs
  bool m_promptUser;
  // stores whether or not the table has changed since it was last saved
  bool m_tableDirty;
  // stores the user options for the presenter
  std::map<QString, QVariant> m_options;
  // Thread to run reducer worker in
  std::unique_ptr<GenericDataProcessorPresenterThread> m_workerThread;
  // A boolean that can be set to pause reduction of the current item
  bool m_pauseReduction;
  // A boolean indicating whether data reduction is confirmed paused
  bool m_reductionPaused;
  // Enumeration of the reduction actions that can be taken
  enum class ReductionFlag { ReduceRowFlag, ReduceGroupFlag, StopReduceFlag };
  // A flag of the next action due to be carried out
  ReductionFlag m_nextActionFlag;
  // load a run into the ADS, or re-use one in the ADS if possible
  Mantid::API::Workspace_sptr
  getRun(const QString &run, const QString &instrument, const QString &prefix);
  // Loads a run from disk
  QString loadRun(const QString &run, const QString &instrument,
                  const QString &prefix, const QString &loader, bool &runFound);
  // prepare a run or list of runs for processing
  Mantid::API::Workspace_sptr
  prepareRunWorkspace(const QString &run, const PreprocessingAlgorithm &alg,
                      const std::map<std::string, std::string> &optionsMap);
  // add row(s) to the model
  void appendRow();
  // add group(s) to the model
  void appendGroup();
  // delete row(s) from the model
  void deleteRow();
  // delete group(s) from the model
  void deleteGroup();
  // clear selected row(s) in the model
  void clearSelected();
  // copy selected rows to clipboard
  void copySelected();
  // copy selected rows to clipboard and then delete them
  void cutSelected();
  // paste clipboard into selected rows
  void pasteSelected();
  // group selected rows together
  void groupRows();
  // expand selection to group
  void expandSelection();
  // expand all groups
  void expandAll();
  // close all groups
  void collapseAll();
  // select all rows / groups
  void selectAll();
  // table io methods
  void newTable();
  void openTable();
  void saveTable();
  void saveTableAs();
  void importTable();
  void exportTable();

  // options
  void showOptionsDialog();
  void initOptions();

  // actions/commands
  void addCommands();

  // decide between processing next row or group
  void doNextAction();

  // process next row/group
  void nextRow();
  void nextGroup();

  // start thread for performing reduction on current row/group asynchronously
  virtual void startAsyncRowReduceThread(RowItem *rowItem, int groupIndex);
  virtual void startAsyncGroupReduceThread(GroupData &groupData,
                                           int groupIndex);

  // end reduction
  void endReduction();

  // pause/resume reduction
  void pause();
  void resume();

  // Check if run has been processed
  bool isProcessed(int position) const;
  bool isProcessed(int position, int parent) const;
  bool m_forceProcessing = false;

  // List of workspaces the user can open
  QSet<QString> m_workspaceList;

  void addHandle(const std::string &name,
                 Mantid::API::Workspace_sptr workspace) override;
  void postDeleteHandle(const std::string &name) override;
  void clearADSHandle() override;
  void renameHandle(const std::string &oldName,
                    const std::string &newName) override;
  void afterReplaceHandle(const std::string &name,
                          Mantid::API::Workspace_sptr workspace) override;
  void saveNotebook(const TreeData &data);
  std::vector<std::unique_ptr<Command>> getTableList();

  // set/get values in the table
  void setCell(int row, int column, int parentRow, int parentColumn,
               const std::string &value) override;
  std::string getCell(int row, int column, int parentRow,
                      int parentColumn) override;
  int getNumberOfRows() override;
  void clearTable() override;
};
}
}
}
#endif /*MANTIDQTMANTIDWIDGETS_GENERICDATAPROCESSORPRESENTER_H*/
