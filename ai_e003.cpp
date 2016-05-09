// 调试开关
#define LOG

#ifdef LOG
#define TEMP
#include <ctime>
#include <fstream>
#include <iomanip>
#include "Pos.h"

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

static const int TAC_TARGETS_N = 10;         // 7个矿+2个基地
static const int MINE_NUM_SHIFT = 5;        // 编号偏移,id = tac_id + shift

static const int HERO_COST[] = {
        NEW_HAMMERGUARD_COST, NEW_MASTER_COST, NEW_BERSERKER_COST, NEW_SCOUTER_COST,
};
static const char *HERO_NAME[] = {"Hammerguard", "Master", "Berserker", "Scouter"};

static const char *ACTIVE_SKILL[] = {"HammerAttack", "Blink", "Sacrifice", "SetObserver"};

static const int OBSERVE_POS_N = 2;


/************************************************************
 * Policy const values
 ************************************************************/
// unchanged values (during the entire game)
static int CAMP = -1;                       // which camp

// base actions
// levelUp
static const double LEVEL_UP_COST = 0.5;    // 升级金钱比例
// buyNewHero
static int BUY_RANK = 42313142;             // 请参考hero_name
// callBack
static const double CALLBACK_RATE = 0.6;    // 召回费的比率
static const int CALLBACK_MIN_DIST2 = 600;  // 召回的必要最小距离
static const int CALLBACK_LVLUP = 2000;     // 召回升级的最小经济水平
static const int CALLBACK_RECV_HP = 2000;   // 召回补血的最小经济水平
// buyLife
static const int BUY_LIFE_ROUNDS = 60;      // 只允许前多少回合买活
static const int BUY_LIFE_ROUNDS_LEFT = 5;  // 复活还剩回合数大于此,可考虑买活
static const int BUY_LIFE_NEARBYS = 1;      // 基地周围有友方单位数大于此,考虑买活

// clearOldInfo()
static const int CLEAN_LIMIT = 6;           // 最多保留回合记录
static const int CLEAN_NUMS = 2;            // 超过最多保留记录后,一次清理数据组数

// Hero
static const double HP_FLEE_ALERT = 0.25;         // 血量预警百分比
static const double HP_BACK_ALERT = 0.45;         // 回基地补血百分比

static const int BATTLE_RANGE = 100;                // 战斗范围
static const int CROWD_RANGE = 4;               // 集结范围
static const int BATTLE_FIELD = 4 * BATTLE_RANGE;   // 战区范围

// Squad
static const int MEMBER_LIMIT = 4;          // 小队最多人数

/************************************************************
 * Real-time sharing values
 ************************************************************/
static Console *console = nullptr;
static Commander *commander = nullptr;
static int Round = -1;
static int Economy;                             // 经济

vector<PUnit *> cur_friends;                    // 当前友军英雄
// 我的基地
vector<PUnit *> vi_enemies;                     // 当前可见敌人英雄,不含野怪
vector<PUnit *> vi_mines;                       // 可见矿(矿的位置是默认的,但状态需要可见才能读取)
vector<PUnit *> vi_monsters;                    // 可见野怪


/************************************************************
 * Storage data
 ************************************************************/
/* 战术核心 */
// 7个矿顺序依次是 0-中矿,1-8点,2-10点,3-4点,4-2点,5-西北,6-东南
/*
 *   2 ——————————— 4
 *   |             |
 *   |      0      |
 *   1 ——————————— 3
 */
static const int SUP_MIN = 50;                  // 优势判分标准
static const int BAK_MAX = -50;                 // 劣势判分标准
static vector<int> SuperiorTactics = {0, 1, 2, 3, 4}; // 优势战术
static vector<int> BackupTactics = {5, 6};      // 备选战术 player1
static queue<int> GetBack;                      // 需要夺回的地点:MD刚失守的,或计划夺取的

// 战术延时
static const int StickRounds = 50;                    // 初始保留战术的时间
static int TCounter = StickRounds;              // 设置战术倒计时

// 战局判断
static const int LEVEL1 = 10;                   // 判断第一界点,低于此不分队
static const int LEVEL2 = 20;                   // 判断第二界点,高于此推基地
static int TargetSitu[TAC_TARGETS_N] = {};        // 占据判断
static int TargetCounter[TAC_TARGETS_N] = {};   // 战术计时
static int FirstWave[TAC_TARGETS_N] = {};       // 第一波: 1-是, 0-否
static vector<int> BackupStore;                 // 临时储存

typedef vector<int> ID_LIST;
// Squad settings
static const int SQUAD_N = 2;                   // 小队数量
static int SquadTargets[SQUAD_N] = {};          // 小队战术id
static vector<ID_LIST> SquadMembers(8, vector<int>(0));            // 各小队成员安排

