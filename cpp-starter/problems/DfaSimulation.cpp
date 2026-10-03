#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <set>

#include "DfaSimulation.hpp"
#include "../problem.hpp"

using namespace std;

// Initialize the parser
void DfaSimulation::initialize_parser(cxxopts::Options &options) {
    options.add_options()
        ("check", "Check words with the DFA", cxxopts::value<std::string>());
}

// Check if the problem is chosen
bool DfaSimulation::is_chosen_problem(const cxxopts::ParseResult &args) {
    return args.count("check") > 0;
}

int DfaSimulation::run(const cxxopts::ParseResult &args) {
    ifstream in(args["input"].as<string>());
    if (!in) {
        cerr << "Error opening input file" << endl;
        return 1;
    }

    string line, start, state;
    getline(in, line);
    getline(in, line);

    getline(in, line);
    {
        stringstream ss(line);
        ss >> start;
    };

    getline(in, line);
    set<string> finals;
    {
        stringstream ss(line);
        while (ss >> state)
            finals.insert(state);
    }

    map<string, map<char, string>> trans;
    string from, to;
    char symbol;
    while (in >> from >> symbol >> to)
        trans[from][symbol] = to;

    ofstream out(args["output"].as<string>());
    if (!out) {
        cerr << "Error opening output file" << endl;
        return 1;
    }

    stringstream words(args["check"].as<string>());
    string word;
    bool first = true;

    while (getline(words, word, ',')) {
        string current = start;
        bool ok = true;

        for (char c : word) {
            if (trans[current].count(c) == 0) {
                ok=false;
                break;
            }
            current = trans[current][c];
        }

        if (!first) out << "\n";
        first = false;
        out << ((ok && finals.count(current)) ? "IGEN" : "NEM");
    }

    return 0;
}