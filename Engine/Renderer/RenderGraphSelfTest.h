#pragma once

#include <string>
#include <vector>

namespace RenderGraphSelfTest
{
    struct Result
    {
        bool Passed = false;
        std::vector<std::string> Failures;
    };

    [[nodiscard]] Result Run();
}
