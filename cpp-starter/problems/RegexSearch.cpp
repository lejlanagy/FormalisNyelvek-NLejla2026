#include <string>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <regex>

#include "RegexSearch.hpp"
#include "../problem.hpp"

using namespace std;

// Egy logsor felepitese (a feladatleirasban megadott kifejezes):
// 10-15 10:18:51.254   601   601 E InputManager-JNI: uzenet
// csoportok: 1 = az egesz, 2 = datum es ido, 3 = process id, 4 = thread id,
//            5 = szint (I, D, V, E, F, A, W), 6 = tag, 7 = uzenet (ha van)
static const regex logLine("(([0-9-]+ [0-9:.]+) +([0-9]+) +([0-9]+) ([IDVEFAW]) (.+?):(?: (.+))?)");

// Egy logsor eleje a szintig: datum, ido, process id, thread id
static const string head = "^[0-9-]+ [0-9:.]+ +[0-9]+ +[0-9]+ ";

// Hany sorra illeszkedik a kifejezes
static int countMatches(const vector<string> &lines, const regex &r) {
    int count = 0;
    for (const string &line : lines)
        if (regex_search(line, r)) count++;
    return count;
}

// Initialize the parser
void RegexSearch::initialize_parser(cxxopts::Options &options) {
    options.add_options()
        ("rgx", "Search in the log file with regexes (task number)", cxxopts::value<int>());
}

// Check if the problem is chosen
bool RegexSearch::is_chosen_problem(const cxxopts::ParseResult &args) {
    return args.count("rgx") > 0;
}

int RegexSearch::run(const cxxopts::ParseResult &args) {
    ifstream in(args["input"].as<string>());
    if (!in) {
        cerr << "Error opening input file" << endl;
        return 1;
    }

    //a logfajl sorainak beolvasasa
    vector<string> lines;
    string line;
    while (getline(in, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1); //windowsos sorvege
        lines.push_back(line);
    }

    int task = args["rgx"].as<int>();
    vector<string> result; //a kimenet sorai

    if (task == 1) {
        //minden alpont egy szam: hany sorra illeszkedik az adott kifejezes
        //10. honap 15-en
        result.push_back(to_string(countMatches(lines, regex("^10-15 "))));
        //10. honap 15-en 10:18:51-kor
        result.push_back(to_string(countMatches(lines, regex("^10-15 10:18:51"))));
        //51 masodperckor (barmelyik perc)
        result.push_back(to_string(countMatches(lines, regex("^[0-9-]+ [0-9]+:[0-9]+:51\\."))));
        //lowmemorykiller tag
        result.push_back(to_string(countMatches(lines, regex(head + "[IDVEFAW] lowmemorykiller"))));
        //10:18:51-kor a 221-es process id
        result.push_back(to_string(countMatches(lines, regex("^[0-9-]+ 10:18:51\\.[0-9]+ +221 "))));
        //hibak (E)
        result.push_back(to_string(countMatches(lines, regex(head + "E "))));
        //a PackageManager warningjai (W)
        result.push_back(to_string(countMatches(lines, regex(head + "W PackageManager"))));
        //ExoPlayer-es debug (D) sorok
        result.push_back(to_string(countMatches(lines, regex(head + "D .*ExoPlayer"))));
        //java allomany poziciot tartalmazo hibak es warningok (E vagy W, .java:)
        result.push_back(to_string(countMatches(lines, regex(head + "[EW] .*\\.java:"))));

        //Thread szo az uzenetben (nem a tagben): a 7. csoportban keressuk
        regex thread("Thread");
        int count = 0;
        for (const string &l : lines) {
            smatch m;
            if (regex_search(l, m, logLine) && m[7].matched && regex_search(m[7].str(), thread))
                count++;
        }
        result.push_back(to_string(count));
    }
    else if (task == 2) {
        //a hibasorokbol (E) a stack trace helye, pl. (HashMap.java:500) -> HashMap.java
        //minden hibasorbol kulon, az elofordulas sorrendjeben, ismetlodessel egyutt
        regex place("\\(([^():]+\\.java):[0-9]+\\)");
        for (const string &l : lines) {
            smatch m, p;
            if (regex_search(l, m, logLine) && m[5].str() == "E" && regex_search(l, p, place))
                result.push_back(p[1].str());
        }
    }
    else if (task == 3) {
        //az osszes kulonbozo process id, az elofordulas sorrendjeben
        set<string> seen;
        for (const string &l : lines) {
            smatch m;
            if (regex_search(l, m, logLine) && seen.count(m[3].str()) == 0) {
                seen.insert(m[3].str());
                result.push_back(m[3].str());
            }
        }
    }
    else if (task == 4) {
        //a legtobb hibat (E) tartalmazo process: hibak szama process id-nkent
        map<string, int> errors;
        vector<string> order; //a process id-k elofordulasi sorrendje (egyenloseg eseten az elso nyer)
        for (const string &l : lines) {
            smatch m;
            if (regex_search(l, m, logLine) && m[5].str() == "E") {
                if (errors.count(m[3].str()) == 0) order.push_back(m[3].str());
                errors[m[3].str()]++;
            }
        }

        string best;
        int bestCount = 0;
        for (const string &pid : order) {
            if (errors[pid] > bestCount) {
                best = pid;
                bestCount = errors[pid];
            }
        }
        result.push_back("PID=" + best + " (" + to_string(bestCount) + " errors)");
    }
    else if (task == 5) {
        //a fo szalon tortent hibak (E) szama: a process id es a thread id megegyezik
        int count = 0;
        for (const string &l : lines) {
            smatch m;
            if (regex_search(l, m, logLine) && m[5].str() == "E" && m[3].str() == m[4].str())
                count++;
        }
        result.push_back(to_string(count));
    }
    else {
        cerr << "Unknown task number: " << task << endl;
        return 1;
    }

    ofstream out(args["output"].as<string>());
    if (!out) {
        cerr << "Error opening output file" << endl;
        return 1;
    }

    for (size_t i = 0; i < result.size(); i++) {
        if (i > 0) out << "\n";
        out << result[i];
    }

    return 0;
}
