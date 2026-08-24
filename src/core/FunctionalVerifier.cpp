#include "FunctionalVerifier.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <sstream>

static bool IsInvalid(double val) {
    return std::isnan(val) || std::isinf(val);
}

TrajectoryVerificationReport FunctionalVerifier::VerifyTrajectory(const std::vector<WeaponModelOutput>& history) {
    TrajectoryVerificationReport report;
    report.totalDataPoints = static_cast<int>(history.size());

    if (history.empty()) {
        report.overallPass = false;
        report.errors.push_back("ERROR: Trajectory history is empty.");
        return report;
    }

    report.timeList.reserve(history.size());
    report.latList.reserve(history.size());
    report.lonList.reserve(history.size());
    report.altList.reserve(history.size());
    report.speedList.reserve(history.size());
    report.pitchList.reserve(history.size());
    report.rollList.reserve(history.size());
    report.yawList.reserve(history.size());

    double maxAngleAbs = 0.0;

    for (size_t i = 0; i < history.size(); ++i) {
        const auto& pt = history[i];

        report.timeList.push_back(pt.sim_time);
        report.latList.push_back(pt.lat);
        report.lonList.push_back(pt.lon);
        report.altList.push_back(pt.alt);
        
        double speed = std::sqrt(pt.vx * pt.vx + pt.vy * pt.vy + pt.vz * pt.vz);
        report.speedList.push_back(speed);

        report.pitchList.push_back(pt.pitch);
        report.rollList.push_back(pt.roll);
        report.yawList.push_back(pt.yaw);

        // NaN / Inf Check
        if (IsInvalid(pt.sim_time) || IsInvalid(pt.lat) || IsInvalid(pt.lon) || IsInvalid(pt.alt) ||
            IsInvalid(pt.vx) || IsInvalid(pt.vy) || IsInvalid(pt.vz) ||
            IsInvalid(pt.pitch) || IsInvalid(pt.roll) || IsInvalid(pt.yaw)) {
            report.nanOrInfCount++;
            if (report.nanOrInfCount <= 5) {
                std::stringstream ss;
                ss << "ERROR: NaN or Inf detected at step " << i << " (Time: " << pt.sim_time << " s)";
                report.errors.push_back(ss.str());
            }
        }

        // Boundary Checks
        if (pt.lat < -90.0 || pt.lat > 90.0) {
            report.outOfBoundsCount++;
            if (report.outOfBoundsCount <= 3) {
                report.errors.push_back("ERROR: Latitude out of bounds [-90, 90]: " + std::to_string(pt.lat));
            }
        }
        if (pt.lon < -180.0 || pt.lon > 180.0) {
            report.outOfBoundsCount++;
            if (report.outOfBoundsCount <= 3) {
                report.errors.push_back("ERROR: Longitude out of bounds [-180, 180]: " + std::to_string(pt.lon));
            }
        }
        if (pt.alt < -1000.0 || pt.alt > 1000000.0) {
            report.outOfBoundsCount++;
            if (report.outOfBoundsCount <= 3) {
                report.warnings.push_back("WARNING: Altitude extreme value: " + std::to_string(pt.alt) + " m");
            }
        }

        // Track max angle for unit verification
        maxAngleAbs = (std::max)({ maxAngleAbs, std::abs(pt.pitch), std::abs(pt.roll), std::abs(pt.yaw) });

        // Position Jump / Smoothness Check
        if (i > 0) {
            const auto& prevPt = history[i - 1];
            double dt = pt.sim_time - prevPt.sim_time;
            if (dt > 1e-6) {
                // Approximate distance in meters (using 1 deg ≈ 111,000m)
                double dLatM = (pt.lat - prevPt.lat) * 111000.0;
                double dLonM = (pt.lon - prevPt.lon) * 111000.0 * std::cos(pt.lat * 3.14159265 / 180.0);
                double dAltM = pt.alt - prevPt.alt;
                double dist = std::sqrt(dLatM * dLatM + dLonM * dLonM + dAltM * dAltM);
                double impliedSpeed = dist / dt;

                if (impliedSpeed > 15000.0) { // Unreasonable velocity jump (> 15 km/s)
                    report.positionJumpCount++;
                    if (report.positionJumpCount <= 3) {
                        std::stringstream ss;
                        ss << "WARNING: Position jump detected at step " << i << " (implied speed: " << impliedSpeed << " m/s)";
                        report.warnings.push_back(ss.str());
                    }
                }
            }
        }
    }

    // Angle Unit Inspection
    if (maxAngleAbs <= 6.28318530718 && maxAngleAbs > 0.01) {
        report.isDegreeUnit = false;
        report.unitCheckLog = "WARNING: Max attitude angle is " + std::to_string(maxAngleAbs) + 
            " (<= 2π). Output appears to be in RADIANS rather than DEGREES!";
        report.warnings.push_back(report.unitCheckLog);
    } else {
        report.isDegreeUnit = true;
        report.unitCheckLog = "INFO: Attitude angles confirmed to be in DEGREES (Max angle: " + std::to_string(maxAngleAbs) + " deg).";
    }

    report.overallPass = (report.nanOrInfCount == 0 && report.outOfBoundsCount == 0);
    return report;
}
