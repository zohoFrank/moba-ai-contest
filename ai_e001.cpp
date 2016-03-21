#include "console.h"
#include <vector>
#include <cstring>
using namespace std;

// 调试开关
#define LOG

/*######################## DATA ###########################*/
/************************************************************
 * Storage data
 ************************************************************/
// 一些常量,存储信息,便于沿用

// const values
static const char* HERO_NAME[] = {"Hammerguard", "Master", "Berserker", "Scouter"};



/************************************************************
 * Current origin data
 ************************************************************/
// 一些实时直接观测值/未经处理的值,便于数据交换

// my heroes info
static vector<PUnit*> my_team;         // 当前team指针向量


// enemy heroes info
static vector<PUnit*> vi_enemy_team;


/************************************************************
 * Policy data
 ************************************************************/
// 经决策器处理后得出的决策结果/处理后数据

// commander:
static vector<vector<int>> teams;       // teams[i]表示第i分组,team[i][j]表示i分组的j单位id

// team leader:
static vector<int> team_instruction;    // team_instruction[i]表示第i分组的小队指令



/*################# Assistant functions ####################*/





/*##################### STATEMENT ##########################*/
/************************************************************
 * Observer
 ************************************************************/
// 汇总所有发现的信息,便于各级指挥官决策
// 必须每次运行均首先调用
struct Observer {
    /*######## DATA #########*/

};



/************************************************************
 * Global commander
 ************************************************************/
// 全局指挥官,分析形势,编队,买活,升级,给小队下达指令.
// 注意小队的状态,处在执行任务的小队,除非被强行终止,否则无法接受全局指挥官指令
struct Commander {

};

/************************************************************
 * Team leader
 ************************************************************/
// 小队长,其实是虚构决策器.结合环境信息,全局指挥官指令,下达采矿/攻击/寻路/逃跑/待命等指令,下达到每一个单位
struct TeamLeader {

};


/************************************************************
 * Hero
 ************************************************************/
// 各自为战的英雄,除非遇到危机情况,皆听从小队长指令.技能选择由英雄各自选择,可以考虑继承关系,对不同英雄实现不同决策.
// state: 警觉---(发现敌人)-->攻击--(hp不足)--->忽视--(hp恢复)-->警觉
struct Hero {
    int cur_hp;
    int cur_mp;
    int cur_team;
    int state;          // !!KEY!! 状态量,{0:alarmed, 1:contact, 2:ignored}
    int ordered;        // 接收team leader指令
    int x, y;           // 位置
    int effect;         // 被作用的效果

    // helpers
    // 判断类
    PUnit* foundEnemy();                    // 是否发现敌人,没有返回nullptr
    PUnit* isUnderAttack();                 // 是否遭受攻击,没有返回nullptr
    bool isSafe(Pos* pos);                  // 某个位置是否安全(计算下遭遇战是否能成功)
    void leaveTeam();                       // 离开小队
    // 忽视类
    void ignoredMoving();                   // 保持行进,忽略攻击,避开危险
    // 攻击类
    void chase(PUnit* enemy);               // 追击
    void giveUp();                          // 放弃攻击,进入警觉态或忽视态
    void hardAttack(PUnit* enemy);          // 全力攻击
    void hitRun(PUnit* enemy);              // 诱敌攻击,转到忽视态
    // 警觉类
    void alarmedMoving(Pos* pos);           // 警觉行进到pos位置(发现即转到攻击态)
    void negativeMoving(Pos* pos);          // 被动行进到pos位置(发现不攻击,但被攻击即还击)

    // main functions
    void makeDecision(int order);           // 根据指令,结合实际情况,设置state
    void doActions();                       // !!仅!!根据state决定动作
};


/*#################### MAIN FUNCTION #######################*/
void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd) {
    Console* console = new Console(map, info, cmd);



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
