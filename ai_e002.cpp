#include "console.h"
#include "filter.h"
#include "const.h"
#include "Pos.h"
#include <fstream>
#include <vector>
#include <string>
#include <cstdio>
#include <iomanip>
#include <sstream>

using namespace std;

// 调试开关
#define LOG

/*######################## DATA ###########################*/
/************************************************************
 * Const values
 ************************************************************/
static const char *HERO_NAME[] = {"Hammerguard", "Master", "Berserker", "Scouter"};
static const Pos KEY_POINTS[] = {
        Pos(75, 146), Pos(116, 114), Pos(136, 76), Pos(117, 33),
        Pos(75, 23), Pos(36, 37), Pos(27, 76), Pos(35, 110)
};
static const int HERO_COST[] = {
        NEW_HAMMERGUARD_COST, NEW_MASTER_COST, NEW_BERSERKER_COST, NEW_SCOUTER_COST,
        NEW_MASTER_COST, NEW_HAMMERGUARD_COST, NEW_BERSERKER_COST, NEW_SCOUTER_COST
};

/************************************************************
 * Policy const values
 ************************************************************/
// Commander
static const int TACTICS = 0;               // 默认开局战术代号
// Commander::levelUp
static const double LEVEL_UP_COST = 0.5;    // 升级金钱比例
// Commander::buyNewHero
static const double BUY_NEW_COST = 1.0 - LEVEL_UP_COST;     // 买新英雄花费
// Commander::callBack
static const int CALL_BACK_N = 1;           // 单回合召回人数
// Commander::tacticArrange
static const int BATTLE_AREA = 144;         // 战区的判定范围
// Commander::analyzeGame()
static const int KEEP_TACTIC = 30;          // 设置同一战术的执行局数

// clearOldInfo()
static const int CLEAN_LIMIT = 10;          // 最多保留回合记录
static const int CLEAN_NUMS = 5;            // 超过最多保留记录后,一次清理数据组数

// Hero
// Hero::safetyEnv()
static const double ALERT = 0.25;           // 血量预警百分比
static const int HOLD = 3;                  // 坚守回合数预警
// Hero::near_xxx()
static const int NEAR_RANGE = 50;           // 判定靠近的常数
// Hero::stepBackwards()
static const int MASTER_PATIENCE = 10;      // master允许单位进入射程的范围

// unchanged values (during the entire game)
static int CAMP = -1;                       // which camp


/************************************************************
 * Real-time sharing values
 ************************************************************/
static Console *console = nullptr;

// observer info
static int Round = 0;
static int my_money = 0;
static vector<PUnit *> current_friends;
static vector<PUnit *> vi_enemies;
// commander order
static int baser[2] = {};                   // 每个点分配的战斗人员数量:
static int miner[7] = {};                   // 机制:需要[再]增加一个单位时+1
// 依次是 0-中矿,1-8点,2-10点,3-4点,4-2点,5-西北,6-东南
static int mine_taken[7] = {};              // 战况:胶着返回0,失去守卫返回-1,占据返回1

/************************************************************
 * Storage data
 ************************************************************/
static vector<int> stored_money;                    //
static vector<string> stored_friends;               // 以heroes为原型
static vector<int> stored_tactic;                   // 储存的战术
static vector<string> stored_mine_situation;        // 储存的各矿点战况

/*################# Assistant functions ####################*/
#ifdef LOG
// log related
static ofstream fout("log_info.txt");

void printUnit(vector<PUnit *> units);

#endif

// about game
int enemyCamp();                                    // 敌人的camp

// data structure related
template<typename T>
void releaseVector(vector<T *> vct);

template<typename T>
void makePushBack(vector<T> vct, string str); // 专处理units storage
int str2int(string str);

string int2str(int n);

string getSubstr(string origin, string start_s, string end_s);      // 包含给定标记串的子串,子串包含stat_s,只包含end_s[第一个字符]
template<typename T>
int sameLastN(vector<T> vct, T a);            // vct中a从结尾开始连续重复的次数

// handling stored data
template<typename T>
void clearOldInfo(vector<T> vct);             // 及时清理陈旧储存信息
void clearExcessData();                                             // 清楚过剩数据

// handling units
Hero &makeHero(PUnit *unit);

int surviveRounds(vector<PUnit *> host, vector<PUnit *> guest);       // 计算存活轮数,如果host强,返回guest存活,负整数;否则,返回host存活,正整数
int surviveRounds(PUnit *host, PUnit *guest);

bool hasBuff(PUnit *unit, const char *buff);                        // 是否有某buff

