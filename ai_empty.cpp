/*
 * 训练ai:
 * 空ai,可以便于测试走位等
 */
#include "console.h"
#include "Pos.h"
#include <fstream>
#include <map>

using namespace std;

static Console *console;
static ofstream print("empty_log.txt");
static map<int, PUnit*> friends;

static int BUY_RANK = 42314132;
static const char *HERO_NAME[] = {"Hammerguard", "Master", "Berserker", "Scouter"};

static const int N = 7;
static int scouters[] = {3, 4, 2, 0, 1, 5, 6};
static int now_s = 0;


void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {

    console = new Console(map, info, cmd);

    UnitFilter filter;
    filter.setAvoidFilter("MilitaryBase", "w");
    filter.setAvoidFilter("Observer", "a");
    vector<PUnit *> friends = console->friendlyUnits(filter);

    if (friends.size() == 0) {
        console->chooseHero(HERO_NAME[0]);
    }




    /******************************/
    if (!friends.empty()) {
        for (int i = 0; i < friends.size(); ++i) {
            Pos test = Pos(105, 61);
            PUnit *pu = friends[i];
            console->selectUnit(pu);
            if (pu->pos == test) {
                console->useSkill("Blink", Pos(5, 5));
            } else {
                console->move(test);
            }
        }
    }
}
