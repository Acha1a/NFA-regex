// nfa_regex.cpp
// NFA-based regex engine (Thompson construction)
// Supports: literals, '.', '*', '\', '|', '(' ')'

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <queue>
#include <stdexcept>
#include <algorithm>

// ============================================================
// 1. NFA data structures
// ============================================================

enum class TransType { Epsilon, Char, Any };

struct Transition {
    TransType type;
    int to;
    char c; // only meaningful when type == Char
};

struct NFAState {
    std::vector<Transition> transitions;
};

struct NFA {
    std::vector<NFAState> states;
    int start = -1;
    int accept = -1;

    int newState() {
        states.emplace_back();
        return static_cast<int>(states.size()) - 1;
    }

    void addEpsilon(int from, int to) {
        states[from].transitions.push_back({ TransType::Epsilon, to, 0 });
    }
    void addChar(int from, int to, char c) {
        states[from].transitions.push_back({ TransType::Char, to, c });
    }
    void addAny(int from, int to) {
        states[from].transitions.push_back({ TransType::Any, to, 0 });
    }
};

// ============================================================
// 2. Thompson construction: pattern -> NFA
// ============================================================

class NFACompiler {
public:
    NFA compile(const std::string& pattern) {
        if (pattern.empty())
            throw std::runtime_error("Pattern must not be empty");

        this->pattern = pattern;
        pos = 0;

        Fragment f = parseAlternation();

        if (pos != pattern.size())
            throw std::runtime_error("Unexpected ')' in pattern");

        nfa.start = f.start;
        nfa.accept = f.accept;
        return std::move(nfa);
    }

private:
    struct Fragment {
        int start;
        int accept;
    };

    NFA nfa;
    std::string pattern;
    size_t pos = 0;

    char peek() const { return pos < pattern.size() ? pattern[pos] : '\0'; }
    char take() { return pattern[pos++]; }

    // Grammar (top-down):
    //   alternation := concat ('|' concat)*
    //   concat      := atom+
    //   atom        := '(' alternation ')' | escape | '.' | literal   ('*')?

    Fragment parseAlternation() {
        Fragment left = parseConcat();
        while (peek() == '|') {
            take();
            Fragment right = parseConcat();

            int s = nfa.newState();
            int a = nfa.newState();
            nfa.addEpsilon(s, left.start);
            nfa.addEpsilon(s, right.start);
            nfa.addEpsilon(left.accept, a);
            nfa.addEpsilon(right.accept, a);
            left = { s, a };
        }
        return left;
    }

    Fragment parseConcat() {
        Fragment result = parseAtom();
        while (peek() != '\0' && peek() != '|' && peek() != ')') {
            Fragment next = parseAtom();
            // concatenate: join accept of left to start of right
            nfa.addEpsilon(result.accept, next.start);
            result.accept = next.accept;
        }
        return result;
    }

    Fragment parseAtom() {
        Fragment atom;
        char c = peek();

        if (c == '(') {
            take();
            atom = parseAlternation();
            if (peek() != ')')
                throw std::runtime_error("Missing ')'");
            take();
        }
        else if (c == '\\') {
            take();
            char lit = peek();
            if (lit == '\0')
                throw std::runtime_error("Dangling backslash");
            take();
            atom = makeLiteral(lit);
        }
        else if (c == '.') {
            take();
            atom = makeAny();
        }
        else if (c == '*' || c == ')' || c == '|') {
            throw std::runtime_error(std::string("Unexpected '") + c + "'");
        }
        else {
            take();
            atom = makeLiteral(c);
        }

        // quantifier
        if (peek() == '*') {
            take();
            if (peek() == '*')
                throw std::runtime_error("Unexpected '*' after quantifier");
            atom = makeStar(atom);
        }
        return atom;
    }

    Fragment makeLiteral(char c) {
        int s = nfa.newState();
        int a = nfa.newState();
        nfa.addChar(s, a, c);
        return { s, a };
    }

    Fragment makeAny() {
        int s = nfa.newState();
        int a = nfa.newState();
        nfa.addAny(s, a);
        return { s, a };
    }

    // a* :  s -> inner.start (ε)
    //       inner.accept -> inner.start (ε, loop)
    //       inner.accept -> a (ε, exit)
    //       s -> a (ε, skip)
    Fragment makeStar(Fragment inner) {
        int s = nfa.newState();
        int a = nfa.newState();
        nfa.addEpsilon(s, inner.start);
        nfa.addEpsilon(inner.accept, inner.start);
        nfa.addEpsilon(inner.accept, a);
        nfa.addEpsilon(s, a);
        return { s, a };
    }
};

// ============================================================
// 3. Simulation: NFA + input string -> matches
// ============================================================

using StateSet = std::set<int>;

// All states reachable from `states` using only ε-transitions.
StateSet epsilonClosure(const NFA& nfa, const StateSet& states) {
    StateSet closure = states;
    std::queue<int> work;
    for (int s : states) work.push(s);

    while (!work.empty()) {
        int s = work.front();
        work.pop();
        for (const auto& t : nfa.states[s].transitions) {
            if (t.type == TransType::Epsilon && closure.find(t.to) == closure.end()) {
                closure.insert(t.to);
                work.push(t.to);
            }
        }
    }
    return closure;
}

// All states reachable by consuming character c.
StateSet move(const NFA& nfa, const StateSet& states, char c) {
    StateSet result;
    for (int s : states) {
        for (const auto& t : nfa.states[s].transitions) {
            if (t.type == TransType::Char && t.c == c)
                result.insert(t.to);
            else if (t.type == TransType::Any)
                result.insert(t.to);
        }
    }
    return result;
}

struct MatchInfo {
    size_t start;
    size_t end; // exclusive
};

// Returns the longest end position starting from `from`, or `from` if no match.
// `found` reports whether any (possibly empty) match exists.
size_t longestMatchEnd(const NFA& nfa, const std::string& input,
    size_t from, bool& found) {
    found = false;
    StateSet current = epsilonClosure(nfa, { nfa.start });

    size_t lastAccept = from;
    if (current.count(nfa.accept)) {
        lastAccept = from;
        found = true;
    }

    for (size_t i = from; i < input.size(); ++i) {
        StateSet next = move(nfa, current, input[i]);
        current = epsilonClosure(nfa, next);
        if (current.empty()) break;

        if (current.count(nfa.accept)) {
            lastAccept = i + 1;
            found = true;
        }
    }
    return lastAccept;
}

// Leftmost-longest, non-overlapping matches. Empty matches are skipped.
std::vector<MatchInfo> findMatches(const NFA& nfa, const std::string& input) {
    std::vector<MatchInfo> result;
    size_t i = 0;

    while (i <= input.size()) {
        bool found = false;
        size_t end = longestMatchEnd(nfa, input, i, found);

        if (found && end > i) {
            result.push_back({ i, end });
            i = end; // move past the match
        }
        else {
            ++i;
        }
    }
    return result;
}

// ============================================================
// 4. Demo
// ============================================================

int main() {
    std::string pattern;
    std::cout << "Pattern: ";
    if (!std::getline(std::cin, pattern) || pattern.empty()) return 0;

    NFACompiler compiler;
    NFA nfa;
    try {
        nfa = compiler.compile(pattern);
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