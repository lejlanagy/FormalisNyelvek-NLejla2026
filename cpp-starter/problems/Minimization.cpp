#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <queue>
#include <algorithm>

#include "Minimization.hpp"
#include "../problem.hpp"

using namespace std;

// Allapotnev szetbontasa betu-elotagra es szam-utotagra, pl. q12 -> ("q", "12")
static pair<string, string> splitName(const string &name) {
    size_t i = name.size();
    while (i > 0 && isdigit((unsigned char) name[i - 1])) i--;
    return {name.substr(0, i), name.substr(i)};
}

// Allapotnevek osszehasonlitasa ugy, hogy q2 < q10 legyen (a szamokat szamkent hasonlitjuk)
static bool nameLess(const string &a, const string &b) {
    auto [pa, na] = splitName(a);
    auto [pb, nb] = splitName(b);
    if (pa != pb || na.empty() || nb.empty()) return a < b;
    if (na.size() != nb.size()) return na.size() < nb.size();
    return na < nb;
}

// Uj allapotnev: q1 -> s1
static string newName(const string &name) {
    string num = splitName(name).second;
    return num.empty() ? "s" + name : "s" + num;
}

// Initialize the parser
void Minimization::initialize_parser(cxxopts::Options &options) {
    options.add_options()
        ("mini", "Minimize the automaton");
}

// Check if the problem is chosen
bool Minimization::is_chosen_problem(const cxxopts::ParseResult &args) {
    return args.count("mini") > 0;
}

int Minimization::run(const cxxopts::ParseResult &args) {
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

    //atmenetek: allapot -> jel -> celallapot (determinisztikus, tehat egy cel van)
    map<string, map<string, string>> delta;
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
        //ugyanaz az atmenet ketszer nem gond, de ket kulonbozo cel mar nem determinisztikus
        if (delta[from].count(symbol) && delta[from][symbol] != to) {
            cerr << "The automaton is not deterministic: " << line << endl;
            return 1;
        }
        delta[from][symbol] = to;
    }

    //1. lepes: az elerheto allapotok meghatarozasa a kezdoallapotbol
    //(ami nem erheto el, az kimarad a minimalis automatabol)
    set<string> reachable = {start};
    queue<string> todo;
    todo.push(start);
    while (!todo.empty()) {
        string q = todo.front();
        todo.pop();
        for (const auto &[a, p] : delta[q]) {
            if (reachable.insert(p).second)
                todo.push(p);
        }
    }

    //2. lepes: haromszoges tablazat
    //az allapotokat nev szerint sorba rendezzuk, a tablazat cellai az allapotparok (i > j)
    //egy cella csillagozott (marked), ha a ket allapot biztosan nem ekvivalens
    vector<string> st(reachable.begin(), reachable.end());
    sort(st.begin(), st.end(), nameLess);
    int n = st.size();
    map<string, int> idx; //allapot -> sorszama a tablazatban
    for (int i = 0; i < n; i++) idx[st[i]] = i;

    vector<vector<bool>> marked(n, vector<bool>(n, false)); //csak az also haromszoget hasznaljuk
    auto isMarked = [&](int i, int j) { return i > j ? marked[i][j] : marked[j][i]; };

    //elso kor: csillagozzuk azokat a parokat, ahol az egyik vegallapot, a masik nem
    for (int i = 0; i < n; i++)
        for (int j = 0; j < i; j++)
            if (finals.count(st[i]) != finals.count(st[j]))
                marked[i][j] = true;

    //tovabbi korok: egy (p, q) par csillagot kap, ha valamelyik jelre
    //a celallapotok (p', q') parja mar csillagozott,
    //vagy az egyiknek van atmenete az adott jelre, a masiknak nincs
    //addig ismeteljuk, amig egy korben mar nem kerul uj csillag a tablazatba
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < i; j++) {
                if (marked[i][j]) continue;
                for (const string &a : alphabet) {
                    auto pi = delta[st[i]].find(a);
                    auto pj = delta[st[j]].find(a);
                    bool hasI = pi != delta[st[i]].end();
                    bool hasJ = pj != delta[st[j]].end();
                    if (hasI != hasJ || (hasI && isMarked(idx[pi->second], idx[pj->second]))) {
                        marked[i][j] = true;
                        changed = true;
                        break;
                    }
                }
            }
        }
    }

    //3. lepes: osszevonas
    //a csillag nelkul maradt cellak parjai ekvivalensek, ezeket vonjuk ossze
    //minden allapot ahhoz a legkisebb nevu allapothoz tartozik, amellyel csillagozatlan a cellaja
    //(a sorrend miatt ez az elso ilyen j), igy az osztaly a legkisebb nevu tagjarol kapja a nevet
    //(pl. {q1, q5} -> s1)
    map<string, int> cls;  //allapot -> osztalya (a nevado allapot sorszama)
    map<int, string> rep;  //osztaly -> nevado (legkisebb nevu) allapot
    for (int i = 0; i < n; i++) {
        int j = 0;
        while (j < i && isMarked(i, j)) j++;
        cls[st[i]] = j;
        rep[j] = st[j];
    }

    //az uj allapotok sorrendje: a nevuk szerint (s0, s1, s2, ...)
    //a rep kulcsai a rendezett tablazatbeli sorszamok, igy mar eleve jo sorrendben vannak
    vector<int> order;
    for (const auto &[c, q] : rep) order.push_back(c);

    ofstream out(args["output"].as<string>());
    if (!out) {
        cerr << "Error opening output file" << endl;
        return 1;
    }

    //allapotok
    for (size_t i = 0; i < order.size(); i++) {
        if (i > 0) out << " ";
        out << newName(rep[order[i]]);
    }
    out << "\n";

    //abece
    for (size_t i = 0; i < alphabetList.size(); i++) {
        if (i > 0) out << " ";
        out << alphabetList[i];
    }
    out << "\n";

    //kezdoallapot: a kezdoallapot osztalya
    out << newName(rep[cls[start]]) << "\n";

    //vegallapotok: egy osztalyban vagy csak veg-, vagy csak nem vegallapotok vannak,
    //ezert eleg a nevadot megnezni; minden osztaly csak egyszer kerul kiirasra
    bool first = true;
    for (int c : order) {
        if (finals.count(rep[c]) == 0) continue;
        if (!first) out << " ";
        first = false;
        out << newName(rep[c]);
    }

    //atmenetek: osztalyonkent egyszer (az osztaly tagjai ugyanoda lepnek, igy nincs duplikalas),
    //a kiindulo allapot, majd a jel szerint rendezve, ahol nincs atmenet, az kimarad
    for (int c : order) {
        const string &q = rep[c];
        for (const auto &[a, p] : delta[q])
            out << "\n" << newName(q) << " " << a << " " << newName(rep[cls[p]]);
    }

    return 0;
}
