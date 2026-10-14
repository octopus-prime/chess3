#include <exception>
#include <iostream>
#include <memory>
#include <string_view>

import nnue.nnue;
import types.square;
import types.side;
import types.type;

int main() {
    using namespace chess;
    using namespace chess::nnue;
    try {
        const auto nnue = load();
        const auto index = make_index(e1, a2, WP, WHITE);
        entry_t entry{};
        nnue->initialize(entry);
        nnue->update(entry, {}, {});
        const auto evaluation = nnue->evaluate(entry, entry, 1);
        std::cout << "Loaded NNUE network\n";
        std::cout << "Index: " << index << '\n';
        std::cout << "Initial evaluation: " << evaluation << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Failed to load NNUE network: " << error.what() << '\n';
        return 1;
    }
}
