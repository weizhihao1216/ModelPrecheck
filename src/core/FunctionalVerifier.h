#ifndef FUNCTIONAL_VERIFIER_H
#define FUNCTIONAL_VERIFIER_H

#include <vector>
#include <string>
#include "../utils/SehHelper.h"

struct TrajectoryVerificationReport {
    int totalDataPoints = 0;
    int nanOrInfCount = 0;
    int positionJumpCount = 0;
    int outOfBoundsCount = 0;

    bool isDegreeUnit = true; // True if output appears to be in degrees, false if radians suspected
    std::string unitCheckLog;

    bool overallPass = false;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    // Series data for charting
    std::vector<double> timeList;
    std::vector<double> latList;
    std::vector<double> lonList;
    std::vector<double> altList;
    std::vector<double> speedList;
    std::vector<double> pitchList;
    std::vector<double> rollList;
    std::vector<double> yawList;
};

class FunctionalVerifier {
public:
    static TrajectoryVerificationReport VerifyTrajectory(const std::vector<WeaponModelOutput>& history);
};

#endif // FUNCTIONAL_VERIFIER_H
