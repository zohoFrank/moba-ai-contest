// 调试开关
#define LOG

#ifdef LOG
#define TEMP
#include <ctime>
#include <fstream>
#include <iomanip>

#endif

#include "console.h"
#include <queue>
#include <random>
#include <sstream>
#include <map>
#include <set>

using namespace std;

typedef Pos Tactic;

class Hero;

class Commander;

class AssaultSquad;

class MainCarrier; class MineDigger; class BattleScouter;

/*######################## DATA ###########################*/
/************************************************************
 * Const values
 ************************************************************/
static const int BIG_INT = 1 << 12;

static const int TAC_TARGETS_N = 9;         // 7个矿+2个基地
static const int MINE_NUM_SHIFT = 5;        // 编号偏移,id = tac_id + shift

static const int HERO_COST[] = {
        NEW_HAMMERGUARD_COST, NEW_MASTER_COST, NEW_BERSERKER_COST, NEW_SCOUTER_COST,
};
static const char *HERO_NAME[] = {"Hammerguard", "Master", "Berserker", "Scouter"};

static const Tactic TACTICS[] = {
        MINE_POS[0], MINE_POS[1], MINE_POS[2], MINE_POS[3],
        MINE_POS[4], MINE_POS[5], MINE_POS[6],
        MILITARY_BASE_POS[0], MILITARY_BASE_POS[1]
};

static const int OBSERVE_POS_N = 1;
static const Pos OBSERVE_POS[OBSERVE_POS_N] = {     // 带偏移量防止碰撞半径引起麻烦
        MINE_POS[0] + Pos(3, 3)
};


/************************************************************
 * Policy const values
 ************************************************************/
// unchanged values (during the entire game)
static int CAMP = -1;                       // which camp

// Commander
// levelUp
static const double LEVEL_UP_COST = 0.5;    // 升级金钱比例
// buyNewHero
static int BUY_RANK = 42314312;             // 请参考hero_name
// callBack
static const int CALLBACK_MIN_DIST2 = 600;  // 召回的必要最小距离
static const int CALLBACK_LVLUP = 800;      // 召回升级的最小经济水平
static const int CALLBACK_RECV_HP = 2000;   // 召回补血的最小经济水平

// clearOldInfo()
static const int CLEAN_LIMIT = 6;           // 最多保留回合记录
static const int CLEAN_NUMS = 2;            // 超过最多保留记录后,一次清理数据组数

// Hero
static const double HP_FLEE_ALERT = 0.2;         // 血量预警百分比
static const double HP_BACK_ALERT = 0.6;         // 回基地补血百分比

static const int BATTLE_RANGE = 625;        // 战区范围

// Squad
static const int MEMBER_LIMIT = 4;          // 小队最多人数

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
/* 战术核心 */
// 7个矿顺序依次是 0-中矿,1-8点,2-10点,3-4点,4-2点,5-西北,6-东南
static const int SUP_MIN = 50;                  // 优势判分标准
static const int BAK_MAX = -50;                 // 劣势判分标准
static vector<int> SuperiorTactics = {0, 1, 2, 3, 4}; // 优势战术
static vector<int> BackupTactics = {5, 6};      // 备选战术 player1
static queue<int> GetBack;                      // 需要夺回的地点:MD刚失守的,或计划夺取的

// 战术延时
static int StickRounds = 40;                    // 初始保留战术的时间
static int TCounter = StickRounds;              // 设置战术倒计时

// 战局判断
static int TargetState[TAC_TARGETS_N] = {};     // 0-未占据,1-占据
static const int LEVEL1 = 8;                   // 判断第一界点,低于此不分队
static const int LEVEL2 = 18;                   // 判断第二界点,高于此推基地

typedef vector<int> ID_LIST;
// Squad settings
static const int SQUAD_N = 8;                   // 小队数量
static int SquadTargets[SQUAD_N] = {};          // 小队战术id
static vector<ID_LIST> SquadMembers(8, vector<int>(0));            // 各小队成员安排

// Squad list
static const int SINGLE_MC_LIMIT = 8;           // MC人数max限制
static vector<AssaultSquad *> AllSquads;        // 所有小队,默认初始化后需要调整参数
static const int MC_I = 1;
static const int MD_I = 5;
static const int BS_I = 7;
static const int SCOUT_OK = 5;                  // 侦查完成的最大距离




/*################# Assistant functions ####################*/
// ================ Log Related ====================
#ifdef LOG
void stopClock(long start);

static ofstream logger("log_info.txt");

void printUnit(vector<PUnit *> units);

void printHeroList(vector<Hero> units);

template<typename T>
void printString(vector<T> vct);      // T重载了<<

void printSquads();                     // print AllSquads/SquadMember/SquadTarget
#endif

// =============== Basic Algorithms ================
// Path finding
Pos parallelChangePos(const Pos &origin, const Pos &reference, int len2, bool away = true);  // 详见实现
Pos verticalChangePos(const Pos &origin, const Pos &reference, int len2);  //详见实现

// String and int transfer
int str2int(string str);

string int2str(int n);

// Data structure related
template<typename T>
void releaseVector(vector<T *> vct);

bool compareLevel(PUnit *a, PUnit *b);

bool operator<(const Pos &a, const Pos &b);

int rangeRandom(int max_no_inclu, int avoid);

// ============== Game and Units ===================
// Global
int enemyCamp();                                    // 敌人的camp
void initilize();                                   // 初始化

// Unit
int buyNewCost(int cost_indx);                      // 当前购买新英雄成本,参数为HERO_NAME的索引
bool canDamage(PUnit *unit, int round=0);           // 单位当前是否可以输出伤害
bool justBeAttacked(PUnit *test);
PUnit *getFriendlyUnit(int id);                     // 节省时间

double surviveRounds(PUnit *host, PUnit *guest);    // 计算存活轮数差:host - guest
PUnit *findID(vector<PUnit *> units, int _id);

// ============== Evaluation ===================
int teamAtk(vector<PUnit *> vct);
double unitDefScore(PUnit *pu);                     // 给单位的实际防守力评估



