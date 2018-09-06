#ifndef MANTID_CUSTOMINTERFACES_REFLBATCHPRESENTERGROUPDELETETEST_H_
#define MANTID_CUSTOMINTERFACES_REFLBATCHPRESENTERGROUPDELETETEST_H_

#include "../../../ISISReflectometry/Presenters/BatchPresenter.h"
#include "../../../ISISReflectometry/Reduction/ReductionWorkspaces.h"
#include "BatchPresenterTest.h"

#include <cxxtest/TestSuite.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace MantidQt::CustomInterfaces;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

class BatchPresenterGroupDeletionTest : public CxxTest::TestSuite,
                                        BatchPresenterTest {
public:
  static BatchPresenterGroupDeletionTest *createSuite() {
    return new BatchPresenterGroupDeletionTest();
  }

  static void destroySuite(BatchPresenterGroupDeletionTest *suite) {
    delete suite;
  }

  void testUpdatesViewWhenGroupDeletedFromDirectSelection() {
    auto reductionJobs = twoEmptyGroupsModel();
    selectedRowLocationsAre(m_jobs, {location(0)});

    EXPECT_CALL(m_jobs, removeRowAt(location(0)));

    auto presenter = makePresenter(m_view, std::move(reductionJobs));
    presenter.notifyDeleteGroupRequested();

    verifyAndClearExpectations();
  }

  void testUpdatesModelWhenGroupDeletedFromDirectSelection() {
    selectedRowLocationsAre(m_jobs, {location(0)});

    auto presenter = makePresenter(m_view, twoEmptyGroupsModel());
    presenter.notifyDeleteGroupRequested();

    auto &groups = unslicedJobsFromPresenter(presenter).groups();
    TS_ASSERT_EQUALS(1, groups.size());
    TS_ASSERT_EQUALS("Group 2", groups[0].name());

    verifyAndClearExpectations();
  }

  void testUpdatesModelWhenGroupDeletedFromMultiselection() {
    selectedRowLocationsAre(m_jobs, {location(0), location(1)});

    auto presenter = makePresenter(m_view, twoEmptyGroupsModel());
    presenter.notifyDeleteGroupRequested();

    auto &groups = unslicedJobsFromPresenter(presenter).groups();
    TS_ASSERT_EQUALS(0, groups.size());

    verifyAndClearExpectations();
  }

  void testUpdatesViewWhenGroupDeletedFromMultiSelection() {
    auto reductionJobs = twoGroupsWithARowModel();
    selectedRowLocationsAre(m_jobs, {location(0), location(1)});

    {
      testing::InSequence s;
      EXPECT_CALL(m_jobs, removeRowAt(location(1)));
      EXPECT_CALL(m_jobs, removeRowAt(location(0)));
    }

    auto presenter = makePresenter(m_view, std::move(reductionJobs));
    presenter.notifyDeleteGroupRequested();

    verifyAndClearExpectations();
  }

  void testUpdatesViewWhenGroupDeletedFromChildRowSelection() {
    auto reductionJobs = twoGroupsWithARowModel();
    selectedRowLocationsAre(m_jobs, {location(0, 0)});

    EXPECT_CALL(m_jobs, removeRowAt(location(0)));

    auto presenter = makePresenter(m_view, std::move(reductionJobs));
    presenter.notifyDeleteGroupRequested();

    verifyAndClearExpectations();
  }

  void testUpdatesViewWhenGroupDeletedFromChildRowMultiSelection() {
    auto reductionJobs = twoGroupsWithARowModel();
    selectedRowLocationsAre(m_jobs, {location(0, 0), location(1, 0)});

    {
      testing::InSequence s;
      EXPECT_CALL(m_jobs, removeRowAt(location(1)));
      EXPECT_CALL(m_jobs, removeRowAt(location(0)));
    }

    auto presenter = makePresenter(m_view, std::move(reductionJobs));
    presenter.notifyDeleteGroupRequested();

    verifyAndClearExpectations();
  }
};

#endif // MANTID_CUSTOMINTERFACES_REFLBATCHPRESENTERGROUPDELETETEST_H_