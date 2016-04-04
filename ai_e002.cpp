// 调试开关
#define LOG

#include "console.h"
#include <fstream>
#include <vector>
#include <string>
#include <cstdio>
#include <iomanip>
#include <sstream>

#ifdef LOG

#include "Pos.h"
#include "const.h"

#endif

using namespace std;

typedef pair<int, Tactic> TacticRound;

class Hero;

struct Commander;
struct Tactic;
/*######################## DATA ###########################*/
/************************************************************
 * Const values
 ************************************************************/
static const int GAME_ROUNDS = 1000;
static const int KEY_POINTS_NUM = 8;
static const Pos KEY_POINTS[] = {
        Pos(75, 146), Pos(116, 114), Pos(136, 76), Pos(117, 33),
        Pos(75, 23), Pos(36, 37), Pos(27, 76), Pos(35, 110)
};
static const int HERO_COST[] = {
        NEW_HAMMERGUARD_COST, NEW_MASTER_COST, NEW_BERSERKER_COST, NEW_SCOUTER_COST,
        NEW_MASTER_COST, NEW_HAMMERGUARD_COST, NEW_BERSERKER_COST, NEW_SCOUTER_COST
};
static const int ATK_CD[] = {
        MILITARY_BASE_ATTACK_MAXCD, 0, 0, HAMMERGUARD_ATTACK_MAXCD, MASTER_ATTACK_MAXCD,
        BERSERKER_ATTACK_MAXCD, SCOUTER_ATTACK_MAXCD
};

static vector<Tactic> TACTIC_LIB;
static void makeTacicLib();

/************************************************************
 * Policy const values
 ************************************************************/
// Commander
static const int OPENING_TACTIC = 0;        // 默认开局战术代号
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
static const double HP_ALERT = 0.1;           // 血量预警百分比
// Hero::near_u
static const double BACKUP_RANGE = 300;         // 支援范围常数
static const double ALERT_RANGE = 80;          // 警戒距离
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
int Economy;                                    // 经济

vector<int> tactic;                             // 设置战术代号

vector<PUnit *> cur_friends;                    // 当前友军英雄
const PUnit *base;                              // 我的基地
vector<PUnit *> vi_enemies;                     // 当前可见敌人英雄,不含野怪
const PUnit *en_base;                           // 敌人基地
vector<PUnit *> vi_mines;                       // 可见矿(矿的位置是默认的,但状态需要可见才能读取)
vector<PUnit *> vi_monsters;                    // 可见野怪

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

template<typename T>
void printString(vector<T> vct);      // T重载了<<
#endif

// =============== Basic Algorithms ================
// Path finding
Pos changePos(const Pos &origin, const Pos &reference, double len, bool away = true);  // 详见实现
Pos nearestKeyPointsDis(Pos pos);                        // 最近的关键点编号,包括各个矿和自定义的关键点

// String and int transfer
int str2int(string str);

string int2str(int n);

// Data structure related
template<typename T>
void releaseVector(vector<T *> vct);

template<typename T>
bool contain(vector<T> vct, const T &elem);

bool operator== (const PUnit *a, const PUnit *b);

bool operator== (const Hero &a, const Hero &b);

bool operator< (const TacticRound &a, const TacticRound &b);
// Handling stored data
template<typename T>
void clearOldInfo(vector<T> &vct);                              // 及时清理陈旧储存信息


// ============== Game and Units ===================
int enemyCamp();                                                    // 敌人的camp

// Unit
bool hasBuff(PUnit *unit, const char *buff);                        // 是否有某buff
bool justBeAttacked(PUnit *test);
vector<PUnit *> range_units(int range, int camp, vector<string> avoids);    // 范围内的单位过滤器
int surviveRounds(PUnit *host, PUnit *guest);       // 计算存活轮数,如果host强,返回guest存活,负整数;否则,返回host存活,正整数
PUnit* findID(vector<PUnit*> units, int _id);
int teamAtk(vector<PUnit *> vct);

