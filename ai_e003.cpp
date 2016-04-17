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

class Hero;

class Commander;

class AssaultSquad;

class MainCarrier; class MineDigger; class BattleScouter;

/*######################## DATA ###########################*/
/************************************************************
 * Const values
 ************************************************************/
static const int BIG_INT = 1 << 29;

static const int TAC_TARGETS_N = 9;         // 7个矿+2个基地
static const int MINE_NUM_SHIFT = 5;        // 编号偏移

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
static const int SCOUT_OK = 9;                  // 侦查完成的最大距离

// 继承上一轮
static int HotId = -1;                          // 储存的hot id
static Tactic Target = MINE_POS[0];             // 储存的targets

// Scouting info
int LastSeenRound[TAC_TARGETS_N] = {};          // 记录上次观测到的回合数
int EnemiesN[TAC_TARGETS_N] = {};               // 记录上次观测时各矿区人数

typedef vector<int> ID_LIST;
// Squad settings
static const int SQUAD_N = 8;                   // 小队数量
static int SquadTargets[SQUAD_N] = {};          // 小队战术id
static ID_LIST SquadMembers[SQUAD_N] = {};      // 各小队成员安排

static vector<int> ScouterList;                 // 侦查目标列表

// Squad list
static AssaultSquad AllSquads[SQUAD_N] = {
        MainCarrier(0),            // type0 任务:主力攻击
        MainCarrier(1),            // type0
        MainCarrier(2),            // type0
        MineDigger(3),             // type1 任务:挖矿
        MineDigger(4),             // type1
        MineDigger(5),             // type1
        BattleScouter(6),          // type2 任务:巡查
        BattleScouter(7)           // type2
};                                // 所有小队,默认初始化后需要调整参数



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
//double unitAtkScore(PUnit *pu);                     // 单位进攻评估



/************************************************************
 * Global commander
 ************************************************************/
// 全局指挥官,分析形势,买活,升级,召回等,也充当本方base角色
// 更新不同战区信息,units可以领任务,然后形成team
// global_state: inferior, equal, superior
// todo 缺少一个判断或者参数,分配各小队人数的时候作为参考
class Commander {
private:
    vector<PUnit *> sector_en;

    PUnit *hot;

    /**********************************************************/
protected:
    // HELPERS
    void getUnits();                                // 获取单位信息

    // tactics 顺序不能错!
    void updateSquad();                             // 更新小队信息
    void lockSquadTarget();                         // 指定小队目标
    void distributeHeroes();                        // todo 更新小队后,视情况调整英雄分配

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
    virtual void makeHeroes();          // make heroes
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

    /*********************************************************/
    virtual PUnit *nearestEnemy() const;
    virtual Hero *getStoredHero(int prev_n);            // 获得之前prev_n局的储存对象

    /*************************Helpers***************************/
    virtual bool outOfField();                          // 离开战场了
    virtual bool timeToSkill() = 0;                     // 技能释放环境判断
    virtual bool timeToFlee();                          // 是否应该逃窜
    virtual bool stuck();                               // 由于未知原因卡住了
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

    // fixme 以下几段代码需要重构并放弃
    void StoreMe();     // 储存该英雄信息

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
        PUnit *ptr = unit->punit;
        // print basic hero info
        logger << left << setw(14) << ptr->name;
        logger << left << setw(5) << ptr->id;
        logger << left << setw(8) << ptr->level;
        logger << left << setw(5) << ptr->hp;
        logger << left << setw(5) << ptr->mp;
        logger << left << setw(5) << ptr->atk;
        logger << left << setw(5) << ptr->def;
        // POS/TAC
        string p = int2str(ptr->pos.x) + "," + int2str(ptr->pos.y);
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

