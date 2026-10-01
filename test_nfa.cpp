#include "nfa.h"
#include <iostream>
#include <string>
#include <vector>

struct TestResult {
    int passed = 0;
    int failed = 0;
};

TestResult g_results;

void expect(const std::string& pattern, const std::string& input,
    const std::vector<std::string>& expected, const std::string& label) {
    std::string patternCopy = pattern;
    try {
        NFA nfa = compileRegex(patternCopy);
        auto matches = findMatches(nfa, input);

        std::vector<std::string> actual;
        for (const auto& m : matches)
            actual.push_back(input.substr(m.start, m.end - m.start));

        if (actual == expected) {
            ++g_results.passed;
            std::cout << "[PASS] " << label << "\n";
        }
        else {
            ++g_results.failed;
            std::cout << "[FAIL] " << label << "\n";
            std::cout << "   pattern: " << patternCopy << "\n";
            std::cout << "   input:   '" << input << "'\n";
            std::cout << "   expected: [";
            for (size_t i = 0; i < expected.size(); ++i)
                std::cout << (i ? ", " : "") << "'" << expected[i] << "'";
            std::cout << "]\n";
            std::cout << "   actual:   [";
            for (size_t i = 0; i < actual.size(); ++i)
                std::cout << (i ? ", " : "") << "'" << actual[i] << "'";
            std::cout << "]\n";
        }
    }
    catch (const std::exception& e) {
        ++g_results.failed;
        std::cout << "[FAIL] " << label << " — exception: " << e.what() << "\n";
    }
}

void expectError(const std::string& pattern, const std::string& label) {
    try {
        NFA nfa = compileRegex(pattern);
        ++g_results.failed;
        std::cout << "[FAIL] " << label << " — expected error but got success\n";
    }
    catch (const std::exception&) {
        ++g_results.passed;
        std::cout << "[PASS] " << label << "\n";
    }
}

int main() {
    // --- Literals ---
    expect("abc", "xxabcxx", { "abc" }, "literal: simple match");
    expect("abc", "xxx", {}, "literal: no match");
    expect("abc", "abc", { "abc" }, "literal: whole string");
    expect("abc", "abcabc", { "abc", "abc" }, "literal: two non-overlapping");

    // --- Dot ---
    expect("a.c", "abc", { "abc" }, "dot: single char");
    expect("a.c", "aXc", { "aXc" }, "dot: any char");
    expect("a.c", "ac", {}, "dot: missing char");
    expect("...", "abcdef", { "abc", "def" }, "dot: three chars");

    // --- Star ---
    expect("a*", "aaab", { "aaa" }, "star: greedy on letters");
    expect("a*b", "aaab", { "aaab" }, "star: prefix of b");
    expect("a*b", "b", { "b" }, "star: zero occurrences");
    expect("a*b", "cb", { "b" }, "star: skip char before b");
    expect("a*b", "aaa", {}, "star: no b at all");

    // --- Dot star ---
    expect("a.*b", "axxxb", { "axxxb" }, "dot-star: single match");
    expect("a.*b", "axxbxxb", { "axxbxxb" }, "dot-star: greedy to last b");
    expect("a.*b", "ab", { "ab" }, "dot-star: empty between");
    expect(".*", "hello", { "hello" }, "dot-star: whole string");

    // --- Escaping ---
    expect("\\.", "a.b", { "." }, "escape: literal dot");
    expect("\\*", "a*b", { "*" }, "escape: literal star");
    expect("\\\\", "a\\b", { "\\" }, "escape: literal backslash");

    // --- Alternation ---
    expect("a|b", "xbxax", { "b", "a" }, "alternation: simple");
    expect("cat|dog", "a cat and a dog", { "cat", "dog" }, "alternation: words");
    expect("ab|cd", "abcd", { "ab", "cd" }, "alternation: two matches");

    // --- Grouping ---
    expect("(ab)*", "ababx", { "abab" }, "group: repeated");
    expect("(a|b)*", "ababx", { "abab" }, "group: alternation repeated");
    expect("(ab)*c", "ababc", { "ababc" }, "group: suffix");
    expect("(a|b)*c", "ababc", { "ababc" }, "group: alt suffix");

    // --- Combinations ---
    expect("a.*b.*c", "axbxc", { "axbxc" }, "combined: chain of dot-stars");
    expect("(a|b)*c", "aaabc", { "aaabc" }, "combined: repeated alt + suffix");

    // --- Empty / edge cases ---
    expect("a*", "", {}, "empty input: star matches nothing");
    expect(".*", "", {}, "empty input: dot-star matches nothing");
    expectError("", "empty pattern: not supported"); // throws

    // --- Errors ---
    expectError("*abc", "error: leading star");
    expectError("a**", "error: double star");
    expectError("(a", "error: missing close paren");
    expectError("a)", "error: unexpected close paren");
    expectError("|a", "error: leading alternation");
    expectError("a|", "error: trailing alternation");
    expectError("a\\", "error: dangling backslash");

    std::cout << "\n=============================\n";
    std::cout << "Passed: " << g_results.passed << "\n";
    std::cout << "Failed: " << g_results.failed << "\n";
    std::cout << "=============================\n";

    return g_results.failed == 0 ? 0 : 1;
}