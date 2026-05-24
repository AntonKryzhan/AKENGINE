#include <AK/Math/Chernoff.hpp>

#include <iomanip>
#include <iostream>

int main()
{
    const AK::ChernoffProbeResult probe = AK::BuildDefaultChernoffProbe();

    std::cout << probe.summary << '\n';
    std::cout << std::fixed << std::setprecision(6);
    for (const AK::ChernoffSample1D& sample : probe.samples)
    {
        std::cout << "x=" << std::setw(8) << sample.x << "  Rlambda_g=" << std::setw(12) << sample.value << '\n';
    }

    return 0;
}
