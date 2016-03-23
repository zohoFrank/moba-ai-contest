#include "console.h"
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>

using namespace std;

// 调试开关
#define LOG
typedef pair<Pos, int> HELP;
/*######################## DATA ###########################*/
/************************************************************
 * Const values
 ************************************************************/
static const char* HERO_NAME[] = {"Hammerguard", "Master", "Berserker", "Scouter"};
static const Pos KEY_POINTS[] = {
        Pos(75, 146), Pos(116, 114), Pos(136, 76), Pos(117, 33),
        Pos(75, 23), Pos(36, 37), Pos(27, 76), Pos(35, 110)
};

// unchanged values (during the entire game)
static int CAMP = -1;                       // which camp


/************************************************************
 * Real-time sharing values
 ************************************************************/
static Console* console = nullptr;

// observer info
static int Round = 0;
static int my_money = 0;
static int enemy_money = 0;
static vector<PUnit*> current_friends;
static vector<PUnit*> vi_enemies;
// commander order
static vector<HELP> needBackup;             // 某些位置请求支援
static int baser[2] = {};                   // 这3个数组为每个点分配的战斗人员数量,
static int miner[7] = {};                   // 机制:需要[再]增加一个单位时+1,
static int point_taker[8] = {};             // 新加入一个支援单位-1,最小可以为负,表示不需要如此多战斗人员


/************************************************************
 * Storage data
 ************************************************************/
static vector<int> stored_money;                    //
static vector<int> stored_enemy_money;              // 之前几个回合的money信息
static vector<string> stored_friends;               // 以heroes为原型
static vector<string> stored_backup;                // 以pos为原型



/*################# Assistant functions ####################*/
#ifdef LOG
// log related
static ofstream fout("log_info.txt");
void printUnit(vector<PUnit*> units);
#endif

// handling stored data
template <typename T> void clearOldInfo(vector<T> vct);             // 及时清理陈旧储存信息
int storedHero(PUnit* hero, int prev_n, string attr);               // prev_n轮之前的对应英雄attr属性值
template <typename T> void makePushBack(vector<T> vct, string str); // 专处理units storage

// handling units
int vsResult(vector<PUnit*> a, vector<PUnit*> b);                   // -1: a胜; 0: 平; 1: b胜
int vsResult(PUnit* a, PUnit* b);                                   // 重载



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
    void storeEconomy();
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
    string storeUnit(PUnit* unit);                  // 将单位信息存储成string,记录以便下回合使用

    // battle analysis



};


/************************************************************
 * Heroes
 ************************************************************/
// 实际上是一个友方英雄类,敌方英雄暂无必要考虑
// attack_state: negative, positive
// move_state: still, move
// skill_state: restricted, available
// safe_env: safe, dangerous, dying
// fight_env: nothing, inferior, equal, superior

class Hero {
private:
    PUnit* this_hero;
    // for conveniently storage and usage
    int type, id;
    // environment
    int safety_env;
    int fight_env;
    // state
    int atk_state;
    int move_state;
    int skill_state;

protected:
    // helpers
    int holdRounds();                           // 按上一回合状态,尚可支撑的回合数
    vector<PUnit*> view_enemy();                // 视野内敌人
    vector<PUnit*> view_friend();               // 视野内队友,todo 包含自己??
    // judge
    void safetyEnv();                           // 判断安全现状
    void fightEnv();                            // 判断战斗势态
    // actions
    void callBackup();                          // 请求支援
    void fastFlee();                            // 快速逃窜
    void chaseAttack();                         // 追击
    void hardAttack();                          // 全力攻击
    void hitRun();                              // 诱敌
    void stayAlarmed();                         // 淡定警戒 -- 不主动攻击,但还击或随时准备做动作,适合挖矿

public:
    // constructor/destructor
    Hero(PUnit* hero);
    ~Hero() {
        delete this_hero;
    }

    // set state and env
    void setEnv(int safety = -1, int fight = -1);
    void setState(int atk = -1, int move = -1, int skill = -1);     // 如果是-1,不改变当前值
    // decision maker
    void selectAction();                        // 考虑所有state的组合
    // store data
    void storeMe();
};



