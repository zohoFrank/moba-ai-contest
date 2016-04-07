// 调试开关
#define LOG

#ifdef LOG

#include <ctime>
#include <fstream>
#include <iomanip>

#endif

#include "console.h"
#include <sstream>
#include <map>
#include <set>

using namespace std;

typedef Pos Tactic;
typedef pair<int, Tactic> TacticRound;

class Hero;

struct Commander;
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
};
static const int ATK_CD[] = {
        MILITARY_BASE_ATTACK_MAXCD, 0, 0, HAMMERGUARD_ATTACK_MAXCD, MASTER_ATTACK_MAXCD,
        BERSERKER_ATTACK_MAXCD, SCOUTER_ATTACK_MAXCD
};
static const char *HERO_NAME[] = {"Hammerguard", "Master", "Berserker", "Scouter"};


/************************************************************
 * Policy const values
 ************************************************************/
static const int TACTIC_APPLIED[] = {6};
static const int APPLIED_ROUND[] = {0};

// Commander
// Commander::levelUp
static const double LEVEL_UP_COST = 0.6;    // 升级金钱比例
// Commander::buyNewHero
static const double BUY_NEW_COST = 1.0;     // 买新英雄花费
static int BUY_RANK = 43124132;             // 请参考hero_name

// Commander::callBack()
static const int BACK_BASE = 2;             // 面对多少敌人,基地召回我方英雄

// clearOldInfo()
static const int CLEAN_LIMIT = 6;           // 最多保留回合记录
static const int CLEAN_NUMS = 2;            // 超过最多保留记录后,一次清理数据组数

// Hero
// Hero::judgeState()
static const double HP_ALERT = 0.2;         // 血量预警百分比
// Hero::near_u
static const int HOLD_RANGE = 144;        // 支援范围常数

// unchanged values (during the entire game)
static int CAMP = -1;                       // which camp

static map<int, Tactic> TacLib;

/************************************************************
 * Real-time sharing values
 ************************************************************/
static Console *console = nullptr;
static Commander *commander = nullptr;
static int Round = -1;
int Economy;                                    // 经济

static set<int> estEnemies;                     // 估计的敌人id

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
static vector<int> str_money;                   // 储存的金钱
static vector<vector<Hero>> str_heroes;         // 储存的英雄
static vector<TacticRound> GamePlans;           // 计划执行的战术


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
Pos parallelChangePos(const Pos &origin, const Pos &reference, int len2, bool away = true);  // 详见实现
Pos verticalChangePos(const Pos &origin, const Pos &reference, int len2, bool clockwize = true);  //详见实现

// String and int transfer
int str2int(string str);

string int2str(int n);

// Data structure related
template<typename T>
void releaseVector(vector<T *> vct);

template<typename T>
bool contain(vector<T *> vct, T *elem);

bool compareLevel(PUnit *a, PUnit *b);

bool operator<(const Pos &a, const Pos &b);

bool operator<(const TacticRound &a, const TacticRound &b);

// Handling stored data
template<typename T>
void clearOldInfo(vector<T> &vct);                  // 及时清理陈旧储存信息


// ============== Game and Units ===================
void makeTactic();                                  // 在第0轮调用一下
int enemyCamp();                                    // 敌人的camp

// Unit
int buyNewCost(int cost_indx);                      // 当前购买新英雄成本,参数为HERO_NAME的索引
bool hasBuff(PUnit *unit, const char *buff);        // 是否有某buff
bool justBeAttacked(PUnit *test);

int surviveRounds(PUnit *host, PUnit *guest);       // 计算存活轮数,如果host强,返回guest存活,负整数;否则,返回host存活,正整数
PUnit *findID(vector<PUnit *> units, int _id);

int teamAtk(vector<PUnit *> vct);

/************************************************************
 * Global commander
 ************************************************************/
// 全局指挥官,分析形势,买活,升级,召回等,也充当本方base角色
// 更新不同战区信息,units可以领任务,然后形成team
// global_state: inferior, equal, superior
struct Commander {
    vector<Hero> heroes;
    vector<Tactic> backup;

