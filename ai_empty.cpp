/*
 * 训练ai:
 * 空ai,可以便于测试走位等
 */
#include "console.h"
#include <fstream>
#include <map>

using namespace std;

static Console *console;
static ofstream print("empty_log.txt");
static map<int, PUnit*> friends;


void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {
    print << "=======" << endl;

    Console *console = new Console(map, info, cmd);

    console->chooseHero("Master");
    UnitFilter filter;
    filter.setAvoidFilter("MilitaryBase", "a");
    vector<PUnit *> current_friends = console->friendlyUnits();

    if (current_friends.empty()) return;

    for (int i = 0; i < current_friends.size(); ++i) {
        PUnit *punit = current_friends[i];
        int id = punit->id;
        friends[id] = punit;
    }

    for (auto miter = friends.begin(); miter != friends.end(); ++miter) {
        int id_test = (*miter).first;
        console->move(MINE_POS[0], friends[id_test]);
//        console->move(MINE_POS[0], console->getUnit(id_test));
    }

    friends.clear();
    print << "********" << endl << endl;
}



