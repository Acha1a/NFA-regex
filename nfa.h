#pragma once
#pragma once
#include <string>
#include <vector>
#include <set>
#include <optional>

enum class TransType { Epsilon, Char, Any };

struct Transition {
    TransType type;
    int to;
    char c;
};

struct NFAState {
    std::vector<Transition> transitions;
};

struct NFA {
    std::vector<NFAState> states;
    int start = -1;
    int accept = -1;

    int newState();
    void addEpsilon(int from, int to);
    void addChar(int from, int to, char c);
    void addAny(int from, int to);
};

NFA compileRegex(const std::string& pattern);

using StateSet = std::set<int>;

StateSet epsilonClosure(const NFA& nfa, const StateSet& states);
StateSet move(const NFA& nfa, const StateSet& states, char c);

struct MatchInfo {
    size_t start;
    size_t end;
};

size_t longestMatchEnd(const NFA& nfa, const std::string& input,
    size_t from, bool& found);
std::vector<MatchInfo> findMatches(const NFA& nfa, const std::string& input);