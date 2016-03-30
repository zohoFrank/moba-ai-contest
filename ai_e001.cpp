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
static const int CLEAN_LIMIT = 8;           // 最多保留回合记录
static const int CLEAN_NUMS = 4;            // 超过最多保留记录后,一次清理数据组数

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
static ofstream logger("log_info.txt");

void printUnit(vector<PUnit *> units);

void printHeroList(vector<Hero> units);

template <typename T> void printString(vector<T> vct);      // T重载了<<
#endif

// algorithms
Pos changePos(const Pos &origin, const Pos &reference, double len, bool away = true);  // 详见实现

// about game
int enemyCamp();                                    // 敌人的camp

// data structure related
template<typename T>
void releaseVector(vector<T *> vct);

void makePushBack(vector<string> &vct, string str);       // 专处理units storage
int str2int(string str);

string int2str(int n);

string getSubstr(string origin, string start_s, string end_s);      // 包含给定标记串的子串,子串包含stat_s,只包含end_s[第一个字符]

// handling stored data
template<typename T>
void clearOldInfo(vector<T> &vct);                  // 及时清理陈旧储存信息
void clearExcessData();                             // 清楚过剩数据

// handling units
int surviveRounds(vector<PUnit *> host, vector<PUnit *> guest);     // 计算存活轮数,如果host强,返回guest存活,负整数;否则,返回host存活,正整数

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
    void storeObserverInfo();

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
    int mineSituation(int mine_n, int prev_n);      // prev_n轮以前mine_n号矿状况

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
// contact: 0-no, 1-yes
class Hero {
private:
    PUnit *this_hero;
    // for conveniently storage and usage
    int type, id;
    // state
    int safety_env;
    int contact;
    // targets
    PUnit *hot_target;
    int tactic_target;
    // cd
    bool attackCd;
    bool skillCd;

protected:
    // helpers
    int holdRounds();                           // 按上一回合状态,尚可支撑的回合数
    int storedHeroAttr(PUnit *hero, int prev_n, string attr);           // prev_n轮之前的对应英雄attr属性值
    vector<PUnit *> near_enemy();               // 靠近的敌人
    vector<PUnit *> view_enemy();               // 视野内敌人
    vector<PUnit *> near_friend();              // 靠近的队友,包含自己
    vector<PUnit *> view_friend();              // 视野内队友,包含自己
    PUnit *nearestEnemy();                      // 最近的敌人
    void taskHelper(int *situation, int len, const Pos *pos_arr);   // 仅辅助doTask()
    // skill
    bool hammerGuardSkill();                    // 重锤技能判断并释放
    bool berserkerSkill();                      // 孤注一掷技能判断并释放
    bool masterSkill();                         // 闪烁技能判断并释放
    bool scouterSkill();                        // 插眼技能判断并释放
    // setter
    void setContact();                          // 判断是否交战
    void safetyEnv();                           // 判断安全现状
    void lockHotTarget();                         // 锁定攻击目标
    void callBackup();                          // 请求援助

    // actions
    // move
    void cdWalk();                              // cd间的躲避步伐
    void stepBackwards();                       // 远程单位被攻击后撤
    void fastFlee();                            // 快速逃窜
    void doTask();                              // 前往目标
    // attack & skill 带必要走位的攻击
    void contactAttack();                          // 全力攻击

public:
#ifdef LOG
    friend void printHeroList(vector<Hero> units);
#endif
    void setPtr(PUnit *unit);                   // 连接ptr

    // constructor/destructor
    Hero(PUnit *hero = nullptr);

    ~Hero();

    // decision maker
    void go();                                  // 调用动作函数
    // store data
    void storeMe();
};


