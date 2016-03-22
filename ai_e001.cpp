#include "console.h"
#include <vector>
#include <string>
using namespace std;

// 调试开关
#define LOG

#define SIZE(X) (int(x.size()))
/*######################## DATA ###########################*/
/************************************************************
 * Const values
 ************************************************************/
static const char* HERO_NAME[] = {"Hammerguard", "Master", "Berserker", "Scouter"};

// unchanged values (during the game)
static int CAMP = -1;                       // which camp
static int PLAYER = -1;                     // which player
static int Round = 0;
static const Pos KEY_POS[] = {};            // key positions

/************************************************************
 * Policy data
 ************************************************************/
// 经决策器处理后得出的决策结果/处理后数据

// global situation
static int situation;                       // todo 是一个比值, 反映战况
static Console* console = nullptr;
// policy data
static int global_state = -1;
static vector<int> bf_fight_state;
static vector<int> bf_tactic_state;

/*################# Assistant functions ####################*/
vector<PUnit*> sumEnemies(vector<PUnit*> friends);
void getWeakestUnit(vector<PUnit*> units, PUnit* weakest);
void getStrongestUnit(vector<PUnit*> units, PUnit* strongest);



/*##################### STATEMENT ##########################*/
/************************************************************
 * Observer
 ************************************************************/
// 汇总所有发现的信息,便于各级指挥官决策
// 必须每次运行均首先调用
struct Observer {
    int round;
    int money;
    // units info
    vector<PUnit*> my_units;
    vector<PUnit*> vi_enemies;
    vector<PUnit*> vi_monsters;

    // run observer
    void observeGame();
};

/************************************************************
 * Global commander
 ************************************************************/
// 全局指挥官,分析形势,买活,升级,召回等
// 更新不同战区信息,units可以领任务,然后形成team
// global_state: inferior, equal, superior

struct Commander {
    int tactic = 0;     // 设置战术代号
    int situation = 0;  // 分析战场局势

    // helper
    void changeTeams();

    // todo 分析战局

    // todo 设置重点战区


};


/************************************************************
 * Battle Field
 ************************************************************/
// 1.分析战区战况.
// 2.领任务的UnitFilter.
// fight_state: inferior, equal, superior
// tactic_state: pushing, hunting, mining, tempting, | fallback, stealing mines, | inspecting

struct BattleField {
    bool active;
    int id;                             // 战区id
    Pos* position;
    UnitFilter* callFilter;
    // todo 分析战区局势参数

    // constructor
    BattleField(
            int _id,
            bool _act = false,
            Pos* _pos
    ) :
            id(_id),
            active(_act),
            position(_pos)
    {}
    ~BattleField() {
        if (!position) {
            delete position;
            position = nullptr;
        }
        if (!callFilter) {
            delete callFilter;
            callFilter = nullptr;
        }
    }

    // methods
    void setActive(bool isActive) {
        active = isActive;
    }
    void analyzeSituation();
    void analyzeBattleFields();
    void nearestBF(Pos* position, BattleField* nearest);      // 某位置的最近战区

};


/************************************************************
 * Heroes
 ************************************************************/
// attack_state: positive, negative
// safe_state: dying, dangerous, safe
// fight_state: nothing, inferior, equal, superior

class Hero {
private:
    PUnit* this_hero;
    Pos* position;
    int hp, mp;
    int attack, defend;
    PBuff* buff;
    // fight??
    PUnit* attackOn;                            // 正在攻击此单位
    PUnit* defendFrom;                          // 承受来自此单位攻击
    // state
    int atk_state;
    int safety_state;
    int fight_state;

protected:
    bool isSafe();
    void underAttack();                         // ??是否承受攻击,无设定defendFrom为nullptr
    void setState(int atk = 0, int sft = 0, int ft = 0);
                                                // 若为0,则不设置

public:
    // todo constructor/deconstructor
    // todo 零散的待整理的高级动作
    virtual void fastFlee() = 0;                // 快速逃窜
    virtual void chaseAttack() = 0;             // 追击
    virtual void hardAttack() = 0;              // 全力攻击
    virtual void hitRun() = 0;                  // 诱敌
    virtual void stayAlarmed() = 0;             // 淡定警戒 -- 不主动攻击,但还击或随时准备做动作,适合挖矿
    // decision maker & actions
    void selectAction();                        // 考虑所有state的组合

};
// todo 注意删除

/***************** Derived classes **************************/
class Hammerguard : public Hero {

};

class Master : public Hero {

};

class Berserker : public Hero {

};

class Scouter : public Hero {

};


/*#################### MAIN FUNCTION #######################*/
void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {
    console = new Console(map, info, cmd);



    delete console;
}



/*################### IMPLEMENTATION #######################*/
/************************************************************
 * Implementation: struct Observer
 ************************************************************/


/************************************************************
 * Implementation: struct Commander
 ************************************************************/

/************************************************************
 * Implementation:
 ************************************************************/