    // cur_friends  vi_enemies
    getUnits();
    // arrange squads 顺序不能错
    lockSquadTarget();
    updateSquad();
    distributeHeroes();
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
// todo 需要修改的bug函数们


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


void Commander::updateSquad() {
    for (int i = 0; i < SQUAD_N; ++i) {
        AllSquads[i].roundUpdate();
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
    // squads
    for (int i = 0; i < SQUAD_N; ++i) {
        AllSquads->SquadCommand();
    }
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
    UnitFilter filter;
    filter.setAreaFilter(new Circle(TACTICS[target_id], battle_range), "a");
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

    sector_f = console->friendlyUnits(filter);

    // 用迭代器遍历并删除指定元素 - 尸体
    for (auto i = sector_en.begin(); i != sector_en.end(); ) {
        if (hasBuff(*i, "Reviving"))
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
        hot_id = hot->id;
        return;
    }
    if (!wait_revive.empty()) {
        hot = wait_revive[0];
        hot_id = hot->id;
        return;
    }

    // 继承
    PUnit *last_hot = findID(sector_en, hot_id);
    if (last_hot != nullptr) {
        hot = last_hot;
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


void AssaultSquad::makeHeroes() {
    int _sz = (int) member_id.size();
    for (int i = 0; i < _sz; ++i) {
        int hero_id = member_id[i];
        PUnit *unit = console->getUnit(hero_id);
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
                logger << "[ERRO] Friend enemy's type not found" << endl;
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

    const Pos onPosition[] = {Pos(1, 0), Pos(-1, 0), Pos(0, 1), Pos(0, -1)};
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
    battle_range = BATTLE_RANGE;
    stick_counter = StickRounds;
    roundUpdate();
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
    makeHeroes();
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
    situation = 0;
}


void MainCarrier::evaluateSituation() {
    if (stick_counter > 0) {
        situation = 0;
        return;
    }   // assert: stick_counter <= 0

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

MineDigger::MineDigger(int _id) : AssaultSquad(_id) { }


void MineDigger::setOthers() {
    type = 1;
    battle_range = BATTLE_RANGE / 4;        // 作战半径减小一半

    PUnit *mine = console->getUnit(target_id + MINE_NUM_SHIFT);
    if (mine == nullptr) {
        mine_energy = BIG_INT;
    } else {
        mine_energy = console->unitArg("energy", "c", mine);
#ifdef LOG
        logger << ">> Target mine energy: " << mine_energy << endl;
#endif
    }
}


void MainCarrier::setOthers() {
    return;
}


void MineDigger::evaluateSituation() {
    if (stick_counter > 0) {
        situation = 0;
        return;
    }   // assert: stick_counter <= 0

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
    hot == nullptr;
}


BattleScouter::BattleScouter(int _id) : AssaultSquad(_id) { }


void BattleScouter::setOthers() {
    return;
}


void BattleScouter::evaluateSituation() {
    if (stick_counter > 0) {
        situation = 0;
        return;
    }   // assert: stick_counter <= 0

    int _sz_f = (int) sector_f.size();
    int _sz_e = (int) sector_en.size();

    // 一旦对改点观测完成即撤出
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


bool Hero::outOfField() {
    int dist2 = dis2(pos, target);
    return dist2 > BATTLE_RANGE;
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


/***********************************************************/

Hero::Hero(int _id, int _hot, int _tactic) :
        id(_id), hot_id(_hot), target_id(_tactic){
    punit = console->getUnit(id);
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
}


Hero::~Hero() {
    punit = nullptr;
    hot = nullptr;
    Hero(-1, -1, 0);
}


/*************************Loader***************************/

void Hero::Emergency() {
    if (timeToFlee()) {
        fastFlee();
        return;
    }

    if (outOfField() || hot == nullptr) {
        justMove();
        return;
    }

    if (!besiege) {
        if (!can_skill && !can_attack) {
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
    Attack();
}


void Hero::justMove() {
    console->move(target, punit);
#ifdef LOG
    logger << "[mov] move to ";
    logger << target << endl;
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
    // hot必须在攻击范围内
    if (!dis2(hot->pos, pos) > range)
        return false;

    // this必须能sacrifice,且至少下一回合能攻击
    if (!can_skill || punit->findSkill("Attack")->cd > 1) {
        return false;
    }

    // hot必须是落单对象
    for (int i = 0; i < vi_enemies.size(); ++i) {
        PUnit *pu = vi_enemies[i];
        int dist2 = dis2(hot->pos, pu->pos);
        if (dist2 < pu->range + pu->speed) {        // 并不落单
            return false;
        }
    }

    // 且hot必须不能产生攻击
    if (hot->findSkill("Attack")->cd <= 1 || hot->canUseSkill("HammerAttack")) {
        return false;
    }

    // 以上都通过,则可以
    return true;
}

//void Berserker::prepareOrAttack() {
//    // assert: not right time to sacrifice
//    int atk_cd = punit->findSkill("Attack")->cd;
//    int skill_cd = punit->findSkill("Sacrifice")->cd;
//    int interval = atk_cd - skill_cd;
//
//    // 考虑:敌人位置圆心,this range半径的 安全+可达点 (可能需要三重循环,考虑精简算法)
//    /*
//     * 算法精简的考虑
//     * 1.求可达:两圆是否相交--距离与半径和比较
//     * 2.验证安全:求公共弦(垂直平分线段两点坐标),其垂线段,遍历验证
//     * 3.遍历筛选:仅遍历与敌人圆 相交的其他敌人(两者距离 < range + enemy_speed)
//     * 最不利:可能需要range * range * enemy_size 的次数,最大可达36 * 36 * 8 = 10368次
//     */
//    if (interval < 0 || besiege) {     // 下一次attack前不能sacrifice,或者围攻状态
//        Hero::Attack();
//        return;
//    } else {
//        UnitFilter
//    }
//
//}


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
        fastFlee();
        return;
    }

    if (outOfField() || hot == nullptr) {
        Hero::justMove();
        return;
    }

    if (!besiege) {
        if (!can_skill && !can_attack) {
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
            return OBSERVE_POS[i];
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
        Hero::fastFlee();
        return;
    }

    if (outOfField() || hot == nullptr) {
        justMove();
        return;
    }

    if (!besiege) {
        if (!can_skill && !can_attack) {
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
 * todo 要更改的内容
 * 1. 十字卡位
 * 2. 致命一击
 *
 * 1. 小队模式: 走位/队形...
 * 2. cdWalk
 * 3. Berserker的致命一击
// * 5. buyLevel, buyNew, 对部分单位进行召回升级callBack, 钱过多时进行买活buyLife
 *
 * 战术:
 * 根据发展阶段定战术?
 * 根据相对实力定战术?
 *
 * temp:
 * 对抗偷基地流
 * 对抗中路优势+分矿流
 */
