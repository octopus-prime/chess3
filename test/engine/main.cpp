#include <iostream>
#include <string_view>

bool run_negamax_tests();
bool run_mate_tests();
bool run_evaluator_tests();
bool run_uci_tests();

int main(int argc, char** argv) {
    const std::string_view suite = argc == 2 ? argv[1] : "all";
    if (argc > 2 || (suite != "all" && suite != "negamax" && suite != "evaluator" && suite != "mates" && suite != "uci")) {
        std::cerr << "Usage: chess_engine_tests [all|negamax|evaluator|mates|uci]\n";
        return 2;
    }
    bool passed = true;
    if (suite == "all" || suite == "negamax")
        passed &= run_negamax_tests();
    if (suite == "all" || suite == "evaluator")
        passed &= run_evaluator_tests();
    if (suite == "all" || suite == "mates")
        passed &= run_mate_tests();
    if (suite == "all" || suite == "uci")
        passed &= run_uci_tests();
    return passed ? 0 : 1;
}
