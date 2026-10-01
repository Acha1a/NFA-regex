#include "nfa.h"
#include <iostream>

int main() {
    std::string pattern;
    std::cout << "Pattern: ";
    if (!std::getline(std::cin, pattern) || pattern.empty()) return 0;

    NFA nfa;
    try {
        nfa = compileRegex(pattern);
    }
    catch (const std::exception& e) {
        std::cerr << "Compile error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Input:   ";
    std::string input;
    std::getline(std::cin, input);

    auto matches = findMatches(nfa, input);
    if (matches.empty()) {
        std::cout << "No matches.\n";
    }
    else {
        for (const auto& m : matches) {
            std::cout << "[" << m.start << ", " << m.end << ") = '"
                << input.substr(m.start, m.end - m.start) << "'\n";
        }
    }
    return 0;
}