#include "console.h"
#include <fstream>
#include <vector>
#include <string>
#include <cstdio>
#include <iomanip>
#include <sstream>

using namespace std;

// 调试开关
#define LOG

class Hero;

struct Commander;
struct Tactic;
/*######################## DATA ###########################*/
/************************************************************
 * Const values
 ************************************************************/
static const int KEY_POINTS_NUM = 8;
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
static const double BUY_NEW_COST = 1.0;     // 买新英雄花费
// Commander::callBack
static const int CALL_BACK_N = 1;           // 单回合召回人数
// Commander::tacticArrange
static const int BATTLE_AREA = 144;         // 战区的判定范围
// Commander::analyzeGame()
static const int KEEP_TACTIC = 10;          // 设置同一战术的执行局数

// clearOldInfo()
static const int CLEAN_LIMIT = 5;           // 最多保留回合记录
static const int CLEAN_NUMS = 2;            // 超过最多保留记录后,一次清理数据组数

// Hero
// Hero::judgeState()
static const double ALERT = 0.25;           // 血量预警百分比
static const int HOLD = 3;                  // 坚守回合数预警
// Hero::near_u
static const int BACKUP_RANGE = 200;        // 支援范围常数
// Hero::stepBackwards()
static const int MASTER_PATIENCE = 10;      // master允许单位进入射程的范围

// unchanged values (during the entire game)
static int CAMP = -1;                       // which camp


/************************************************************
 * Real-time sharing values
 ************************************************************/
static Console *console = nullptr;
static Commander *commander = nullptr;
static int Round = 0;

// 7个矿顺序依次是 0-中矿,1-8点,2-10点,3-4点,4-2点,5-西北,6-东南

/************************************************************
 * Storage data
 ************************************************************/
static vector<int> str_money;                   //
static vector<string> str_friends;              // 以heroes为原型
static vector<int> str_tactic;                  // 储存的战术



/*################# Assistant functions ####################*/
// ================ Log Related ====================
#ifdef LOG
static ofstream logger("log_info.txt");

void printUnit(vector<PUnit *> units);

void printHeroList(vector<Hero> units);

template<typename T>
void printString(vector<T> vct);      // T重载了<<
#endif

// =============== Basic Algorithms ================
// Path finding
Pos changePos(const Pos &origin, const Pos &reference, double len, bool away = true);  // 详见实现

// String and int transfer
int str2int(string str);

string int2str(int n);

// Data structure related
template<typename T>
void releaseVector(vector<T *> vct);

void makePushBack(vector<string> &vct, string str);                 // 专处理units storage
string getSubstr(string origin, string start_s, string end_s);      // 包含给定标记串的子串,子串包含stat_s,只包含end_s[第一个字符]

// Handling stored data
template<typename T>
void clearOldInfo(vector<T> &vct);             // 及时清理陈旧储存信息
void clearExcessData();                                             // 清楚过剩数据


// ============== Game and Units ===================
int enemyCamp();                                                    // 敌人的camp
// Unit state
bool hasBuff(PUnit *unit, const char *buff);                        // 是否有某buff
int teamHP(vector<PUnit *> vct);

int roundAtk(vector<PUnit *> vct);

int roundDef(vector<PUnit *> vct);

int skillDamage(vector<PUnit *> vct, int rounds);

int roundLifeRecover(vector<PUnit *> vct);

// Battle
int surviveRounds(vector<PUnit *> host,
                  vector<PUnit *> guest);             // 计算存活轮数,如果host强,返回guest存活,负整数;否则,返回host存活,正整数
vector<PUnit *> range_units(int range, int camp, vector<string> avoids);    // 范围内的单位过滤器
int storedHeroAttr(PUnit *hero, int prev_n, string attr);                   // prev_n轮之前的对应英雄attr属性值

/************************************************************
 * Global commander
 ************************************************************/
// 全局指挥官,分析形势,买活,升级,召回等,也充当本方base角色
// 更新不同战区信息,units可以领任务,然后形成team
// global_state: inferior, equal, superior
struct Commander {
    int Economy;

    vector<int> tactic;                             // 设置战术代号

    vector<PUnit *> cur_friends;
    const PUnit *base;

    vector<PUnit *> vi_enemies;
    const PUnit *en_base;                           // 敌人基地

    vector<PUnit *> vi_mines;
    vector<PUnit *> vi_monsters;

    vector<Hero> heroes;

    /**********************************************************/
    // constructor
    Commander();

