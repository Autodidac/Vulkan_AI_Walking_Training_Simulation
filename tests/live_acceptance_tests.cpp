#include "acceptance.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_set>

int main()
{
    const runner::acceptance::Report report =
        runner::acceptance::run_live_acceptance_matrix();

    if (report.cases.size() < 10u)
    {
        std::cerr << "Acceptance matrix unexpectedly contains only "
            << report.cases.size() << " cases\n";
        return EXIT_FAILURE;
    }

    std::unordered_set<std::string> names{};
    for (const runner::acceptance::CaseResult& result : report.cases)
    {
        if (!names.insert(result.name).second)
        {
            std::cerr << "Duplicate acceptance case: " << result.name << '\n';
            return EXIT_FAILURE;
        }
        std::cout << (result.passed ? "[PASS] " : "[FAIL] ")
            << result.name << ": " << result.detail << '\n';
    }

    std::cout << "Runner live acceptance matrix: "
        << report.passed_count() << '/' << report.cases.size() << " passed\n";
    return report.passed() ? EXIT_SUCCESS : EXIT_FAILURE;
}