    /**********************************************************/
    // constructor
    Commander();

    ~Commander();

    // HELPERS
    void makeHeroes();                              // 制造一个全体我方英雄对象的向量
    void getUnits();                                // 获取单位信息
    void estimateEnemies();                         // 估计敌人人数

    // tactics
    void analyzeSituation();                        // 分析形势,设置战术
    void tacticArrange();                           // 向向量中的英雄分配战术
    // base actions
    void attack();                                  // 基地攻击
    void buyNewHero();                              // 买英雄
    void buyLife();                                 // 买活英雄
    void levelUp();                                 // 升级英雄
    void spendMoney();                              // 买英雄/买等级/买活的选择
    void callBack();                                // 召回英雄

    /**********************************************************/
    // LOADER
    void RunAnalysis();

    void TeamAct();                                 // 基地和英雄动作
    void StoreAndClean();                           // 储存

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

    int round;                                  // 便于区分
    Tactic target;                              // 战术
    int hot_id;                                 // 便于储存

    PUnit *hot;
    // shared units vectors
    vector<PUnit *> my_view_en;                 // 视野范围内的敌人

    /*************************Setters**************************/
    PUnit *nearestEnemy() const;

    // setter
    Hero *getStoredHero(int prev_n);            // 获得之前prev_n局的储存对象
    void lockHotUnit();                         // 锁定攻击目标

    /*************************Helpers***************************/
    bool timeToFlee();                          // 是否应该逃窜
    bool stuck();                               // 由于未知原因卡住了

    /**************************Actions**************************/
    // 仅move
    void cdWalk();                              // cd间的躲避步伐
    void fastFlee();                            // 快速逃窜步伐
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

    /*************************Loader***************************/
    // Commander接口
    void setTarget(Tactic t);                   // 设置战术

    // LOADER
    void HeroAct();         // 调用一切动作接口
    void StoreMe();     // 储存该英雄信息

#ifdef LOG
    void printAtkInfo() const;
#endif
};


/*#################### MAIN FUNCTION #######################*/
void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {
#ifdef LOG
    logger << "====ROUND " << Round << " STARTS====" << endl;
    logger << "@Economy: " << Economy << endl;
    long start = clock();
#endif
    makeTactic();

    // Create pointers
    console = new Console(map, info, cmd);
    commander = new Commander();

    // Run Commander, analyze and arrange tactics
    commander->RunAnalysis();

    // Hero do actions
    commander->TeamAct();

    // Store all
    commander->StoreAndClean();

    delete commander;
    delete console;

#ifdef LOG
    logger << endl;
    long end = clock();
    double interval = 1.0 * (end - start) / CLOCKS_PER_SEC * 1000;
    logger << "$$ Time Consumed(ms): " << interval << endl;
    logger << endl << endl;
#endif
}



/*################### IMPLEMENTATION #######################*/
/************************************************************
 * Implementation: Assistant function
 ************************************************************/

#ifdef LOG

