#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <vector>

#include "Determinization.hpp"
#include "../problem.hpp"

using namespace std;

// A 2. tablazat egy sora: az uj allapot (q-k halmaza) es hogy az egyes jelekre
// mely q-k halmazaba jutunk belole
struct Row {
    set<string> qs;
    map<string, set<string>> cells;
};

// Initialize the parser
void Determinization::initialize_parser(cxxopts::Options &options) {
    options.add_options()
        ("det", "Determinize the automaton");
}

// Check if the problem is chosen
bool Determinization::is_chosen_problem(const cxxopts::ParseResult &args) {
    return args.count("det") > 0;
}

int Determinization::run(const cxxopts::ParseResult &args) {
    ifstream in(args["input"].as<string>());
    if (!in) {
        cerr << "Error opening input file" << endl;
        return 1;
    }

    string line, item, start;

    getline(in, line); //allapotok
    set<string> states;
    {
        stringstream ss(line);
        while (ss >> item)
            states.insert(item);
    }

    getline(in, line); //bemeneti abece, a beolvasas sorrendjeben is megtartjuk a kiirashoz
    vector<string> alphabetList;
    set<string> alphabet;
    {
        stringstream ss(line);
        while (ss >> item) {
            alphabetList.push_back(item);
            alphabet.insert(item);
        }
    }

    getline(in, line); //kezdoallapot
    {
        stringstream ss(line);
        ss >> start;
    }
    if (states.count(start) == 0) {
        cerr << "Invalid start state: " << start << endl;
        return 1;
    }

    getline(in, line); //vegallapotok
    set<string> finals;
    {
        stringstream ss(line);
        while (ss >> item) {
            if (states.count(item) == 0) {
                cerr << "Invalid final state: " << item << endl;
                return 1;
            }
            finals.insert(item);
        }
    }

    //1. tablazat: az eredeti automata
    //sorok: q allapotok, oszlopok: jelek, cellak: ahova a jellel eljutunk (q-k halmaza)
    map<string, map<string, set<string>>> table1;
    for (const string &q : states)
        for (const string &a : alphabet)
            table1[q][a] = {};

    string from, symbol, to;
    while (getline(in, line)) {
        stringstream ss(line);
        if (!(ss >> from)) continue; //ures sor
        if (!(ss >> symbol >> to)) {
            cerr << "Invalid transition: " << line << endl;
            return 1;
        }
        //hibakezeles: csak az elejen felsorolt allapotok es jelek ervenyesek
        if (states.count(from) == 0 || states.count(to) == 0) {
            cerr << "Invalid state in transition: " << line << endl;
            return 1;
        }
        if (alphabet.count(symbol) == 0) {
            cerr << "Invalid symbol in transition: " << line << endl;
            return 1;
        }
        table1[from][symbol].insert(to);
    }

    //2. tablazat: a determinisztikus automata
    //az elso sor a kezdoallapot: {q0}
    //minden sor cellait az 1. tablazatbol toltjuk ki: a sorban levo q-k soraibol
    //az adott jel oszlopaban talalhato halmazok unioja
    //ha egy cellaban olyan halmaz all, ami meg nem sor, akkor uj sorkent a tablazat aljara kerul
    //a sorok sorrendje adja az uj allapotok neveit: s0, s1, s2, ...
    vector<Row> table2;
    map<set<string>, int> rowIndex; //melyik q-halmaz hanyadik sor

    table2.push_back({{start}, {}});
    rowIndex[{start}] = 0;

    for (size_t i = 0; i < table2.size(); i++) {
        for (const string &a : alphabet) { //abece sorrendben
            set<string> target;
            for (const string &q : table2[i].qs)
                target.insert(table1[q][a].begin(), table1[q][a].end());

            table2[i].cells[a] = target;

            //ures cella: nincs atmenet, nem lesz belole uj sor
            if (!target.empty() && rowIndex.count(target) == 0) {
                rowIndex[target] = table2.size();
                table2.push_back({target, {}});
            }
        }
    }

    //egy uj allapot vegallapot, ha a halmazaban van eredeti vegallapot
    vector<bool> isFinal(table2.size(), false);
    for (size_t i = 0; i < table2.size(); i++)
        for (const string &q : table2[i].qs)
            if (finals.count(q)) isFinal[i] = true;

    ofstream out(args["output"].as<string>());
    if (!out) {
        cerr << "Error opening output file" << endl;
        return 1;
    }

    //allapotok
    for (size_t i = 0; i < table2.size(); i++) {
        if (i > 0) out << " ";
        out << "s" << i;
    }
    out << "\n";

    //abece
    for (size_t i = 0; i < alphabetList.size(); i++) {
        if (i > 0) out << " ";
        out << alphabetList[i];
    }
    out << "\n";

    //kezdoallapot
    out << "s0\n";

    //vegallapotok
    bool first = true;
    for (size_t i = 0; i < table2.size(); i++) {
        if (!isFinal[i]) continue;
        if (!first) out << " ";
        first = false;
        out << "s" << i;
    }

    //atmenetek a 2. tablazatbol: soronkent, azon belul jelek szerint, az ures cellak kimaradnak
    for (size_t i = 0; i < table2.size(); i++)
        for (const auto &[a, target] : table2[i].cells)
            if (!target.empty())
                out << "\n" << "s" << i << " " << a << " " << "s" << rowIndex[target];

    return 0;
}