/************************************************************
 * Global commander
 ************************************************************/
// 全局指挥官,分析形势,买活,升级,召回等,也充当本方base角色
// 更新不同战区信息,units可以领任务,然后形成team
// global_state: inferior, equal, superior
class Commander {
private:
    vector<PUnit *> sector_en;

    PUnit *hot;

    /**********************************************************/
protected:
    // CONSTRUCTION
    void getUnits();                                // 获取单位信息
    // tactics 顺序不能错!
    void updateSquad();                             // 更新小队信息
    void squadSet();                                // 小队成员和目标分配

    // OTHER HELPERS
    int judgeSituation(int squad);                  // 封装接口,根据squad::situation返回
    void markTarget(int target);                    // 封装接口,标记待(全体)夺取,放入GetBack
    void callBackupSquad(int needed_n);             // 组织一个小队,回防
    // squadSet helpers
    int getTotalLevels(int squad);                  // 获得小队总等级
    void moveMembers(int from, int to, int n);      // 在小队间移动成员
    void gatherAll();                               // 聚集所有小队到0号
    bool timeToPush();                              // 推基地时机判断
    void pushEnemyCamp();                           // 推基地
    void handle(int squad, int levels, int situ);   // 处理动作

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

};



/************************************************************
 * Assault Squads
 ************************************************************/
// 突击小队
class AssaultSquad {
public:
    int id, type;                       // 小队编号
    int battle_range;                   // 作战半径
    int situation;                      // 状况
    int stick_counter;                  // 保持战术的时间

    // get from commander
    vector<int> member_id;              // 成员id,便于储存和查询
    int target_id;                      // 战术编号:0-6矿,10-11基地

    // settings
    bool besiege;                       // 包围攻击,同时设置小队成员的besiege标志

    vector<Hero *> members;             // 成员指针,便于调用
    vector<PUnit *> sector_en;         // 区域敌人
    vector<PUnit *> sector_f;           // 区域朋友

    int hot_id;                         // 热点对象id
    PUnit *hot;                         // 热点对象指针

    /*************************HELPER****************************/
    virtual void clean();               // 静态对象,每回合开始需要重置一些东西
    virtual void resetTacMonitor(int _n = StickRounds);   // 重设计数器/判断器
    // construct
    virtual void setOthers() = 0;       // 用来给子类设置自身独有的成员变量
    virtual void setBesiege();          // 设置小队/成员的besiege标志
    virtual void getUnits();            // get sector_en sector_f
    virtual void getAllCmdInfo();       // 从全局变量获得信息
    virtual void lockHot();             // get hot
    virtual void setHeroes();           // set heroes' targets and hots
    virtual void evaluateSituation() = 0;   // 评估situation,+ postive, - negative

    /*************************Actions****************************/
    virtual void crossBesiege();        // 十字卡位包围准备
    virtual void slipAttack();          // 游动攻击准备

public:
    AssaultSquad(int _id);
    virtual ~AssaultSquad();

    /*************************LOADER****************************/
    virtual void roundUpdate();
    virtual void SquadCommand();        // 每回合调用一次
};



/*****************************Main Carrier*******************************/
// 主力战队
class MainCarrier : public AssaultSquad {
public:
    MainCarrier(int _id);

    virtual void setOthers() override;
    virtual void evaluateSituation() override;
};



/*****************************Mine Digger*******************************/
// 挖矿小队
class MineDigger : public AssaultSquad {
public:
    int mine_energy;

    /**********************************************************/
    MineDigger(int _id);

    virtual void setOthers() override;
    virtual void evaluateSituation() override;
};



/*****************************Battle Scouter*******************************/
// 侦查小队
class BattleScouter : public AssaultSquad {
public:
    virtual void lockHot() override;

    BattleScouter(int _id);

    virtual void setOthers() override;
    virtual void evaluateSituation() override;
};



/************************************************************
 * Heroes
 ************************************************************/
// 重新封装PUnit数据,便于数据储存
class Hero {
public:
    int id, target_id, hot_id;
    int round;                                  // 便于区分
    /************************一次性调用*************************/
    PUnit *punit;
    Tactic target;                              // 战术
    PUnit *hot;
    // 常用
    int type, hp, atk, speed, range;
    Pos pos;

    bool can_skill;
    bool can_attack;
    bool besiege;                               // 抵近攻击/包围攻击,需要小队设置
    bool emergency;                             // 有紧急情况,不需要继续别的指令

    /*********************************************************/
    virtual PUnit *nearestEnemy() const;

    /*************************Helpers***************************/
    virtual bool outOfField();                          // 离开战场了
    virtual bool timeToSkill() = 0;                     // 技能释放环境判断
    virtual bool timeToFlee();                          // 是否应该逃窜
    virtual void checkHot();                            // 检查一下热点目标是否有问题

    /**************************Actions**************************/
    // 仅move
    virtual void cdWalk();                              // cd间的躲避步伐
    virtual void fastFlee();                            // 快速逃窜步伐
    virtual void justMove();                            // 一般移动接口

public:
    /**********************************************************/
    // constructor/destructor
    Hero(int _id, int _hot = -1, int _tactic = 0);
    Hero(PUnit *me, PUnit *hot = nullptr, int t_id = 0);

    virtual ~Hero();

    /*************************Loader***************************/
    // 动作集成
    virtual void Emergency();                   // 一般紧急动作接口 (排除:需要逃跑,离开战场,没有攻击对象,同时-不能释放技能且不能进攻)
    virtual void Attack();                      // 一般攻击接口

    // 统一调用接口
    virtual void HeroAct();

#ifdef LOG
    void printAtkInfo() const;
#endif
};



/*****************************Hammerguard*******************************/
class HammerGuard : public Hero {
public:
    // override
    virtual bool timeToSkill() override;

public:
    HammerGuard(int _id, int _hot = -1, int _tactic = 0);
    HammerGuard(PUnit *me, PUnit *hot = nullptr, int t_id = 0);

    virtual void Attack() override;
};