// log related
void printUnit(vector<PUnit *> units) {
    if (units.empty()) {
        logger << "!! get empty lists" << endl;
        return;
    };
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
    logger << "@Hero list" << endl;
    if (units.empty()) {
        logger << "!! get empty Hero list" << endl;
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
    logger << left << setw(10) << "TAC";
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
        string p = int2str(unit.pos.x) + "," + int2str(unit.pos.y);
        logger << left << setw(10) << p;
        string t = int2str(unit.target.x) + "," + int2str(unit.target.y);
        logger << left << setw(10) << t;
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
Pos parallelChangePos(
        const Pos &origin,
        const Pos &reference,
        int len2,
        bool away
) {
    /*
     * @par:
     * const Pos &origin - 初始位置
     * const Pos &reference - 参考位置
     * double len - 要移动的距离
     * bool away - true为原理参考点,false为接近参考点
     */
    double len = sqrt(len2 * 1.0);
    double dist = dis(origin, reference);
    // 正方向单位向量
    double unit_x = 1.0 * (reference.x - origin.x) / dist;
    double unit_y = 1.0 * (reference.y - origin.y) / dist;
    // 行进的向量
    double len_x = unit_x * len;
    double len_y = unit_y * len;
    Pos target;
    if (away) {
        target.x = (int) (origin.x - len_x);
        target.y = (int) (origin.y - len_y);
    } else {
        target.x = (int) (origin.x + len_x);
        target.y = (int) (origin.y + len_y);
    }
    // 目的坐标
#ifdef LOG
    logger << "#parallelChangePos() : " << endl;
    logger << origin << reference << len << target << endl;
#endif
    return target;
}


Pos verticalChangePos(
        const Pos &origin,
        const Pos &reference,
        int len2,
        bool clockwize
) {
    double len = sqrt(len2 * 1.0);
    Pos ref = reference - origin;
    // 求单位法线向量,先假设是顺时针
    Pos vref(ref.y, -ref.x);        // 与ref垂直成90°
    double len_vref = dis2(vref, Pos(0, 0));
    // vref方向的单位向量
    double vx = 1.0 * vref.x / len_vref;
    double vy = 1.0 * vref.y / len_vref;

    // 移动
    Pos far_p;
    far_p.x = (int) (len * vx + origin.x);
    far_p.y = (int) (len * vy + origin.y);

#ifdef LOG
    logger << "#verticalChangePos() : " << endl;
    logger << origin << reference << len << far_p << endl;
#endif
    return far_p;
}


// about game
int enemyCamp() {
    if (CAMP == 0) return 1;
    else return 0;
}


// data structure related
template<typename T>
bool contain(vector<T *> vct, T *elem) {
    // 只能针对PUnit *使用
    for (int i = 0; i < vct.size(); ++i) {
        if (vct[i] == elem) {
            return true;
        }
    }
    return false;
}


bool compareLevel(PUnit *a, PUnit *b) {
    return (a->level < b->level);
}


bool operator<(const Pos &a, const Pos &b) {
    if (a.x != b.x) {
        return a.x < b.x;
    } else {
        return a.y < b.y;
    }
}


bool operator<(const TacticRound &a, const TacticRound &b) {
    return a.first > b.first;       // 从大到小排序,有利于当作栈调用
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
int buyNewCost(int cost_idx) {
    int type = cost_idx + 3;
    int occurs = 1;
    for (int i = 0; i < cur_friends.size(); ++i) {
        if (cur_friends[i]->typeId == type) {
            occurs++;
        }
    }
    return HERO_COST[cost_idx] * occurs;
}


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


PUnit *findID(vector<PUnit *> units, int _id) {
    if (_id == -1) return nullptr;

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


void makeTactic() {
    /*
     * 0-9 矿区
     * 10-11 基地
     */
    if (Round > 0) return;

    // Set Default Tactics
    // 0-9,预留给矿区
    for (int i = 0; i < MINE_NUM; ++i) {
        TacLib[i] = MINE_POS[i];
    }
    // 10-11,预留给基地
    for (int j = 0; j < MILITARY_BASE_NUM; ++j) {
        TacLib[10 + j] = MILITARY_BASE_POS[j];
    }

    // Add Plans
    int TACTIC_NUM = sizeof(TACTIC_APPLIED) / sizeof(int);
    for (int i = 0; i < TACTIC_NUM; ++i) {
        int r = APPLIED_ROUND[i];
        int t_idx = TACTIC_APPLIED[i];

        if (t_idx < 0 || t_idx >= TacLib.size() || r < 0 || r > GAME_ROUNDS)
            return;

        Tactic t = TacLib[t_idx];
        GamePlans.push_back(make_pair(r, t));
    }

    // sorting
    sort(GamePlans.begin(), GamePlans.end());

#ifdef LOG
    logger << "@Game Plans" << endl;
    logger << left << setw(5) << "RND";
    logger << left << setw(10) << "POS" << endl;
    for (int k = 0; k < GamePlans.size(); ++k) {
        logger << left << setw(5) << GamePlans[k].first;
        string p = int2str(GamePlans[k].second.x) + "," + int2str(GamePlans[k].second.y);
        logger << left << setw(10) << p << endl;
    }
#endif
}


/************************************************************
 * Implementation: class Commander
 ************************************************************/

/**********************Constructors***********************/

Commander::Commander() {
    // CAMP
    CAMP = console->camp();
    // Round
    Round = console->round();
    // Economy
    Economy = console->gold();

    // cur_friends  vi_enemies
    getUnits();
    // heroes
    makeHeroes();
    // estimate enemies
    estimateEnemies();
}


Commander::~Commander() {
    backup.clear();
    heroes.clear();
    // PUnit*不能释放!
    releaseVector(cur_friends);
    base = nullptr;
    releaseVector(vi_enemies);
    en_base = nullptr;
    releaseVector(vi_mines);
    releaseVector(vi_monsters);
    clearOldInfo(str_money);
//    clearOldInfo(str_heroes);
}

/**************************Helpers**************************/

void Commander::getUnits() {
    // friends
    UnitFilter f1;
    f1.setCampFilter(CAMP);
    f1.setAvoidFilter("MilitaryBase", "a");
    f1.setAvoidFilter("Observer", "w");
    cur_friends = console->friendlyUnits(f1);
    // base
    base = console->getMilitaryBase();

    // enemies 没有cleanAll()
    UnitFilter f2;
    f2.setCampFilter(enemyCamp());
    f2.setAvoidFilter("MilitaryBase", "a");
    f2.setAvoidFilter("Observer", "w");
    vi_enemies = console->enemyUnits(f2);
    // en_base
    UnitFilter f3;
    f3.setCampFilter(enemyCamp());
    f3.setTypeFilter("MilitaryBase", "a");
    vector<PUnit *> fi_units = console->enemyUnits(f3);
    if (fi_units.size() != 0 && fi_units[0]->isBase()) {
        en_base = fi_units[0];
    }

    // vi_mines
    UnitFilter f4;
    f4.setTypeFilter("Mine", "a");
    vi_mines = console->enemyUnits(f4);
    // vi_monsters
    UnitFilter f5;
    f5.cleanAll();
    f5.setTypeFilter("Dragon", "a");
    f5.setTypeFilter("Roshan", "w");
    vi_monsters = console->enemyUnits(f5);

#ifdef LOG
    // 检查筛选是否正确
    for (int i = 0; i < vi_monsters.size(); ++i) {
        if (!vi_monsters[i]->isWild()) {
            logger << "!ERROR! Have non-wild units in vector vi_monsters!" << endl;
        }
    }
    // print
    logger << "@Friends" << endl;
    printUnit(cur_friends);
    logger << endl;
    logger << "@Visible enemies" << endl;
    printUnit(vi_enemies);
    logger << endl;
    logger << "@Visible monsters" << endl;
    printUnit(vi_monsters);
    logger << endl;
#endif
}


void Commander::makeHeroes() {
    for (int i = 0; i < cur_friends.size(); ++i) {
        Hero temp(cur_friends[i]);
        heroes.push_back(temp);
    }
}


void Commander::estimateEnemies() {
    for (int i = 0; i < vi_enemies.size(); ++i) {
        estEnemies.insert(vi_enemies[i]->id);
    }
}


/*************************Tactics**************************/

void Commander::analyzeSituation() {        // todo
    /*
     * 主要责任:
     * 分析局势,并修改或增加战术池中的战术
     * *暂时先完成近距离协作机制
     */

}


void Commander::tacticArrange() {
    /*
     * 主要责任:
     * 并不理会战术是否合理,直接根据战术池分配任务
     * 同一任务只分配一次,剩余的由Hero在构造时,自行完成对上一轮数据的继承
     */
    // 选出该执行的战术,GamePlans已排序,越靠后Round越小
    vector<TacticRound> store;
    while (true) {
        if (GamePlans.empty()) break;

        TacticRound tr = GamePlans.back();
        if (tr.first == Round) {
            store.push_back(tr);
            GamePlans.pop_back();
        } else {
            while (GamePlans.back().first < Round)
                GamePlans.pop_back();       // 防止万一,除去多余plans
            break;
        }
    }

    // 近距离支援
    for (int j = 0; j < heroes.size(); ++j) {
        if (heroes[j].hot != nullptr) return;           // 有打击目标则不支援
        Hero h = heroes[j];
        for (int i = 0; i < backup.size(); ++i) {
            Pos bk = backup[i];
            if (dis2(h.pos, bk) <= HOLD_RANGE) {
                h.setTarget(bk);
            }
        }
    }

    // 战术安排
    if (store.empty()) return;              // 如果没有要执行的战术,不改变

    int n = 0;
    // 所有人全部重新分配
    for (int i = 0; i < heroes.size(); ++i) {       // 循环安排
        heroes[i].setTarget(store[n].second);
#ifdef LOG
        logger << "$" << heroes[i].id << " is ordered to do task " << n << endl;
#endif
        n = (int) (++n % store.size());
    }
}


/*************************Base actions**************************/

void Commander::attack() {
    Pos our_base = MILITARY_BASE_POS[CAMP];
    UnitFilter filter;
    filter.setAvoidFilter("MilitaryBase", "a");
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
    int new_i = BUY_RANK % 10 - 1;
    int cost = buyNewCost(new_i);

    if (new_i >= 0 && new_i < 4 && buyNewCost(new_i) < Economy) {
        console->chooseHero(HERO_NAME[new_i]);
        BUY_RANK /= 10;
    }
}


void Commander::levelUp() {  // toedit 主要策略点
    /*
     * 制定简单原则:
     * 1. 每回合只升级每个英雄最多一次
     * 2. 从升级花费最少的英雄开始
     */
    // 只有靠近的单位才能升级
    UnitFilter filter;
    filter.setAreaFilter(new Circle(base->pos, LEVELUP_RANGE), "a");
    filter.setLevelFilter(0, 6);
    vector<PUnit *> nearHeroes = console->friendlyUnits(filter);

    if (nearHeroes.size() == 0) {
        return;
    }

    // 按等级从小到大排序
    sort(nearHeroes.begin(), nearHeroes.end(), compareLevel);
    vector<PUnit *> toLevelUp;
    int round_cost = 0;

    // 贪心
    for (int i = 0; i < nearHeroes.size(); ++i) {
        if (round_cost >= LEVEL_UP_COST * Economy) {
            if (!toLevelUp.empty()) {
                toLevelUp.pop_back();           // 弹出,使花费严格小于标准
            }
            break;
        }
        round_cost += console->levelUpCost(nearHeroes[i]->level);
        toLevelUp.push_back(nearHeroes[i]);
    }

    // 结算
    for (int j = 0; j < toLevelUp.size(); ++j) {
        console->buyHeroLevel(toLevelUp[j]);    // go
    }
}


void Commander::buyLife() {
    // todo 暂时没有发现买活的意义
    return;
}


void Commander::spendMoney() {
    // todo 还没有加买活,策略不佳
    if (cur_friends.size() < 4 || estEnemies.size() >= cur_friends.size()) {
        buyNewHero();
    } else {
        levelUp();
    }
}


void Commander::callBack() {
    // todo 设计成强制设置Hero的Tactic值,暂时认为快速召回没有太大意义
    // 条件判断
    UnitFilter filter;
    filter.setAreaFilter(new Circle(MILITARY_BASE_POS[CAMP], MILITARY_BASE_VIEW), "a");
    filter.setAvoidFilter("Observer", "a");
    filter.setCampFilter(enemyCamp());
    int base_en = (int) console->enemyUnits(filter).size();
    if (base_en >= BACK_BASE)
        // 进行结算,召回响应人数的己方英雄
        for (int i = 0; i < base_en; ++i) {
            heroes[i].setTarget(MILITARY_BASE_POS[CAMP]);
        }
}


/*************************Interface**************************/

void Commander::RunAnalysis() {
    analyzeSituation();         // 可能更改了plans
    tacticArrange();            // 分配任务
}


void Commander::TeamAct() {
    // base
    callBack();
    attack();
    spendMoney();
    // heroes
    for (int i = 0; i < heroes.size(); ++i) {
        heroes[i].HeroAct();
    }
#ifdef LOG
    printHeroList(heroes);
#endif
}


void Commander::StoreAndClean() {
    for (int i = 0; i < heroes.size(); ++i) {
        heroes[i].StoreMe();
    }
    str_money.push_back(Economy);
}



/************************************************************
 * Implementation: class Hero
 ************************************************************/

/*************************Setters**************************/

PUnit *Hero::nearestEnemy() const {
    // 可攻击对象不存在
    if (vi_enemies.size() == 0)
        return nullptr;

    int min_dist = MAP_SIZE * MAP_SIZE;
    PUnit *selected = nullptr;

    for (int i = 0; i < vi_enemies.size(); ++i) {
        PUnit *it = vi_enemies[i];
        int dist = dis2(it->pos, pos);
        if (dist < min_dist) {
            selected = it;
            min_dist = dist;
        }
    }
    return selected;
}


Hero *Hero::getStoredHero(int prev_n) {
    if (prev_n >= str_heroes.size())
        return nullptr;

    vector<Hero> round = str_heroes[str_heroes.size() - 1 - prev_n];
    Hero *same = nullptr;
    for (int i = 0; i < round.size(); ++i) {
        Hero *temp = &round[i];
        if (temp->type == type && temp->id == id) {
            same = temp;
        }
    }
    return same;
}


void Hero::lockHotUnit() {      // toedit 主要策略点
    /*
     * @优先级:
     * WinOrDie
     * WaitRevive
     * Dizzy
     * 最弱单位
     * @小心:
     * 上一回合的热点单位死亡或召回
     * 被卡住了无法攻击
     * 追击了一段时间即停止
     */
    // fixme 有问题

    if (my_view_en.size() == 0) {
        hot = nullptr;
        hot_id = -1;
        return;
    }

    vector<PUnit *> win_or_die;
    vector<PUnit *> wait_revive;
    vector<PUnit *> dizzy;

    // 特殊buff
    for (int i = 0; i < my_view_en.size(); ++i) {
        PUnit *en = my_view_en[i];
        // WinOrDie
        if (hasBuff(en, "WinOrDie")) {
            win_or_die.push_back(en);
        }
        // WaitRevive
        if (hasBuff(en, "WaitRevive")) {
            wait_revive.push_back(en);
        }
        // Dizzy
        if (hasBuff(en, "Dizzy")) {
            dizzy.push_back(en);
        }
    }

    if (!win_or_die.empty()) {
        hot = win_or_die[0];
        hot_id = hot->id;
        return;
    }
    if (!wait_revive.empty()) {
        hot = wait_revive[0];
        hot_id = hot->id;
        return;
    }
    if (!dizzy.empty()) {
        hot = dizzy[0];
        hot_id = hot->id;
        return;
    }


    // 寻找血量最低单位
    int min_hp = 20000;
    int index = -1;
    for (int j = 0; j < my_view_en.size(); ++j) {
        if (my_view_en[j]->hp < min_hp) {
            min_hp = my_view_en[j]->hp;
            index = j;
        }
    }   // assert: my_view_en not empty
    hot = my_view_en[index];
    hot_id = hot->id;
}


/**************************Helpers**************************/

bool Hero::timeToFlee() {
    // 防止berserker误判
    if (hasBuff(punit, "WinOrDie"))
        return false;

    // 被卡住
    if (stuck())
        return true;

    // 血量过低
    if (hp < HP_ALERT * punit->max_hp) {
        return true;
    }

    return false;
}


bool Hero::stuck() {
    Hero *last = getStoredHero(1);
    Hero *last2 = getStoredHero(2);
    if (last == nullptr || last2 == nullptr)
        return false;
    else
        return (last->pos.x == pos.x && last->pos.y == pos.y
                && last2->pos.x == pos.x && last2->pos.y == pos.y);      // 无法重载
}


/**************************Actions**************************/

void Hero::cdWalk() {       // toedit 主要策略点
    PUnit *nearest = nearestEnemy();
    if (nearest == nullptr)
        return;

    Pos ref_p = nearestEnemy()->pos;               // position of reference
    // 撤离的距离为保持两者间距一个speed
    Pos far_p = parallelChangePos(pos, ref_p, speed, true);
    console->move(far_p, punit);        // go
}


void Hero::fastFlee() {
    PUnit *nearest = nearestEnemy();
    if (nearest == nullptr) {
        console->move(MILITARY_BASE_POS[CAMP], punit);           // go
        return;
    }

    Pos ref = nearestEnemy()->pos;
    // 撤离距离为尽量远离任何最近的单位
    if (type == 4 && punit->canUseSkill("Blink")) {     // master的闪烁
        Pos far_p = parallelChangePos(pos, ref, BLINK_RANGE, true);
        console->useSkill("Blink", far_p, punit);       // go
#ifdef LOG
        logger << "[skill] Blink from ";
        logger << pos << " to " << far_p << endl;
#endif
    } else {
        Pos far_p = parallelChangePos(pos, ref, speed, true);
        console->move(far_p, punit);                    // go
#ifdef LOG
        logger << "[move] flee to ";
        logger << far_p << endl;
#endif
    }
}


void Hero::justMove() {
    if (type == 6 && punit->canUseSkill("SetObserver")) {
        // 如果离关键点比较近,那么插眼 fixme 有眼就不插了
        Pos set = MINE_POS[0];
        int dist = dis2(set, pos);
        if (dist < SET_OBSERVER_RANGE) {
            console->useSkill("SetObserver", set, punit);   // go
#ifdef LOG
            logger << "[skill] SetObserver at pos=";
            logger << set;
            logger << "  dist=" << dist << endl;
#endif
            return;
        }
    }

    console->move(target, punit);      // go
#ifdef LOG
    logger << "[move] ";
    logger << target << endl;
#endif
}


void Hero::hammerguardAttack() {
    // cd中
    if (!punit->canUseSkill("HammerAttack") && !punit->canUseSkill("Attack")) {
        cdWalk();
#ifdef LOG
        logger << "[move] cd walk" << endl;
#endif
        return;
    }

    // 攻击
    if (punit->canUseSkill("HammerAttack")) {
        // 选择攻击英雄
        UnitFilter filter;
        filter.setAreaFilter(new Circle(pos, HAMMERATTACK_RANGE), "a");
        filter.setCampFilter(enemyCamp());
        filter.setAvoidFilter("Observer", "a");
        vector<PUnit *> to_hit = console->enemyUnits(filter);

        for (int i = 0; i < to_hit.size(); ++i) {
            if (to_hit[i]->findBuff("WinOrDie")) {
                console->useSkill("HammerAttack", to_hit[i], punit);
#ifdef LOG
                logger << "[skill] HammerAttack at:";
                printAtkInfo();
#endif
                return;
            }
        }

        // 没有优质攻击目标
        console->useSkill("HammerAttack", hot, punit);  // go
#ifdef LOG
        logger << "[skill] HammerAttack at:";
        printAtkInfo();
#endif
    } else {
        console->attack(hot, punit);                    // go
#ifdef LOG
        printAtkInfo();
#endif
    }
}


void Hero::berserkerAttack() {      // toedit 主要策略点 - 致命一击
    /*
     * 使用Sacrifice的条件:
     * 1.没有AttackCd
     * 2.遍历所有敌人,应该不存在:其射程既能包含我,又没有AttackCd
     */
    // fixme 目前策略比较失败

    // Sacrifice中
    if (punit->findBuff("WinOrDie") != nullptr && punit->canUseSkill("Attack")) {
        console->attack(hot, punit);        // go
    }

    // cd中
    if (!punit->canUseSkill("Attack")) {
        cdWalk();
#ifdef LOG
        logger << "[move] cd walk" << endl;
#endif
        return;
    } // assert: can attack

    // 结算
    if (punit->canUseSkill("Sacrifice") && punit->canUseSkill("Attack")) {
        console->useSkill("Sacrifice", hot, punit);     // go
#ifdef LOG
        logger << "[skill] Sacrifce" << endl;
#endif
    } else {    // assert: can attack
        console->attack(hot, punit);                    // go
#ifdef LOG
        printAtkInfo();
#endif
    }
}


void Hero::masterAttack() {
    /*
     * blink追击的条件: (master普通攻击没有cd)
     * 1.与该单位的距离(range, range + blink_range]
     * 2.该单位一击便歹
     */
    int dist = dis2(hot->pos, pos);
    if (dist > range && dist <= range + BLINK_RANGE && hot->hp < atk) {
        Pos chase_p = parallelChangePos(pos, hot->pos, dist - range / 2, false);
        console->useSkill("Blink", chase_p, punit);     // go
#ifdef LOG
        logger << "[skill] chasing by Blink";
        logger << pos << chase_p << endl;
#endif
    } else {
        console->attack(hot, punit);                    // go
#ifdef LOG
        printAtkInfo();
#endif
    }
}


void Hero::scouterAttack() {
    // 没有cd
    console->attack(hot, punit);        // go
#ifdef LOG
    printAtkInfo();
#endif
}


void Hero::contactAttack() {
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
        hot = nullptr;
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
    Hero *last = getStoredHero(1);
    if (last == nullptr) {
        target = TacLib[0];
        hot_id = -1;
    } else {
        target = last->target;
        hot_id = last->hot_id;
    }
}


void Hero::setUnits() {
    // fixme 野怪,除去observer
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
}


Hero::~Hero() {
    my_view_en.clear();
    punit = nullptr;
    hot = nullptr;
}


/*************************Loader***************************/

void Hero::setTarget(Tactic t) {
    target = t;
}


void Hero::HeroAct() {
#ifdef LOG
    logger << endl;
    logger << "@Act overview:" << endl;
    logger << name << "(" << id << "):" << endl;
    logger << ">> Decide to: " << endl;
#endif
    // todo
    // 逃跑
    if (timeToFlee()) {
        fastFlee();
        return;
    }

    // 不得离开目标矿太远
    if (dis2(target, pos) > view || hot == nullptr) {
        justMove();
        return;
    } else {
        contactAttack();
    }
}


void Hero::StoreMe() {
    Hero temp(*this);
    if (str_heroes.empty()) {
        vector<Hero> vct;
        vct.clear();
        vct.push_back(temp);
        str_heroes.push_back(vct);
        return;
    }

    // 检查最后一个向量的round
    int last_r = str_heroes.back().back().round;
    // 分情况处理储存
    if (last_r == Round) {
        str_heroes.back().push_back(temp);
    } else {
        vector<Hero> vct;
        vct.clear();
        vct.push_back(temp);
        str_heroes.push_back(vct);
    }
}

#ifdef LOG

void Hero::printAtkInfo() const {
    logger << "[atk] ";
    logger << hot->name << "(" << hot->id << ") ";
    logger << "dist = " << dis2(hot->pos, pos) << endl;
}

#endif


/*
 * todo 要更改的内容
 * 1. 集群攻击某单位(将lockhot移到Commander里面),固定的可见单位排序系统
 * 2. 逃窜的连续性
 * 3. Berserker的致命一击
 * 5. buyLevel, buyNew, 对部分单位进行召回升级callBack, 钱过多时进行买活buyLife
 * 7. 寻路:垂直逃逸
 * 8. hot目标逃逸后,如何处理
 */
