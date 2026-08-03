#include "acceptance.hpp"

#include <cstdlib>
#include <iostream>

int main()
{
    const runner::acceptance::Report report =
        runner::acceptance::run_live_acceptance_matrix();

    for (const runner::acceptance::CaseResult& result : report.cases)
    {
        std::cout << (result.passed ? "[PASS] " : "[FAIL] ")
            << result.name << ": " << result.detail << '\n';
    }

    std::cout << "Runner live acceptance matrix: "
        << report.passed_count() << '/' << report.cases.size() << " passed\n";
    return report.passed() ? EXIT_SUCCESS : EXIT_FAILURE;
}
