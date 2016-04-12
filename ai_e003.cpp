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

struct Hero;

class Commander;

class AssaultSquad;

/*######################## DATA ###########################*/
/************************************************************
 * Const values
 ************************************************************/
static const int HERO_COST[] = {
        NEW_HAMMERGUARD_COST, NEW_MASTER_COST, NEW_BERSERKER_COST, NEW_SCOUTER_COST,
};
static const char *HERO_NAME[] = {"Hammerguard", "Master", "Berserker", "Scouter"};

static const int TAC_TARGETS_N = 9;         // 7个矿+2个基地
static const Tactic TACTICS[] = {
        MINE_POS[0], MINE_POS[1], MINE_POS[2], MINE_POS[3],
        MINE_POS[4], MINE_POS[5], MINE_POS[6],
        MILITARY_BASE_POS[0], MILITARY_BASE_POS[1]
};


/************************************************************
 * Policy const values
 ************************************************************/
// unchanged values (during the entire game)
static int CAMP = -1;                       // which camp

// Commander
// Commander::levelUp
static const double LEVEL_UP_COST = 0.5;    // 升级金钱比例
// Commander::buyNewHero
static int BUY_RANK = 42314132;             // 请参考hero_name
// Commander::callBack()
static const int BACK_BASE = 2;             // 面对多少敌人,基地召回我方英雄

// clearOldInfo()
static const int CLEAN_LIMIT = 6;           // 最多保留回合记录
static const int CLEAN_NUMS = 2;            // 超过最多保留记录后,一次清理数据组数

// Hero
static const double HP_ALERT = 0.2;         // 血量预警百分比
static const int BATTLE_RANGE = 625;        // 战区范围


/************************************************************
 * Real-time sharing values
 ************************************************************/
static Console *console = nullptr;
static Commander *commander = nullptr;
static int Round = -1;
static int Economy;                             // 经济

static set<int> EstEnemies;                     // 估计的敌人id

vector<PUnit *> cur_friends;                    // 当前友军英雄
const PUnit *base;                              // 我的基地
vector<PUnit *> vi_enemies;                     // 当前可见敌人英雄,不含野怪
const PUnit *en_base;                           // 敌人基地
vector<PUnit *> vi_mines;                       // 可见矿(矿的位置是默认的,但状态需要可见才能读取)
vector<PUnit *> vi_monsters;                    // 可见野怪


/************************************************************
 * Storage data
 ************************************************************/
static vector<vector<Hero>> StrHeroes;          // 储存的英雄

/* 战术核心 */
// 继承上一轮
static int HotId = -1;                          // 储存的hot id
static Tactic Target = MINE_POS[0];             // 储存的targets
// todo
static const int SQUAD_N = 3;                   // 小队数量 todo 三个小队三种类型任务
static int SquadTargets[SQUAD_N] = {};          // 小队战术id
static int SquadHots[SQUAD_N] = {};             // 小队热点id

// 7个矿顺序依次是 0-中矿,1-8点,2-10点,3-4点,4-2点,5-西北,6-东南
static int SuperiorTactics[] = {0, 1, 2, 3, 4}; // 优势战术
static int SupTN = 5;
static int BackupTactics[] = {5, 6};            // 备选战术 player1
static int BakTN = 2;

// 战术延时
static int StickRounds = 40;                    // 初始保留战术的时间
static int HoldTill = 50;                       // 连续守住回合数,才采取动作
static int TCounter = StickRounds;              // 设置战术倒计时

// 战局判断
static int Situation = 0;                       // 0-HOLD_TILL是僵持,负数是失守,>HOLD_TILL是占据

// 其他待定
// Commander::analyzeSitu
static int Kills = 0;                           // 杀敌数
static int Deaths = 0;                          // 死亡数


/*################# Assistant functions ####################*/
// ================ Log Related ====================
#ifdef LOG
void stopClock(long start);

static ofstream logger("log_info.txt");

void printUnit(vector<PUnit *> units);