// helping surviveRounds()
int teamHP(vector<PUnit *> vct);

int roundAtk(vector<PUnit *> vct);

int roundDef(vector<PUnit *> vct);

int skillDamage(vector<PUnit *> vct, int rounds);

int roundLifeRecover(vector<PUnit *> vct);




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
// 全局指挥官,分析形势,买活,升级,召回等,也充当本方base角色
// 更新不同战区信息,units可以领任务,然后形成team
// global_state: inferior, equal, superior

struct Commander {
    int tactic;                                     // 设置战术代号
    // constructor
    Commander();

    // helper of helper
    int mineSitu(int mine_n, int prev_n);           // prev_n轮以前mine_n号矿状况

    // helper,均自带条件判断,再结算
    void scanMines();                               // 扫描各个战区的战况
    void analyzeSituation();                        // 分析形势,设置战术
    void tacticArrange();                           // 设置预先设好的矿/基地需求安排
    void attack();                                  // 基地攻击
    void buyNewHero();                              // 买英雄
    void levelUp();                                 // 升级英雄
    void callBack();                                // 召回英雄

    // battle analysis
    void handleGame();                              // 调用一切必要的设置函数
    void storeCmdInfo();                            // 储存

};


/************************************************************
 * Heroes
 ************************************************************/
// 实际上是一个友方英雄类,敌方英雄暂无必要考虑
// 由于单位种类太少,技能也很少,所以暂不计划采用继承的方式
// safe_env: 0-safe, 1-dangerous, 2-dying

class Hero {
private:
    PUnit *this_hero;
    // for conveniently storage and usage
    int type, id;
    // environment
    int safety_env;
    // targets
    PUnit *hot_target;
    int tactic_target;

protected:
    // helpers
    int holdRounds();                           // 按上一回合状态,尚可支撑的回合数
    int storedHeroAttr(PUnit *hero, int prev_n, string attr);           // prev_n轮之前的对应英雄attr属性值
    vector<PUnit *> near_enemy();               // 靠近的敌人
    vector<PUnit *> view_enemy();               // 视野内敌人
    vector<PUnit *> near_friend();              // 靠近的队友,包含自己
    vector<PUnit *> view_friend();              // 视野内队友,包含自己
    PUnit* nearestEnemy();                      // 最近的敌人
    void taskHelper(int* situation, int len, Pos* pos_arr);   // 仅辅助doTask()
    // judge
    void safetyEnv();                           // 判断安全现状
    void lockWeakest();                         // 锁定攻击目标
    // actions
    // communicate
    void callBackup();                          // 请求援助
    // move
    void stepBackwards();                       // 远程单位被攻击后撤
    void fastFlee();                            // 快速逃窜
    void doTask();                              // 前往目标
    // attack
    void chaseAttack();                         // 追击
    void hardAttack();                          // 全力攻击
    void hitRun();                              // 诱敌

public:
    // constructor/destructor
    Hero() { Hero(nullptr); }

    Hero(PUnit *hero);

    ~Hero() {
        delete this_hero;
    }

    // decision maker
    void setAll();                              // 调用一切必要的函数动作
    void go();                                  // 考虑所有state的组合
    // store data
    void storeMe();
};


/*#################### MAIN FUNCTION #######################*/
void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {
    console = new Console(map, info, cmd);
    // 获取比赛信息
    Observer *observer = new Observer();
    observer->observeGame();
    observer->storeEconomy();
    // 分析比赛局势
    Commander *commander = new Commander();
    commander->handleGame();
    commander->storeCmdInfo();
    // make hero list
    vector<Hero &> heroes;
    for (int i = 0; i < current_friends.size(); ++i) {
        Hero &temp = makeHero(current_friends[i]);
        heroes.push_back(temp);
    }
    // heroes do actions and store info
    for (int j = 0; j < heroes.size(); ++j) {
        heroes[j].setAll();
        heroes[j].go();
        heroes[j].storeMe();
    }

    // clear vector and release pointers
    clearExcessData();
    releaseVector(current_friends);
    releaseVector(vi_enemies);
    delete observer;
    delete commander;
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
            PUnit *unit = units[i];
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
            PUnit *unit = units[i];
            fout << left << setw(10) << unit->pos;
            fout << left << setw(6) << unit->camp;
            fout << left << setw(5) << unit->hp;
            fout << endl;
        }
    }
    return;

}

#endif

// about game
int enemyCamp() {
    if (CAMP == 0) return 1;
    else return 0;
}

