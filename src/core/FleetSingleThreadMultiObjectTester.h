#ifndef FLEET_SINGLE_THREAD_MULTI_OBJECT_TESTER_H
#define FLEET_SINGLE_THREAD_MULTI_OBJECT_TESTER_H

#include <string>
#include <vector>
#include "SingleThreadMultiObjectTester.h"

class MultiObjectHarness;

struct FleetMultiObjectModelSpec {
    MultiObjectHarness* harness = nullptr;
    std::string modelName;
    int objectCount = 2;
};

struct FleetMultiObjectTestConfig {
    std::vector<FleetMultiObjectModelSpec> models;
    int stepCount = 100;
    double stepDt = 0.02;
    double tolerance = 1e-8;
    MultiObjectSchedule schedule = MultiObjectSchedule::Forward;
    uint32_t randomSeed = 1;
};

struct FleetMultiObjectResult {
    int globalObjectId = 0;
    int modelIndex = 0;
    int localObjectId = 0;
    std::string modelName;
    MultiObjectResult detail;
};

struct FleetMultiObjectTestReport {
    int modelCount = 0;
    int totalObjectCount = 0;
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
    std::vector<FleetMultiObjectResult> objectResults;
    std::vector<std::string> logMessages;
};

/// Cross-model single-thread interleaving: baseline per object, then one shared
/// schedule that steps objects from different model harnesses in turn.
class FleetSingleThreadMultiObjectTester {
public:
    static FleetMultiObjectTestReport Run(const FleetMultiObjectTestConfig& config);
};

#endif // FLEET_SINGLE_THREAD_MULTI_OBJECT_TESTER_H