// Squad list
static const int SINGLE_MC_LIMIT = 8;           // MC人数max限制
static const int MC_I = 1;
static const int MD_I = 5;
static const int BS_I = 7;
static const int SCOUT_OK = 5;                  // 侦查完成的最大距离

// need initilization
static Tactic TACTICS[TAC_TARGETS_N] = {
        MINE_POS[0], MINE_POS[1], MINE_POS[2], MINE_POS[3],
        MINE_POS[4], MINE_POS[5], MINE_POS[6],
        MILITARY_BASE_POS[0], MILITARY_BASE_POS[1],
        Pos(),/*第一波*/
};
static vector<AssaultSquad *> AllSquads;        // 所有小队,默认初始化后需要调整参数
static Pos ASSEMBLY_POINTS[TAC_TARGETS_N] = {   // 第一波的集散点,需要初始化修改
        TACTICS[0], Pos(44, 75), Pos(44, 75), Pos(105, 75),
        Pos(105, 75), Pos(32, 128), Pos(117, 22),
        TACTICS[7], TACTICS[8]
};
static Pos OBSERVE_POS[OBSERVE_POS_N] = {       // 设Observer的点,需要初始化修改
        MINE_POS[0], Pos(20, 28)
};


/*################# Assistant functions ####################*/
// ================ Log Related ====================
#ifdef LOG
void stopClock(long start);

static ofstream logger("__0.txt");

void printUnit(vector<PUnit *> units);