// data structure related
template<typename T>
void releaseVector(vector<T *> vct) {
    for (int i = 0; i < vct.size(); ++i) {
        delete vct[i];
    }
    vct.clear();
}

template<typename T>
void makePushBack(vector<T> vct, string str) {
    /*
     * 如果没有任何单位在本回合储存过,创建一个string并储存
     * 如果有,弹出最后一个元素结尾,增长改写,再塞入
     */
    if (stored_friends.empty())
        stored_friends.push_back(str);
    // 判断是否有同一回合string
    string long_str = stored_friends.back();
    string round_str = getSubstr(long_str, "round:", ",");
    string n_str = getSubstr(round_str, ":", ",");
    string n = n_str.substr(1, n_str.length() - 1);
    int r = str2int(n);
    if (r == Round) {   // assert: 同一回合的字符串
        string same_round = stored_friends.back();  // 该回合的串
        same_round = same_round + str;
        stored_friends.back() = same_round;
    } else {            // assert: 不是同一回合的字符串
        stored_friends.push_back(str);
    }
}

int str2int(string str) {
    stringstream buff;
    buff.clear();
    int number;
    buff << str;
    buff >> number;
    return number;
}

string int2str(int n) {
    stringstream int2str;
    string str;
    int2str.clear();
    int2str << n;
    int2str >> str;
    return str;
}

string getSubstr(string origin, string start_s, string end_s) {
    unsigned long start = origin.find(start_s);
    if (start == string::npos)
        return "npos";
    unsigned long end = origin.find(end_s);
    unsigned long len = end - start + 1;
    string substr = origin.substr(start, len);
    return substr;
}

template<typename T>
int sameLastN(vector<T> vct, T a) {
    int cnt = 0;
    for (int i = (int) (vct.size() - 1); i >= 0; --i) {
        if (vct[i] == a) {
            cnt++;
        } else {
            break;
        }
    }
    return cnt;
}

// handling stored data
template<typename T>
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

void clearExcessData() {
    clearOldInfo(stored_money);
    clearOldInfo(stored_friends);
    clearOldInfo(stored_tactic);
    clearOldInfo(stored_mine_situation);
}


// handling units
Hero &makeHero(PUnit *unit) {
    Hero hero(unit);
    return hero;
}

int surviveRounds(vector<PUnit *> host, vector<PUnit *> guest) {      // toedit 主要策略点
    /*
     * 计算公式:
     * 技能杀伤 = 先忽略技能预估战斗轮数,然后计算保守值
     * 生命恢复 = 本轮全体生命恢复
     * 死亡回合 = (hp + 生命恢复 - 技能杀伤) / (对方攻击 - 我方防守 + 生命恢复)
     * **注意: 由于每回合都计算一次,且数据是传给commander而不是hero,每个英雄自己也计算是否逃跑,因此暂不详细分析是否有英雄死亡导致的战斗力下降
     */
    // 生命值
    int host_HP = teamHP(host);
    int guest_HP = teamHP(guest);

    // 攻击能力
    int host_RA = roundAtk(host);
    int guest_RA = roundAtk(guest);
    // 防守能力
    int host_RD = roundDef(host);
    int guest_RD = roundDef(guest);
    // 生命恢复
    int host_RLR = roundLifeRecover(host);
    int guest_RLR = roundLifeRecover(guest);
    // 预估坚持回合数
    int host_rounds = (host_HP + host_RLR) / (guest_RA - host_RD + host_RLR);
    int guest_rounds = (guest_HP + guest_RLR) / (host_RA - guest_RD + guest_RLR);
    int min_rounds = max(0, min(host_rounds, guest_rounds));        // 至少比0大
    // 总技能杀伤
    int host_SD = skillDamage(host, min_rounds);
    int guest_SD = skillDamage(guest, min_rounds);
    // 再次计算回合
    host_rounds = (host_HP + host_RLR - guest_SD) / (guest_RA - host_RD + host_RLR);
    if (host_rounds < 0) host_rounds = 0;
    guest_rounds = (guest_HP + guest_RLR - host_SD) / (host_RA - guest_RD + guest_RLR);
    if (guest_rounds < 0) guest_rounds = 0;
    // 返回值
    if (host_rounds > guest_rounds) {
        return -guest_rounds;           // 如果host强,返回guest存活轮数,负整数
    } else {
        return host_rounds;             // 如果guest强,返回host存活轮数,正整数
    }
}

int surviveRounds(PUnit *host, PUnit *guest) {
    vector<PUnit *> host_v;
    vector<PUnit *> guest_v;
    host_v.push_back(host);
    guest_v.push_back(guest);
    return surviveRounds(host_v, guest_v);
}

