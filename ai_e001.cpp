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
static const int CLEAN_LIMIT = 10;      // 最多保留回合记录
static const int CLEAN_NUMS = 5;        // 超过最多保留记录后,一次清理数据组数
static const int NEAR_RANGE = 44;       // 判定靠近的常数

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

// vector related
template <typename T> void releaseVector(vector<T*> vct);
template <typename T> void makePushBack(vector<T> vct, string str); // 专处理units storage

// handling stored data
template <typename T> void clearOldInfo(vector<T> vct);             // 及时清理陈旧储存信息
int storedHero(PUnit* hero, int prev_n, string attr);               // prev_n轮之前的对应英雄attr属性值
void clearExcessData();                                             // 清楚过剩数据

// handling units
void makeHero(PUnit* unit, Hero* hero_ptr);
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
    // store
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
    // todo
    void analyzeGame();
    void giveOrders();
    void storeCmdInfo();

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
    vector<PUnit*> near_enemy();                // 靠近的敌人
    vector<PUnit*> view_enemy();                // 视野内敌人
    vector<PUnit*> near_friend();               // 靠近的队友,包含自己
    vector<PUnit*> view_friend();               // 视野内队友,包含自己
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
    // 分析比赛局势
    Commander* commander = new Commander();
    commander->analyzeGame();
    commander->giveOrders();
    commander->storeCmdInfo();
    // 英雄选择动作
    // make hero list
    vector<Hero*> heroes;
    for (int i = 0; i < current_friends.size(); ++i) {
        Hero* temp = nullptr;
        makeHero(current_friends[i], temp);
        heroes.push_back(temp);
    }
    // heroes do action and store info
    for (int j = 0; j < heroes.size(); ++j) {
        heroes[j]->selectAction();
        heroes[j]->storeMe();
    }

    // clear vector and release pointers
    clearExcessData();
    releaseVector(heroes);
    releaseVector(current_friends);
    releaseVector(vi_enemies);
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

// vector related
template <typename T>
void releaseVector(vector<T*> vct) {
    for (int i = 0; i < vct.size(); ++i) {
        delete vct[i];
    }
    vct.clear();
}

template <typename T>
void makePushBack(vector<T> vct, string str) {
    /*
     * 如果没有任何单位在本回合储存过,创建一个string并储存
     * 如果有,弹出最后一个元素结尾,增长改写,再塞入
     */

}

// handling stored data
template <typename T>
void clearOldInfo(vector<T> vct) {
    if (vct.size() > CLEAN_LIMIT) {
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

void clearExcessData() {
    clearOldInfo(stored_money);
    clearOldInfo(stored_enemy_money);
    clearOldInfo(stored_friends);
    clearOldInfo(stored_backup);
}

// handling units
void makeHero(PUnit* unit, Hero* hero_ptr) {
    hero_ptr = new Hero(unit);
}

int vsResult(vector<PUnit*> a, vector<PUnit*> b) {
    /*
     * 计算公式:
     * 技能杀伤 = todo 分类讨论,力求准确
     * 生命恢复 = todo
     * 死亡回合 = (hp + 生命恢复 - 技能杀伤) / (对方攻击 - 我方防守 + 生命恢复)
     */

}

int vsResult(PUnit* a, PUnit* b) {
    vector<PUnit*> aa;
    vector<PUnit*> bb;
    aa.push_back(a);
    bb.push_back(b);
    vsResult(aa, bb);
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
    int vs_result = vsResult(near_friend(), near_enemy());  // 近距离队友与敌人的战况
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

// public set state and env

void Hero::setEnv(
        int safety = -1,
        int fight = -1
) {
    if (safety != -1) {
        safety_env = safety;
    }
    if (fight != -1) {
        fight_env = fight;
    }
}

void Hero::setState(
        int atk = -1,
        int move = -1,
        int skill = -1
) {
    if (atk != -1) {
        atk_state = atk;
    }
    if (move != -1) {
        move_state = move;
    }
    if (skill != -1) {
        skill_state = skill;
    }
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
    my_info = my_info + "maxmp:" + this_hero->max_mp + ",";
    my_info = my_info + "safe_e:" + safety_env + ",";
    my_info = my_info + "fight_e:" + fight_env + ",";
    my_info = my_info + "atk_s:" + atk_state + ",";
    my_info = my_info + "move_s:" + move_state + ",";
    my_info = my_info + "skill_s:" + skill_state + "}";
    makePushBack(stored_friends, my_info);
}