/************************************************************
 * Global commander
 ************************************************************/
// 全局指挥官,分析形势,买活,升级,召回等,也充当本方base角色
// 更新不同战区信息,units可以领任务,然后形成team
// global_state: inferior, equal, superior
struct Commander {
    vector<Hero> heroes;
    vector<TacticRound> plans;                      // 计划执行的计划

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
    void addTactic(int i, int r);                   // 增加一个战术和对应执行轮数
    // LOADER
    void RunAnalysis();
    void HeroesAct();
    void StoreAndClean();                            // 储存

};

/************************************************************
 * Tactic
 ************************************************************/
// 一个战术包括地点/单位/行进路径
struct Tactic {
    int id;
    int type;           // 0-mine, 1-attack
    Pos target;
    vector<Pos> path;

    // constructor 默认采中间矿
    Tactic(int id, int type, const Pos &target, const vector<Pos> &path) :
            id(id), type(type), target(target), path(path) { }

    Tactic(int id = 0, int type = 0, const Pos &target = MINE_POS[0]) :
            id(id), type(type), target(target) { }

    // attribute
    bool hasPath();
};

/************************************************************
 * Heroes
 ************************************************************/
// 重新封装PUnit数据,便于数据储存
// 由于单位种类太少,技能也很少,所以暂不计划采用继承的方式
class Hero {
public:
    PUnit *punit;
    /*************************Info**************************/
    string name;
    int type, id;
    int hp, mp;
    int exp, level;
    int atk, def;
    int speed, view, range;
    Pos pos;

    int target;                                 // 战术编号
    int hot_id;                                 // 便于储存

    PUnit *hot;
    // shared units vectors
    vector<PUnit *> my_view_en;                 // 视野范围内的敌人

    /*************************Setters**************************/
    PUnit *nearestEnemy(vector<PUnit *> &ignore) const;         // 视野范围内最近的敌人
    PUnit *nearestEnemy() const;

    // setter
    Hero getStoredHero(int prev_n);             // 获得之前prev_n局的储存对象
    Pos callBackup();                           // 请求援助
    void lockHotUnit();                         // 锁定攻击目标
    void checkHotUnit();                        // 再次确认攻击目标

    /*************************Helpers***************************/
    vector<PUnit*> whoHitMe();                  // 返回攻击自己的单位
    bool timeToFlee();                          // 该

    /**************************Actions**************************/
    // 仅move
    void cdWalk();                              // cd间的躲避步伐
    void fastFlee();                            // 快速逃窜步伐
    void stepBackwards();                       // 远程单位被攻击后撤
    void justMove();                            // 前往目标

    // 仅attack
    void hammerguardAttack();                   //
    void berserkerAttack();                     //
    void masterAttack();                        //
    void scouterAttack();                       //

    // 条件判断,并调用以上move和attack接口
    void contactAttack();                       // 全力攻击,调用移动接口

    /**********************************************************/
    // constructor/destructor
    void setPtr(PUnit *unit);                   // 连接ptr
    void setUnits();                            // 设置成员变量

    Hero(PUnit *hero = nullptr);
    ~Hero();
    Hero(const Hero &hero);

    /*************************Loader***************************/
    // Commander接口
    void setTarget(int tac_n);                  // 设置战术

    // LOADER
    void Act();         // 调用一切动作接口
    void StoreMe();     // 储存该英雄信息
};