/******************************Berserker********************************/
class Berserker : public Hero {
public:
    // override
    virtual bool timeToSkill() override;

    Berserker(int _id, int _hot = -1, int _tactic = 0);
    Berserker(PUnit *me, PUnit *hot = nullptr, int t_id = 0);

    virtual void Attack() override;
};



/*******************************Master*********************************/
class Master : public Hero {
public:
    // override
    virtual bool timeToSkill() override;
    virtual void fastFlee() override;

    // my methods
    Pos blinkTarget(bool chase = true);                                      // 闪烁位置

    Master(int _id, int _hot = -1, int _tactic = 0);
    Master(PUnit *me, PUnit *hot = nullptr, int t_id = 0);

    virtual void Emergency() override;
    virtual void Attack() override;
};



/******************************Scouter********************************/
class Scouter : public Hero {
public:
    // override
    virtual bool timeToSkill() override;

    // my methods
    Pos observeTarget();                                    // 监视者设置位置

    Scouter(int _id, int _hot = -1, int _tactic = 0);
    Scouter(PUnit *me, PUnit *hot = nullptr, int t_id = 0);

    virtual void Emergency() override;
    virtual void Attack() override;
    virtual void justMove() override;
};



/*#################### MAIN FUNCTION #######################*/
void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {
#ifdef LOG
    logger << endl << endl;
    logger << "====ROUND " << Round << " STARTS====" << endl;
    logger << "@Economy: " << Economy << endl;
    long start = clock();
#endif

    // Create pointers
    console = new Console(map, info, cmd);
    if (Round < 1) {
        initilize();
    }
    commander = new Commander();

    // Hero do actions // todo 时间消耗大户
    commander->TeamAct();

    delete commander;
    delete console;

#ifdef LOG
    logger << ">> Total" << endl;
    stopClock(start);
    logger << endl;
    printSquads();
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
    logger << left << setw(10) << "HOTID";
    logger << left << setw(10) << "BUFF";
    logger << endl;
    // print content
    for (int i = 0; i < units.size(); ++i) {
        Hero *unit = units[i];
        PUnit *ptr = unit->punit;
        // print basic hero info
        logger << left << setw(14) << ptr->name;
        logger << left << setw(5) << ptr->id;
        logger << left << setw(8) << ptr->level;
        logger << left << setw(5) << ptr->hp;
        logger << left << setw(5) << ptr->mp;
        logger << left << setw(5) << ptr->atk;
        logger << left << setw(5) << ptr->def;
        // POS/TAC/HOT
        string p = int2str(ptr->pos.x) + "," + int2str(ptr->pos.y);
        logger << left << setw(10) << p;
        string t = int2str(unit->target.x) + "," + int2str(unit->target.y);
        logger << left << setw(10) << t;
        logger << left << setw(10) << unit->hot_id;
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
}


void printSquads() {
    logger << "@ Squad and Hero info" << endl;
    if (AllSquads.empty()) {
        logger << "!! Get empty squads" << endl;
    }

    // print content
    for (int i = 0; i < SQUAD_N; ++i) {
        // print title
        logger << left << setw(5) << "TYPE";
        logger << left << setw(5) << "ID";
        logger << left << setw(5) << "NO.";
        logger << left << setw(5) << "SITU";
        logger << left << setw(7) << "CNTER";
        logger << left << setw(7) << "BESG";
        logger << left << setw(7) << "HOTID";
        logger << left << setw(7) << "TARGET" << endl;
        logger << left << setw(5) << AllSquads[i]->type;
        logger << left << setw(5) << AllSquads[i]->id;
        logger << left << setw(5) << AllSquads[i]->member_id.size();
        logger << left << setw(5) << AllSquads[i]->situation;
        logger << left << setw(7) << AllSquads[i]->stick_counter;
        logger << left << setw(7) << AllSquads[i]->besiege;
        logger << left << setw(7) << AllSquads[i]->hot_id;
        logger << left << setw(7) << AllSquads[i]->target_id << endl;
        printHeroList(AllSquads[i]->members);
        logger << "-----------" << endl;
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
        int len2
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

    return far_p;
}


// about game
int enemyCamp() {
    if (CAMP == 0) return 1;
    else return 0;
}

void initilize() {
    // 初始化AllSquads
    if (AllSquads.empty()) {
        for (int i = 0; i <= MC_I; ++i) {                  // [0,1]
            MainCarrier *temp = new MainCarrier(i);
            AllSquads.push_back(temp);
        }
        for (int j = MC_I + 1; j <= MD_I; ++j) {                  // [2,5]
            MineDigger *temp = new MineDigger(j);
            AllSquads.push_back(temp);
        }
        for (int k = MD_I + 1; k <= BS_I; ++k) {                  // [6,7]
            BattleScouter *temp = new BattleScouter(k);
            AllSquads.push_back(temp);
        }
    }
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


int rangeRandom(int max_no_inclu, int avoid) {
    srand((unsigned int) Round);
    int to = rand() % max_no_inclu;
    if (to == avoid) {
        to = ++to % max_no_inclu;
    }
    return to;
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


bool canDamage(PUnit *unit, int round) {
    PBuff *dizzy = const_cast<PBuff *>(unit->findBuff("Dizzy"));
    PSkill *attack = const_cast<PSkill *>(unit->findSkill("Attack"));
    PSkill *ham_atk = const_cast<PSkill *>(unit->findSkill("HammerAttack"));

    if ((dizzy && dizzy->timeLeft > round)) {
        return false;
    } else if (attack->cd <= round ||
            (ham_atk && ham_atk->cd <= round)) {
        return true;
    } else {
        return false;
    }
}


bool justBeAttacked(PUnit *test) {
    if (test->findBuff("BeAttacked") &&
        test->findBuff("BeAttacked")->timeLeft == HURT_LAST_TIME)
        return true;
    else
        return false;
}


PUnit *getFriendlyUnit(int id) {
    for (int i = 0; i < cur_friends.size(); ++i) {
        PUnit *u = cur_friends[i];
        if (id == u->id) {
            return u;
        }
    }
    return nullptr;
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

    // cur_friends  vi_enemies
    getUnits();
    // 顺序不能错
    squadSet();
    updateSquad();
}


Commander::~Commander() {
    // PUnit*不能释放!
    releaseVector(cur_friends);
    base = nullptr;
    releaseVector(vi_enemies);
    en_base = nullptr;
    releaseVector(vi_mines);
    releaseVector(vi_monsters);
}

/**************************CON**************************/

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


void Commander::updateSquad() {
    for (int i = 0; i < SQUAD_N; ++i) {
        AllSquads[i]->roundUpdate();
    }
}


void Commander::squadSet() {
    /*
     * 1.优先填充靠前的MC队伍
     * 2.type>0的队伍劣势时,将单位交换给MC队伍,从前往后填充
     * 3.检查MC队伍情况,有占领的分部分人数到MD留守,MC分配新的目标;失守的换个目标
     */
    // 所有英雄id
    vector<int> all;
    for (int i = 0; i < cur_friends.size(); ++i) {
        all.push_back(cur_friends[i]->id);
    }

    // 释放失守小队的id
    for (int j = 0; j < SQUAD_N; ++j) {                 // 从i = 2开始扫描
        if (AllSquads[j]->situation < BAK_MAX) {      // 劣势了
            SquadMembers[j].clear();
        }
    }

    // 所有已指派的英雄id
    vector<int> assigned;
    for (int k = 0; k < SQUAD_N; ++k) {
        vector<int> temp = SquadMembers[k];
        for (int i = 0; i < temp.size(); ++i) {
            assigned.push_back(temp[i]);
        }
    }

    // 求集合差,得redundant
    vector<int> redundant(10);
    sort(all.begin(), all.end());
    sort(assigned.begin(), assigned.end());
    auto it = set_difference(all.begin(), all.end(), assigned.begin(), assigned.end(), redundant.begin());
    redundant.resize((unsigned long) (it - redundant.begin()));

    // 分配redundant到各mc
    int _space = (int) (SINGLE_MC_LIMIT - SquadMembers[0].size());
    if (_space > 0) {
        for (int i = 0; i < _space && !redundant.empty(); ++i) {
            SquadMembers[0].push_back(redundant.back());
            redundant.pop_back();
        }
    }

    // 根据形势判断
    for (int s = 0; s < SQUAD_N; ++s) {
        handle(s, getTotalLevels(s), judgeSituation(s));
    }


    /*
    // toedit 扫描MC小队,视情况变更目标
    for (int t = 0; t <= 1; ++t) {                      // t-index of MC
        if (AllSquads[t]->situation < BAK_MAX) {       // if lost
            // change to backup target
            int _sz = (int) BackupTactics.size();
            srand((unsigned int) Round);
            int new_t = BackupTactics[rand() % _sz];
            if (new_t == SquadTargets[t]) {
                new_t = ++new_t % _sz;
            }
            SquadTargets[t] = new_t;
            AllSquads[t]->resetTacMonitor(StickRounds);
#ifdef TEMP
            logger << ">> ## [cmd] change to BACKUP plan" << endl;
#endif
        } else if (AllSquads[t]->situation > SUP_MIN) {// if occupied
            if (SquadMembers[t].size() < 4) return;
            // left a MD squad
            for (int i = 2; i <= 5; ++i) {              // 扫描所有MD
                if (SquadMembers[i].empty()) {          // 发现空MD
                    if (SquadTargets[t] == 0) {         // 中间矿留一个
                        SquadMembers[i].push_back(SquadMembers[t].back());
                        SquadMembers[t].pop_back();
                    } else {                            // 野矿留两个
                        for (int j = 0; j < SINGLE_MD_LIMIT; ++j) {
                            SquadMembers[i].push_back(SquadMembers[t].back());
                            SquadMembers[t].pop_back();
                        }
                    }
                    SquadTargets[i] = SquadTargets[t];  // 设target
                    AllSquads[i]->resetTacMonitor(0);   // reset
#ifdef TEMP
                    logger << "## [cmd] spare a MD" << endl;
#endif
                    break;                              // 一个MD就够了!
                }
            }
            // and change MC to another superior target
            int _sz = (int) SuperiorTactics.size();
            srand((unsigned int) Round);
            int new_t = SuperiorTactics[rand() % _sz];
            if (new_t == SquadTargets[t]) {
                new_t = ++new_t % _sz;
            }
            SquadTargets[t] = new_t;
            AllSquads[t]->resetTacMonitor(StickRounds);
#ifdef TEMP
            logger << ">> ## [cmd] change to SUPERIOR plan" << endl;
#endif
        }
    }

    // todo 什么时候推基地
    */
}


/**************************HELPERS************************/

int Commander::judgeSituation(int index) {
    AssaultSquad *squad = AllSquads[index];
    if (squad->situation < BAK_MAX) return -1;
    if (squad->situation > SUP_MIN) return 1;
    return 0;
}


void Commander::markTarget(int target) {
    GetBack.push(target);
}


void Commander::callBackupSquad(int needed_n) {
    int left = needed_n;
    for (int i = 0; i < SQUAD_N; ++i) {
        if (i == 1) continue;
        int call = min(left, (int) SquadMembers[i].size());
        moveMembers(i, 1, call);
        left -= call;
        if (left <= 0) return;
    }
    SquadTargets[1] = 7 + CAMP;
}


int Commander::getTotalLevels(int squad) {
    ID_LIST &list = SquadMembers[squad];
    int _sz = (int) list.size();
    int total = _sz;
    for (int i = 0; i < _sz; ++i) {
        total += getFriendlyUnit(list[i])->level;
    }
    return total;
}


void Commander::moveMembers(int from, int to, int n) {
    for (int i = 0; i < n; ++i) {
        SquadMembers[to].push_back(SquadMembers[from].back());
        SquadMembers[from].pop_back();
    }

}


void Commander::gatherAll() {
    // move all to squad 0
    for (int i = 1; i < SQUAD_N; ++i) {
        int _sz = (int) SquadMembers[i].size();
        moveMembers(i, 0, _sz);
    }
}


bool Commander::timeToPush() {
    int _sz = (int) cur_friends.size();
    int total_lvls = _sz;
    for (int i = 0; i < _sz; ++i) {
        total_lvls += cur_friends[i]->level;
    }
    return (total_lvls > LEVEL2);
}


void Commander::pushEnemyCamp() {
    int tac = 7 + CAMP;
    gatherAll();
    SquadTargets[0] = tac;
    AllSquads[0]->resetTacMonitor(StickRounds);
}


void Commander::handle(int squad, int levels, int situ) {
    int phase;
    if (levels < LEVEL1) {
        phase = 0;
    } else if (levels > LEVEL1 && levels < LEVEL2) {
        phase = 1;
    } else {
        phase = 2;
    }

    /*
     * timeToPush() -> push
     * (^2, 0) -> nothing
     * i = 0
     * (0, ^0) -> change target but not members
     * (>0, 1) -> left a MD, change target
     * (>0, -1) -> change target but not members
     * i > 1, phase == 0
     * (0, ^-1) -> nothing
     * (0, -1) -> dismiss and push getback
     */
    int now = SquadTargets[squad];
    int sup_sz = (int) SuperiorTactics.size();
    int bak_sz = (int) BackupTactics.size();

    if (timeToPush()) {
        pushEnemyCamp();
        return;
    }

    if (!GetBack.empty()) {
        int tac = GetBack.front();
        // clear
        while (!GetBack.empty()) {
            GetBack.pop();
        }
        gatherAll();
        SquadTargets[0] = tac;
        AllSquads[0]->resetTacMonitor(StickRounds);
#ifdef TEMP
        logger << ">> [cmd] lost a MD: gather all and fight!" << endl;
#endif
    }

    if (situ == 0
        || (squad > MC_I && situ < 1)) {
        // do nothing
#ifdef TEMP
        logger << ">> [cmd] gluing: do nothing" << endl;
#endif
        return;
    }

    if ((squad == 0 && phase == 0 && situ != 0)
        || (squad > MC_I && phase > 0 && situ == -1)) {
        // change target but not members
        if (situ == 1) {        // superior
            SquadTargets[squad] = SuperiorTactics[rangeRandom(sup_sz, now)];
        } else {                // situ = -1, backup
            SquadTargets[squad] = BackupTactics[rangeRandom(bak_sz, now)];
        }
        AllSquads[squad]->resetTacMonitor(StickRounds);
#ifdef TEMP
        logger << ">> [cmd] too few members: change!" << endl;
#endif
        return;
    }

    if (squad == 0 && phase > 0 && situ == 1) {
        // left a MD and change target (SUP)
        int change_n;       // 留下的md人数
        if (now == 0) {     // 如果是中矿
            change_n = 1;
        } else {
            change_n = 3;
        }
        // .left a MD
        int md_i = 0;
        for (int i = MC_I + 1; i <= MD_I; ++i) {
            if (SquadMembers[i].empty()) {
                moveMembers(squad, i, change_n);
                md_i = i;
                break;
            }
        }
        if (md_i == 0) return;      // md都满了,不太可能 fixme 不鲁棒点
        // .change targets
        SquadTargets[md_i] = SquadTargets[squad];
        SquadTargets[squad] = SuperiorTactics[rangeRandom(sup_sz, now)];
        AllSquads[squad]->resetTacMonitor(StickRounds);
        return;
    }

    if (squad > MC_I && situ == -1) {
        // dismiss, mark a getback
        SquadMembers[squad].clear();
        SquadTargets[squad] = 0;
        AllSquads[squad]->resetTacMonitor(StickRounds);
        return;
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
    Pos base = MILITARY_BASE_POS[CAMP];
    int _sz = (int) cur_friends.size();

    // 召回升级,每回合只召回一个
    if (Economy > CALLBACK_LVLUP) {
        int min_level = BIG_INT;
        int min_i = -1;
        for (int i = 0; i < _sz; ++i) {
            PUnit *u = cur_friends[i];
            if (u->level < min_level && dis2(u->pos, base) > CALLBACK_MIN_DIST2) {
                min_level = u->level;
                min_i = i;
            }
        }
        if (min_i != -1) {
            console->callBackHero(cur_friends[min_i], base + Pos(3, 0));
#ifdef TEMP
            logger << ">> [cmd] hero being called back to LEVEULUP. id=" << min_i << endl;
#endif
        }
    }

    // 召回补血
    if (Economy > CALLBACK_RECV_HP) {
        for (auto i = cur_friends.begin(); i != cur_friends.end(); ++i) {
            if (!(*i)->findBuff("WinOrDie")
                && (*i)->hp < HP_FLEE_ALERT * (*i)->max_hp) {
                console->callBackHero((*i), base + Pos(-3, 0));
            }
        }
    }

    // 召回防守
    int cnt = 0;
    for (int j = 0; j < vi_enemies.size(); ++j) {   // 遍历计数,更节省时间
        Pos p = vi_enemies[j]->pos;
        if (dis2(p, base) < MILITARY_BASE_VIEW) {
            cnt++;
        }
    }
    if (cnt > 0) {
        callBackupSquad(cnt);
    }
}


/*************************Interface**************************/

void Commander::TeamAct() {
    // base
    callBack();
    baseAttack();
    spendMoney();
    // squads
    for (int i = 0; i < SQUAD_N; ++i) {
        AllSquads[i]->SquadCommand();
    }
}



/************************************************************
 * Implementation: class AssaultSquad
 ************************************************************/

/*************************HELPER****************************/

void AssaultSquad::clean() {
    sector_en.clear();
    member_id.clear();
    members.clear();
    hot = nullptr;
}


void AssaultSquad::resetTacMonitor(int _n) {
    stick_counter = _n;
    situation = 0;
}


void AssaultSquad::getAllCmdInfo() {
    member_id = SquadMembers[id];
    target_id = SquadTargets[id];
}


void AssaultSquad::getUnits() {
    Pos tar = TACTICS[target_id];
    UnitFilter filter;
    filter.setAreaFilter(new Circle(tar, battle_range), "a");
    filter.setAvoidFilter("Observer", "a");
    filter.setAvoidFilter("Mine", "w");
    filter.setHpFilter(1, 100000);

    // 根据战术目标,设定打击单位范围
    if (tar != MINE_POS[0]) {
        // 攻击其他矿时还攻击野怪/军事基地
        sector_en = console->enemyUnits(filter);
    } else {
        // 攻击中矿时仅攻击对手
        filter.setCampFilter(enemyCamp());
        sector_en = console->enemyUnits(filter);
    }

    filter.setCampFilter(CAMP);
    sector_f = console->friendlyUnits(filter);

    // 用迭代器遍历并删除指定元素 - 尸体
    for (auto i = sector_en.begin(); i != sector_en.end(); ) {
        if ((*i)->findBuff("Reviving"))
            i = sector_en.erase(i);
        else
            i++;
    }
}


void AssaultSquad::lockHot() {
    /*
     * @优先级:
     * WinOrDie
     * WaitRevive
     * 最弱单位
     */

    // todo 由于队形调整浪费时间,需要一直沿用某一hot对象,直到其死亡或逃逸

    if (sector_en.size() == 0) {
        hot = nullptr;
        hot_id = -1;
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
        if (en->findBuff("WinOrDie")) {
            win_or_die.push_back(en);
        }
        // WaitRevive
        if (en->findBuff("WinOrDie")) {
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
        hot_id = hot->id;
        return;
    }
    if (!wait_revive.empty()) {
        hot = wait_revive[0];
        hot_id = hot->id;
        return;
    }


    // 最弱
    if (index == -1) {
        hot = nullptr;
        hot_id = -1;
    } else {
        hot = sector_en[index];
        hot_id = hot->id;
    }
}


void AssaultSquad::setHeroes() {
    int _sz = (int) member_id.size();
    for (int i = 0; i < _sz; ++i) {
        int hero_id = member_id[i];
        PUnit *unit = getFriendlyUnit(hero_id);
        Hero *hero = nullptr;
        switch (unit->typeId) {
            case 3:
                hero = new HammerGuard(unit, hot, target_id);
                break;
            case 4:
                hero = new Master(unit, hot, target_id);
                break;
            case 5:
                hero = new Berserker(unit, hot, target_id);
                break;
            case 6:
                hero = new Scouter(unit, hot, target_id);
                break;
            default:
#ifdef LOG
                logger << "[ERRO] Friend hero's type not found" << endl;
#endif
                break;
        }
        members.push_back(hero);
    }
}


/*************************Actions****************************/

void AssaultSquad::crossBesiege() {
    if (hot == nullptr || besiege == false)
        return;            // 没人就算了

    const Pos onPosition[] = {Pos(1, 0), Pos(-1, 0), Pos(0, 1), Pos(0, -1),
                              Pos(1, 1), Pos(-1, -1), Pos(1, -1), Pos(-1, 1)};
    Pos target = hot->pos;

    for (int i = 0; i < members.size(); ++i) {
        Pos rightPos = target + onPosition[i];
        PUnit *unit = members[i]->punit;
        members[i]->besiege = true;                 // 先设定besiege标志

        if (unit->pos != rightPos) {
            target = rightPos;                      // warn 下一轮target又会更新,应该不用担心
        }

        // 通过接口执行,避免不必要的误判 warn 不通过console破坏封装
        members[i]->HeroAct();
    }
}


void AssaultSquad::slipAttack() {
    for (int i = 0; i < members.size(); ++i) {
        Hero *h = members[i];
        h->besiege = false;
        h->HeroAct();
    }
}


/*************************Cmd load****************************/

void AssaultSquad::setBesiege() {
    if (members.size() < 3) {
        besiege = false;
    } else {
        besiege = true;
    }

    // set members
    for (int i = 0; i < members.size(); ++i) {
        members[i]->besiege = this->besiege;
    }
}


// public

AssaultSquad::AssaultSquad(int _id) {
    id = _id;
    situation = 0;
    battle_range = BATTLE_RANGE;
    stick_counter = StickRounds;
    clean();

    getAllCmdInfo();        // target, mem_id

    // setting
    stick_counter--;
    setBesiege();
    getUnits();
    lockHot();
    setHeroes();
}


AssaultSquad::~AssaultSquad() {
    clean();
}


/*************************LOADER****************************/

void AssaultSquad::roundUpdate() {
    clean();

    getAllCmdInfo();        // target, mem_id

    // setting
    stick_counter--;
    setOthers();
    setBesiege();
    getUnits();
    lockHot();
    setHeroes();
    evaluateSituation();
}


void AssaultSquad::SquadCommand() {
    if (besiege) {
        crossBesiege();
    } else {
        slipAttack();
    }
}


/************************************************************
 * Implementation: class MainCarrier
 ************************************************************/

MainCarrier::MainCarrier(int _id) : AssaultSquad(_id) {
    type = 0;
    setOthers();
    evaluateSituation();
}


void MainCarrier::setOthers() {
    return;
}


void MainCarrier::evaluateSituation() {
    if (stick_counter > 0) {
        return;
    }   // assert: stick_counter <= 0
    if (member_id.empty()) {
        resetTacMonitor(StickRounds);
    }

    int _sz_f = (int) sector_f.size();
    int _sz_e = (int) sector_en.size();

    // 没有人,negative,扣分
    if (_sz_f == 0) {
        situation -= BIG_INT;
        return;
    }   // assert: _sz_f != 0

    // 占领了,positive,从0开始+1
    if (_sz_e == 0) {
        situation = max(0, ++situation);
        return;
    }

    // 胶着且人数不占优,-1
    if (_sz_e > _sz_f) {
        situation = min(0, --situation);
    }
}


/************************************************************
 * Implementation: class MineDigger
 ************************************************************/

MineDigger::MineDigger(int _id) : AssaultSquad(_id) {
    type = 1;
    setOthers();
    evaluateSituation();
}


void MineDigger::setOthers() {
    type = 1;
    battle_range = BATTLE_RANGE / 4;        // 作战半径减小一半

    PUnit *mine = console->getUnit(target_id + MINE_NUM_SHIFT);
    if (mine == nullptr) {
        mine_energy = BIG_INT;
    } else {
        mine_energy = console->unitArg("energy", "c", mine);
#ifdef TEMP
        logger << ">> mine energy near MD id=" << id << ": " << mine_energy << endl;
#endif
    }
}


void MineDigger::evaluateSituation() {
    if (member_id.empty()) {
        situation = 0;
        return;
    }

    int _sz_f = (int) sector_f.size();
    int _sz_e = (int) sector_en.size();

    // 敌人人数占优或者矿连续没有能量,negative
    if (_sz_e - _sz_f > 1 || mine_energy < 2) {
        situation -= BIG_INT;
        return;
    }

    // 有monster以外的交战,negative
    for (int i = 0; i < sector_en.size(); ++i) {
        if (!sector_en[i]->isWild()) {
            situation = min(0, --situation);
        }
    }
    return;

    // 否则一直保持采矿状态
    situation = 0;

}


/************************************************************
 * Implementation: class BattleScouter
 ************************************************************/

void BattleScouter::lockHot() {
    hot = nullptr;
}


BattleScouter::BattleScouter(int _id) : AssaultSquad(_id) {
    type = 2;
    setOthers();
    evaluateSituation();
}


void BattleScouter::setOthers() {
    return;
}


void BattleScouter::evaluateSituation() {
    if (stick_counter > 0) {
        return;
    }   // assert: stick_counter <= 0
    if (member_id.empty()) {
        resetTacMonitor(StickRounds);
    }

    // 一旦对改点观测完成即撤出 fixme 存在问题,不能把situation设负,会被回收
    for (int i = 0; i < members.size(); ++i) {
        Pos p = members[i]->pos;
        if (dis2(p, TACTICS[target_id]) < SCOUT_OK) {
            situation -= BIG_INT;
            return;
        }
    }

    // 否则,继续
    situation = 0;
}



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

/**************************Helpers**************************/

bool Hero::timeToFlee() {
    // 防止berserker误判
    if (punit->findBuff("WinOrDie"))
        return false;

    // 血量过低
    if (hp < HP_FLEE_ALERT * punit->max_hp) {
        return true;
    }

    return false;
}


bool Hero::outOfField() {
    int dist2 = dis2(pos, target);
    return dist2 > BATTLE_RANGE;
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


/***********************************************************/

Hero::Hero(int _id, int _hot, int _tactic) :
        id(_id), hot_id(_hot), target_id(_tactic){
    punit = getFriendlyUnit(_id);
    target = TACTICS[target_id];
    hot = console->getUnit(hot_id);
    type = punit->typeId;
    hp = punit->hp;
    atk = punit->atk;
    speed = punit->speed;
    range = punit->range;
    pos = punit->pos;

    can_skill = punit->canUseSkill(SKILL_NAME[punit->typeId + 5]);
    can_attack = punit->canUseSkill("Attack");
    besiege = true;
    emergency = false;
}


Hero::Hero(PUnit *me, PUnit *hot, int t_id) :
        punit(me), hot(hot), target_id(t_id) {
    id = punit->id;
    if (hot == nullptr) {
        hot_id = -1;
    } else {
        hot_id = hot->id;
    }

    // 一次性调用
    target = TACTICS[target_id];
    type = punit->typeId;
    hp = punit->hp;
    atk = punit->atk;
    speed = punit->speed;
    range = punit->range;
    pos = punit->pos;

    can_skill = punit->canUseSkill(SKILL_NAME[punit->typeId + 5]);
    can_attack = punit->canUseSkill("Attack");
    emergency = false;
}


Hero::~Hero() {
    punit = nullptr;
    hot = nullptr;
}


/*************************Loader***************************/

void Hero::Emergency() {
    if (timeToFlee()) {
        emergency = true;
        fastFlee();
        return;
    }

    if (outOfField() || hot == nullptr) {
        emergency = true;
        justMove();
        return;
    }

    if (!besiege) {
        if (!can_skill && !can_attack) {
            emergency = true;
            cdWalk();
            return;
        }
    }
}


void Hero::Attack() {
    console->attack(hot, punit);
#ifdef LOG
    printAtkInfo();
#endif
}


void Hero::HeroAct() {
    Emergency();
    if (!emergency) {
        Attack();
    }
}


void Hero::justMove() { // assert: out of field or hot = nullptr
    if (hp < HP_BACK_ALERT * punit->max_hp) {
        console->move(MILITARY_BASE_POS[CAMP], punit);
    } else {
        console->move(target, punit);
#ifdef LOG
        logger << "[mov] move to ";
        logger << target << endl;
#endif
    }
}


#ifdef LOG

void Hero::printAtkInfo() const {
    logger << "[atk] ";
    logger << hot->name << "(" << hot->id << ") ";
    logger << "dist = " << dis2(hot->pos, pos) << endl;
}

#endif

/************************************************************
 * Implementation: class HammerGuard
 ************************************************************/
// protected

bool HammerGuard::timeToSkill() {
    int dist2 = dis2(hot->pos, pos);
    bool under_fire = dist2 < HAMMERATTACK_RANGE;
    if (can_skill && under_fire) {
        return true;
    } else {
        return false;
    }
}


// public

HammerGuard::HammerGuard(int _id, int _hot, int _tactic) : Hero(_id, _hot, _tactic) {}


HammerGuard::HammerGuard(PUnit *me, PUnit *hot, int t_id) : Hero(me, hot, t_id) { }


void HammerGuard::Attack() {
#ifdef LOG
    logger << ">> Hammerguard (" << id << ")" << endl;
#endif

    // 技能攻击
    if (timeToSkill()) {
        console->useSkill("HammerAttack", hot, punit);  // go
#ifdef LOG
        logger << "[skill] HammerAttack at:";
        printAtkInfo();
#endif
    } else {
        // 普通攻击
        Hero::Attack();
    }
}



/************************************************************
 * Implementation: class Berserker
 ************************************************************/
// protected

bool Berserker::timeToSkill() {
    /*
     * 不能SACRIFICE的条件:
     * .技能冷却或
     * .hot单位太远
     * .在敌方可随时攻击的单位射程内
     * .hot本身可以随时反击
     */
    // this必须能sacrifice,且至少下一回合能攻击
    if (!can_skill || punit->findSkill("Attack")->cd > 1) {
        return false;
    }

    // hot必须在攻击范围内
    int dist2 = dis2(hot->pos, pos);
    if (dist2 > range + speed * 5)
        return false;


    // hot必须是落单对象
    int rounds = max((dist2 - range), 0) / speed + 1;
    for (int i = 0; i < vi_enemies.size(); ++i) {
        PUnit *pu = vi_enemies[i];
        if (hot->pos == pu->pos) continue;          // same unit
        if (!canDamage(pu, rounds)) continue;       // no damage

        if (dist2 < pu->range) {                    // 并不落单
            return false;
        }
    }

    // 且hot必须不能产生攻击
    if (canDamage(hot, rounds)) {
        return false;
    }

    // 以上都通过,则可以
#ifdef TEMP
    logger << ">> [BSK] use skill sacrifice. id=" << id << endl;
#endif
    return true;
}


// public
Berserker::Berserker(int _id, int _hot, int _tactic) :
        Hero(_id, _hot, _tactic){ }


Berserker::Berserker(PUnit *me, PUnit *hot, int t_id) :
        Hero(me, hot, t_id) { }


void Berserker::Attack() {
    // Sacrifice中
    if (punit->findBuff("WinOrDie") != nullptr && can_attack) {
        Hero::Attack();
        return;
    }

    if (timeToSkill()) {
        console->useSkill("Sacrifice", hot, punit);
#ifdef LOG
        logger << "[skill] Sacrifce" << endl;
#endif
    } else {
        Hero::Attack();
    }
}



/************************************************************
 * Implementation: class Master
 ************************************************************/
// protected

bool Master::timeToSkill() {
    /*
     * 仅在进攻时进行判断
     */
    int dist2 = dis2(hot->pos, pos);
    return (dist2 > range && dist2 <= range + BLINK_RANGE && hot->hp < atk);
}


void Master::fastFlee() {
    if (can_skill) {
        console->useSkill("Blink", blinkTarget(false), punit);
    } else {
        Hero::fastFlee();
    }

}


Pos Master::blinkTarget(bool chase) {
    // 进攻型
    if (hot == nullptr)
        return Pos(-1, -1);

    int dist2 = dis2(hot->pos, pos);
    if (chase) {
        Pos chase_p = parallelChangePos(pos, hot->pos, dist2 - range / 2, false);
        return chase_p;
    } else {
        Pos flee_p = parallelChangePos(pos, hot->pos, BLINK_RANGE, true);
        return flee_p;
    }
}



// public

Master::Master(int _id, int _hot, int _tactic) :
        Hero(_id, _hot, _tactic){ }


Master::Master(PUnit *me, PUnit *hot, int t_id) :
        Hero(me, hot, t_id) { }


void Master::Emergency() {
    if (Hero::timeToFlee()) {
        emergency = true;
        fastFlee();
        return;
    }

    if (outOfField() || hot == nullptr) {
        emergency = true;
        Hero::justMove();
        return;
    }

    if (!besiege) {
        if (!can_skill && !can_attack) {
            emergency = true;
            cdWalk();
            return;
        }
    }
}


void Master::Attack() {
    // 追赶
    if (timeToSkill()) {
        console->useSkill("Blink", blinkTarget(), punit);
#ifdef LOG
        logger << "[skill] chasing by Blink";
        logger << pos << blinkTarget() << endl;
#endif
    } else {
        Hero::Attack();
    }
}



/************************************************************
 * Implementation: class Scouter
 ************************************************************/
// protected
// toedit 关于释放技能目前设点,在开头更改
bool Scouter::timeToSkill() {
    return observeTarget() != Pos(-1, -1);
}


Pos Scouter::observeTarget() {
    for (int i = 0; i < OBSERVE_POS_N; ++i) {
        int dist2 = dis2(pos, OBSERVE_POS[i]);
        if (dist2 < SET_OBSERVER_RANGE) {
            UnitFilter filter;
            filter.setAreaFilter(new Circle(OBSERVE_POS[i], OBSERVER_VIEW), "a");
            filter.setTypeFilter("Observer", "a");
            vector<PUnit *> observers = console->friendlyUnits(filter);
            if (!observers.empty()) {
                return OBSERVE_POS[i];
            }
        }
    }
    return Pos(-1, -1);
}


// public
Scouter::Scouter(int _id, int _hot, int _tactic) :
        Hero(_id, _hot, _tactic){ }


Scouter::Scouter(PUnit *me, PUnit *hot, int t_id) :
        Hero(me, hot, t_id) { }


void Scouter::Emergency() {
    if (Hero::timeToFlee()) {
        emergency = true;
        Hero::fastFlee();
        return;
    }

    if (outOfField() || hot == nullptr) {
        emergency = true;
        justMove();
        return;
    }

    if (!besiege) {
        if (!can_skill && !can_attack) {
            emergency = true;
            cdWalk();
            return;
        }
    }
}


void Scouter::Attack() {
    Hero::Attack();
}


void Scouter::justMove() {
    Pos ob_p = observeTarget();
    if (ob_p != Pos(-1, -1)) {
        console->useSkill("SetObserver", ob_p, punit);
#ifdef LOG
        logger << ">> [skill] set observer at ";
        logger << ob_p << endl;
#endif
    } else {
        Hero::justMove();
    }
}



/*
[NO TESTS]
Update:
. imple. squadSet()

Fixed bugs:

Non-fixed problems:
. if MD lost a mine, no one is going to get it back
. when too few members, still split!

. members are too separate
. ?do not judge the state properly

. hold camp / destroy camp

In 3 branches: HEAD, develop, origin/dev
 */

