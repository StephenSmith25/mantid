// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2019 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +

#ifndef MANTIDWIDGETS_SCRIPTREPOSITORYVIEWTEST_H_
#define MANTIDWIDGETS_SCRIPTREPOSITORYVIEWTEST_H_

#include <cxxtest/TestSuite.h>
#include <gmock/gmock.h>

#include "MantidAPI/ScriptRepositoryFactory.h"
#include "MantidQtWidgets/Common/ScriptRepositoryView.h"
#include "MockScriptRepository.h"
#include "MantidAPI/ScriptRepositoryFactory.h"

using MantidQt::API::ScriptRepositoryView;
using namespace testing;




class ScriptRepositoryViewTest : public CxxTest::TestSuite {
public:
  // This pair of boilerplate methods prevent the suite being created statically
  // This means the constructor isn't called when running other tests
  static ScriptRepositoryViewTest *createSuite() {
    return new ScriptRepositoryViewTest();
  }
  static void destroySuite(ScriptRepositoryViewTest *suite) { delete suite; }

  void test_scriptRepository_tries_to_install_before_creation() {

		}

  void test_no_widget_is_created_if_install_fails() {}

  void test_widget_is_created_if_already_install() {
		setUpMockRepo();
		ScriptRepositoryView repo;

	}

  void test_file_loads_if_cell_selected() {}

  void test_fields_update_when_current_selection_changes() {}

  void test_repodelegate() {}

  void test_checkboxdelegate() {}

  void test_RemoveEntryDelegate() {}

  void test_Openfolderlink() {}

private:
	void setUpMockRepo(bool valid = true) {
		if (valid)
			Mantid::API::ScriptRepositoryFactory::Instance().subscribe<Mantid::API::MockValidScriptRepositoryImpl>("ScriptRepositoryImpl");
		else
			Mantid::API::ScriptRepositoryFactory::Instance().subscribe<Mantid::API::MockInvalidScriptRepositoryImpl>("ScriptRepositoryImpl");
	}
};

#endif /*MANTIDWIDGETS_SCRIPTREPOSITORYVIEWTEST_H_ */
