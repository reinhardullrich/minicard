// Compile with minicard/Solver.cc, utils/Options.cc and utils/System.cc.
// Compare every original-variable assignment with its exported CNF extension.
#include "minicard/Solver.h"
#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
using namespace Minisat;
struct Constraint { std::vector<int> literals; int bound; bool clause; };
static Lit lit(int x) { return mkLit(std::abs(x)-1, x<0); }
static bool value(int x, unsigned mask) { return bool(mask & (1u<<(std::abs(x)-1))) != (x<0); }
static unsigned checks = 0;
static void require(bool condition) { if (!condition) { std::cerr << "FAIL at check " << checks << '\n'; std::exit(1); } }

static void check(Solver& source, int n, const std::vector<Constraint>& constraints, const std::vector<int>& assumptions) {
    vec<Lit> as;
    for (int x : assumptions) as.push(lit(x));
    FILE* file = tmpfile();
    require(file != NULL);
    source.toDimacs(file, as);
    rewind(file);
    std::string text;
    for (int c; (c = fgetc(file)) != EOF;) text += char(c);
    fclose(file);
    std::istringstream input(text);
    std::string p, kind;
    int variables;
    unsigned long long count;
    require(bool(input >> p >> kind >> variables >> count) && p == "p" && kind == "cnf" && variables >= 0);
    Solver decoded;
    for (int i = 0; i < std::max(variables,n); ++i) decoded.newVar();
    for (unsigned long long i = 0; i < count; ++i) {
        vec<Lit> clause;
        int x;
        do {
            require(bool(input >> x) && x >= -variables && x <= variables);
            if (x) clause.push(lit(x));
        } while (x);
        decoded.addClause(clause);
    }
    std::string extra;
    require(!(input >> extra));
    for (unsigned mask = 0; mask < (1u << n); ++mask) {
        bool expected = true;
        for (int x : assumptions) expected &= value(x,mask);
        for (const auto& c : constraints) {
            int sum = 0;
            for (int x : c.literals) sum += value(x,mask);
            expected &= c.clause ? sum > 0 : sum <= c.bound;
        }
        vec<Lit> fixed;
        for (int j = 0; j < n; ++j) fixed.push(mkLit(j, !(mask & (1u << j))));
        ++checks;
        require(decoded.solveLimited(fixed) == (expected ? l_True : l_False));
    }
}

static void add(Solver& s, const Constraint& c) {
    vec<Lit> literals;
    for (int x : c.literals) literals.push(lit(x));
    if (c.clause) s.addClause(literals); else s.addAtMost(literals,c.bound);
}

int main() {
    for (int bound = -1; bound <= 5; ++bound) {
        Solver s;
        s.detect_clause = false;
        for (int j = 0; j < 4; ++j) s.newVar();
        std::vector<Constraint> constraints{{{1,1,-2,3},bound,false}};
        add(s,constraints[0]);
        check(s,4,constraints,{});
        vec<Lit> old; old.push(mkLit(3)); s.solveLimited(old);
        check(s,4,constraints,{-4}); // export argument must override the last solve's assumptions
        check(s,4,constraints,{4,-4});
    }
    std::mt19937 rng(987612);
    for (int trial = 0; trial < 1000; ++trial) {
        int n = 1 + rng()%7;
        unsigned planted = rng()%(1u<<n);
        Solver s;
        s.detect_clause = trial%2;
        for (int j = 0; j < n; ++j) s.newVar();
        std::vector<Constraint> constraints;
        for (int stage = 0; stage < 8; ++stage) {
            Constraint c{{},0,rng()%3==0};
            int length = rng()%(2*n+1), sum = 0;
            for (int j = 0; j < length; ++j) {
                int x = (1+int(rng()%n))*(rng()%2?1:-1);
                c.literals.push_back(x); sum += value(x,planted);
            }
            c.bound = trial%2 ? sum : int(rng()%(length+3))-1;
            if (trial%2 && c.clause && sum == 0) c.literals.push_back(planted&1 ? 1 : -1);
            constraints.push_back(c); add(s,c);
            vec<Lit> old; old.push(mkLit(0)); s.solveLimited(old);
            if (s.okay()) s.garbageCollect();
            check(s,n,constraints,{stage%2 ? 1 : -1});
        }
    }
    std::cout << "PASS DIMACS assignment checks=" << checks << '\n';
}
