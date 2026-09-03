#ifndef SINGLE_THREAD_MULTI_OBJECT_TESTER_H
#define SINGLE_THREAD_MULTI_OBJECT_TESTER_H

#include <string>
#include <vector>
#include "UserCodeHarness.h"

class MultiObjectHarness;

enum class MultiObjectSchedule {
    Forward = 0,
    Reverse = 1,
    DeterministicRandom = 2
};

struct MultiObjectTestConfig {
    int objectCount = 3;
    int stepCount = 100;
    double stepDt = 0.02;
    double tolerance = 1e-8;
    MultiObjectSchedule schedule = MultiObjectSchedule::Forward;
    uint32_t randomSeed = 1;
};

struct MultiObjectResult {
    int objectId = 0;
    int baselineReturnCode = 0;
    int interleavedReturnCode = 0;
    bool exceptionOccurred = false;
    unsigned long exceptionCode = 0;
    int faultStep = -1;
    double maxPositionDeviation = 0.0;
    std::string detail;
    std::vector<TrajectorySample> baselineTrajectory;
    std::vector<TrajectorySample> interleavedTrajectory;
};

struct MultiObjectTestReport {
    int objectCount = 0;
    int stepCount = 0;
    int completedObjects = 0;
    int interferenceCount = 0;
    int exceptionCount = 0;
    double tolerance = 0.0;
    double maxPositionDeviation = 0.0;
    double maxFrameTimeMs = 0.0;
    double memoryDeltaMB = 0.0;
    bool usedSingleThread = true;
    std::string verdict;
    std::string summary;
    std::vector<MultiObjectResult> objectResults;
    std::vector<std::string> logMessages;
};

class SingleThreadMultiObjectTester {
public:
    static MultiObjectTestReport Run(MultiObjectHarness& harness,
                                     const MultiObjectTestConfig& config,
                                     const RandomValueBlob& values);
};

#endif // SINGLE_THREAD_MULTI_OBJECT_TESTER_H