void printHeroList(vector<Hero> units);

template<typename T>
void printString(vector<T> vct);      // T重载了<<
#endif

// =============== Basic Algorithms ================
// Path finding
void modifyTactic(int *tactics, int size);                // 对于player1和0的区别,修改战术
Pos parallelChangePos(const Pos &origin, const Pos &reference, int len2, bool away = true);  // 详见实现
Pos verticalChangePos(const Pos &origin, const Pos &reference, int len2, bool clockwize = true);  //详见实现

// String and int transfer
int str2int(string str);

string int2str(int n);

// Data structure related
template<typename T>
void releaseVector(vector<T *> vct);

bool compareLevel(PUnit *a, PUnit *b);

bool operator<(const Pos &a, const Pos &b);

// Handling stored data
template<typename T>
void clearOldInfo(vector<T> &vct);                  // 及时清理陈旧储存信息


// ============== Game and Units ===================
// Global
int enemyCamp();                                    // 敌人的camp

// Unit
int buyNewCost(int cost_indx);                      // 当前购买新英雄成本,参数为HERO_NAME的索引
bool hasBuff(PUnit *unit, const char *buff);        // 是否有某buff
bool justBeAttacked(PUnit *test);

double surviveRounds(PUnit *host, PUnit *guest);    // 计算存活轮数差:host - guest
PUnit *findID(vector<PUnit *> units, int _id);

// ============== Evaluation ===================
int teamAtk(vector<PUnit *> vct);
double unitDefScore(PUnit *pu);                     // 给单位的实际防守力评估
double unitAtkScore(PUnit *pu);                     // todo 单位进攻评估

/************************************************************
 * Global commander
 ************************************************************/
// 全局指挥官,分析形势,买活,升级,召回等,也充当本方base角色
// 更新不同战区信息,units可以领任务,然后形成team
// global_state: inferior, equal, superior
class Commander {
private:
    vector<Hero *> heroes;
    vector<PUnit *> sector_en;

    PUnit *hot;

    /**********************************************************/
protected:
    // HELPERS
    void getUnits();                                // 获取单位信息

    // tactics 顺序不能错!
    void lockTarget();                              // 分析形势,设置战术
    void lockSquadTarget();                         // 指定小队目标
    void regroup();                                 // 重组小队
    void lockHot();                                 // 锁定团队热点攻击对象
    void makeHeroes();                              // 制造英雄
    void makeSquads();                              // 制造小队

    // base actions
    void baseAttack();                              // 基地攻击
    void buyNewHero();                              // 买英雄
    void buyLife();                                 // 买活英雄
    void levelUp();                                 // 升级英雄
    void spendMoney();                              // 买英雄/买等级/买活的选择
    void callBack();                                // 召回英雄

public:
    // constructor
    Commander();

    ~Commander();

    /**********************************************************/
    // LOADER
    void TeamAct();                                 // 基地和英雄动作
    void StoreAndClean();                           // 储存

};



/************************************************************
 * Assault Squad
 ************************************************************/
// 突击小队
class AssaultSquad {
private:
    int member_n;                       // 成员人数
    vector<int> member_id;              // 成员id,便于储存和查询
    vector<Hero *> members;             // 成员指针,便于调用
    vector<Pos> stand;                  // 站位

    PUnit *hot_id;                      // 热点对象id
    PUnit *hot;                         // 热点对象指针

    int target_id;                      // 战术编号:0-6矿,10-11基地
    int task_type;                      // todo 还没有配置
    int t_counter;                      // 战术倒计时
    int situation;                      // 小队战况

protected:
    /*************************HELPER****************************/
    // construct
    void makeHeroes();                  // 根据需要取用英雄
    void lockHot();

    // tasks
    void scout();
    void goMining();
    void readyBattle();

    // path finding
    void adjustPos();


public:
    AssaultSquad(int _member_n, int _tid, int _ttype, int _tcounter);
    ~AssaultSquad();

