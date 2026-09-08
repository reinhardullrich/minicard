// Build with minicard/Solver.cc, utils/Options.cc, utils/System.cc and -lz.
// Checks remain active in Release builds.
#include "minicard/Solver.h"
#include "minicard/opb.h"
#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>
using namespace Minisat;

struct Constraint {
    std::vector<int> coefficients;
    int bound;
};

static bool satisfies(const std::vector<Constraint>& constraints, unsigned mask) {
    for (const auto& c : constraints) {
        int sum = 0;
        for (unsigned j = 0; j < c.coefficients.size(); ++j)
            if (mask & (1u << j)) sum += c.coefficients[j];
        if (sum != c.bound) return false;
    }
    return true;
}

int main() {
    {
        Solver s;
        const char* input = "+1 x1 >= 1;";
        readConstr(input, s);
        input = "+1 x1 +1 x2 = 1;";
        readConstr(input, s);
        vec<Lit> as;
        if (s.solveLimited(as) != l_True || s.modelValue(0) != l_True || s.modelValue(1) != l_False)
            return 1;
    }
    std::mt19937 rng(812);
    unsigned checks = 0;
    for (int trial = 0; trial < 2000; ++trial) {
        int n = 1 + rng() % 7;
        unsigned planted = rng() % (1u << n);
        Solver s;
        s.detect_clause = trial % 2;
        for (int j = 0; j < n; ++j) s.newVar();
        std::vector<Constraint> constraints;
        for (int stage = 0; stage < 5; ++stage) {
            Constraint c;
            c.bound = 0;
            std::ostringstream line;
            for (int j = 0; j < n; ++j) {
                int coefficient = rng() % 2 ? 1 : -1;
                c.coefficients.push_back(coefficient);
                line << (coefficient > 0 ? "+1" : "-1") << " x" << j + 1 << ' ';
                if (planted & (1u << j)) c.bound += coefficient;
            }
            if (trial % 3 == 0) c.bound = int(rng() % (2*n+3)) - n - 1;
            line << "= " << c.bound << ';';
            constraints.push_back(c);
            std::string text = line.str();
            const char* input = text.c_str();
            readConstr(input, s);
            for (unsigned mask = 0; mask < (1u << n); ++mask) {
                vec<Lit> as;
                for (int j = 0; j < n; ++j) as.push(mkLit(j, !(mask & (1u << j))));
                if (s.solveLimited(as) != (satisfies(constraints, mask) ? l_True : l_False)) {
                    std::cerr << "OPB mismatch trial=" << trial << " stage=" << stage << " mask=" << mask << '\n';
                    return 1;
                }
                ++checks;
            }
        }
    }
    std::cout << "PASS OPB assignment checks=" << checks << '\n';
}