    ~Commander();

    // HELP CONSTRUCTOR
    void makeHeroes();                              // 制造一个全体我方英雄对象的向量
    void getUnits();                                // 获取单位信息

    // tactics
    void analyzeSituation();                        // 分析形势,设置战术
    void tacticArrange();                           // 向向量中的英雄分配战术
    // base actions
    void attack();                                  // 基地攻击
    void buyNewHero();                              // 买英雄
    void levelUp();                                 // 升级英雄
    void callBack();                                // 召回英雄

    /**********************************************************/
    // Interface to outside
    void addTactic(int i);                          // 增加一个战术
    // LOADER
    void RunAnalysis();

    void HeroesAct();

    void StoreCmdInfo();                            // 储存

};

/************************************************************
 * Tactic
 ************************************************************/
// 一个战术包括地点/单位/行进路径
struct Tactic {
    int id;
    Pos target;
    vector<Pos> path;

    // constructor
    Tactic(int id, const Pos &target, const vector<Pos> &path) : id(id), target(target), path(path) { }

    Tactic(int id, const Pos &target) : id(id), target(target) { }

    // attribute
    bool hasPath();
};

/************************************************************
 * Heroes
 ************************************************************/
// 实际上是一个友方英雄类,敌方英雄暂无必要考虑
// 由于单位种类太少,技能也很少,所以暂不计划采用继承的方式
// state: 0-safe, 1-dangerous, 2-dying
// contact: 0-no, 1-yes
class Hero {
public:
    PUnit *hero_ptr;

    int type, id;

    int state;
    int contact;
    bool attackCd;
    bool skillCd;

    // tactic
    PUnit *hot;
    Tactic target;

    // shared units vectors
    vector<PUnit *> range_en;                    // 射程范围内的敌人
    vector<PUnit *> near_u;                      // 支援范围内的单位

    /*************************Setters**************************/
    PUnit *nearestEnemy();                      // 最近的敌人

    // setter
    void setContact();                          // 判断是否交战
    void judgeState();                          // 判断安全现状
    void lockHotTarget();                       // 锁定攻击目标
    void callBackup();                          // 请求援助

    /**************************Actions**************************/
    // move,基本不调用其他动作接口
    void cdWalk();                              // cd间的躲避步伐
    void stepBackwards();                       // 远程单位被攻击后撤
    void fastFlee();                            // 快速逃窜
    void doTask();                              // 前往目标

    // attack,调用move接口
    bool hammerguardAttack();                   // 重锤技能判断并释放
    bool berserkerAttack();                     // 孤注一掷技能判断并释放
    bool masterAttack();                        // 闪烁技能判断并释放
    bool scouterAttack();                       // 插眼技能判断并释放

    // 调用attack类接口
    void contactAttack();                       // 全力攻击,调用移动接口

    /**********************************************************/
    // constructor/destructor
    void setPtr(PUnit *unit);                   // 连接ptr
    void setUnits();                            // 设置常用的单位向量
    Hero(PUnit *hero = nullptr);

    ~Hero();

    // 储存
    void storeMe();

    /*************************Loader***************************/
    // LOADER
    void Act();         // 调用一切接口
};


/*#################### MAIN FUNCTION #######################*/
void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {
    // Define Tactics
    const int TACTIC_APPLIED[] = {};
    int TACTIC_NUM = sizeof(TACTIC_APPLIED) / sizeof(int);
    // Create pointers
    console = new Console(map, info, cmd);
    commander = new Commander();
    // Set tactics
    for (int i = 0; i < TACTIC_NUM; ++i) {
        commander->addTactic(i);
    }
    // Run Commander, analyze and arrange tactics
    commander->RunAnalysis();

    // Hero do actions
    commander->HeroesAct();

    // Store all
    commander->StoreCmdInfo();

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
    if (units.empty()) return;
    // if hero
    // print title
    logger << left << setw(14) << "NAME";
    logger << left << setw(5) << "ID";
    logger << left << setw(8) << "LEVEL";
    logger << left << setw(5) << "HP";
    logger << left << setw(5) << "MP";
    logger << left << setw(5) << "ATK";
    logger << left << setw(5) << "DEF";
    logger << left << setw(10) << "POS";
    logger << left << setw(10) << "BUFF";
    logger << endl;
    // print content
    for (int i = 0; i < units.size(); ++i) {
        PUnit *unit = units[i];
        // print basic hero info
        logger << left << setw(14) << unit->name;
        logger << left << setw(5) << unit->id;
        logger << left << setw(8) << unit->level;
        logger << left << setw(5) << unit->hp;
        logger << left << setw(5) << unit->mp;
        logger << left << setw(5) << unit->atk;
        logger << left << setw(5) << unit->def;
        // print pos
        string pos = int2str(unit->pos.x) + "," + int2str(unit->pos.y);
        logger << left << setw(10) << pos;
        // print buff
        vector<PBuff> buff = unit->buffs;
        for (int j = 0; j < buff.size(); ++j) {
            string buff_name = buff[j].name;
            string buff_str = buff_name + "(" + int2str(buff[j].timeLeft) + ")";
            logger << left << setw(15) << buff_str;
        }
        // over
        logger << endl;
    }
}