/*#################### MAIN FUNCTION #######################*/
void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {
    console = new Console(map, info, cmd);
    // 获取比赛信息
    Observer* observer = new Observer();
    observer->observeGame();
    observer->storeEconomy();



    delete observer;
    delete console;
}



/*################### IMPLEMENTATION #######################*/
/************************************************************
 * Implementation: Assistant function
 ************************************************************/
#ifdef LOG
// log related
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

// handling stored data
template <typename T>
void clearOldInfo(vector<T> vct) {
    const int CLEAN_NUMS = 10;
    const int CLEAN_LIMIT = 20;
    if (vct.size() > 20) {
        int check = 0;
        for (auto i = vct.begin(); i != vct.end(); i++) {
            check++;
            vct.erase(i);
            if (check == CLEAN_NUMS) break;
        }
    }
}

int storedHero(PUnit* hero, int prev_n, string attr) {
    string json_str = "";
    // todo
}



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

void Observer::storeEconomy() {
    clearOldInfo(stored_money);
    clearOldInfo(stored_enemy_money);
    stored_money.push_back(my_money);
    stored_enemy_money.push_back(enemy_money);
}


/************************************************************
 * Implementation: class Commander
 ************************************************************/


/************************************************************
 * Implementation: class Hero
 ************************************************************/
// protected helper

// protected judge
void Hero::safetyEnv() {
    /*
     * 策略: 常数 ALERT(血量百分比) HOLD(坚持回合,int)
     * 1.计算与上回合血量差,按照此伤害程度,再过HOLD回合内死亡的,dying
     * 2.血量超过ALERT,计算视野中敌人和我方的对战结果,先死亡的,dangerous
     * 3.血量超过ALERT,计算视野中敌人和我方的对战结果,后死亡的,safe(包括没遇到任何敌人)
     * 4.血量不超过ALERT,计算对战,先死亡的,dying
     * 5.血量不超过ALERT,计算对战,后死亡的,dangerous
     */
    const double ALERT = 0.25;
    const int HOLD = 3;
    // 1
    int holds = holdRounds();
    if (holds < HOLD) {
        safety_env = 2;
        return;
    }
    // 2-5
    int vs_result = vsResult(view_friend(), view_friend());
    int hp = this_hero->hp;
    int max_hp = this_hero->max_hp;
    if (hp > ALERT * max_hp) {
        if (vs_result == -1) {
            safety_env = 1;
            return;
        } else {
            safety_env = 0;
            return;
        }
    } else {
        if (vs_result == -1) {
            safety_env = 2;
            return;
        } else {
            safety_env = 1;
            return;
        }
    }
}


// public constructor
Hero::Hero(PUnit *hero) {
    this_hero = hero;
    // 如果是敌人英雄,则不设置参数
    if (hero->camp != CAMP)
        return;
    // 设置参数
    type = this_hero->typeId;
    id = this_hero->id;
    // 延续上一回合决策
    Hero last_hero = storedHero(this_hero, 1);
    safety_env = last_hero.safety_env;
    fight_env = last_hero.fight_env;
    atk_state = last_hero.atk_state;
    move_state = last_hero.move_state;
    skill_state = last_hero.skill_state;
}

// public store data
void Hero::storeMe() {
    string my_info = "";
    // store the data as JSON style
    my_info = my_info + "{camp:" + this_hero->camp + ",";
    my_info = my_info + "type:" + type + ",";
    my_info = my_info + "id:" + id + ",";
    my_info = my_info + "hp:" + this_hero->hp + ",";
    my_info = my_info + "mp:" + this_hero->mp + ",";
    my_info = my_info + "attack:" + this_hero->atk + ",";
    my_info = my_info + "defend:" + this_hero->def + ",";
    my_info = my_info + "level:" + this_hero->level + ",";
    my_info = my_info + "maxhp:" + this_hero->max_hp + ",";
    my_info = my_info + "maxmp:" + this_hero->max_mp + "},";
    makePushBack(stored_friends, my_info);
}