bool hasBuff(PUnit *unit, const char *buff) {
    vector<PBuff> buffs = unit->buffs;
    for (int i = 0; i < buffs.size(); ++i) {
        if (strcmp(lowerCase(buffs[i].name), lowerCase(buff)) == 0)
            return true;
    }
    return false;
}


// helping surviveRounds()
int teamHP(vector<PUnit *> vct) {
    int team_hp = 0;
    for (int i = 0; i < vct.size(); ++i) {
        team_hp += vct[i]->hp;
    }
    return team_hp;
}

int roundAtk(vector<PUnit *> vct) {
    int round_atk = 0;
    for (int i = 0; i < vct.size(); ++i) {
        round_atk += vct[i]->atk;
    }
    return round_atk;
}

int roundDef(vector<PUnit *> vct) {
    int round_def = 0;
    for (int i = 0; i < vct.size(); ++i) {
        round_def += vct[i]->def;
    }
    return round_def;
}

int skillDamage(vector<PUnit *> vct, int rounds) {
    /*
     * 攻击技能包括,HammerAttack, 暂不不考虑Sacrifice
     * 由于冷却时间太长,所以不考虑第二次施法
     */
    int skill_damage = 0;
    for (int i = 0; i < vct.size(); ++i) {
        if (vct[i]->typeId == 3) {  // HammerGuard,从Punit 519行左右可以看出
            int mp_after = (int) (vct[i]->mp + rounds * 0.01 * vct[i]->max_mp);
            if (mp_after > HAMMERATTACK_MP) {
                skill_damage += HAMMERATTACK_DAMAGE[vct[i]->level];
            }
        } else
            continue;
    }
    return skill_damage;
}

int roundLifeRecover(vector<PUnit *> vct) {
    int round_life_recover = 0;
    for (int i = 0; i < vct.size(); ++i) {
        if (vct[i]->typeId == 0) {      // 0 - base
            round_life_recover += 2;
        } else {
            round_life_recover += 0.01 * vct[i]->max_hp;
        }
    }
    return round_life_recover;
}


/************************************************************
 * Implementation: class Observer
 ************************************************************/
void Observer::getEconomyInfo() {
    my_money = console->gold();
}

void Observer::getUnits() {
    UnitFilter filter;
    filter.setAvoidFilter("MilitaryBase", "a");         // 不包括base
    current_friends = console->friendlyUnits(filter);
    vi_enemies = console->enemyUnits(filter);
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
}


/************************************************************
 * Implementation: class Commander
 ************************************************************/
// constructor
Commander::Commander() {
    if (stored_tactic.empty()) {
        tactic = TACTICS;
    } else {
        tactic = stored_tactic.back();
    }
}

// helper of helper


// helper
void Commander::tacticArrange() {     // toedit 主要策略点
    /*
     * 这个函数设置预先设计好的几套战术,在调用analyzeSituation()后,直接根据tactic值简单设置战术
     * // 开局
     * 0.聚集中路
     * 1.西北矿+中路
     * 2.东南矿+中路
     * // 中盘
     * 3.中圈8点+4点矿+中心矿+p0插眼 (优势守中圈)
     *-9.中圈8点+中心矿(准优势)
     * 4.中圈8点+东南矿 (劣势偷经济)
     * 5.西北+东南矿 (游击)
     * 6.准备攻击中圈+偷base (旗鼓相当)
     * // 胜负倾斜
     * 7.全体回守 (要输了)
     * 8.全体推基地 (要赢了) - 感觉没有必要
     */
    // 战术至少保持KEEP_TACTIC轮
    if (Round % KEEP_TACTIC != 0)
        return;
    // 总人数
    int n = (int) current_friends.size();

    UnitFilter filter;
    // 各战区人数数组
    int mine_n[7] = {};
    for (int i = 0; i < MINE_NUM; ++i) {
        filter.setAreaFilter(new Circle(MINE_POS[i], BATTLE_AREA), "a");
        mine_n[i] = (int) console->friendlyUnits(filter).size();
    }
    int base_n[2] = {};
    for (int j = 0; j < MILITARY_BASE_NUM; ++j) {
        filter.setAreaFilter(new Circle(MILITARY_BASE_POS[j], BATTLE_AREA), "a");
        base_n[j] = (int) console->friendlyUnits(filter).size();
    }
    // 根据战术,设定每个地点应该具有的人数
    switch (tactic) {
        case 0:
            miner[0] = n;
            break;
        case 1:
            miner[5] = n / 2;
            miner[0] = n - miner[5];
            break;
        case 2:
            miner[6] = n / 2;
            miner[0] = n - miner[6];
            break;
        case 3:
            miner[1] = n / 3;
            miner[3] = n / 3;
            miner[0] = n - miner[1] - miner[3];
            break;
        case 4:
            miner[1] = n / 2;
            miner[6] = n - miner[1];
            break;
        case 5:
            miner[5] = n / 2;
            miner[6] = n - miner[5];
            break;
        case 6:
            baser[enemyCamp()] = 1;
            miner[0] = n - 1;
            break;
        case 7:
            baser[CAMP] = n;
            break;
        case 8:
            baser[enemyCamp()] = n;
            break;
        case 9:
            miner[1] = n / 2;
            miner[0] = n - miner[1];
            break;
        default:
            break;
    }
    // 减去现有人数,得到调整人数
    for (int k = 0; k < MINE_NUM; ++k) {
        miner[k] -= mine_n[k];
    }
    for (int l = 0; l < MILITARY_BASE_NUM; ++l) {
        baser[l] -= base_n[l];
    }
}