void printHeroList(vector<Hero> units) {
    if (units.empty()) {
        logger << "!! get empty units" << endl;
        return;
    }
    // print title of each column
    logger << left << setw(14) << "NAME";
    logger << left << setw(5) << "ID";
    logger << left << setw(8) << "LEVEL";
    logger << left << setw(5) << "HP";
    logger << left << setw(5) << "MP";
    logger << left << setw(5) << "ATK";
    logger << left << setw(5) << "DEF";
    logger << left << setw(10) << "POS";
    logger << left << setw(5) << "SAFE";
    logger << left << setw(5) << "CON";
    logger << left << setw(5) << "CD";
    logger << left << setw(10) << "BUFF";
    logger << endl;
    // print content
    for (int i = 0; i < units.size(); ++i) {
        PUnit *unit = units[i].hero_ptr;
        // print basic hero info
        logger << left << setw(14) << unit->name;
        logger << left << setw(5) << unit->id;
        logger << left << setw(8) << unit->level;
        logger << left << setw(5) << unit->hp;
        logger << left << setw(5) << unit->mp;
        logger << left << setw(5) << unit->atk;
        logger << left << setw(5) << unit->def;
        // print pos
        string pos = int2str(unit->pos.x) + "," + int2str(unit->pos.y);
        logger << left << setw(10) << pos;
        // SAFE/CON/TACT/CD
        logger << left << setw(5) << units[i].state;
        logger << left << setw(5) << units[i].contact;
        string cd = int2str(units[i].skillCd) + " " + int2str(units[i].attackCd);
        logger << left << setw(5) << cd;
        // print buff
        vector<PBuff> buff = unit->buffs;
        for (int j = 0; j < buff.size(); ++j) {
            string buff_name = buff[j].name;
            string buff_str = buff_name + "(" + int2str(buff[j].timeLeft) + ")";
            logger << left << setw(15) << buff_str;
        }
        // over
        logger << endl;
    }

}

template<typename T>
void printString(vector<T> vct) {
    for (int i = 0; i < vct.size(); ++i) {
        logger << vct[i] << endl;
    }
}

#endif

// algorithms
Pos changePos(
        const Pos &origin,
        const Pos &reference,
        double len,
        bool away
) {
    /*
     * @par:
     * const Pos &origin - 初始位置
     * const Pos &reference - 参考位置
     * double len - 要移动的距离
     * bool away - true为原理参考点,false为接近参考点
     */
    // 正方向单位向量
    Pos unit = (reference - origin) * (1.0 / dis(origin, reference));
    // 行进的向量
    Pos move;
    if (away) {
        move = -len * unit;
    } else {
        move = len * unit;
    }
    // 目的坐标
    Pos target = origin + move;
    return target;
}

// about game
int enemyCamp() {
    if (CAMP == 0) return 1;
    else return 0;
}

// data structure related
template<typename T>
void releaseVector(vector<T *> vct) {
    for (int i = 0; i < vct.size(); ++i) {
        if (vct[i] == nullptr) continue;
        else {
//            delete vct[i];        // 不能删除指针!!
            vct[i] = nullptr;
        }
    }
    vct.clear();
}