/*#################### MAIN FUNCTION #######################*/
void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {
    // Define Tactics
    const int TACTIC_APPLIED[] = {};
    const int APPLIED_ROUND[] = {};
    int TACTIC_NUM = sizeof(TACTIC_APPLIED) / sizeof(int);
    // Create pointers
    console = new Console(map, info, cmd);
    commander = new Commander();
    // Set tactics
    for (int i = 0; i < TACTIC_NUM; ++i) {
        commander->addTactic(i, APPLIED_ROUND[i]);
    }
    // Run Commander, analyze and arrange tactics
    commander->RunAnalysis();

    // Hero do actions
    commander->HeroesAct();

    // Store all
    commander->StoreAndClean();


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
        // POS
        string pos = int2str(unit->pos.x) + "," + int2str(unit->pos.y);
        logger << left << setw(10) << pos;
        // BUFF
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
    logger << left << setw(5) << "TAC";
    logger << left << setw(10) << "BUFF";
    logger << endl;
    // print content
    for (int i = 0; i < units.size(); ++i) {
        Hero unit = units[i];
        // print basic hero info
        logger << left << setw(14) << unit.name;
        logger << left << setw(5) << unit.id;
        logger << left << setw(8) << unit.level;
        logger << left << setw(5) << unit.hp;
        logger << left << setw(5) << unit.mp;
        logger << left << setw(5) << unit.atk;
        logger << left << setw(5) << unit.def;
        // POS/TAC
        logger << left << setw(10) << unit.pos;
        logger << left << setw(5) << units[i].target;
        // BUFF
        vector<PBuff> buff = unit.punit->buffs;
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
bool contain(vector<T> vct, const T &elem) {
//    return binary_search(vct.begin(), vct.end(), elem); 英雄太少没必要
    for (int i = 0; i < vct.size(); ++i) {
        if (vct[i] == elem) {
            return true;
        }
    }
    return false;
}


bool operator== (const PUnit *a, const PUnit *b) {
    if (
            a->typeId == b->typeId &&
            a->id == b->id &&
            a->exp == b->exp &&
            a->hp == b->hp &&
            a->mp == b->mp
            )                       // fixme 没有round的笨方法
        return true;
    else
        return false;
}


bool operator< (const TacticRound &a, const TacticRound &b) {
    return a.first < b.first;
}


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


int surviveRounds(PUnit *host, PUnit *guest) {
    /*
     * 计算公式:
     * 技能杀伤 = 先忽略技能预估战斗轮数,然后计算保守值
     * 生命恢复 = 本轮全体生命恢复
     * 死亡回合 = (hp + 生命恢复 - 技能杀伤) / (对方攻击 - 我方防守)
     */
    int host_hp_rcv = console->unitArg("hp", "rate", host);
    int guest_hp_rcv = console->unitArg("hp", "rate", guest);
    int host_r = (host->hp + host_hp_rcv) / max(host->atk - guest->def, 5);
    int guest_r = (guest->hp + guest_hp_rcv) / max(guest->atk - host->def, 5);
    return (host_r > guest_r) ? -guest_r : host_r;
}


PUnit* findID(vector<PUnit*> units, int _id) {
    for (int i = 0; i < units.size(); ++i) {
        if (units[i]->id == _id) {
            return units[i];
        }
    }
    return nullptr;
}

bool justBeAttacked(PUnit *test) {
    if (hasBuff(test, "BeAttacked") &&
            test->findBuff("BeAttcked")->timeLeft == HURT_LAST_TIME)
        return true;
    else
        return false;
}


int teamAtk(vector<PUnit *> vct) {
    int round_atk = 0;
    for (int i = 0; i < vct.size(); ++i) {
        round_atk += vct[i]->atk;
    }
    return round_atk;
}



/************************************************************
 * Implementation: class Commander
 ************************************************************/

/**********************Constructors***********************/

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

/***********************Help Constructors************************/

void Commander::getUnits() {
#ifdef LOG
    logger << "... getUnits()" << endl;
#endif
    UnitFilter filter;
    // friends
    filter.setCampFilter(CAMP);
    filter.setAvoidFilter("MilitaryBase", "a");
    filter.setAvoidFilter("Observer", "w");
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

/*************************Tactics**************************/




/*************************Base actions**************************/

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


/*************************Interface**************************/


void Commander::HeroesAct() {
    for (int i = 0; i < heroes.size(); ++i) {
        heroes[i].Act();
    }
}

void Commander::addTactic(int i, int r) {
    if (i < 0 || i >= TACTIC_LIB.size() || r < 0 || r > GAME_ROUNDS)
        return;

    Tactic t = TACTIC_LIB[i];
    plans.push_back(make_pair(r, t));
}


void Commander::StoreAndClean() {
    // fixme need reviewing
    for (int i = 0; i < heroes.size(); ++i) {
        heroes[i].StoreMe();
    }
    str_money.push_back(Economy);
    clearOldInfo(str_money);
    clearOldInfo(str_heroes);
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

/*************************Setters**************************/

PUnit *Hero::nearestEnemy(vector<PUnit *> &ignore) const {
    /*
     * 除矿以外一切可攻击单位,可以在ignore中指定被忽略的对象
     */
    // 可攻击对象不存在
    if (my_view_en.size() == 0)
        return nullptr;

    double min_dist = MAP_SIZE * 1.0;
    PUnit *selected = nullptr;

    for (int i = 0; i < my_view_en.size(); ++i) {
        PUnit *it = my_view_en[i];
        double dist = dis(it->pos, pos);
        if (dist < min_dist && !contain(ignore, it)) {
            selected = it;
            min_dist = dist;
        }
    }
    return selected;
}


PUnit *Hero::nearestEnemy() const {
    vector<PUnit *> empty;
    empty.clear();
    return nearestEnemy(empty);
}


Hero Hero::getStoredHero(int prev_n) {
    if (prev_n >= str_heroes.size())
        return NULL;

    vector<Hero> round = str_heroes[str_heroes.size() - 1 - prev_n];
    Hero same = NULL;
    for (int i = 0; i < round.size(); ++i) {
        Hero temp = round[i];
        if (temp.type == type && temp.id == id) {
            same = temp;
        }
    }
    return same;        // 可能是NULL,表示刚出现的英雄
}


// todo commander接收call backup并观察当前单位是否敌人,使用setTarget设置战术目标
Pos Hero::callBackup() {
    return pos;
}


void Hero::lockHotUnit() {      // toedit 主要策略点
    /*
     * @优先级:
     * WinOrDie
     * WaitRevive
     * 继承上一轮
     * 最弱单位
     * @小心:
     * 上一回合的热点单位死亡或召回
     * 被卡住了无法攻击
     * 追击了一段时间即停止
     */
    // 特殊buff
    for (int i = 0; i < my_view_en.size(); ++i) {
        PUnit *en = my_view_en[i];
        // WinOrDie
        if (hasBuff(en, "WinOrDie")) {
            hot = en;
            return;
        }
        // WaitRevive
        if (hasBuff(en, "WaitRevive")) {
            hot = en;
            return;
        }
    }

    // 继承
    Hero last = getStoredHero(1);
    if (last != NULL) {
        PUnit *same_enemy = findID(my_view_en, last.hot_id);
        if (same_enemy != nullptr && same_enemy->hp > 0) {      // 避免不存在/死亡/召回
            hot = same_enemy;
            return;
        }
    }

    // 没有继承
    int min_sr = 1000;      // 最少存活轮数
    int max_sr = 0;         // 最多存活轮数
    int index = -1;
    bool has_beater = false;// 有人可打
    for (int j = 0; j < my_view_en.size(); ++j) {
        PUnit* enemy = my_view_en[j];
        int sr_r = surviveRounds(punit, enemy);
        if (sr_r < 0) {     // host强
            sr_r = -sr_r;
            if (sr_r < min_sr) {
                min_sr = sr_r;
                index = j;
                has_beater = true;
            }
        } else {            // guest强
            if (!has_beater && sr_r > max_sr) {
                max_sr = sr_r;
                index = j;
            }
        }
    }
    hot = my_view_en[index];
}


void Hero::checkHotUnit() {
    int hot_last_hit = console->unitArg("lastHit", int2str(id), hot);
    if (hot_last_hit == -1 || Round - hot_last_hit > ATK_CD[type]) {
        // 根本没有打到或者近几回合都没有打到,可能出于某种原因,比如卡位或者逃跑追不上
        vector<PUnit*> to_ignore;
        to_ignore.clear();
        to_ignore.push_back(hot);
        hot = nearestEnemy(to_ignore);      // 有可能为空! toedit 更详细的决策:重载带忽略参数的lockHotUnit(vector...)
    } else
        return;
}


/**************************Helpers**************************/

vector<PUnit *> Hero::whoHitMe() {

}


bool Hero::timeToFlee() {
    if (hp < HP_ALERT * punit->max_hp) {
        return true;
    } else {
        vector<PUnit *> hitters = whoHitMe();
        if (teamAtk(hitters) > hp)
            return true;
    }

    return false;
}


/**************************Actions**************************/

void Hero::cdWalk() {
    Pos hot_p = hot->pos;               // positon of hot target
    // 撤离的距离为保持两者间距一个speed
    Pos far_p = changePos(pos, hot_p, speed - dis(hot_p, pos), true);
    console->move(far_p, punit);        // go
}


void Hero::fastFlee() {
    // 忽略Sacrifice技能
    if (hasBuff(punit, "WinOrDie"))
        return;

    Pos ref = nearestEnemy()->pos;
    // 撤离距离为尽量远离任何最近的单位
    if (type == 4 && punit->canUseSkill("Blink")) {     // master的闪烁
        Pos far_p = changePos(pos, ref, BLINK_RANGE, true);
        console->useSkill("Blink", far_p, punit);       // go
    } else {
        Pos far_p = changePos(pos, ref, speed, true);
        console->move(far_p, punit);                    // go
    }
}


void Hero::stepBackwards() {
    if (justBeAttacked(punit)) {
        vector<PUnit *> hitters = whoHitMe();
        Pos sum(0, 0);
        for (int i = 0; i < hitters.size(); ++i) {
            Pos p = hitters[i]->pos;
            sum = sum + changePos(pos, p, range - dis(p, pos), true);
        }
        // 计算平均值
        Pos target = sum * (1.0 / (double) hitters.size());
        console->move(target, punit);       // go
#ifdef  LOG
        logger << "**[MSG] stepBackwards():" << endl;
        logger << "**just hit by " << endl;
        for (int j = 0; j < hitters.size(); ++j) {
            logger << hitters[j]->pos << " ";
        }
        logger << endl;
        logger << "**move to ";
        logger << target << endl;
#endif
    }
}


void Hero::justMove() {
    if (type == 6 && punit->canUseSkill("SetObserver")) {
        // 如果离关键点比较近,那么插眼
        Pos nearest_key = nearestKeyPointsDis(pos);
        if (dis(nearest_key, pos) < SET_OBSERVER_RANGE) {
            console->useSkill("SetObserver", nearest_key, punit);   // go
            return;
        }
    }

    console->move(TACTIC_LIB[target].path, punit);      // go
}


void Hero::hammerguardAttack() {
    // cd中
    if (!punit->canUseSkill("HammerAttack") && !punit->canUseSkill("Attack")) {
        cdWalk();
        return;
    }

    // 攻击
    if (punit->canUseSkill("HammerAttack")) {
        console->useSkill("HammerAttack", hot, punit);  // go
    } else {
        console->attack(hot, punit);                    // go
    }
}


void Hero::berserkerAttack() {
    /*
     * 使用Sacrifice的条件:
     * 1.没有AttackCd
     * 2.遍历所有敌人,应该不存在:其射程既能包含我,又没有AttackCd
     */
    // cd中
    if (!punit->canUseSkill("Attack")) {
        cdWalk();
        return;
    }

    bool safe_env = true;
    for (int i = 0; i < my_view_en.size(); ++i) {
        PUnit *en = my_view_en[i];
        double dist = dis(pos, en->pos);
        if (en->range >= dist) {
            safe_env = false;
            break;
        }
    }
    // 结算
    if (punit->canUseSkill("Attack") && safe_env) {
        console->useSkill("Sacrifice", hot, punit);     // go
    } else {
        console->attack(hot, punit);                    // go
    }
}


void Hero::masterAttack() {
    /*
     * blink追击的条件: (master普通攻击没有cd)
     * 1.与该单位的距离(range, range + blink_range]
     * 2.该单位一击便歹
     */
    double dist = dis(hot->pos, pos);
    if (dist > range && dist <= range + BLINK_RANGE && hot->hp < atk) {
        Pos chase_p = changePos(pos, hot->pos, dist - range / 2, false);
        console->useSkill("Blink", chase_p, punit);     // go
    } else {
        console->attack(hot, punit);                    // go
    }
}


void Hero::scouterAttack() {
    // 没有cd
    console->attack(hot, punit);        // go
}


void Hero::contactAttack() {
    // 要逃走
    if (timeToFlee()) {
        fastFlee();
        return;
    }

    // 无攻击目标
    if (hot == nullptr) {
        justMove();
        return;
    }

    // attack
    switch (type) {
        case 3:
            hammerguardAttack();
            break;
        case 4:
            masterAttack();
            break;
        case 5:
            berserkerAttack();
            break;
        case 6:
            scouterAttack();
            break;
        default:
            break;
    }
}


/***********************************************************/

void Hero::setPtr(PUnit *unit) {
    if (unit == nullptr) {
        name = "";
        type = -1;
        id = -1;
        hp = 0;
        mp = 0;
        exp = 0;
        level = 0;
        atk = 0;
        def = 0;
        speed = 0;
        view = 0;
        range = 0;
        contact = true;
        hot = nullptr;
        target = NULL;
        hot_id = -1;
        return;
    }

    punit = unit;
    name = unit->name;
    type = unit->typeId;
    id = unit->id;
    hp = unit->hp;
    mp = unit->mp;
    exp = unit->exp;
    level = unit->level;
    atk = unit->atk;
    def = unit->def;
    speed = unit->speed;
    view = unit->speed;
    pos = unit->pos;

    // 读取记录
    Hero last = getStoredHero(1);
    if (last == NULL) {
        contact = 1;
        target = NULL;
        hot_id = -1;
    } else {
        contact = last.contact;
        target = last.target;
        hot_id = last.hot_id;
    }
}


void Hero::setUnits() {
    UnitFilter filter;
    filter.setAreaFilter(new Circle(punit->pos, punit->view), "a");
    filter.setCampFilter(enemyCamp());
    filter.setAvoidFilter("Observer", "a");
    my_view_en = console->enemyUnits(filter);
}


Hero::Hero(PUnit *hero) {
    // 顺序不能颠倒
    setPtr(hero);
    setUnits();
    lockHotUnit();
    checkHotUnit();
}


Hero::Hero(const Hero &hero) {
    punit = nullptr;
    name = hero.name;
    type = hero.type;
    hp = hero.hp;
    mp = hero.mp;
    exp = hero.exp;
    level = hero.level;
    atk = hero.atk;
    def = hero.def;
    speed = hero.speed;
    view = hero.view;
    range = hero.range;
    pos.x = hero.pos.x;
    pos.y = hero.pos.y;
    contact = hero.contact;
    target = hero.target;
    hot = nullptr;
    my_view_en.clear();
}


Hero::~Hero() {
    my_view_en.clear();
    punit = nullptr;
    hot = nullptr;
}


/*************************Loader***************************/

void Hero::setTarget(int tac_n) {
    target = tac_n;
}


void Hero::Act() {
    // todo
    if (hot == nullptr || contact == 0) {
        justMove();
    } else if (contact == 1) {
        contactAttack();
    }
}




/*
 * 1.近距离协助机制
// * 2.cdWalk()
// * 3.重写contactAttack()
 * 4.分配任务采取直接指定制,(过n回合全局分析一下效果,再调整战术——暂时不写)
 *                      或者,按英雄能力阶段分配战术,能力满足要求时组队打野
 * 5.
 */