void Commander::scanMines() {
    for (int i = 0; i < MINE_NUM; ++i) {
        // 扫描该矿点人数
        UnitFilter filter;
        filter.setAreaFilter(new Circle(MINE_POS[i], BATTLE_AREA), "a");
        int my_n = (int) console->friendlyUnits(filter).size();
        int enemy_n = (int) console->enemyUnits(filter).size();
        // 判断是否占据或失守
        if (my_n >= 1 && enemy_n >= 1) {
            mine_taken[i] = 0;
        }
        if (my_n <= 1 && enemy_n >= 3) {    // 少于一人对大于3人,即为失守
            mine_taken[i] = -1;
        }
        if (my_n >= 3 && enemy_n <= 1) {
            mine_taken[i] = 1;
        }
        if (my_n == 0 && enemy_n == 0) {    // 矿区不可见,或者没有人
            return;                         // 不更改
        }
    }
}

// helper
void Commander::attack() {
    Pos our_base = MILITARY_BASE_POS[CAMP];
    UnitFilter filter;
    filter.setAvoidFilter("MilitaryBase");
    filter.setAreaFilter(new Circle(our_base, MILITARY_BASE_RANGE), "a");
    vector<PUnit *> enemies = console->enemyUnits(filter);
    if (enemies.empty()) return;
    // 寻找血量最低的敌方单位
    PUnit *lowest = nullptr;
    for (int i = 0; i < enemies.size(); ++i) {
        int min_hp = -1;
        if (enemies[i]->hp < min_hp) {
            lowest = enemies[i];
            min_hp = enemies[i]->hp;
        }
    }
    // 攻击
    console->baseAttack(lowest);    // go 基地攻击
}

void Commander::buyNewHero() {    // toedit 主要策略点
    /*
     * 出新英雄顺序是:H->M->B->S->M->H->B->S
     * ??感觉出英雄顺序影响难以评估
     * ??一有机会就买英雄?
     */
    int n_heroes = (int) current_friends.size();
    int cost = 0;
    // 循环判断,可以连续买英雄
    while (cost < BUY_NEW_COST * my_money) {
        switch (n_heroes) {     // go 买英雄
            case 0:
                console->chooseHero("Hammerguard");
                break;
            case 1:
                console->chooseHero("Master");
                break;
            case 2:
                console->chooseHero("Berserker");
                break;
            case 3:
                console->chooseHero("Scouter");
                break;
            case 4:
                console->chooseHero("Master");
                break;
            case 5:
                console->chooseHero("Hammerguard");
                break;
            case 6:
                console->chooseHero("Berserker");
                break;
            case 7:
                console->chooseHero("Scouter");
                break;
            default:
                break;
        }
        cost += HERO_COST[n_heroes];
        n_heroes++;
    }
}

