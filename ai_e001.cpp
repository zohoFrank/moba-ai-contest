#include "console.h"
#include <fstream>
#include <vector>
#include <string>
#include <tkDecls.h>
#include <iomanip>

using namespace std;

// 调试开关
#define LOG

#define SIZE(X) (int((x).size()))
/*######################## DATA ###########################*/
/************************************************************
 * Const values
 ************************************************************/
static const char* HERO_NAME[] = {"Hammerguard", "Master", "Berserker", "Scouter"};
static const Pos KEY_POINTS[] = {Pos(75, 146), Pos(116, 114), Pos(136, 76), Pos(117, 33),
                                 Pos(75, 23), Pos(36, 37), Pos(27, 76), Pos(35, 110)};

// unchanged values (during the game)
static int CAMP = -1;                       // which camp

/************************************************************
 * Sharing values
 ************************************************************/
static Console* console = nullptr;

// observer info
static int Round = 0;
static int my_money = 0;
static int enemy_money = 0;
static vector<PUnit*> current_friends;
static vector<PUnit*> vi_enemies;
// commander order
static vector<Pos> needBackup;              // 某些位置请求支援
static int baser[2] = {};                   // 这3个数组为每个点分配的战斗人员数量,
static int miner[7] = {};                   // 机制:需要[再]增加一个单位时+1,
static int point_taker[8] = {};             // 新加入一个支援单位-1,最小可以为负,表示不需要如此多战斗人员

/*################# Assistant functions ####################*/
void getWeakestUnit(vector<PUnit*> units, PUnit* weakest);
void getStrongestUnit(vector<PUnit*> units, PUnit* strongest);
// log related
#ifdef LOG
static ofstream fout("log_info.txt");
void printUnit(vector<PUnit*> units);
#endif

/*##################### STATEMENT ##########################*/
/************************************************************
 * Observer
 ************************************************************/
// 汇总所有发现的信息,便于各级指挥官决策
// 必须每次运行均首先调用
class Observer {
protected:
    // helpers
    void getEconomyInfo();
    void getUnits();

public:
    // run observer
    void observeGame();
};

/************************************************************
 * Global commander
 ************************************************************/
// 全局指挥官,分析形势,买活,升级,召回等
// 更新不同战区信息,units可以领任务,然后形成team
// global_state: inferior, equal, superior

struct Commander {
    int tactic = 0;     // 设置战术代号
    int situation = 0;  // 分析战场局势

    // helper
    void changeTeams();

    // todo 分析战局

    // todo 设置重点战区


};

/************************************************************
 * Heroes
 ************************************************************/
// attack_state: positive, negative
// safe_state: dying, dangerous, safe
// fight_state: nothing, inferior, equal, superior

class Hero {
private:
    PUnit* this_hero;
    Pos* position;
    int hp, mp;
    int attack, defend;
    PBuff* buff;
    // fight??
    PUnit* attackOn;                            // 正在攻击此单位
    PUnit* defendFrom;                          // 承受来自此单位攻击
    // state
    int atk_state;
    int safety_state;
    int fight_state;

protected:
    bool isSafe();
    void underAttack();                         // ??是否承受攻击,无设定defendFrom为nullptr
    void setState(int atk = 0, int sft = 0, int ft = 0);
                                                // 若为0,则不设置

public:
    // todo constructor/deconstructor
    // todo 零散的待整理的高级动作
    virtual void fastFlee() = 0;                // 快速逃窜
    virtual void chaseAttack() = 0;             // 追击
    virtual void hardAttack() = 0;              // 全力攻击
    virtual void hitRun() = 0;                  // 诱敌
    virtual void stayAlarmed() = 0;             // 淡定警戒 -- 不主动攻击,但还击或随时准备做动作,适合挖矿
    // decision maker & actions
    void selectAction();                        // 考虑所有state的组合

};

/***************** Derived classes **************************/
class Hammerguard : public Hero {

};

class Master : public Hero {

};

class Berserker : public Hero {

};

class Scouter : public Hero {

};


/*#################### MAIN FUNCTION #######################*/
void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {
    console = new Console(map, info, cmd);
    // 获取比赛信息
    Observer* observer = new Observer();
    observer->observeGame();



    delete observer;
    delete console;
}



/*################### IMPLEMENTATION #######################*/
/************************************************************
 * Implementation: Assistant function
 ************************************************************/
#ifdef LOG
void printUnit(vector<PUnit *> units) {
    // if hero
    if (units[0]->isHero()) {
        // print title of each column
        fout << left << setw(6) << "TYPE";
        fout << left << setw(16) << "NAME";
        fout << left << setw(5) << "ID";
        fout << left << setw(5) << "LEVEL";
        fout << left << setw(5) << "HP";
        fout << left << setw(5) << "MP";
        fout << left << setw(5) << "ATK";
        fout << left << setw(5) << "DEF";
        fout << left << setw(10) << "POS";
        fout << left << setw(10) << "BUFF";
        fout << endl;
        // print content
        for (int i = 0; i < units.size(); ++i) {
            PUnit* unit = units[i];
            // print basic hero info
            fout << left << setw(6) << "Hero";
            fout << left << setw(16) << unit->name;
            fout << left << setw(5) << unit->id;
            fout << left << setw(5) << unit->level;
            fout << left << setw(5) << unit->hp;
            fout << left << setw(5) << unit->mp;
            fout << left << setw(5) << unit->atk;
            fout << left << setw(5) << unit->def;
            fout << left << setw(10) << unit->pos;
            // print buff
            vector<PBuff> buff = unit->buffs;
            for (int j = 0; j < buff.size(); ++j) {
                fout << left << setw(15) << buff[j].name << "(" << buff[j].timeLeft << ")";
            }
            // over
            fout << endl;
        }
        return;
    }

    // if mine or base
    if (units[0]->isMine()) {
        // print titles
        fout << left << setw(10) << "POS";
        fout << left << setw(6) << "CAMP";
        fout << left << setw(5) << "HP";
        fout << endl;
        // print content
        for (int i = 0; i < units.size(); ++i) {
            PUnit* unit = units[i];
            fout << left << setw(10) << unit->pos;
            fout << left << setw(6) << unit->camp;
            fout << left << setw(5) << unit->hp;
            fout << endl;
        }
    }
    return;

}
#endif

/************************************************************
 * Implementation: class Observer
 ************************************************************/
void Observer::getEconomyInfo() {
    my_money = console->gold();
}

void Observer::getUnits() {
    current_friends = console->friendlyUnits();
    vi_enemies = console->enemyUnits();
}

void Observer::observeGame() {
    if (Round == 0) {
        CAMP = console->camp();
#ifdef LOG
        fout << "==== Game begins ====" << endl;
        fout << "camp: " << CAMP << endl;
#endif
    }
    Round = console->round();
    getEconomyInfo();
    getUnits();
#ifdef LOG
    fout << "==== Round: " << Round << " ====" << endl;
    fout << "my money: " << my_money << endl;
    fout << "Hero info: " << endl;
    fout << "//FRIEND//" << endl;
    printUnit(current_friends);
    fout << "//VISIBLE ENEMY//" << endl;
    printUnit(vi_enemies);
#endif
}


/************************************************************
 * Implementation:
 ************************************************************/

/************************************************************
 * Implementation:
 ************************************************************/
