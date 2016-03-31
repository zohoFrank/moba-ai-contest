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

typedef pair<int, Tactic> TRecord;
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
static const int CLEAN_LIMIT = 6;           // 最多保留回合记录
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
static vector<vector<Hero>> str_heroes;         // 储存的英雄



/*################# Assistant functions ####################*/
// ================ Log Related ====================
#ifdef LOG
static ofstream logger("log_info.txt");
void printUnit(vector<PUnit *> units);
void printHeroList(vector<Hero> units);
template<typename T> void printString(vector<T> vct);      // T重载了<<
#endif

// =============== Basic Algorithms ================
// Path finding
Pos changePos(const Pos &origin, const Pos &reference, double len, bool away = true);  // 详见实现

// String and int transfer
int str2int(string str);
string int2str(int n);

// Data structure related
template<typename T> void releaseVector(vector<T *> vct);

// Handling stored data
template<typename T> void clearOldInfo(vector<T> &vct);                              // 及时清理陈旧储存信息


// ============== Game and Units ===================
int enemyCamp();                                                    // 敌人的camp

// Unit
bool hasBuff(PUnit *unit, const char *buff);                        // 是否有某buff
vector<PUnit *> range_units(int range, int camp, vector<string> avoids);    // 范围内的单位过滤器

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

    void Store();                            // 储存

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
    PUnit hero_ptr;

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
    void judgeContact();                        // 判断是否交战
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
    commander->Store();

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
        PUnit *unit = &units[i].hero_ptr;
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


// handling units

bool hasBuff(PUnit *unit, const char *buff) {
    vector<PBuff> buffs = unit->buffs;
    for (int i = 0; i < buffs.size(); ++i) {
        if (strcasecmp(buffs[i].name, buff) == 0)
            return true;
    }
    return false;
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
    base = nullptr;
    releaseVector(cur_friends);
    releaseVector(vi_enemies);
    en_base = nullptr;
    releaseVector(vi_mines);
    releaseVector(vi_monsters);
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



/************************************************************
 * Implementation: Tactic
 ************************************************************/

bool Tactic::hasPath() {
    return !path.empty();
}


/************************************************************
 * Implementation: class Hero
 ************************************************************/



/*
 * 1.近距离协助机制
 * 2.cdWalk()
 * 3.重写contactAttack()
 * 4.分配任务采取直接指定制,(过n回合全局分析一下效果,再调整战术——暂时不写)
 *                      或者,按英雄能力阶段分配战术,能力满足要求时组队打野
 * 5.
 */