void Commander::levelUp() {  // toedit 主要策略点
    /*
     * 升级策略似乎挺复杂,制定简单原则:
     * 1.从升级花费最少的英雄开始升级
     * 2.每个回合升级花费不超过LEVEL_UP_COST金钱
     * 英雄太少,免去向量排序(多次排序得不偿失),采用蛮力循环算法
     */
    int round_cost = 0;
    vector<PUnit *> toLevelUp;
    const int TEMP = (const int) current_friends.size();
    int flags[TEMP] = {};               // 设记号,为处理同一英雄多次升级

    while (true) {
        if (round_cost > LEVEL_UP_COST * my_money) {
            toLevelUp.pop_back();       // 弹出,防止升级了最后的单位,却不够升级第一个最小cost单位
            break;
        }
        PUnit *tempHero = nullptr;
        int tempIndex = -1;             // 为flags[]方便
        int min_cost = console->levelUpCost(HERO_LEVEL_LIMIT) + 10;
        // 找最小的cost
        for (int i = 0; i < current_friends.size(); ++i) {
            int level = current_friends[i]->level + flags[i];   // !!注意加上flags
            int cost = console->levelUpCost(level);
            if (cost < min_cost) {
                tempHero = current_friends[i];
                tempIndex = i;
                min_cost = cost;
            }
        }
        // assert: tempHero是最小的cost
        flags[tempIndex]++;             // 更新flags
        round_cost += min_cost;         // 更新round_cost
        toLevelUp.push_back(tempHero);  // 将单位加入待升级组中
    }
    // 实际执行升级
    for (int j = 0; j < toLevelUp.size(); ++j) {
        console->buyHeroLevel(toLevelUp[j]);    // go 英雄买活
    }
}

void Commander::callBack() {
    /*
     * *召回的条件:
     * base在战斗中处于劣势
     */
    // 获取双方战斗能力
    UnitFilter filter;
    filter.setAreaFilter(new Circle(MILITARY_BASE_POS[CAMP], MILITARY_BASE_VIEW), "a");
    filter.setAvoidFilter("MilitaryBase");
    vector<PUnit *> near_enemies = console->enemyUnits(filter);
    vector<PUnit *> near_friends = console->friendlyUnits(filter);
    PUnit *base = const_cast<PUnit *>(console->getMilitaryBase());
    near_friends.push_back(base);
    // 分析对战结果及召唤人数
    int result = surviveRounds(near_friends, near_enemies);
    if (result < 0) {
        return;
    } else {
        // 召回动作开始
        int n = CALL_BACK_N;  // 召回人数
        for (int i = 0; i < current_friends.size(); ++i) {
            // 是否在附近
            Pos hero_pos = current_friends[i]->pos;
            double dist = dis(hero_pos, MILITARY_BASE_POS[CAMP]);
            // 是否处于战斗状态
            bool beAttacked = hasBuff(current_friends[i], "BeAttacked");
            // 执行召回
            if (dist > MILITARY_BASE_VIEW && beAttacked) {
                console->callBackHero(current_friends[i], MILITARY_BASE_POS[CAMP]); // go 召回英雄
                n--;
            }
            // 召回够人数后退出
            if (n == 0) return;
        }
    }
}

void Commander::analyzeSituation() {    // toedit 主要策略点
    /*
     * fixme 临时规则:观察上一回合与本回合是否有矿区失守,如果有,视现有战术决定调整战术
     */
    // 开局执行指定战术
    if (Round < KEEP_TACTIC) {
        tactic = TACTICS;
        return;
    }
    // 中矿是决策的关键点
    int pre_1 = mineSitu(0, 1);         // 中矿上一局
    int pre_2 = mineSitu(0, 2);         // 中矿上第二局
    if (pre_1 == -1 && pre_2 == -1 && mine_taken[0] == -1) {            // 中矿失守三局
        tactic = 5;                     // 改攻击两个角
    } else if (pre_1 == 0 && pre_2 == 0 && mine_taken[0] == 0) {        // 中圈僵持三局
        if (tactic == 0) {              // 已经全力占中
            tactic = 5;                 // 改攻击两个角
        } else {                        // 否则
            tactic = 0;                 // 全力攻击中路
        }
    } else if (pre_1 == 1 && pre_2 == 1 && mine_taken[0] == 1) {        // 中矿拿下三局
        if (tactic == 0) {              // 已经全力占中
            tactic = 9;                 // 开一个新矿
        } else {                        // 没有全力就已经占中了
            tactic = 3;                 // 多开一个分矿
        }
    }
}

// battle analysis

void Commander::storeCmdInfo() {
    stored_money.push_back(my_money);
    stored_tactic.push_back(tactic);
    // store stored_mine_stu
    string toStore = "";
    for (int i = 0; i < MINE_NUM; ++i) {
        if (mine_taken[i] == 0) {
            toStore += "0";
        } else if (mine_taken[i] == 1) {
            toStore += "1";
        } else {
            toStore += "-";
        }
    }
}



/************************************************************
 * Implementation: class Hero
 ************************************************************/
