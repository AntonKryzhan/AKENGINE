#include <AK/World/WorldCoordinates.hpp>

#include <iostream>
#include <iomanip>

int main()
{
    const AK::LargeWorldConfig config{};

    const AK::WorldPosition camera = AK::MakeWorldPosition(1000000.125, 12.0, -999999.875, config.cellSizeMeters);
    const AK::WorldPosition object = AK::AddLocalOffset(camera, {1.0f, 0.0f, 1.0f}, config.cellSizeMeters);
    const AK::CameraRelativePosition relative = AK::ToCameraRelativeFloat(object, camera, config);
    const double directFloatStep = AK::EstimateFloatStepMeters(1000000.0);
    const double localFloatStep = AK::EstimateFloatStepMeters(relative.distanceMeters);
    const double badDepthRatio = AK::EstimateDepthResolutionRatio(0.01f, 1000000.0f);
    const double saneDepthRatio = AK::EstimateDepthResolutionRatio(0.05f, 1000.0f);

    std::cout << "AK World probe\n";
    std::cout << "camera: " << AK::ToDebugString(camera, 3) << "\n";
    std::cout << "object: " << AK::ToDebugString(object, 3) << "\n";
    std::cout << std::fixed << std::setprecision(9);
    std::cout << "relative: "
              << relative.value.x << ", "
              << relative.value.y << ", "
              << relative.value.z << "\n";
    std::cout << "direct float step at 1e6m: " << directFloatStep << " m\n";
    std::cout << "camera-relative float step: " << localFloatStep << " m\n";
    std::cout << "depth far/near bad ratio: " << badDepthRatio << "\n";
    std::cout << "depth far/near sane ratio: " << saneDepthRatio << "\n";
    std::cout << AK::BuildLargeWorldProbeSummary() << "\n";

    if (!relative.finite || relative.precisionRisk)
    {
        std::cout << "worldprobe status: warning\n";
        return 2;
    }

    std::cout << "worldprobe status: ok\n";
    return 0;
}
