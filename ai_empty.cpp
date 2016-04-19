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

static int BUY_RANK = 42314132;
static const char *HERO_NAME[] = {"Hammerguard", "Master", "Berserker", "Scouter"};

static const int N = 7;
static int scouters[] = {3, 4, 2, 0, 1, 5, 6};
static int now_s = 0;


void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {

    console = new Console(map, info, cmd);

    console->chooseHero(HERO_NAME[0]);

    UnitFilter filter;
    filter.setAvoidFilter("MilitaryBase", "a");
    filter.setAvoidFilter("Observer", "w");
    vector<PUnit *> friends = console->friendlyUnits(filter);

    if (!friends.empty()) {
        for (int i = 0; i < friends.size(); ++i) {
            PUnit *pu = friends[i];
            console->selectUnit(pu);

            if (now_s < N) {
                console->move(MINE_POS[scouters[now_s]]);
                UnitFilter mine_f;
                mine_f.setAreaFilter(new Circle(MINE_POS[scouters[now_s]], 20), "a");
                mine_f.setTypeFilter("Mine", "a");
                vector<PUnit *> mines = console->enemyUnits(mine_f);
                if (!mines.empty())
                    now_s++;
            }
        }
    }
}
