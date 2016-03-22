#include "console.h"
#include <vector>
#include <cstring>
using namespace std;

// 调试开关
#define LOG

#define SIZE(X) (int(x.size()))
typedef pair<int, int> POSITION;
/*######################## DATA ###########################*/
/************************************************************
 * Const values
 ************************************************************/
static const char* HERO_NAME[] = {"Hammerguard", "Master", "Berserker", "Scouter"};
// unchanged values (during the game)
static int CAMP = -1;                  // which camp
static int PLAYER = -1;             // which player
static vector<POSITION> mine_pos;   // mine position
// todo 标记典型positions

/************************************************************
 * Policy data
 ************************************************************/
// 经决策器处理后得出的决策结果/处理后数据

// global situation
static int situation;                       // todo 是一个比值, 反映战况
static Console* console = nullptr;
// policy object pointers
static Observer* observer = nullptr;        // Observer对象指针
static vector<BattleField*> BattleFields;   // 所有战区组成的序列
static vector<Hero*> Heroes;                // Hero对象指针向量,注意多态

/*################# Assistant functions ####################*/


/*##################### STATEMENT ##########################*/
/************************************************************
 * Observer
 ************************************************************/
// 汇总所有发现的信息,便于各级指挥官决策
// 必须每次运行均首先调用
struct Observer {
    // game info
    int round;
    // player info
    int player, camp, money;
    // units info
    vector<PUnit*> my_units;
    vector<PUnit*> vis_enemies;
    vector<PUnit*> vis_monsters;

    // helpers
    void createBattleFileds();              // 全局只需执行一次,创建战区信息,之后交给战区自行更改状态
    void revealMines();                     // 全局只需执行一次,显示mine位置


    // run observer
    void observeGame();
};

/************************************************************
 * Battle Field
 ************************************************************/
// 1.分析战区战况.
// 2.领任务的UnitFilter.
struct BattleField {
    bool active;
    int id;                             // 战区id
    Pos* position;
    UnitFilter* filter;

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
        if (!filter) {
            delete filter;
            filter = nullptr;
        }
    }

    // methods
    void setActive(bool isActive) {
        active = isActive;
    }
    void analyzeSituation();
    void analyzeBattleFields();




};


/************************************************************
 * Global commander
 ************************************************************/
// 全局指挥官,分析形势,买活,升级,召回等
// 更新不同战区信息,units可以领任务,然后形成team
struct Commander {
    int tactic = 0;     // 设置战术代号
    int situation = 0;  // 分析战场局势

    // helper
    void changeTeams();

    // todo 战术代号的具体内容

    // todo 战场局势分析,并设置situation

};

/************************************************************
 * Heroes
 ************************************************************/
// 各自为战的英雄,除非遇到危机情况,皆听从小队长指令.技能选择由英雄各自选择,可以考虑继承关系,对不同英雄实现不同决策.
// state: 警觉---(发现敌人)-->攻击--(hp不足)--->忽视--(hp恢复)-->警觉

class Hero {
private:
    PUnit* this_hero;
    Pos* position;
    int hp, mp;
    int attack, defend;
    PBuff* buff;
    int state;      // todo 设计的精华部分,详细考虑
    int ordered;    // todo 最好建几个结构体,丰富state和ordered

protected:
    bool isSafe();
    PUnit* foundEnemy();                        // 是否发现敌人,无返回nullptr
    PUnit* underAttack();                       // 是否承受攻击,无返回nullptr
    PUnit* viLowestHP();                        // 可见的最低血量敌人
    PUnit* viNearestEnemy();                    // 可见的最近敌人
public:
    // todo constructor/deconstructor
    // todo 零散的待整理的高级动作
    virtual void fastFlee() = 0;                // 快速逃窜
    virtual void chaseAttack() = 0;             // 追击
    virtual void hardAttack() = 0;              // 全力攻击
    virtual void hitRun() = 0;                  // 诱敌
    virtual void stayAlarmed() = 0;             // 淡定警戒 -- 不主动攻击,但还击或随时准备做动作,适合挖矿

};
// todo 注意删除

class Hammerguard : public Hero {

};



/*#################### MAIN FUNCTION #######################*/
void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {




    delete observer;
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
 * Implementation: struct TeamLeader
 ************************************************************/


/************************************************************
 * Implementation: struct Commander
 ************************************************************/
