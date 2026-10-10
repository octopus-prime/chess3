#include <iostream>
#include <string_view>

bool run_lookup_tests();
bool run_position_tests();
bool run_perft_tests();

int main(int argc, char** argv) {
    const std::string_view suite = argc == 2 ? argv[1] : "all";
    if (argc > 2 || (suite != "all" && suite != "lookup" &&
                     suite != "position" && suite != "perft")) {
        std::cerr << "Usage: chess_tests [all|lookup|position|perft]\n";
        return 2;
    }
    bool passed = true;
    if (suite == "all" || suite == "lookup")
        passed &= run_lookup_tests();
    if (suite == "all" || suite == "position")
        passed &= run_position_tests();
    if (suite == "all" || suite == "perft")
        passed &= run_perft_tests();
    return passed ? 0 : 1;
}
