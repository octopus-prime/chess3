#include <iostream>
#include <string_view>

bool run_negamax_tests();
bool run_evaluator_tests();

int main(int argc, char** argv) {
    const std::string_view suite = argc == 2 ? argv[1] : "all";
    if (argc > 2 || (suite != "all" && suite != "negamax" && suite != "evaluator")) {
        std::cerr << "Usage: chess_engine_tests [all|negamax|evaluator]\n";
        return 2;
    }
    bool passed = true;
    if (suite == "all" || suite == "negamax")
        passed &= run_negamax_tests();
    if (suite == "all" || suite == "evaluator")
        passed &= run_evaluator_tests();
    return passed ? 0 : 1;
}