void makePushBack(vector<string> &vct, string str) {
//#ifdef LOG
//    logger << "(... using makePushBack())" << endl;
//#endif
    /*
     * 如果没有任何单位在本回合储存过,创建一个string并储存
     * 如果有,弹出最后一个元素结尾,增长改写,再塞入
     */
    if (vct.empty())
        vct.push_back(str);
    // 判断是否有同一回合string
    string long_str = vct.back();
    string round_str = getSubstr(long_str, "round:", ",");
    string n_str = getSubstr(round_str, ":", ",");
    string n = n_str.substr(1, n_str.length() - 1);
    int r = str2int(n);
    if (r == Round) {   // assert: 同一回合的字符串
        string same_round = vct.back();     // 该回合的串
        same_round = same_round + str;      // 改写一下
        vct.pop_back();
        vct.push_back(same_round);
    } else {            // assert: 不是同一回合的字符串
        vct.push_back(str);
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
    unsigned long end = origin.find(end_s, start);
    if (start == string::npos || end == string::npos)
        return "npos";

    unsigned long len = end - start + 1;
    string substr = origin.substr(start, len);
    return substr;
}

// handling stored data
template<typename T>
void clearOldInfo(vector<T> &vct) {
    if (vct.size() > CLEAN_LIMIT) {
#ifdef LOG
        logger << "(... Clearing excess data)" << endl;
#endif
        int check = 0;
        for (auto i = vct.begin(); i != vct.end(); i++) {
            check++;
            vct.erase(i);
            if (check == CLEAN_NUMS) return;
        }
    }
}

void clearExcessData() {
    // todo 删除过时的热点
    clearOldInfo(str_money);
    clearOldInfo(str_friends);
    clearOldInfo(str_tactic);
}


// handling units
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

bool hasBuff(PUnit *unit, const char *buff) {
    vector<PBuff> buffs = unit->buffs;
    for (int i = 0; i < buffs.size(); ++i) {
        if (strcasecmp(buffs[i].name, buff) == 0)
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
        if (vct[i]->typeId == 3) {  // HammerGuard,从PUnit 519行左右可以看出
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


int storedHeroAttr(PUnit *hero, int prev_n, string attr) {
    if (prev_n >= str_friends.size())
        return -1;

    string str = str_friends[str_friends.size() - 1 - prev_n];
    // identification
    string confirm = "{type:" + int2str(hero->typeId);
    confirm = confirm + ",id:" + int2str(hero->id);
    string hero_str = getSubstr(str, confirm, "}");     // 捕捉英雄字符串
    if (hero_str == "npos")
        return -1;                                      // !!如果前一回合没有这个单位,返回-1
    string attr_str = getSubstr(hero_str, attr, ",");   // 搜寻属性串
    string n_str = getSubstr(attr_str, ":", ",");      // 读属性
    string n = n_str.substr(1, n_str.length() - 2);     // robust?
    int value = str2int(n);
    return value;
}



/************************************************************
 * Implementation: class Commander
 ************************************************************/
// constructor
Commander::Commander() {
#ifdef LOG
    logger << "... Construct Commander" << endl;
#endif
    // Round
    Round = console->round();
    // Economy
    Economy = console->gold();

    // cur_friends  vi_enemies
    getUnits();
    // heroes
    makeHeroes();
}


Commander::~Commander() {
#ifdef LOG
    logger << "... Deconstruct Commander" << endl;
#endif
    tactic.clear();
    heroes.clear();
    // PUnit*不能释放!
    cur_friends.clear();
    vi_enemies.clear();
}


void Commander::getUnits() {
#ifdef LOG
    logger << "... getUnits()" << endl;
#endif
    UnitFilter filter;
    // friends
    filter.setCampFilter(CAMP);
    filter.setAvoidFilter("MilitaryBase", "a");
    cur_friends = console->friendlyUnits(filter);
    // base
    base = console->getMilitaryBase();

    // enemies 没有cleanAll()
    filter.setCampFilter(enemyCamp());
    cur_friends = console->enemyUnits(filter);
    // en_base
    filter.cleanAll();
    filter.setCampFilter(enemyCamp());
    filter.setTypeFilter("MilitaryBase", "a");
    vector<PUnit *> fi_units = console->enemyUnits(filter);
    if (fi_units.size() != 0 && fi_units[0]->isBase()) {
        en_base = fi_units[0];
    }

    // vi_mines
    filter.cleanAll();
    filter.setTypeFilter("Mine", "a");
    vi_mines = console->enemyUnits(filter);
    // vi_monsters
    filter.cleanAll();
    filter.setTypeFilter("Dragon", "a");
    filter.setTypeFilter("Roshan", "w");
    vi_monsters = console->enemyUnits(filter);

#ifdef LOG
    for (int i = 0; i < vi_monsters.size(); ++i) {
        if (!vi_monsters[i]->isWild()) {
            logger << "!ERROR! Have non-wild units in vector vi_monsters!" << endl;
        }
    }
#endif
}


void Commander::attack() {
#ifdef LOG
    logger << "... attack()" << endl;
#endif

    Pos our_base = MILITARY_BASE_POS[CAMP];
    UnitFilter filter;
    filter.setAvoidFilter("MilitaryBase");
    filter.setAreaFilter(new Circle(our_base, MILITARY_BASE_RANGE), "a");
    vector<PUnit *> enemies = console->enemyUnits(filter);
    if (enemies.empty()) return;
    // 寻找血量最低的敌方单位
    PUnit *lowest = nullptr;
    int min_hp = -1;
    for (int i = 0; i < enemies.size(); ++i) {
        if (enemies[i]->hp < min_hp) {
            lowest = enemies[i];
            min_hp = enemies[i]->hp;
        }
    }
    // 攻击
    console->baseAttack(lowest);    // go 基地攻击
}

void Commander::buyNewHero() {    // toedit 主要策略点
#ifdef LOG
    logger << "... buyNewHero()" << endl;
#endif

    /*
     * 出新英雄顺序是:H->M->B->S->M->H->B->S
     * ??感觉出英雄顺序影响难以评估
     * ??一有机会就买英雄?
     */
    int n_heroes = (int) cur_friends.size();
    int cost = 0;
    // 先标记,后购买.循环判断,可以连续买英雄
    vector<string> to_buy;
    while (cost <= BUY_NEW_COST * Economy) {
        switch (n_heroes) {     // go 买英雄
            case 0:
                to_buy.push_back("Hammerguard");
                break;
            case 1:
                to_buy.push_back("Master");
                break;
            case 2:
                to_buy.push_back("Berserker");
                break;
            case 3:
                to_buy.push_back("Scouter");
                break;
            case 4:
                to_buy.push_back("Master");
                break;
            case 5:
                to_buy.push_back("Hammerguard");
                break;
            case 6:
                to_buy.push_back("Berserker");
                break;
            case 7:
                to_buy.push_back("Scouter");
                break;
            default:
                break;
        }
        cost += HERO_COST[n_heroes];
        n_heroes++;
    }
    to_buy.pop_back();      //  弹出多余的那个
    // 依次购买
    for (int i = 0; i < to_buy.size(); ++i) {
        console->chooseHero(to_buy[i]);
    }
}

void Commander::levelUp() {  // toedit 主要策略点
#ifdef LOG
    logger << "... levelUp()" << endl;
#endif
    /*
     * 升级策略似乎挺复杂,制定简单原则:
     * 1.从升级花费最少的英雄开始升级
     * 2.每个回合升级花费不超过LEVEL_UP_COST金钱
     * 英雄太少,免去向量排序(多次排序得不偿失),采用蛮力循环算法
     */
    int round_cost = 0;
    vector<PUnit *> toLevelUp;
    const int TEMP = (const int) cur_friends.size();
    if (TEMP == 0)
        return;

    int *flags = new int[TEMP];               // 设记号,为处理同一英雄多次升级

    while (true) {
        if (round_cost > LEVEL_UP_COST * Economy) {
            toLevelUp.pop_back();       // 弹出,防止升级了最后的单位,却不够升级第一个最小cost单位
            break;
        }
        PUnit *tempHero = nullptr;
        int tempIndex = -1;             // 为flags[]方便
        int min_cost = console->levelUpCost(HERO_LEVEL_LIMIT) + 10;
        // 找最小的cost
        for (int i = 0; i < cur_friends.size(); ++i) {
            int level = cur_friends[i]->level + flags[i];   // !!注意加上flags
            int cost = console->levelUpCost(level);
            if (cost < min_cost) {
                tempHero = cur_friends[i];
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
    delete flags;
}

// battle analysis

void Commander::StoreCmdInfo() {
    str_tactic.push_back(tactic);
}


/************************************************************
 * Implementation: Tactic
 ************************************************************/

bool Tactic::hasPath() {
    return !path.empty();
}


/************************************************************
 * Implementation: class Hero
 ************************************************************/

PUnit *Hero::nearestEnemy() {
    /*
     * 视野内最近的敌人
     */
    vector<PUnit *> views = view_enemy();
    double min_distance = MAP_SIZE * 1.0;
    int index = -1;
    for (int i = 0; i < views.size(); ++i) {
        double dist = dis(hero_ptr->pos, views[i]->pos);
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


bool Hero::hammerguardAttack() {         // toedit 主要策略点
    /*
     * 使用优先场合:
     * 1.一招毙命:伤害大于任何一个的血量剩余值
     * 否则攻击热点单位
     */
    // 预判断
    vector<PUnit *> enemies = near_enemy();
    if (enemies.size() == 0)                        // 没有敌人
        return false;
    if (!hero_ptr->canUseSkill("HammerAttack"))    // 不能使用技能(cd或mp不够)
        return false;

    // 如果有sacrifice单位,直接攻击之
    if (hot->hp == 1 && hot->typeId == 5) {
        console->useSkill("HammerAttack", hot, hero_ptr);       // go
        return true;
    }

    // 符合1,且选择血量尽量高者
    int skill_damage = HAMMERATTACK_DAMAGE[hero_ptr->level];
    int highest_hp = 0;
    int highest_index = -1;
    for (int i = 0; i < enemies.size(); ++i) {
        int temp_hp = enemies[i]->hp;
        // 1
        if (temp_hp < skill_damage) {
            if (temp_hp > highest_hp) {
                highest_index = i;
                highest_hp = temp_hp;
            }
        }
    }
    // 结算
    if (highest_index != -1) {      // 找到符合要求的unit
        console->useSkill("HammerAttack", enemies[highest_index], hero_ptr);
    } else {                        // 否则攻击热点单位
        console->useSkill("HammerAttack", hot, hero_ptr);       // go
    }
    return true;
}


bool Hero::berserkerAttack() {
    /*
     * 使用场合:(以下条件同时存在)
     * 1.没有被攻击状态
     * 2.没有可以施展技能的Hammerguard
     * 3.人数占优
     */
    // 预判断
    vector<PUnit *> enemies = near_enemy();
    vector<PUnit *> friends = near_friend();
    if (enemies.size() == 0)
        return false;
    if (!hero_ptr->canUseSkill("Sacrifice"))
        return false;

    // 1
    bool isAttacked = hasBuff(hero_ptr, "BeAttacked");
    // 2
    bool hasStrongHG = false;
    for (int i = 0; i < enemies.size(); ++i) {
        if (enemies[i]->canUseSkill("HammerAttack")) {
            hasStrongHG = true;
            break;
        }
    }
    // 结算
    if (!isAttacked && !hasStrongHG && friends.size() > enemies.size()) {
        console->useSkill("Sacrifice", hot, hero_ptr);          // go
        return true;
    } else {
        return false;
    }
}


bool Hero::masterAttack() {
    if (!hero_ptr->canUseSkill("Blink"))
        return false;

    Pos me = hero_ptr->pos;
    Pos you = hot->pos;
    int atk = hero_ptr->atk;
    Circle my_range(me, hero_ptr->range);

    if (state != 2 && hot->hp < atk) {      // 进攻型
        double move_len = dis(you, me) - hero_ptr->range;
        Pos move = changePos(me, you, move_len, false);
        console->useSkill("Blink", move, hero_ptr);        // go
        return true;
    } else if (state == 2) {        // 防守型
        double move_len = BLINK_RANGE;
        Pos move = changePos(me, you, move_len, true);
        console->useSkill("Blink", move, hero_ptr);        // go
        return true;
    } else {        // 没有必要
        return false;
    }
}


bool Hero::scouterAttack() {
    /*
     * 路过关键点插眼
     */
    if (!hero_ptr->canUseSkill("SetObserver"))
        return false;
    // 路过无眼即插眼
    Pos me = hero_ptr->pos;
    Circle *pass_by = new Circle(me, OBSERVER_VIEW);
    // 判断无眼
    UnitFilter filter;
    filter.setAreaFilter(pass_by, "a");
    filter.setTypeFilter("Observer", "a");          // fixme 是否有observer?
    if (console->friendlyUnits(filter).size() != 0)
        return false;                               // 有眼就不插了
    for (int i = 0; i < KEY_POINTS_NUM; ++i) {
        Pos key = KEY_POINTS[i];
        if (pass_by->contain(key)) {
            console->useSkill("SetObserver", key, hero_ptr);       // go
            return true;
        }
    }
    return false;
}


// protected setters
void Hero::setContact() {
    if (view_enemy().size() == 0) {
        if (hasBuff(hero_ptr, "IsMining")) {
            contact = 1;
            return;
        } else {
            contact = 0;
            return;
        }
    } else {
        contact = 1;
        return;
    }
}


void Hero::judgeState() {    // toedit 主要策略点
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
        state = 0;             // safe
        return;
    }
    // 1
    int holds = holdRounds();
    if (holds < HOLD) {             // 按上一回合分析撑不了几回合了
        state = 2;             // dying
        return;
    }
    // 2-5
    int vs_result = surviveRounds(near_friend(), near_enemy());  // 近距离队友与敌人的战况
    int hp = hero_ptr->hp;
    int max_hp = hero_ptr->max_hp;
    if (hp > ALERT * max_hp) {
        if (vs_result > 0) {        // 如果enemy强
            state = 1;         // dangerous
            return;
        } else {                    // 否则
            state = 0;         // safe
            return;
        }
    } else {
        if (vs_result > 0) {        // 如果enemy强
            state = 2;         // dying
            return;
        } else {                    // 否则
            state = 1;         // dangerous
            return;
        }
    }
}


void Hero::lockHotTarget() {
    /*
     * 锁定最弱目标,优先顺序如下:
     * 1.WinOrDie状态的单位
     * 2.WaitRevive状态的单位
     * 3.最不耐打——坚持回合数最少的目标
     */
    // fixme
    UnitFilter filter;
    filter.setAreaFilter(new Circle(hero_ptr->pos, hero_ptr->range), "a");
    vector<PUnit *> enemies = console->enemyUnits(filter);
    if (enemies.empty()) {
        hot = nullptr;
        return;
    }
    vector<PUnit *> friends = near_friend();
    vector<PUnit *> temp;
    int rounds_to_die = 1000;
    int first_die_index = -1;
    int wait_revive_index = -1;
    for (int i = 0; i < enemies.size(); ++i) {
        PUnit *enemy = enemies[i];
        if (enemy->hp == 0) continue;                   // 防止单位死亡有延迟
        // 1
        if (enemy->typeId == 5 && enemy->hp <= 1) {     // fixme 没有WinOrDie的buff?
            hot = enemy;
            return;
        }
        // 2
        if (hasBuff(enemy, "WaitRevive")) {
            hot = enemy;
            return;
        }
        // 3
        temp.clear();
        temp.push_back(enemy);
        int r = surviveRounds(near_friend(), temp);
        if (r < 0) {            // 打得过
            r = -r;             // 变r为正数
            if (r < rounds_to_die) {
                rounds_to_die = r;
                first_die_index = i;
            }
        } else {                // 一队人打不过一个,坐等其他函数叫人
            continue;
        }
    }
    // 遍历结束
    if (first_die_index != -1) {      // 遍历有收获
        hot = enemies[first_die_index];
    } else {                    // 全都打不过
        hot = nearestEnemy();        // 打最近的一个
    }
#ifdef LOG
    logger << "...... Lock hot target: id=" << hot->id << endl;
#endif
}


void Hero::cdWalk() {
    // todo
}

void Hero::stepBackwards() {    // fixme 不鲁棒策略点
#ifdef LOG
    logger << "...... stepBackwards()" << endl;
#endif
    // fixme debug
    /*
     * 有些远程英雄,需要躲避近程攻击
     */
    PUnit *nearest = nearestEnemy();
    if (!nearest)
        return;                 // 空指针则返回
    Pos hero_pos = hero_ptr->pos;
    Pos enemy_pos = nearest->pos;
    double now_dis = dis(hero_pos, enemy_pos);
    if (now_dis < hero_ptr->range - MASTER_PATIENCE) {             // 如果刚刚进入攻击范围内一点
        hot = nearest;                                       // 设为攻击热点
        Pos new_pos = hero_pos;
        double distance = now_dis;
        Pos unit_pos = (hero_pos - enemy_pos) * (1.0 / distance);   // 单位向量
        // 现有位置不停叠加单位向量,试探保持距离的最近点
        while (distance < hero_ptr->range - MASTER_PATIENCE) {
            new_pos = new_pos + unit_pos;
            distance = dis(new_pos, enemy_pos);
        }
        console->move(new_pos, hero_ptr);      // go 单位移动
    } else return;
}

void Hero::fastFlee() {
    /*
     * 不顾一切逃跑到基地
     * master考虑使用闪烁技能
     */
    // 条件判断
    if (state != 2)
        return;
    Pos base = MINE_POS[CAMP];
    // 如果是master
    if (type == 4 && hero_ptr->canUseSkill("Blink")) {
        masterAttack();                      // go
    } else {
        console->move(base, hero_ptr);     // go
    }
//    contact = 0;
}

void Hero::doTask() {
#ifdef LOG
    logger << "...... doTask()" << endl;
#endif
    if (type == 6 && hero_ptr->canUseSkill("SetObserver")) {
        scouterAttack();                     // go 移动过程中使用技能
    }

    // fixme
//    taskHelper(miner, MINE_NUM, MINE_POS);
//    taskHelper(baser, MILITARY_BASE_NUM, MILITARY_BASE_POS);
    console->move(MINE_POS[0], hero_ptr);        // go
}

// attack
void Hero::contactAttack() {                // toedit 主要策略点
#ifdef LOG
    logger << "...... contactAttack()" << endl;
#endif
    /*
     * 发现敌人->移动靠近敌人->攻击敌人,并标记为全局热点
     */

    // fixme

    if (hot == nullptr) return;

    Circle my_range(hero_ptr->pos, hero_ptr->range);
    Pos target_pos = hot->pos;
    if (my_range.contain(target_pos)) {
        console->attack(hot, hero_ptr);
    } else {
        doTask();
    }
//    if (!skillCd) {
//        switch (type) {
//            case 3:
//                hammerguardAttack();
//                break;
//            case 4:
////                masterAttack();
//                break;
//            case 5:
////                berserkerAttack();
//                break;
//            case 6:         // scouter
//                break;
//            default:
//                break;
//        }
//    } else if (!attackCd) {
//        console->attack(hot, hero_ptr);         // go
//    }
}

// public constructor
Hero::Hero(PUnit *hero) {
    setPtr(hero);
    setUnits();
}

void Hero::setPtr(PUnit *unit) {
    if (unit == nullptr) return;

    hero_ptr = unit;
    // 如果是敌人英雄,则不设置参数
    if (unit->camp != CAMP)
        return;
    // 设置当前参数
    type = hero_ptr->typeId;
    id = hero_ptr->id;
    hot = nullptr;
    // 延续上一回合决策
    if (Round > 2) {
        state = storedHeroAttr(hero_ptr, 1, "safe_e");
        target = storedHeroAttr(hero_ptr, 1, "mine");
        // dealing with cd
        attackCd = !hero_ptr->canUseSkill("Attack");
        skillCd = true;
        for (int i = 8; i < 12; ++i) {
            if (hero_ptr->canUseSkill(SKILL_NAME[i])) {    // 具体请查看常量
                skillCd = false;
                break;
            }
        }
    } else {
        state = 0;
        target = TACTICS;
        attackCd = false;
        skillCd = false;
    }

    // setAll
    setContact();
    lockHotTarget();
    judgeState();
    callBackup();
}

Hero::~Hero() {
    hero_ptr = nullptr;
    return;         // 不能删除指针!
}

// public decision maker


void Hero::go() {   // go
#ifdef LOG
    logger << "... go()" << endl;
#endif

//    if (!contact) {                     // no contact
//        doTask();
//        return;
//    } else if (state == 2) {       // dying
//        fastFlee();
//        return;
//    } else if (!skillCd || !attackCd) { // attack or use skill
//        contactAttack();
//        return;
//    } else {                            // can do nothing
//        cdWalk();
//    }
    // fixme
    if (hot == nullptr) {
        doTask();
    } else {
        console->attack(hot, hero_ptr);
    }
}


// public store data
void Hero::storeMe() {
    string my_info = "";
    // store the data as JSON style
    my_info = my_info + "{type:" + int2str(type) + ",";
    my_info = my_info + "id:" + int2str(id) + ",";
    my_info = my_info + "hp:" + int2str(hero_ptr->hp) + ",";
    my_info = my_info + "mp:" + int2str(hero_ptr->mp) + ",";
    my_info = my_info + "level:" + int2str(hero_ptr->level) + ",";
    my_info = my_info + "safe_e:" + int2str(state) + ",";
    my_info = my_info + "contact:" + int2str(contact) + ",";
    my_info = my_info + "round:" + int2str(Round) + ",";
    my_info = my_info + "posx:" + int2str(hero_ptr->pos.x) + ",";
    my_info = my_info + "posy:" + int2str(hero_ptr->pos.y) + ",";
    makePushBack(str_friends, my_info);
}




/*
 * 1.近距离协助机制
 * 2.cdWalk()
 * 3.重写contactAttack()
 * 4.分配任务采取直接指定制,(过n回合全局分析一下效果,再调整战术——暂时不写)
 *                      或者,按英雄能力阶段分配战术,能力满足要求时组队打野
 * 5.
 */