// protected helper
int Hero::holdRounds() {
    int last_hp = storedHeroAttr(this_hero, 1, "hp");
    if (this_hero->hp > last_hp)
        return -1;                              // 如果现在血量反而多,那么返回-1,表示预估不会死亡
    int holds = this_hero->hp / (last_hp - this_hero->hp);
    return holds;
}

int Hero::storedHeroAttr(PUnit *hero, int prev_n, string attr) {
    string str = stored_friends[stored_friends.size() - 1 - prev_n];
    // identification
    string confirm = "{type:" + hero->typeId;
    confirm = confirm + ",id:" + hero->id;
    string hero_str = getSubstr(str, confirm, "}");     // 捕捉英雄字符串
    if (hero_str == "npos")
        return -1;                                      // !!如果前一回合没有这个单位,返回-1
    string attr_str = getSubstr(hero_str, attr, ",");   // 搜寻属性串
    string n_str = getSubstr(attr_str, ":", ",");      // 读属性
    string n = n_str.substr(1, n_str.length() - 2);     // robust?
    int value = str2int(n);
    return value;
}

vector<PUnit *> Hero::near_enemy() {
    UnitFilter filter;
    filter.setAreaFilter(new Circle(this_hero->pos, NEAR_RANGE), "a");
    return console->enemyUnits(filter);
}

vector<PUnit *> Hero::view_enemy() {
    UnitFilter filter;
    filter.setAreaFilter(new Circle(this_hero->pos, this_hero->view), "a");
    return console->enemyUnits(filter);
}

vector<PUnit *> Hero::near_friend() {
    UnitFilter filter;
    filter.setAreaFilter(new Circle(this_hero->pos, NEAR_RANGE), "a");
    return console->friendlyUnits(filter);
}

vector<PUnit *> Hero::view_friend() {
    UnitFilter filter;
    filter.setAreaFilter(new Circle(this_hero->pos, this_hero->view), "a");
    return console->friendlyUnits(filter);
}

PUnit *Hero::nearestEnemy() {
    /*
     * 视野内最近的敌人
     */
    vector<PUnit*> views = view_enemy();
    double min_distance = MAP_SIZE * 1.0;
    int index = -1;
    for (int i = 0; i < views.size(); ++i) {
        double dist = dis(this_hero->pos, views[i]->pos);
        if (dist < min_distance) {
            index = i;
            min_distance = dist;
        }
    }
    if (index != -1)
        return views[index];
    else
        return nullptr;
}

void Hero::taskHelper(int* situation, int len, Pos* pos_arr) {
    Pos me = this_hero->pos;
    // 扫描矿区
    for (int i = 0; i < len; ++i) {
        Circle* battle_field = new Circle(pos_arr[i], BATTLE_AREA);
        if (situation[i] > 0) {                     // 需要人力
            if (battle_field->contain(me))          // 正好在战区
                break;                              // 就继续这个任务,不再寻求别的任务
            else {                                  // 否则,领任务
                situation[i]--;
                tactic_target = i;
                break;      // 不再继续扫描任务,因此id越小的矿越容易被满足
            }
        } else if (situation[i] < 0) {              // 需要撤出
            if (battle_field->contain(me)) {        // 在战区
                situation[i]++;
                continue;                           // 寻求别的任务
            }
        }
    }
}

// protected judge
void Hero::safetyEnv() {    // toedit 主要策略点
    // safe_env: safe, dangerous, dying
    /*
     * 策略: 常数 ALERT(血量百分比) HOLD(坚持回合,int)
     * 0.近距离没有敌方的,safe.
     * 1.计算与上回合血量差,按照此伤害程度,再过HOLD回合内死亡的,dying
     * 2.血量超过ALERT,计算近距离敌人和我方的对战结果,先死亡的,dangerous
     * 3.血量超过ALERT,计算近距离敌人和我方的对战结果,后死亡的,safe(包括没遇到任何敌人)
     * 4.血量不超过ALERT,计算对战,先死亡的,dying
     * 5.血量不超过ALERT,计算对战,后死亡的,dangerous
     */
    // 0
    if (near_enemy().empty()) {     // 近距离没有敌人
        safety_env = 0;             // safe
        return;
    }
    // 1
    int holds = holdRounds();
    if (holds < HOLD) {             // 按上一回合分析撑不了几回合了
        safety_env = 2;             // dying
        return;
    }
    // 2-5
    int vs_result = surviveRounds(near_friend(), near_enemy());  // 近距离队友与敌人的战况
    int hp = this_hero->hp;
    int max_hp = this_hero->max_hp;
    if (hp > ALERT * max_hp) {
        if (vs_result > 0) {        // 如果enemy强
            safety_env = 1;         // dangerous
            return;
        } else {                    // 否则
            safety_env = 0;         // safe
            return;
        }
    } else {
        if (vs_result > 0) {        // 如果enemy强
            safety_env = 2;         // dying
            return;
        } else {                    // 否则
            safety_env = 1;         // dangerous
            return;
        }
    }
}

