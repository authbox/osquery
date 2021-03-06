/**
 *  Copyright (c) 2014-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under both the Apache 2.0 license (found in the
 *  LICENSE file in the root directory of this source tree) and the GPLv2 (found
 *  in the COPYING file in the root directory of this source tree).
 *  You may select, at your option, one of the above-listed licenses.
 */

#include <gtest/gtest.h>

#include <osquery/dispatcher.h>

namespace osquery {

class DispatcherTests : public testing::Test {
  void TearDown() override {
    Dispatcher::instance().resetStopping();
  }
};

TEST_F(DispatcherTests, test_singleton) {
  auto& one = Dispatcher::instance();
  auto& two = Dispatcher::instance();
  EXPECT_EQ(&one, &two);
}

class TestRunnable : public InternalRunnable {
 public:
  explicit TestRunnable() : InternalRunnable("TestRunnable") {}

  virtual void start() override {
    ++i;
  }

  void reset() {
    i = 0;
  }

  size_t count() {
    return i;
  }

 private:
  static std::atomic<size_t> i;
};

std::atomic<size_t> TestRunnable::i{0};

TEST_F(DispatcherTests, test_service_count) {
  auto runnable = std::make_shared<TestRunnable>();

  auto service_count = Dispatcher::instance().serviceCount();
  // The service exits after incrementing.
  auto s = Dispatcher::addService(runnable);
  EXPECT_TRUE(s);

  // Wait for the service to stop.
  Dispatcher::joinServices();

  // Make sure the service is removed.
  EXPECT_EQ(service_count, Dispatcher::instance().serviceCount());
}

TEST_F(DispatcherTests, test_run) {
  auto runnable = std::make_shared<TestRunnable>();
  runnable->mustRun();
  runnable->reset();

  // The service exits after incrementing.
  Dispatcher::addService(runnable);
  Dispatcher::joinServices();
  EXPECT_EQ(1U, runnable->count());
  EXPECT_TRUE(runnable->hasRun());

  // This runnable cannot be executed again.
  auto s = Dispatcher::addService(runnable);
  EXPECT_FALSE(s);

  Dispatcher::joinServices();
  EXPECT_EQ(1U, runnable->count());
}

TEST_F(DispatcherTests, test_independent_run) {
  // Nothing stops two instances of the same service from running.
  auto r1 = std::make_shared<TestRunnable>();
  auto r2 = std::make_shared<TestRunnable>();
  r1->mustRun();
  r2->mustRun();
  r1->reset();

  Dispatcher::addService(r1);
  Dispatcher::addService(r2);
  Dispatcher::joinServices();

  EXPECT_EQ(2U, r1->count());
}

class BlockingTestRunnable : public InternalRunnable {
 public:
  explicit BlockingTestRunnable() : InternalRunnable("BlockingTestRunnable") {}

  virtual void start() override {
    // Wow that's a long sleep!
    pauseMilli(100 * 1000);
  }
};

TEST_F(DispatcherTests, test_interruption) {
  auto r1 = std::make_shared<BlockingTestRunnable>();
  r1->mustRun();
  Dispatcher::addService(r1);

  // This service would normally wait for 100 seconds.
  r1->interrupt();

  Dispatcher::joinServices();
  EXPECT_TRUE(r1->hasRun());
}

TEST_F(DispatcherTests, test_stop_dispatcher) {
  Dispatcher::stopServices();

  auto r1 = std::make_shared<TestRunnable>();
  auto s = Dispatcher::addService(r1);
  EXPECT_FALSE(s);
}
}