/*#################### MAIN FUNCTION #######################*/
void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {
#ifdef LOG
    logger << "====Loading Observer====" << endl;
#endif
    console = new Console(map, info, cmd);
    // 获取比赛信息
    Observer *observer = new Observer();
    observer->observeGame();
    observer->storeObserverInfo();
#ifdef LOG
    logger << "====Successfully load observer====" << endl;
    logger << "camp: " << CAMP << endl;
    logger << "====Loading Commander====" << endl;
#endif
    // 分析比赛局势
    Commander *commander = new Commander();
    commander->handleGame();
    commander->storeCmdInfo();
#ifdef LOG
    logger << "====Successfully load Commander====" << endl;
    logger << "====Loading Hero====" << endl;
    logger << "... Creating Hero List" << endl;
#endif
    // make hero list
    vector<Hero> heroes;        // fixme debug
    for (int i = 0; i < current_friends.size(); ++i) {
        Hero temp(nullptr);
        temp.setPtr(current_friends[i]);
        heroes.push_back(temp);
    }
#ifdef LOG
    logger << "@Hero info" << endl;
    printHeroList(heroes);
    logger << "@Enemy info" << endl;
    printUnit(vi_enemies);
    logger << "... Heroes do their actions" << endl;
#endif
    // heroes do actions and store info
    for (int j = 0; j < heroes.size(); ++j) {
        heroes[j].go();
        heroes[j].storeMe();
    }
#ifdef LOG
    logger << "====Successfully handle all Heroes!====" << endl;
    logger << "====Clear excess info====" << endl;
#endif
    // clear vector and release pointers
    clearExcessData();
    releaseVector(current_friends);
    releaseVector(vi_enemies);
    delete observer;
    delete commander;
    delete console;

#ifdef LOG
    logger << "====Perfect end====" << endl;
    logger << "====Stored Info===" << endl;
    logger << "@Stored Economy" << endl;
    printString(stored_money);
    logger << "@Stored Friends" << endl;
    printString(stored_friends);
    logger << "@Stored Tactic" << endl;
    printString(stored_tactic);
    logger << "@Present Miner" << endl;
    for (int k = 0; k < MINE_NUM; ++k) {
        logger << miner[k] << " ";
    }
    logger << endl;
    logger << "@Present Baser" << endl;
    logger << baser[0] << " " << baser[1] << endl;
    logger << "@Stored Situation" << endl;
    printString(stored_mine_situation);
    logger << endl;
    logger << endl;
#endif
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
    logger << left << setw(5) << "TACT";
    logger << left << setw(5) << "CD";
    logger << left << setw(10) << "BUFF";
    logger << endl;
    // print content
    for (int i = 0; i < units.size(); ++i) {
        PUnit *unit = units[i].this_hero;
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
        logger << left << setw(5) << units[i].safety_env;
        logger << left << setw(5) << units[i].contact;
        logger << left << setw(5) << units[i].tactic_target;
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

template <typename T>
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
    clearOldInfo(stored_money);
    clearOldInfo(stored_friends);
    clearOldInfo(stored_tactic);
    clearOldInfo(stored_mine_situation);
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


/************************************************************
 * Implementation: class Observer
 ************************************************************/
void Observer::getEconomyInfo() {
    my_money = console->gold();
}

void Observer::getUnits() {
    UnitFilter filter;
    filter.setAvoidFilter("MilitaryBase", "a");         // 不包括base
    filter.setAvoidFilter("Mine", "w");
    current_friends = console->friendlyUnits(filter);
    vi_enemies = console->enemyUnits(filter);
}

void Observer::observeGame() {
    if (Round == 0) {
        CAMP = console->camp();
    }
    Round = console->round();
    getEconomyInfo();
    getUnits();
#ifdef LOG
    logger << "#### Round: " << Round << " ####" << endl;
    logger << "@money: " << my_money << endl;
    logger << "@Proccessing" << endl;
#endif
}

void Observer::storeObserverInfo() {
    stored_money.push_back(my_money);
#ifdef LOG
    logger << "... Store the money" << endl;
#endif
}


/************************************************************
 * Implementation: class Commander
 ************************************************************/
// constructor
Commander::Commander() {
#ifdef LOG
    logger << "... Construct Commander" << endl;
#endif

    if (stored_tactic.empty()) {
        tactic = TACTICS;
    } else {
        tactic = stored_tactic.back();
    }
}

// helper of helper
int Commander::mineSituation(int mine_n, int prev_n) {
#ifdef LOG
    logger << "... mineSituation()" << endl;
#endif

    if (stored_mine_situation.empty())
        return 0;

    int index = (int) (stored_mine_situation.size() - 1 - prev_n);
    string round_str = stored_mine_situation[index];
    char mine_situation = round_str[mine_n];
    switch (mine_situation) {
        case '1':
            return 1;
        case '0':
            return 0;
        case '-':
            return -1;
        default:
            return 0;           // 意外错误,则默认该矿点胶着
    }
}

// helper
void Commander::scanMines() {
#ifdef LOG
    logger << "... scanMines()" << endl;
#endif

    if (Round <= KEEP_TACTIC) {
        return;
    }

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

void Commander::analyzeSituation() {    // toedit 主要策略点
#ifdef LOG
    logger << "... analyzeSituation()" << endl;
#endif
    /*
     * fixme 临时规则:观察上一回合与本回合是否有矿区失守,如果有,视现有战术决定调整战术
     */
    // 开局执行指定战术
    if (Round < KEEP_TACTIC) {
        tactic = TACTICS;
        return;
    }
    // 中矿是决策的关键点
    int pre_1 = mineSituation(0, 1);         // 中矿上一局
    int pre_2 = mineSituation(0, 2);         // 中矿上第二局
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

void Commander::tacticArrange() {     // toedit 主要策略点
#ifdef LOG
    logger << "... tacticArrange()" << endl;
#endif
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
    if (Round % KEEP_TACTIC != 0 || Round <= KEEP_TACTIC)
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
        filter.setAvoidFilter("MilitaryBase");
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
    int n_heroes = (int) current_friends.size();
    int cost = 0;
    // 先标记,后购买.循环判断,可以连续买英雄
    vector<string> to_buy;
    while (cost <= BUY_NEW_COST * my_money) {
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
    const int TEMP = (const int) current_friends.size();
    if (TEMP == 0)
        return;

    int *flags = new int[TEMP];               // 设记号,为处理同一英雄多次升级

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
    delete flags;
}

void Commander::callBack() {
#ifdef LOG
    logger << "... callBack()" << endl;
#endif
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
    // 预处理
    if (near_enemies.size() == 0)
        return;
    else
        baser[CAMP]++;
    // 处理
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

// battle analysis
void Commander::handleGame() {
    scanMines();
    analyzeSituation();
    tacticArrange();
    // base
    attack();
    buyNewHero();
    levelUp();
    callBack();
}

void Commander::storeCmdInfo() {
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
    stored_mine_situation.push_back(toStore);
}



/************************************************************
 * Implementation: class Hero
 ************************************************************/
// protected helper
int Hero::holdRounds() {
#ifdef LOG
    logger  << "(using holdRounds()...)" << endl;
#endif

    if (Round <= 1)
        return 1000;

    int last_hp = storedHeroAttr(this_hero, 1, "hp");
    if (last_hp == -1)
        return 1000;

    if (this_hero->hp >= last_hp)
        return 1000;                              // 如果现在血量反而多,那么返回-1,表示预估不会死亡
    int holds = this_hero->hp / (last_hp - this_hero->hp);
    return holds;
}

int Hero::storedHeroAttr(PUnit *hero, int prev_n, string attr) {
    if (prev_n >= stored_friends.size())
        return -1;

    string str = stored_friends[stored_friends.size() - 1 - prev_n];
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
    vector<PUnit *> views = view_enemy();
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

void Hero::taskHelper(int *situation, int len, const Pos *pos_arr) {
    Pos me = this_hero->pos;
    // 扫描矿区
    for (int i = 0; i < len; ++i) {
        Circle *battle_field = new Circle(pos_arr[i], BATTLE_AREA);
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
    // fixme 如果没有寻找到任务
}

// skill
bool Hero::hammerGuardSkill() {         // toedit 主要策略点
    /*
     * 使用优先场合:
     * 1.一招毙命:伤害大于任何一个的血量剩余值
     * 否则攻击热点单位
     */
    // 预判断
    vector<PUnit *> enemies = near_enemy();
    if (enemies.size() == 0)                        // 没有敌人
        return false;
    if (!this_hero->canUseSkill("HammerAttack"))    // 不能使用技能(cd或mp不够)
        return false;

    // 如果有sacrifice单位,直接攻击之
    if (hot_target->hp == 1 && hot_target->typeId == 5) {
        console->useSkill("HammerAttack", hot_target, this_hero);       // go
        return true;
    }

    // 符合1,且选择血量尽量高者
    int skill_damage = HAMMERATTACK_DAMAGE[this_hero->level];
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
        console->useSkill("HammerAttack", enemies[highest_index], this_hero);
    } else {                        // 否则攻击热点单位
        console->useSkill("HammerAttack", hot_target, this_hero);       // go
    }
    return true;
}

bool Hero::berserkerSkill() {
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
    if (!this_hero->canUseSkill("Sacrifice"))
        return false;

    // 1
    bool isAttacked = hasBuff(this_hero, "BeAttacked");
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
        console->useSkill("Sacrifice", hot_target, this_hero);          // go
        return true;
    } else {
        return false;
    }
}

bool Hero::masterSkill() {
    if (!this_hero->canUseSkill("Blink"))
        return false;

    Pos me = this_hero->pos;
    Pos you = hot_target->pos;
    int atk = this_hero->atk;
    Circle my_range(me, this_hero->range);

    if (safety_env != 2 && hot_target->hp < atk) {      // 进攻型
        double move_len = dis(you, me) - this_hero->range;
        Pos move = changePos(me, you, move_len, false);
        console->useSkill("Blink", move, this_hero);        // go
        return true;
    } else if (safety_env == 2) {        // 防守型
        double move_len = BLINK_RANGE;
        Pos move = changePos(me, you, move_len, true);
        console->useSkill("Blink", move, this_hero);        // go
        return true;
    } else {        // 没有必要
        return false;
    }
}

bool Hero::scouterSkill() {
    /*
     * 路过关键点插眼
     */
    if (!this_hero->canUseSkill("SetObserver"))
        return false;
    // 路过无眼即插眼
    Pos me = this_hero->pos;
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
            console->useSkill("SetObserver", key, this_hero);       // go
            return true;
        }
    }
    return false;
}

// protected setters
void Hero::setContact() {
    if (view_enemy().size() == 0) {
        if (hasBuff(this_hero, "IsMining")) {
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

void Hero::lockHotTarget() {
    /*
     * 锁定最弱目标,优先顺序如下:
     * 1.WinOrDie状态的单位
     * 2.WaitRevive状态的单位
     * 3.最不耐打——坚持回合数最少的目标
     */
    // fixme
    UnitFilter filter;
    filter.setAreaFilter(new Circle(this_hero->pos, this_hero->range), "a");
    vector<PUnit *> enemies = console->enemyUnits(filter);
    if (enemies.empty()) {
        hot_target = nullptr;
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
            hot_target = enemy;
            return;
        }
        // 2
        if (hasBuff(enemy, "WaitRevive")) {
            hot_target = enemy;
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
        hot_target = enemies[first_die_index];
    } else {                    // 全都打不过
        hot_target = nearestEnemy();        // 打最近的一个
    }
#ifdef LOG
    logger << "...... Lock hot target: id=" << hot_target->id << endl;
#endif
}

void Hero::callBackup() {
    /*
     * 如果处于dangerous,求援
     */
    if (safety_env == 1) {
        miner[tactic_target]++;
    }
    return;
}

// protected actions
// move
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
    // 如果是master
    if (type == 4 && this_hero->canUseSkill("Blink")) {
        masterSkill();                      // go
    } else {
        console->move(base, this_hero);     // go
    }
//    contact = 0;
}

void Hero::doTask() {
#ifdef LOG
    logger << "...... doTask()" << endl;
#endif
    if (type == 6 && this_hero->canUseSkill("SetObserver")) {
        scouterSkill();                     // go 移动过程中使用技能
    }

    // fixme
//    taskHelper(miner, MINE_NUM, MINE_POS);
//    taskHelper(baser, MILITARY_BASE_NUM, MILITARY_BASE_POS);
    console->move(MINE_POS[0], this_hero);        // go
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

    if (hot_target == nullptr) return;

    Circle my_range(this_hero->pos, this_hero->range);
    Pos target_pos = hot_target->pos;
    if (my_range.contain(target_pos)) {
        console->attack(hot_target, this_hero);
    } else {
        doTask();
    }
//    if (!skillCd) {
//        switch (type) {
//            case 3:
//                hammerGuardSkill();
//                break;
//            case 4:
////                masterSkill();
//                break;
//            case 5:
////                berserkerSkill();
//                break;
//            case 6:         // scouter
//                break;
//            default:
//                break;
//        }
//    } else if (!attackCd) {
//        console->attack(hot_target, this_hero);         // go
//    }
}

// public constructor
Hero::Hero(PUnit *hero) { setPtr(hero); }

void Hero::setPtr(PUnit *unit) {
    if (unit == nullptr) return;

    this_hero = unit;
    // 如果是敌人英雄,则不设置参数
    if (unit->camp != CAMP)
        return;
    // 设置当前参数
    type = this_hero->typeId;
    id = this_hero->id;
    hot_target = nullptr;
    // 延续上一回合决策
    if (Round > 2) {
        safety_env = storedHeroAttr(this_hero, 1, "safe_e");
        tactic_target = storedHeroAttr(this_hero, 1, "mine");
        // dealing with cd
        attackCd = !this_hero->canUseSkill("Attack");
        skillCd = true;
        for (int i = 8; i < 12; ++i) {
            if (this_hero->canUseSkill(SKILL_NAME[i])) {    // 具体请查看常量
                skillCd = false;
                break;
            }
        }
    } else {
        safety_env = 0;
        tactic_target = TACTICS;
        attackCd = false;
        skillCd = false;
    }

    // setAll
    setContact();
    lockHotTarget();
    safetyEnv();
    callBackup();
}

Hero::~Hero() {
    this_hero = nullptr;
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
//    } else if (safety_env == 2) {       // dying
//        fastFlee();
//        return;
//    } else if (!skillCd || !attackCd) { // attack or use skill
//        contactAttack();
//        return;
//    } else {                            // can do nothing
//        cdWalk();
//    }
    // fixme
    if (hot_target == nullptr) {
        doTask();
    } else {
        console->attack(hot_target, this_hero);
    }
}


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
    my_info = my_info + "contact:" + int2str(contact) + ",";
    my_info = my_info + "round:" + int2str(Round) + ",";
    my_info = my_info + "mine:" + int2str(tactic_target) + ",}";
    makePushBack(stored_friends, my_info);
}




/*
 * 1.近距离协助机制
 * 2.cdWalk()
 * 3.重写contactAttack()
 * 4.分配任务采取直接指定制,(过n回合全局分析一下效果,再调整战术——暂时不写)
 *                      或者,按英雄能力阶段分配战术,能力满足要求时组队打野
 * 5.
 */