void Hero::lockWeakest() {
    /*
     * 一般性锁定最弱目标:
     * 最不耐打——坚持回合数最少的目标
     */
    vector<PUnit*> enemies = near_enemy();
    vector<PUnit*> friends = near_friend();
    vector<PUnit*> temp;
    int min_r = 1000;
    int index = -1;
    for (int i = 0; i < enemies.size(); ++i) {
        temp.clear();
        temp.push_back(enemies[i]);
        int r = surviveRounds(near_friend(), temp);
        if (r < 0) {            // 打得过
            r = -r;             // 变r为正数
            if (r < min_r) {
                min_r = r;
                index = i;
            }
        } else {                // 一队人打不过一个,坐等其他函数叫人
            continue;
        }
    }
    // 遍历结束
    if (index != -1) {      // 遍历有收获
        hot_target = enemies[index];
    } else {                    // 全都打不过
        hot_target = nearestEnemy();        // 打最近的一个
    }
}

// protected actions
// communicate
void Hero::callBackup() {
    /*
     * 如果处于dangerous,求援
     */
    if (safety_env == 1) {
        miner[tactic_target]++;
    }
    return;
}


// move
void Hero::stepBackwards() {    // fixme 不鲁棒策略点
    /*
     * 有些远程英雄,需要躲避近程攻击(目前只有Master)
     */
    if (type != 4) return;
    PUnit* nearest = nearestEnemy();
    if (!nearest)
        return;                 // 空指针则返回
    Pos hero_pos = this_hero->pos;
    Pos enemy_pos = nearest->pos;
    double now_dis = dis(hero_pos, enemy_pos);
    if (now_dis < this_hero->range - MASTER_PATIENCE) {             // 如果刚刚进入攻击范围内一点
        hot_target = nearest;                                       // 设为攻击热点
        Pos new_pos = hero_pos;
        double distance = now_dis;
        Pos unit_pos = (hero_pos - enemy_pos) * (1.0 / distance);   // 单位向量
        // 现有位置不停叠加单位向量,试探保持距离的最近点
        while (distance < this_hero->range - MASTER_PATIENCE) {
            new_pos = new_pos + unit_pos;
            distance = dis(new_pos, enemy_pos);
        }
        console->move(new_pos, this_hero);      // go 单位移动
    } else return;
}

void Hero::fastFlee() {
    /*
     * 不顾一切逃跑到基地
     * master考虑使用闪烁技能
     */
    // 条件判断
    if (safety_env != 2)
        return;
    Pos base = MINE_POS[CAMP];
    console->move(base, this_hero);     // go
    // 如果是master
    if (type == 4 && this_hero->canUseSkill("Blink")) {
        // todo
    }
}

void Hero::doTask() {
    taskHelper(miner, MINE_NUM, MINE_POS);
    taskHelper(baser, MILITARY_BASE_NUM, MILITARY_BASE_POS);
    console->move(tactic_target, this_hero);        // go
}

// attack


// public constructor
Hero::Hero(PUnit *hero) {
    this_hero = hero;
    // 如果是敌人英雄,则不设置参数
    if (hero->camp != CAMP)
        return;
    // 设置参数
    type = this_hero->typeId;
    id = this_hero->id;
    hot_target = nullptr;
    // 延续上一回合决策
    if (Round > 1) {
        safety_env = storedHeroAttr(this_hero, 1, "safe_e");
    } else {
        safety_env = 0;
    }
    // tactic_target
    tactic_target = TACTICS;
}

// public decision maker


// public store data
void Hero::storeMe() {
    string my_info = "";
    // store the data as JSON style
    my_info = my_info + "{type:" + int2str(type) + ",";
    my_info = my_info + "id:" + int2str(id) + ",";
    my_info = my_info + "hp:" + int2str(this_hero->hp) + ",";
    my_info = my_info + "mp:" + int2str(this_hero->mp) + ",";
    my_info = my_info + "level:" + int2str(this_hero->level) + ",";
    my_info = my_info + "safe_e:" + int2str(safety_env) + ",";
    my_info = my_info + "round:" + int2str(Round) + ",";
    my_info = my_info + "mine:" + int2str(tactic_target) + ",}";
    makePushBack(stored_friends, my_info);
}