void printHeroList(vector<Hero> units);

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
bool canDamage(PUnit *unit, int round, Pos pos);           // 单位当前是否可以输出伤害
PUnit *getFriendlyUnit(int id);                     // 节省时间

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
    void scanMines();                                 // 扫描矿
    // 封装接口,根据squad::situation返回
    void markTarget(int target);                    // 封装接口,标记待(全体)夺取,放入GetBack
    // 获得小队总等级
    int getPhase();                                 // 根据目前等级反馈战斗阶段
    void changeTactic(int a, int b);              // 改变战术
    void moveMembers(int from, int to, int n);      // 在小队间移动成员
    void gatherAll();                               // 聚集所有小队到0号
    bool timeToPush();                              // 推基地时机判断
    void pushEnemyCamp();                           // 推基地

    // handle functions
    void handle(int phase);   // 处理动作

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
    PUnit *leader;                      // 领队
    vector<PUnit *> sector_en;          // 区域敌人
    vector<PUnit *> sector_f;           // 区域朋友

    int hot_id;                         // 热点对象id
    PUnit *hot;                         // 热点对象指针

    /*************************HELPER****************************/
    virtual void clean();               // 静态对象,每回合开始需要重置一些东西
    virtual void resetTacMonitor(int _n = StickRounds);   // 重设计数器/判断器
    virtual bool firstWave();           // 判断第一波的接口
    virtual Pos crowdedPos();           // 小队紧凑调整的目标设置

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
    bool emergent;                              // 有紧急情况,不需要继续别的指令
    Pos formation;                              // 队形调整的推荐目标

    /*********************************************************/
    virtual PUnit *nearestEnemy() const;

    /*************************Helpers***************************/
    virtual bool outOfField();                          // 离开战场了
    virtual bool timeToSkill() = 0;                     // 技能释放环境判断
    virtual bool timeToFlee();                          // 是否应该逃窜
    virtual void checkHot();                            // 检查一下热点目标是否有问题
    // 返回一个推荐的调整队形的位置

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

    virtual void justMove() override;
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

    // Hero do actions
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
    for (int i = 0; i <= 1; ++i) {
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
    double len = sqrt(len2 * 1.0) - 2;  // 防止计算误差
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
    // 初始化TACTICS
    TACTICS[9] = (CAMP == 0) ? Pos(65, 63) : Pos(89, 86);

    // 初始化OBSERVE_POS
    OBSERVE_POS[1] = (CAMP == 0) ? Pos(20, 28) : Pos(123, 128);

    // 初始化ASSEMBLY_POINTS
    ASSEMBLY_POINTS[0] = (CAMP == 0) ? Pos(65, 63) : Pos(89, 86);

    // 初始化FirstWave
    FirstWave[0] = 1;

    // 初始化AllSquads
    if (AllSquads.empty()) {
        for (int i = 0; i <= MC_I; ++i) {                  // [0,1]
            MainCarrier *temp = new MainCarrier(i);
            AllSquads.push_back(temp);
        }
//        for (int j = MC_I + 1; j <= MD_I; ++j) {                  // [2,5]
//            MineDigger *temp = new MineDigger(j);
//            AllSquads.push_back(temp);
//        }
//        for (int k = MD_I + 1; k <= BS_I; ++k) {                  // [6,7]
//            BattleScouter *temp = new BattleScouter(k);
//            AllSquads.push_back(temp);
//        }
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


bool canDamage(PUnit *unit, int round, Pos pos) {
    PBuff *dizzy = const_cast<PBuff *>(unit->findBuff("Dizzy"));
    PSkill *attack = const_cast<PSkill *>(unit->findSkill("Attack"));
    PSkill *ham_atk = const_cast<PSkill *>(unit->findSkill("HammerAttack"));
    int dist2 = dis2(pos, unit->pos);

    if (dizzy && dizzy->timeLeft >= round) {
        return false;
    }

    if ((attack->cd <= round && unit->range >= dist2)
        || (ham_atk && ham_atk->cd <= round && HAMMERATTACK_RANGE >= dist2)) {
        return true;
    } else {
        return false;
    }
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

    // 顺序不能错
    getUnits();
    scanMines();
    squadSet();
    updateSquad();
}


Commander::~Commander() {
    // PUnit*不能释放!
    releaseVector(cur_friends);
    releaseVector(vi_enemies);
    releaseVector(vi_mines);
    releaseVector(vi_monsters);
}

/**************************CON**************************/

void Commander::getUnits() {
    // friends
    UnitFilter f1;
    f1.setCampFilter(CAMP);
    f1.setAvoidFilter("MilitaryBase", "w");
    f1.setAvoidFilter("Observer", "a");
    cur_friends = console->friendlyUnits(f1);
    // base
    console->getMilitaryBase();

    // enemies 没有cleanAll()
    UnitFilter f2;
    f2.setCampFilter(enemyCamp());
    f2.setAvoidFilter("MilitaryBase", "w");
    f2.setAvoidFilter("Observer", "a");
    vi_enemies = console->enemyUnits(f2);

    // vi_mines
    UnitFilter f4;
    f4.setTypeFilter("Mine", "w");
    vi_mines = console->enemyUnits(f4);
    // vi_monsters
    UnitFilter f5;
    f5.cleanAll();
    f5.setTypeFilter("Dragon", "w");
    f5.setTypeFilter("Roshan", "a");
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
    for (int i = 0; i <= 1; ++i) {
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

    // 按id平均分配到两个MC
    SquadMembers[0].clear();
    SquadMembers[1].clear();
    int _sz = (int) all.size();
    for (int j = 0; j < _sz; ++j) {
        if (j < _sz / 2) {
            SquadMembers[0].push_back(all[j]);
        } else {
            SquadMembers[1].push_back(all[j]);
        }
    }

    // handle
    handle(getPhase());
}


/**************************HELPERS************************/

void Commander::scanMines() {
    int _sz = (int) vi_enemies.size();
    int _szf = (int) cur_friends.size();
    /* special target */
    // camp - 1:交战, 2:无交战
    int camp_id = 7 + CAMP;
    Pos camp_p = TACTICS[camp_id];
    bool has_danger = false;
    for (int i = 0; i < _sz; ++i) {
        Pos en_p = vi_enemies[i]->pos;
        int dist2 = dis2(camp_p, en_p);
        // warn 一有人就召回
        if (dist2 < 16 * BATTLE_RANGE) {
            has_danger = true;
            break;
        }
    }
    if (!has_danger) {
        TargetSitu[camp_id] = 2;
    } else {
        TargetSitu[camp_id] = 1;
    }

    // enemy camp - 0:无观察, 1:交战
    int en_camp_id = 7 + enemyCamp();
    Pos en_camp_p = TACTICS[en_camp_id];
    bool no_one = true;
    for (int j = 0; j < _szf; ++j) {
        Pos p = cur_friends[j]->pos;
        int dist2 = dis2(en_camp_p, p);
        if (dist2 < 4 * BATTLE_RANGE) {
            no_one = false;
            break;
        }
    }
    if (no_one) {   // 没人,开始计时
        TargetSitu[en_camp_id] = 0;
        TargetCounter[en_camp_id]++;
    } else {
        TargetSitu[en_camp_id] = 1;
        TargetCounter[en_camp_id] = 0;
    }

    // mines
    for (int m = 0; m < MINE_NUM; ++m) {
        int cnt_f = 0;
        int cnt_en = 0;
        for (int k = 0; k < _sz; ++k) {
            Pos en_p = vi_enemies[k]->pos;
            int dist2 = dis2(TACTICS[m], en_p);
            if (dist2 < BATTLE_RANGE) {
                cnt_en++;
            }
        }
        for (int l = 0; l < _szf; ++l) {
            Pos p = cur_friends[l]->pos;
            int dist2 = dis2(TACTICS[m], p);
            if (dist2 < BATTLE_RANGE) {
                cnt_f++;
            }
        }
        // set counter and situation
        // center - -1:失守, 1:交战, 2:占据
        if (m == 0) {
            if (cnt_f == 0 && TCounter < 0 && TargetSitu[m] != 2) {
                TargetSitu[m] = -1;
                TargetCounter[m] = 0;
            } else if (cnt_f != 0 && cnt_en == 0) {
                TargetSitu[m] = 2;
                TargetCounter[m]++;
            } else {
                TargetSitu[m] = 1;
                TargetCounter[m] = 0;
            }
        } else {
            // others - -1:与非野单位交战, 1:打野采矿状态
            if (cnt_en > cnt_f) {
                TargetSitu[m] = -1;
                TargetCounter[m] = 0;
            } else {
                TargetSitu[m] = 1;
                TargetCounter[m]++;
            }
        }
        TCounter--;
    }

#ifdef LOG
    logger << "@Target Situation:" << endl;
    logger << "center: " << TargetSitu[0] << endl;
    logger << "m1: " << TargetSitu[1] << endl;
    logger << "m3: " << TargetSitu[3] << endl;
    logger << "base: " << TargetSitu[camp_id] << endl;
    logger << "en_base: " << TargetSitu[en_camp_id] << endl << endl;
#endif

}


void Commander::markTarget(int target) {
    GetBack.push(target);
}


int Commander::getPhase() {
    int levels = (int) cur_friends.size();
    for (int i = 0; i < cur_friends.size(); ++i) {
        levels += cur_friends[i]->level;
    }

    if (levels < LEVEL1) {
        return 1;
    } else if (levels >= LEVEL1 && levels < LEVEL2) {
        return 2;
    } else {
        return 3;
    }
}


void Commander::changeTactic(int a, int b) {
    int now0 = SquadTargets[0];
    int now1 = SquadTargets[1];
    // update first wave
    for (int i = 0; i < TAC_TARGETS_N; ++i) {
        if (i == a && now0 != a) {
            FirstWave[i] = 1;
        } else if (i == b && now1 != b) {
            FirstWave[i] = 1;
        } else {
            FirstWave[i] = 0;
        }
    }

    SquadTargets[0] = a;
    TargetCounter[0] = 0;
    SquadTargets[1] = b;
    TargetCounter[1] = 0;
    TCounter = StickRounds;
}


void Commander::moveMembers(int from, int to, int n) {
    int move_n = min(n, (int) SquadMembers[from].size());
    for (int i = 0; i < move_n; ++i) {
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
    int tac = 7 + enemyCamp();
    gatherAll();
    SquadTargets[0] = tac;
    AllSquads[0]->resetTacMonitor(StickRounds);
}


void Commander::handle(int phase) {
    // todo 需要整理
    int backup1 = (CAMP == 0) ? 1 : 4;      // 最近的圈矿
    int backup2 = (CAMP == 0) ? 2 : 3;      // 第三近的圈矿
    int backup3 = (CAMP == 0) ? 3 : 2;      // 次近的圈矿
    int backup4 = (CAMP == 0) ? 4 : 1;      // 第四近的圈矿
    int backups[] = {0, backup1, backup2, backup3, backup4, 5, 6};
    int situations[] = {
            TargetSitu[0], TargetSitu[backup1], TargetSitu[backup3], TargetSitu[backup4],
            TargetSitu[5], TargetSitu[6]
    };
    int target0 = SquadTargets[0];
    int target1 = SquadTargets[1];

    // 开局
    if (Round < 24) {
        SquadTargets[0] = 9;
        SquadTargets[1] = 9;
        return;
    }
    if (Round < 3 * StickRounds) {
        SquadTargets[0] = 0;
        SquadTargets[1] = 0;
        return;
    }

    // 0
    if (target0 == 0 && target1 == 0) {
        int situ = TargetSitu[0];
        if (situ == -1) {           // 失守
            if (phase == 1) {
                changeTactic(backup1, backup1);
            } else if (phase == 2) {
                changeTactic(backup1, backup2);
            }
        } else if (situ == 2) {     // 暂时占据
            if (TargetCounter[0] > SUP_MIN)  {      // 长时间占据
                changeTactic(0, backup1);
#ifdef TEMP
                logger << ">> [handle] superior, change to 0,b1" << endl;
#endif
            }
        }
    }

    // back1
    if (target0 == backup1 && target1 == backup1) {
        if (phase == 2) {
            changeTactic(backup1, backup2);
        }
        if (TargetSitu[backup1] == -1) {
            changeTactic(backup2, backup2);
        }
    }

    // back2
    if (target0 == backup2 && target1 == backup2) {
        if (phase == 2) {
            changeTactic(backup1, backup2);
        }
        if (TargetSitu[backup2] == -1) {
            changeTactic(backup1, backup1);
        }
    }

    // back5
    if (target0 == 5 && target1 == 5) {
        int situ = TargetSitu[5];
        if (situ == -1) {
            changeTactic(6, 6);
        } else if (phase == 2) {
            changeTactic(5, 6);
        }
    }

    // back1 + back2
    if (target0 == backup1 && target1 == backup2) {
        int situ1 = TargetSitu[backup1];
        int situ2 = TargetSitu[backup2];
        if (situ1 == -1 && situ2 != -1) {
            changeTactic(backup3, backup2);
        } else if (situ1 != -1 && situ2 == -1) {
            changeTactic(backup1, backup3);
        } else if (situ1 == -1 && situ2 == -1) {
            changeTactic(0, 0);
        }
    }

    // 0 + back1
    if (target0 == 0 && target1 == backup1) {
        int situ0 = TargetSitu[0];
        int situ1 = TargetSitu[backup1];
        if (situ0 == 1) {
            changeTactic(0, 0);
#ifdef TEMP
            logger << ">> [handle] danger on 0, change to 0,0" << endl;
#endif
        }
        if(situ1 == -1) {
            changeTactic(0, backup2);
        }
    }



    /********************************************************************/
    // camp
    int camp_id = 7 + CAMP;
    PUnit *base = const_cast<PUnit *>(console->getMilitaryBase());
    int situ_camp = TargetSitu[camp_id];
    if (target0 == camp_id && target1 == camp_id) {
        if (situ_camp == 0 || situ_camp == 2) {
            int t0 = BackupStore[0];
            int t1 = BackupStore[1];
            changeTactic(t0, t1);
            BackupStore.clear();
        }
        return;
    }

    // enemy camp
    int en_camp_id = 7 + enemyCamp();
    int situ_en_camp = TargetSitu[en_camp_id];
    int now_cnt = TargetCounter[en_camp_id];
    if (target0 == en_camp_id && target1 == en_camp_id) {
        if (situ_en_camp == 0 && now_cnt < StickRounds / 2) {
            changeTactic(0, 0);
        }
        // todo 可以考虑风筝召回单位的对手
    }

    /********************************************************************/
    // back to camp
    if (situ_camp == 1) {
        bool push = false;
        // 如果对手基地相对血量较低,不如直接攻击
        if (target0 == en_camp_id && target1 == en_camp_id) {
            PUnit *en_base = console->getUnit(enemyCamp() + 3);
            if (en_base && en_base->hp < base->hp) {
                push = true;
            }
        }
        // backup present tactic
        if (!push) {
            BackupStore.push_back(target0);
            BackupStore.push_back(target1);
            changeTactic(camp_id, camp_id);
        }
        return;
    }

    // push enemy camp
    if (phase == 3 && now_cnt > StickRounds) {
        changeTactic(en_camp_id, en_camp_id);
    }

#ifdef LOG
    logger << "@Target:" << endl;
    logger << "squad0: " << SquadTargets[0] << endl;
    logger << "squad1: " << SquadTargets[1] << endl;
#endif


}


/*************************Base actions**************************/

void Commander::baseAttack() {
    Pos our_base = MILITARY_BASE_POS[CAMP];
    UnitFilter filter;
    filter.setAreaFilter(new Circle(our_base, MILITARY_BASE_RANGE), "w");
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
    if (Round < 20) {
        return;
    }

    UnitFilter filter;
    filter.setAreaFilter(new Circle(MILITARY_BASE_POS[CAMP], LEVELUP_RANGE), "a");
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
    if (SquadTargets[0] != 0 || SquadTargets[1] != 0 || Round > BUY_LIFE_ROUNDS)
        return;

    // base周边有朋友单位的,且reviving单位还剩x个回合以上的,复活
    int nearby = 0;
    vector<PUnit *> to_revive;
    to_revive.clear();
    Pos camp = MILITARY_BASE_POS[CAMP];
    int cost = 0;
    for (int i = 0; i < cur_friends.size(); ++i) {
        PUnit *unit = cur_friends[i];
        PBuff *revive = const_cast<PBuff *>(unit->findBuff("Reviving"));
        // nearby units (not dead)
        int dist2 = dis2(unit->pos, camp);
        if (dist2 < BATTLE_FIELD && !revive && unit->hp > unit->max_hp * HP_BACK_ALERT) {
            nearby++;
        }
        // possible reviving units
        if (revive && revive->timeLeft >= BUY_LIFE_ROUNDS_LEFT) {
            cost += console->buyBackCost(unit->level);
            to_revive.push_back(unit);
        }
        if (cost > Economy) {
            cost -= console->buyBackCost(unit->level);
            to_revive.pop_back();
        }
    }
#ifdef TEMP
        logger << ">> [buylife] nearbys = " << nearby << endl;
        logger << ">> [buylife] torevive.size = " << to_revive.size() << endl;
#endif

    // sum up
    if (nearby >= BUY_LIFE_NEARBYS) {
        for (int i = 0; i < to_revive.size(); ++i) {
            PUnit *unit = to_revive[i];
            console->buyBackHero(unit);
#ifdef TEMP
            logger << ">> [buylife] buy life of " << unit->name;
            logger << " id = " << unit->id << endl;
#endif
        }
    }
}


void Commander::spendMoney() {
    // todo 还没有加买活,策略不佳
    buyNewHero();
    levelUp();
    buyLife();
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
}


/*************************Interface**************************/

void Commander::TeamAct() {
    // base
    callBack();
    baseAttack();
    spendMoney();
    // squads
    for (int i = 0; i <= 1; ++i) {
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


bool AssaultSquad::firstWave() {
    return FirstWave[target_id] == 1;
}


Pos AssaultSquad::crowdedPos() {
    Pos recommand;
    for (int i = 0; i < members.size(); ++i) {
        Hero *hero = members[i];
        Pos p = hero->pos;
        int dist2 = dis2(p, TACTICS[target_id]);
        if (dist2 > battle_range) continue;

        if (hero->type == 4) {          // master
            recommand = p;
            break;
        }
    }

    return recommand;           // warn 可能为-1,-1
}


void AssaultSquad::getAllCmdInfo() {
    member_id = SquadMembers[id];
    target_id = SquadTargets[id];
}


void AssaultSquad::getUnits() {
    Pos tar = TACTICS[target_id];
    UnitFilter filter;
    filter.setAreaFilter(new Circle(tar, battle_range), "w");
    filter.setAvoidFilter("Observer", "w");
    filter.setAvoidFilter("Mine", "a");
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
    // 攻击敌方基地时仅攻击塔
    if (target_id == 7 + enemyCamp()) {
        hot_id = 3 + enemyCamp();
        hot = console->getUnit(hot_id);
        return;
    }

    // 如果没有敌人攻击Observer
    if (sector_en.size() == 0) {
        UnitFilter filter;
        filter.setAreaFilter(new Circle(TACTICS[target_id], battle_range), "w");
        filter.setTypeFilter("Observer", "w");
        filter.setAvoidFilter("Mine", "w");
        vector<PUnit *> observers = console->enemyUnits(filter);
        if (!observers.empty()) {
            hot = observers[0];
            return;
        } else {
            hot = nullptr;
            hot_id = -1;
            return;
        }
    }

//    vector<PUnit *> win_or_die;
//    vector<PUnit *> wait_revive;

    // 寻找最弱单位
    int index = -1;
    double min = 1.0 * BIG_INT;
    // 特殊buff
    int _sz = (int) sector_en.size();
    for (int i = 0; i < _sz; ++i) {
//        // WinOrDie
//        if (en->findBuff("WinOrDie")) {
//            win_or_die.push_back(en);
//        }
//        // WaitRevive
//        if (en->findBuff("WinOrDie")) {
//            wait_revive.push_back(en);
//        }

        // 最弱
        double score = unitDefScore(sector_en[i]);
        if (score < min) {
            index = i;
            min = score;
        }
    }

    /* 结算 */
//    // 特殊buff
//    if (!win_or_die.empty()) {
//        hot = win_or_die[0];
//        hot_id = hot->id;
//        return;
//    }
//    if (!wait_revive.empty()) {
//        hot = wait_revive[0];
//        hot_id = hot->id;
//        return;
//    }


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
    leader = nullptr;
    for (int i = 0; i < _sz; ++i) {
        int hero_id = member_id[i];
        PUnit *unit = getFriendlyUnit(hero_id);
        PBuff *revive = const_cast<PBuff *>(unit->findBuff("Reviving"));
        Hero *hero = nullptr;
        switch (unit->typeId) {
            case 3:
                hero = new HammerGuard(unit, hot, target_id);
                break;
            case 4:
                hero = new Master(unit, hot, target_id);
                if (!revive) leader = unit;
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
    if (leader == nullptr && !members.empty()) {
        leader = members[0]->punit;
    }
}


void AssaultSquad::crossBesiege() {
    // set target for non-leader
    for (int j = 0; j < members.size(); ++j) {
        Hero *hero = members[j];
        int dist2leader = dis2(leader->pos, hero->pos);
        if (hero->id == leader->id) {
            continue;
        } else if (dist2leader < BATTLE_FIELD) {
            hero->target = leader->pos;
        }
    }

    if (hot == nullptr || besiege == false) {
        for (int i = 0; i < members.size(); ++i) {
            members[i]->HeroAct();
        }
        return;            // 没人就算了
    }

    const Pos onPosition[] = {Pos(1, 0), Pos(-1, 0), Pos(0, 1), Pos(0, -1),
                              Pos(1, 1), Pos(-1, -1), Pos(1, -1), Pos(-1, 1)};
    Pos target = hot->pos;

    for (int i = 0; i < members.size(); ++i) {
        Pos rightPos = target + onPosition[i];
        PUnit *unit = members[i]->punit;
        members[i]->besiege = true;                 // 先设定besiege标志

        if (unit->pos != rightPos) {
            members[i]->target = rightPos;                      // warn 下一轮target又会更新,应该不用担心
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
    if (sector_f.size() < 3) {
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
    // battle range
    if (target_id == 7 + CAMP) {
        battle_range = 4 * BATTLE_FIELD;
    } else {
        battle_range = BATTLE_RANGE;
    }
    stick_counter = StickRounds;
    clean();

    getAllCmdInfo();        // target, mem_id

    // setting
    stick_counter--;
    getUnits();
    lockHot();
    setHeroes();
    setBesiege();
}


AssaultSquad::~AssaultSquad() {
    clean();
}


/*************************LOADER****************************/

void AssaultSquad::roundUpdate() {
    clean();

    getAllCmdInfo();        // target, mem_id

    // setting
//    stick_counter--;
    setOthers();
    getUnits();
    lockHot();
    setHeroes();
    setBesiege();
//    evaluateSituation();
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
    if (_sz_f == 0 || (_sz_f == 1 && target_id == (enemyCamp() + 7))) {
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
    battle_range = BATTLE_RANGE;        // 作战半径减小一半

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
    if (_sz_e - _sz_f > 1 || _sz_f == 0) {
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

    int min_dist = BIG_INT;
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

    // 区域没有友军
    int dist2target = dis2(pos, TACTICS[target_id]);
    if (dist2target < BATTLE_FIELD && target_id == 0) {
        int near_friends = 0;
        for (int i = 0; i < cur_friends.size(); ++i) {
            PUnit *unit = cur_friends[i];
            if (unit->id == id) continue;
            int dist2 = dis2(pos, unit->pos);
            if (dist2 < BATTLE_RANGE) {
                near_friends++;
            }
        }

        int near_enemies = 0;
        for (int j = 0; j < vi_enemies.size(); ++j) {
            PUnit *unit = vi_enemies[j];
            int dist2 = dis2(pos, unit->pos);
            if (dist2 < BATTLE_RANGE) {
                near_enemies++;
            }
        }
        int near_monsters = 0;
        for (int k = 0; k < vi_monsters.size(); ++k) {
            PUnit *monster = vi_monsters[k];
            int dist2 = dis2(monster->pos, TACTICS[target_id]);
            if (!vi_monsters[k]->findBuff("Reviving") && dist2 < BATTLE_FIELD) {
                near_monsters++;
            }
        }

        if (near_friends <= 0
            && (near_enemies >= 2 || near_monsters > 0)) {
            return true;
        }
    }

    return false;
}


bool Hero::outOfField() {
    int dist2 = dis2(pos, target);
    return dist2 > BATTLE_FIELD;
}


void Hero::checkHot() {
    // warn 功能已覆盖lockHot()方法
    int dist2 = dis2(pos, hot->pos);
    if (dist2 > speed) {
        hot = nearestEnemy();
        hot_id = hot->id;
    } else {
        return;
    }
}


/**************************Actions**************************/

void Hero::cdWalk() {       // toedit 主要策略点
    Pos walkto;
    if (formation != Pos()) {
        walkto = formation;
    } else {
        PUnit *nearest = nearestEnemy();
        if (nearest == nullptr)
            return;

        Pos ref_p = nearest->pos;               // position of reference
        // 撤离的距离为保持两者间距一个speed
        Pos far_p = parallelChangePos(pos, ref_p, speed, true);
        walkto = far_p;
#ifdef TEMP
        logger << ">> [cdwalk] cdwalk to ";
        logger << walkto << endl;
#endif
    }

    console->move(target, punit);        // go
}


void Hero::fastFlee() {
//    PUnit *nearest = nearestEnemy();
//    if (nearest == nullptr) {
//        console->move(MILITARY_BASE_POS[CAMP], punit);           // go
//        return;
//    }
//
//    Pos ref = nearestEnemy()->pos;
//    Pos far_p = parallelChangePos(pos, ref, speed, true);
//    console->move(far_p, punit);                    // go
//#ifdef LOG
//    logger << "[move] flee to ";
//    logger << far_p << endl;
//#endif
    console->move(MILITARY_BASE_POS[CAMP], punit);
}


/***********************************************************/

Hero::Hero(int _id, int _hot, int _tactic) :
        id(_id), hot_id(_hot), target_id(_tactic) {
    punit = getFriendlyUnit(_id);
    target = TACTICS[target_id];
    hot = console->getUnit(hot_id);
    type = punit->typeId;
    hp = punit->hp;
    atk = punit->atk;
    speed = punit->speed;
    range = punit->range;
    pos = punit->pos;

    can_skill = punit->canUseSkill(ACTIVE_SKILL[punit->typeId - 3]);
    can_attack = punit->canUseSkill("Attack");
    besiege = true;
    emergent = false;
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

    can_skill = punit->canUseSkill(ACTIVE_SKILL[punit->typeId - 3]);
    can_attack = punit->canUseSkill("Attack");
    emergent = false;
}


Hero::~Hero() {
    punit = nullptr;
    hot = nullptr;
}


/*************************Loader***************************/

void Hero::Emergency() {
    if (timeToFlee()) {
        emergent = true;
        fastFlee();
        return;
    }

    if (outOfField() || hot == nullptr) {
        emergent = true;
        justMove();
        return;
    }

    if (besiege) {
        justMove();
    } else {
        if (!can_skill && !can_attack) {
            emergent = true;
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
    if (!emergent) {
        Attack();
    }
}


void Hero::justMove() {     // assert: out of field or hot = nullptr
    if (hp < HP_BACK_ALERT * punit->max_hp) {
        console->move(MILITARY_BASE_POS[CAMP], punit);
#ifdef LOG
        logger << "[mov] recover hp to base" << endl;
#endif
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
    if (can_skill && under_fire && !hot->isBase()) {
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
    if (dist2 > speed) {
        target = hot->pos;
        return false;
    }

    // 不能被其他单位攻击
    for (int i = 0; i < vi_enemies.size(); ++i) {
        PUnit *pu = vi_enemies[i];
        if (canDamage(pu, 1, pos)) {     // if can damage
            return false;
        }
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
    if (hot == nullptr || !can_skill)
        return Pos(-1, -1);

    int dist2 = dis2(hot->pos, pos);
    if (chase) {
        Pos chase_p = parallelChangePos(pos, hot->pos, dist2 - range / 2, false);
        return chase_p;
    } else {
        PUnit *nearest = nearestEnemy();
        if (nearest == nullptr) {
            return Pos();
        } else {
            Pos flee_p = parallelChangePos(pos, nearest->pos, BLINK_RANGE, true);
            return flee_p;
        }
    }
}



// public

Master::Master(int _id, int _hot, int _tactic) :
        Hero(_id, _hot, _tactic){ }


Master::Master(PUnit *me, PUnit *hot, int t_id) :
        Hero(me, hot, t_id) { }


void Master::justMove() {   // assert: out of field or hot = nullptr
    if (hp < HP_BACK_ALERT * punit->max_hp) {
        console->move(MILITARY_BASE_POS[CAMP], punit);
    } else if (can_skill && outOfField()) {
        Pos p = parallelChangePos(pos, target, BLINK_RANGE, false);
        console->useSkill("Blink", p, punit);
#ifdef LOG
        logger << "[skill] blink move to ";
        logger << p << endl;
#endif
    } else {
        console->move(target, punit);
#ifdef LOG
        logger << "[mov] move to ";
        logger << target << endl;
#endif
    }
}


void Master::Emergency() {
    if (Hero::timeToFlee()) {
        emergent = true;
        fastFlee();
        return;
    }

    if (outOfField() || hot == nullptr) {
        emergent = true;
        justMove();
        return;
    }

    if (!besiege) {
        if (!can_skill && !can_attack) {
            emergent = true;
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
    if (!can_skill) return Pos(-1, -1);

    for (int i = 0; i < OBSERVE_POS_N; ++i) {
        Pos tar = OBSERVE_POS[i];
        int dist2 = dis2(pos, tar);
        if (dist2 < BATTLE_FIELD) {
            UnitFilter filter;
            filter.setAreaFilter(new Circle(tar, OBSERVER_VIEW), "w");
            filter.setTypeFilter("Observer", "w");
            vector<PUnit *> observers = console->friendlyUnits(filter);
            if (observers.empty()) {
                Pos shift = parallelChangePos(pos, tar, SET_OBSERVER_RANGE, false);
#ifdef TEMP
                logger << ">> [SCT] set observer at ";
                logger << shift << " pos = ";
                logger << pos << endl << endl;
#endif
                return shift;
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
    if (timeToFlee()) {
        emergent = true;
        fastFlee();
        return;
    }

    if (outOfField() || hot == nullptr) {
        emergent = true;
        justMove();
        return;
    }

    if (!besiege) {
        if (!can_skill && !can_attack) {
            emergent = true;
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
[TESTED]
Update:
. leader = master
. hp alert increased

Fixed bugs:
. master blink

Non-fixed problems:
. sqaud formation
. FIRST WAVE
. call back efficiently

TODO:
. disturbing enemy camp
. robust attack strategy

 */