    /*************************LOADER****************************/
    void SquadAct();
    void StoreMe();
};


/************************************************************
 * Heroes
 ************************************************************/
// 重新封装PUnit数据,便于数据储存
// 由于单位种类太少,技能也很少,所以暂不计划采用继承的方式
struct Hero {
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

    // todo
    bool can_skill;
    bool can_attack;

    Tactic target;                              // 战术
    int hot_id;                                 // 便于储存
    PUnit *hot;

    /*************************Setters**************************/
    PUnit *nearestEnemy() const;
    Hero *getStoredHero(int prev_n);            // 获得之前prev_n局的储存对象
    void setPtr(PUnit *unit);                   // 连接ptr

    /*************************Helpers***************************/
    bool timeToFlee();                          // 是否应该逃窜
    bool stuck();                               // 由于未知原因卡住了
    void checkHot();                            // 检查一下热点目标是否有问题

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

    /**********************************************************/
    // constructor/destructor
    Hero(PUnit *hero, PUnit *hot, Tactic target);   // Commander需要的构造器

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

    // Create pointers
    console = new Console(map, info, cmd);
    commander = new Commander();

    // Hero do actions // todo 时间消耗大户
    commander->TeamAct();

    // Store all
    commander->StoreAndClean();

    delete commander;
    delete console;

#ifdef LOG
    logger << ">> Total" << endl;
    stopClock(start);
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


void printHeroList(vector<Hero *> units) {
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
        Hero *unit = units[i];
        // print basic hero info
        logger << left << setw(14) << unit->name;
        logger << left << setw(5) << unit->id;
        logger << left << setw(8) << unit->level;
        logger << left << setw(5) << unit->hp;
        logger << left << setw(5) << unit->mp;
        logger << left << setw(5) << unit->atk;
        logger << left << setw(5) << unit->def;
        // POS/TAC
        string p = int2str(unit->pos.x) + "," + int2str(unit->pos.y);
        logger << left << setw(10) << p;
        string t = int2str(unit->target.x) + "," + int2str(unit->target.y);
        logger << left << setw(10) << t;
        // BUFF
        vector<PBuff> buff = unit->punit->buffs;
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


void stopClock(long start) {
    long end = clock();
    double interval = 1.0 * (end - start) / CLOCKS_PER_SEC * 1000;
    logger << "$$ Time Consumed(ms): " << interval << endl;
    logger << endl << endl;
}

#endif

// algorithms
void modifyBackupTactic(int *tactics, int size) {
//    for (int i = 0; i < BakTN; ++i) {
//
//    }
    return;     // todo 决定是否丰富战术
}

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
    int _sz = (int) buffs.size();
    for (int i = 0; i < _sz; ++i) {
        if (strcasecmp(buffs[i].name, buff) == 0)
            return true;
    }
    return false;
}


double surviveRounds(PUnit *host, PUnit *guest) {
    /*
     * 计算公式:
     * 技能杀伤 = 先忽略技能预估战斗轮数,然后计算保守值
     * 生命恢复 = 本轮全体生命恢复
     * 死亡回合 = (hp + 生命恢复 - 技能杀伤) / (对方攻击 - 我方防守)
     */
    int host_hp_rcv = console->unitArg("hp", "rate", host);
    int guest_hp_rcv = console->unitArg("hp", "rate", guest);
    double host_r = 1.0 * (host->hp + host_hp_rcv) / max(host->atk - guest->def, 5);
    double guest_r = 1.0 * (guest->hp + guest_hp_rcv) / max(guest->atk - host->def, 5);
    return (host_r - guest_r);
}


PUnit *findID(vector<PUnit *> units, int _id) {
    if (_id == -1) return nullptr;

    int _sz = (int) units.size();
    for (int i = 0; i < _sz; ++i) {
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

    int _sz = (int) vct.size();
    for (int i = 0; i < _sz; ++i) {
        round_atk += vct[i]->atk;
    }
    return round_atk;
}


double unitDefScore(PUnit *pu) {
    /*
     * 计算公式:
     * score = hp / max{N - def, 5}
     * N = 100, 作为攻击力常数
     */
    const int N = 100;
    double s = 1.0 * pu->hp / max(N - pu->def, 5);
    return s;
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

    // 顺序不能错
    modifyBackupTactic(BackupTactics, BakTN);
    // cur_friends  vi_enemies sector_en
    getUnits();
    // analyze
    lockTarget();
    lockHot();
    // make team
    makeHeroes();
}


Commander::~Commander() {
    heroes.clear();
    // PUnit*不能释放!
    releaseVector(cur_friends);
    base = nullptr;
    releaseVector(vi_enemies);
    en_base = nullptr;
    releaseVector(vi_mines);
    releaseVector(vi_monsters);
    clearOldInfo(StrHeroes);
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
    logger << "@Sector enemies" << endl;
    printUnit(sector_en);
#endif
}


/*************************Tactics**************************/

void Commander::lockHot() {     // toedit 主要策略点
    /*
     * @优先级:
     * WinOrDie
     * WaitRevive
     * 最弱单位
     */

    UnitFilter filter;
    filter.setAreaFilter(new Circle(Target, BATTLE_RANGE), "a");
    filter.setAvoidFilter("Observer", "a");
    filter.setAvoidFilter("Mine", "w");
    filter.setHpFilter(1, 100000);

    // 根据战术目标,设定打击单位范围
    if (Target != MINE_POS[0]) {
        // 攻击其他矿时还攻击野怪/军事基地
        sector_en = console->enemyUnits(filter);
    } else {
        // 攻击中矿时仅攻击对手
        filter.setCampFilter(enemyCamp());
        sector_en = console->enemyUnits(filter);
    }

    // 用迭代器遍历并删除指定元素 - 尸体
    for (auto i = sector_en.begin(); i != sector_en.end(); ) {
        if (hasBuff(*i, "Reviving"))
            i = sector_en.erase(i);
        else
            i++;
    }

    if (sector_en.size() == 0) {
        hot = nullptr;
        HotId = -1;
        return;
    }

    vector<PUnit *> win_or_die;
    vector<PUnit *> wait_revive;

    // 寻找最弱单位
    int index = -1;
    double min = INT_MAX;
    // 特殊buff
    int _sz = (int) sector_en.size();
    for (int i = 0; i < _sz; ++i) {
        PUnit *en = sector_en[i];
        // WinOrDie
        if (hasBuff(en, "WinOrDie")) {
            win_or_die.push_back(en);
        }
        // WaitRevive
        if (hasBuff(en, "WaitRevive")) {
            wait_revive.push_back(en);
        }

        // 最弱
        double score = unitDefScore(sector_en[i]);
        if (score < min) {
            index = i;
            min = score;
        }
    }

    /* 结算 */
    // 特殊buff
    if (!win_or_die.empty()) {
        hot = win_or_die[0];
        HotId = hot->id;
        return;
    }
    if (!wait_revive.empty()) {
        hot = wait_revive[0];
        HotId = hot->id;
        return;
    }

    // 继承
    PUnit *last_hot = findID(sector_en, HotId);
    if (last_hot != nullptr) {
        hot = last_hot;
        return;
    }

    // 最弱
    try {
        hot = sector_en[index];
        HotId = hot->id;
    } catch (exception &e) {    // index = -1
        hot = nullptr;
        HotId = -1;
    }

}


void Commander::lockTarget() {
    // todo 灵活性不够,需要整理
    // 倒计时结束再考虑改变战术
    TCounter--;
    if (TCounter > 0)
        return;

    // todo 如果有能力足够,打基地

    /*
     * warn 这一段代码并没有用sector_en,因为它还没有在lockhot中被设置
     * fixme 代码之间顺序容易紊乱,后期需要调整
     */
    UnitFilter filter;
    filter.setAreaFilter(new Circle(Target, BATTLE_RANGE), "a");
    filter.setAvoidFilter("Mine", "a");
    filter.setAvoidFilter("Observer", "w");
    vector<PUnit *> tar_friends = console->friendlyUnits(filter);
    filter.setCampFilter(enemyCamp());
    vector<PUnit *> tar_enemies = console->enemyUnits(filter);
    // 用迭代器遍历并删除指定元素 - 尸体
    for (auto i = tar_enemies.begin(); i != tar_enemies.end(); ) {
        if (hasBuff(*i, "Reviving"))
            i = tar_enemies.erase(i);
        else
            i++;
    }
    // 找矿对象
    UnitFilter mine_filter;
    mine_filter.setAreaFilter(new Circle(Target, 10), "a");
    mine_filter.setTypeFilter("Mine", "a");
    vector<PUnit *> mine = console->enemyUnits(mine_filter);
    bool mine_empty = false;
    if (mine.size() == 1 && mine[0]->isMine()) {
        int energy = console->unitArg("energy", "c", mine[0]);
        mine_empty = (energy < 2);
#ifdef LOG
        logger << ">> Mine checked!  energy = " << energy << endl;
#endif
    }

    // 设置标记
    if (tar_friends.empty()) {          // 失守了 - friends=0 warn 每次切换战术重新计时,因此不用担心跑过去过程中就失守
        Situation = -1;
    } else if (tar_enemies.empty() || mine_empty){    // 占据了 - friends>0, enemies=0
        Situation = max(1, ++Situation);
    } else {                            // 僵持中 - friends>0, enemies>0
        Situation = 0;
    }

    // 如果lost,调整战术
    if (Situation < 0) {                     // 失守
        srand((unsigned int) Round / 19);
        int index = rand() % BakTN;
        Target = MINE_POS[BackupTactics[index]];
        // renew
        Situation = 0;
        TCounter = StickRounds;
#ifdef LOG
        logger << ">> [LOST]Change plans to " << BackupTactics[index] << endl;
#endif
    } else if (Situation > HoldTill) {     // 占据
        // 偷基地
        if (cur_friends.size() >= 7) {
            Target = MILITARY_BASE_POS[enemyCamp()];
            Situation = 0;
            TCounter = StickRounds;
            return;
        }
        // 围剿
        srand((unsigned int) Round / 17);
        int index = rand() % SupTN;
        Target = MINE_POS[SuperiorTactics[index]];
        // renew
        Situation = 0;
        TCounter = StickRounds;
#ifdef LOG
        logger << ">> [OCUP]Change plans to " << SuperiorTactics[index] << endl;
#endif
    }
}


void Commander::makeHeroes() {
    int _sz = (int) cur_friends.size();
    for (int i = 0; i < _sz; ++i) {
        heroes.push_back(new Hero(cur_friends[i], hot, Target));
    }
}


/*************************Base actions**************************/

void Commander::baseAttack() {
    Pos our_base = MILITARY_BASE_POS[CAMP];
    UnitFilter filter;
    filter.setAreaFilter(new Circle(our_base, MILITARY_BASE_RANGE), "a");
    vector<PUnit *> enemies = console->enemyUnits(filter);
    if (enemies.empty()) return;

    // 随便攻击一个,不用想太多
    console->baseAttack(enemies[0]);    // go 基地攻击
}


void Commander::buyNewHero() {    // toedit 主要策略点
    int new_i = BUY_RANK % 10 - 1;
    int cost = buyNewCost(new_i);

    if (new_i >= 0 && new_i < 4 && cost < Economy) {
        console->chooseHero(HERO_NAME[new_i]);
#ifdef LOG
        logger << "@buy new hero" << endl;
        logger << ">> new_i=" << new_i << endl;
        logger << ">> buy rank = " << BUY_RANK << endl;
#endif
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
    int _sz = (int) nearHeroes.size();
    for (int i = 0; i < _sz; ++i) {
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
    int _sz2 = (int) toLevelUp.size();
    for (int j = 0; j < _sz2; ++j) {
        console->buyHeroLevel(toLevelUp[j]);    // go
    }
}


void Commander::buyLife() {
    // todo 暂时没有发现买活的意义
    return;
}


void Commander::spendMoney() {
    // todo 还没有加买活,策略不佳
    buyNewHero();
    levelUp();
}


void Commander::callBack() {
    // todo 设计成强制设置Hero的Tactic值,暂时认为快速召回没有太大意义
    // todo 对于小股骚扰效果不佳
    // 条件判断
    UnitFilter filter;
    filter.setAreaFilter(new Circle(MILITARY_BASE_POS[CAMP], MILITARY_BASE_VIEW), "a");
    filter.setAvoidFilter("Observer", "a");
    filter.setCampFilter(enemyCamp());
    int base_en = (int) console->enemyUnits(filter).size();
    if (base_en >= BACK_BASE) {
//        // 进行结算,召回响应人数的己方英雄
//        for (int i = 0; i < base_en; ++i) {
//            srand((unsigned int) Round / 13);
//            int index = (int) (rand() % heroes.size());
//            heroes[index]->setTarget(MILITARY_BASE_POS[CAMP]);
//        }
        Target = MILITARY_BASE_POS[CAMP];
        StickRounds = 50;
    }
}


/*************************Interface**************************/

void Commander::TeamAct() {
    // base
    callBack();
    baseAttack();
    spendMoney();
    // heroes
    int _sz = (int) heroes.size();
    for (int i = 0; i < _sz; ++i) {
        heroes[i]->HeroAct();
    }
#ifdef LOG
    printHeroList(heroes);
#endif
}


void Commander::StoreAndClean() {
    int _sz = (int) heroes.size();
    for (int i = 0; i < _sz; ++i) {
        heroes[i]->StoreMe();
    }
}


/************************************************************
 * Implementation: class AssaultSquad
 ************************************************************/

/*************************HELPER****************************/



/*************************LOADER****************************/





/************************************************************
 * Implementation: class Hero
 ************************************************************/

/*************************Setters**************************/

PUnit *Hero::nearestEnemy() const {
    int _sz = (int) vi_enemies.size();
    // 可攻击对象不存在
    if (_sz == 0)
        return nullptr;

    int min_dist = INT_MAX;
    PUnit *selected = nullptr;

    for (int i = 0; i < _sz; ++i) {
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
    if (prev_n >= StrHeroes.size())
        return nullptr;

    vector<Hero> round = StrHeroes[StrHeroes.size() - 1 - prev_n];
    Hero *same = nullptr;
    int _sz = (int) round.size();
    for (int i = 0; i < _sz; ++i) {
        Hero *temp = &round[i];
        if (temp->type == type && temp->id == id) {
            same = temp;
        }
    }
    return same;
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


void Hero::checkHot() {
    // todo
}


/**************************Actions**************************/

void Hero::cdWalk() {       // toedit 主要策略点
    PUnit *nearest = nearestEnemy();
    if (nearest == nullptr)
        return;

    Pos ref_p = nearest->pos;               // position of reference
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
    if (type == 4 && can_skill) {     // master的闪烁
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
    if (type == 6 && can_skill) {
        // todo 插眼策略点,未完成 (有眼就不插了)
        // 如果离关键点比较近,那么插眼
        Pos set = MINE_POS[0];
        int dist = dis2(set, pos);
        if (dist < SET_OBSERVER_RANGE / 2) {
            console->useSkill("SetObserver", pos - Pos(2, 0), punit);   // go
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
    if (!can_skill && !can_attack) {
        cdWalk();
#ifdef LOG
        logger << "[move] cd walk" << endl;
#endif
        return;
    }

    // 攻击
    if (can_skill) {
        if (dis2(pos, hot->pos) < HAMMERATTACK_RANGE) {
            console->useSkill("HammerAttack", hot, punit);  // go
#ifdef LOG
            logger << "[skill] HammerAttack at:";
            printAtkInfo();
#endif
            return;
        }
    }

    // assert: 不能使用技能,或能使用但是没有可行对象
    console->attack(hot, punit);                    // go
#ifdef LOG
    printAtkInfo();
#endif
}


void Hero::berserkerAttack() {      // toedit 主要策略点 - 致命一击
    /*
     * 使用Sacrifice的条件:
     * 1.没有AttackCd
     * 2.遍历所有敌人,应该不存在:其射程既能包含我,又没有AttackCd
     */
    // fixme 目前策略比较失败

    // Sacrifice中
    if (punit->findBuff("WinOrDie") != nullptr && can_attack) {
        console->attack(hot, punit);        // go
        return;
    }

    // cd中
    if (!can_attack) {
        cdWalk();
#ifdef LOG
        logger << "[move] cd walk" << endl;
#endif
        return;
    } // assert: can attack

    // 讨论环境是否安全
    bool safe = true;
    // todo unfinished

    // 结算
    if (safe && can_skill && can_attack) {
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
    int dist2 = dis2(hot->pos, pos);
    // 退后
    if (dist2 < MASTER_RANGE) {
        Pos bak_p = parallelChangePos(pos, hot->pos, range, true);
        console->move(bak_p, punit);
        return;
    }

    // 追赶
    if (dist2 > range && dist2 <= range + BLINK_RANGE && hot->hp < atk) {
        Pos chase_p = parallelChangePos(pos, hot->pos, dist2 - range / 2, false);
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
        // 不得不设的
        hot = nullptr;
        hot_id = -1;
        target = Pos(0, 0);
        return;
    }

    punit = unit;
    name = unit->name;
    type = unit->typeId;
    id = unit->id;
    hp = unit->hp;
    exp = unit->exp;
    mp = unit->mp;
    level = unit->level;
    atk = unit->atk;
    def = unit->def;
    speed = unit->speed;
    view = unit->speed;
    pos = unit->pos;
}


Hero::Hero(PUnit *hero, PUnit *hot, Tactic target) {
    setPtr(hero);
    this->hot = hot;
    if (hot == nullptr) {
        hot_id = -1;
    } else {
        this->hot_id = hot->id;
    }
    this->target = target;
    checkHot();

    // can use skill? (saving time)
    int skill_index = type + 5;     // warn skill_name_index - type_name_index = 5
    can_skill = punit->canUseSkill(SKILL_NAME[skill_index]);
    can_attack = punit->canUseSkill("Attack");
}


Hero::~Hero() {
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
    long start = clock();
#endif
    // todo
    // 逃跑
    if (timeToFlee()) {
        fastFlee();
        return;
    }

    // 不得离开目标矿太远
    if (dis2(target, pos) > BATTLE_RANGE || hot == nullptr) {
        justMove();
        return;
    } else {
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
#ifdef LOG
    logger << ">> this hero consumed: " << endl;
    stopClock(start);
#endif
}


void Hero::StoreMe() {
    Hero temp(*this);
    if (StrHeroes.empty()) {
        vector<Hero> vct;
        vct.clear();
        vct.push_back(temp);
        StrHeroes.push_back(vct);
        return;
    }

    // 检查最后一个向量的round
    int last_r = StrHeroes.back().back().round;
    // 分情况处理储存
    if (last_r == Round) {
        StrHeroes.back().push_back(temp);
    } else {
        vector<Hero> vct;
        vct.clear();
        vct.push_back(temp);
        StrHeroes.push_back(vct);
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
 * 1. 小队模式: 走位/队形...
 * 2. cdWalk
 * 3. Berserker的致命一击
// * 5. buyLevel, buyNew, 对部分单位进行召回升级callBack, 钱过多时进行买活buyLife
 *
 *
 * temp:
 * 对抗偷基地流
 * 对抗中路优势+分矿流
 */
