/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the LICENSE
 * file in the root directory of this source tree.
 */
#include <hermes/SynthTraceParser.h>

#include <gtest/gtest.h>

namespace {

using namespace facebook::hermes::tracing;

struct SynthTraceParserTest : public ::testing::Test {
  std::unique_ptr<llvm::MemoryBuffer> bufFromStr(const std::string &str) {
    llvm::StringRef ref{str.data(), str.size()};
    return llvm::MemoryBuffer::getMemBufferCopy(ref);
  }
};

TEST_F(SynthTraceParserTest, ParseHeader) {
  const char *src = R"(
{
  "version": 2,
  "globalObjID": 258,
  "sourceHash": "6440b537af26795e5f452bcd320faccb02055a4f",
  "runtimeConfig": {
    "gcConfig": {
      "minHeapSize": 1000,
      "initHeapSize": 33554432,
      "maxHeapSize": 536870912,
      "occupancyTarget": 0.75,
      "effectiveOOMThreshold": 20,
      "shouldReleaseUnused": "none",
      "name": "foo",
      "allocInYoung": false,
    },
    "maxNumRegisters": 100,
    "ES6Symbol": false,
    "enableSampledStats": true,
    "vmExperimentFlags": 123
  },
  "env": {
    "mathRandomSeed": 123,
    "callsToDateNow": [],
    "callsToNewDate": [],
    "callsToDateAsFunction": [],
  },
  "trace": []
}
)";
  auto result = parseSynthTrace(bufFromStr(src));
  const SynthTrace &trace = std::get<0>(result);
  const hermes::vm::RuntimeConfig &rtconf = std::get<1>(result);
  const hermes::vm::MockedEnvironment &env = std::get<2>(result);

  hermes::SHA1 expectedHash{{0x64, 0x40, 0xb5, 0x37, 0xaf, 0x26, 0x79,
                             0x5e, 0x5f, 0x45, 0x2b, 0xcd, 0x32, 0x0f,
                             0xac, 0xcb, 0x02, 0x05, 0x5a, 0x4f}};
  EXPECT_EQ(trace.sourceHash(), expectedHash);
  EXPECT_EQ(trace.records().size(), 0);

  const ::hermes::vm::GCConfig &gcconf = rtconf.getGCConfig();
  EXPECT_EQ(gcconf.getMinHeapSize(), 1000);
  EXPECT_EQ(gcconf.getInitHeapSize(), 33554432);
  EXPECT_EQ(gcconf.getMaxHeapSize(), 536870912);
  EXPECT_EQ(gcconf.getOccupancyTarget(), 0.75);
  EXPECT_EQ(gcconf.getEffectiveOOMThreshold(), 20);
  EXPECT_FALSE(gcconf.getShouldReleaseUnused());
  EXPECT_EQ(gcconf.getName(), "foo");
  EXPECT_FALSE(gcconf.getAllocInYoung());

  EXPECT_EQ(rtconf.getMaxNumRegisters(), 100);
  EXPECT_FALSE(rtconf.getES6Symbol());
  EXPECT_TRUE(rtconf.getEnableSampledStats());
  EXPECT_EQ(rtconf.getVMExperimentFlags(), 123);

  EXPECT_EQ(env.mathRandomSeed, 123);
  EXPECT_EQ(env.callsToDateNow.size(), 0);
  EXPECT_EQ(env.callsToNewDate.size(), 0);
  EXPECT_EQ(env.callsToDateAsFunction.size(), 0);
}

TEST_F(SynthTraceParserTest, RuntimeConfigDefaults) {
  const char *src = R"(
{
  "version": 2,
  "globalObjID": 258,
  "sourceHash": "6440b537af26795e5f452bcd320faccb02055a4f",
  "runtimeConfig": {},
  "env": {
    "mathRandomSeed": 123,
    "callsToDateNow": [],
    "callsToNewDate": [],
    "callsToDateAsFunction": [],
  },
  "trace": []
}
  )";
  auto result = parseSynthTrace(bufFromStr(src));
  const hermes::vm::RuntimeConfig &rtconf = std::get<1>(result);

  EXPECT_EQ(rtconf.getGCConfig().getMinHeapSize(), 0);
  EXPECT_EQ(rtconf.getGCConfig().getInitHeapSize(), 33554432);
  EXPECT_EQ(rtconf.getGCConfig().getMaxHeapSize(), 536870912);
  EXPECT_FALSE(rtconf.getEnableSampledStats());
}

TEST_F(SynthTraceParserTest, SynthVersionMismatch) {
  const char *src = R"(
{
  "version": 0,
  "globalObjID": 258,
  "sourceHash": "6440b537af26795e5f452bcd320faccb02055a4f",
  "runtimeConfig": {
    "gcConfig": {
      "initHeapSize": 33554432,
      "maxHeapSize": 536870912
    }
  },
  "env": {
    "mathRandomSeed": 123,
    "callsToDateNow": [],
    "callsToNewDate": [],
    "callsToDateAsFunction": [],
  },
  "trace": []
}
  )";
  ASSERT_THROW(parseSynthTrace(bufFromStr(src)), std::invalid_argument);
}

TEST_F(SynthTraceParserTest, SynthVersionInvalidKind) {
  const char *src = R"(
{
  "version": true,
  "globalObjID": 258,
  "sourceHash": "6440b537af26795e5f452bcd320faccb02055a4f",
  "runtimeConfig": {
    "gcConfig": {
      "initHeapSize": 33554432,
      "maxHeapSize": 536870912
    }
  },
  "env": {
    "mathRandomSeed": 123,
    "callsToDateNow": [],
    "callsToNewDate": [],
    "callsToDateAsFunction": [],
  },
  "trace": []
}
  )";
  ASSERT_THROW(parseSynthTrace(bufFromStr(src)), std::invalid_argument);
}

TEST_F(SynthTraceParserTest, SynthMissingVersion) {
  const char *src = R"(
{
  "globalObjID": 258,
  "sourceHash": "6440b537af26795e5f452bcd320faccb02055a4f",
  "runtimeConfig": {
  },
  "env": {
    "mathRandomSeed": 123,
    "callsToDateNow": [],
    "callsToNewDate": [],
    "callsToDateAsFunction": [],
  },
  "trace": []
}
  )";
  EXPECT_NO_THROW(parseSynthTrace(bufFromStr(src)));
}

} // namespace
