#include "nfa.h"
#include <queue>
#include <stdexcept>

int NFA::newState() {
    states.emplace_back();
    return static_cast<int>(states.size()) - 1;
}

void NFA::addEpsilon(int from, int to) {
    states[from].transitions.push_back({ TransType::Epsilon, to, 0 });
}
void NFA::addChar(int from, int to, char c) {
    states[from].transitions.push_back({ TransType::Char, to, c });
}
void NFA::addAny(int from, int to) {
    states[from].transitions.push_back({ TransType::Any, to, 0 });
}

// ---------- Compiler ----------

namespace {

    struct Fragment {
        int start;
        int accept;
    };

    class Compiler {
    public:
        NFA compile(const std::string& pattern) {
            if (pattern.empty())
                throw std::runtime_error("empty pattern");
            this->pattern = pattern;
            pos = 0;
            Fragment f = parseAlternation();
            if (pos != pattern.size())
                throw std::runtime_error("unexpected ')'");
            nfa.start = f.start;
            nfa.accept = f.accept;
            return std::move(nfa);
        }

    private:
        NFA nfa;
        std::string pattern;
        size_t pos = 0;

        char peek() const { return pos < pattern.size() ? pattern[pos] : '\0'; }
        char take() { return pattern[pos++]; }

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
                if (peek() != ')') throw std::runtime_error("missing ')'");
                take();
            }
            else if (c == '\\') {
                take();
                char lit = peek();
                if (lit == '\0') throw std::runtime_error("dangling backslash");
                take();
                atom = makeLiteral(lit);
            }
            else if (c == '.') {
                take();
                atom = makeAny();
            }
            else if (c == '*' || c == ')' || c == '|') {
                throw std::runtime_error(std::string("unexpected '") + c + "'");
            }
            else {
                take();
                atom = makeLiteral(c);
            }
            if (peek() == '*') {
                take();
                if (peek() == '*') throw std::runtime_error("unexpected '*'");
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

} // namespace

NFA compileRegex(const std::string& pattern) {
    Compiler c;
    return c.compile(pattern);
}

// ---------- Simulation ----------

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

StateSet move(const NFA& nfa, const StateSet& states, char c) {
    StateSet result;
    for (int s : states) {
        for (const auto& t : nfa.states[s].transitions) {
            if (t.type == TransType::Char && t.c == c) result.insert(t.to);
            else if (t.type == TransType::Any) result.insert(t.to);
        }
    }
    return result;
}

size_t longestMatchEnd(const NFA& nfa, const std::string& input,
    size_t from, bool& found) {
    found = false;
    StateSet current = epsilonClosure(nfa, { nfa.start });
    size_t lastAccept = from;
    if (current.count(nfa.accept)) { lastAccept = from; found = true; }

    for (size_t i = from; i < input.size(); ++i) {
        StateSet next = move(nfa, current, input[i]);
        current = epsilonClosure(nfa, next);
        if (current.empty()) break;
        if (current.count(nfa.accept)) { lastAccept = i + 1; found = true; }
    }
    return lastAccept;
}

std::vector<MatchInfo> findMatches(const NFA& nfa, const std::string& input) {
    std::vector<MatchInfo> result;
    size_t i = 0;
    while (i <= input.size()) {
        bool found = false;
        size_t end = longestMatchEnd(nfa, input, i, found);
        if (found && end > i) {
            result.push_back({ i, end });
            i = end;
        }
        else {
            ++i;
        }
    }
    return result;
}