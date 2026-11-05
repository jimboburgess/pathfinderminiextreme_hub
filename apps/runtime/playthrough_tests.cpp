#include "runtime.h"
#include "data/game.h"
#include "dungeon/dungeon.h"
#include "dungeon/combat.h"
#include "map/activemap.h"
#include "graphics/charcreationscreen.h"
#include "characters/sheet.h"
#include "input/menu.h"
#include <iostream>
#include <fstream>
#include <stdexcept>

namespace {
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void wait(unsigned ms) { for(unsigned i=0;i<ms;i+=10) pfme::runtime::advance(10); }
void tap(const char* action) { pfme::runtime::input(action,true);wait(30);pfme::runtime::input(action,false);wait(350); }
void face(Direction direction) { for(int i=0;i<8 && moveDirection!=direction;i++)tap("cw");check(moveDirection==direction,"Facing via encoder"); }
void capture(const char* name) {
    std::ofstream file(name,std::ios::binary);
    auto* pixels=pfme::runtime::pixels();
    for(int i=0;i<57600;i++){file.put(char(pixels[i]&255));file.put(char(pixels[i]>>8));}
}
bool select(MenuAction action) {
    if(!menuState.isOpen || !menuState.depth) return false;
    const auto* menu=menuState.menuStack[menuState.depth-1];
    int index=0,target=-1;
    for(int i=0;i<menu->itemCount;i++) {
        const auto& item=menu->items[i];
        if(!(item.classMask & MENU_CLASS_FIGHTER) || !isMenuItemVisible(item.action))continue;
        if(item.action==action)target=index;
        ++index;
    }
    if(target<0 || !isMenuItemEnabled(action))return false;
    for(int i=0;i<32 && menuState.cursorIndex!=target;i++)tap("cw");
    if(menuState.cursorIndex!=target)return false;
    tap("a"); return true;
}
int monsterHP() {int hp=0;for(int i=0;i<dungeon.entityCount;i++)if(dungeon.entities[i].type==ENTITY_MONSTER)hp+=std::max(0,int(dungeon.entities[i].character.health.currentHP));return hp;}
}
int main(int argc,char** argv) {
    try {
        pfme::runtime::initialize(argc>1?static_cast<uint32_t>(std::stoul(argv[1])):2);
        check(gameState==GAME_START,"Original title");capture("play-title.rgb565");
        tap("a");check(gameState==GAME_CHARACTER_CREATION,"New Game enters original character creation");
        tap("a");check(getCharacterCreationState()==CCS_WEAPON_STYLE_SELECT,"Fighter weapon style");
        tap("a");check(getCharacterCreationState()==CCS_VIEW_STATS,"Production rolled statistics");capture("play-stats.rgb565");
        tap("a");check(isCharacterMenuOpen(),"Original character menu");
        tap("a");check(isCharacterNameEntryActive(),"Character naming follows acceptance");
        tap("encoder");check(gameState==GAME_TOWN && player.health.currentHP>0,"Accepted character enters town");capture("play-town.rgb565");
        pfme::runtime::input("encoder",true);wait(800);check(isCharacterSheetVisible(),"Long press opens original sheet");
        pfme::runtime::input("encoder",false);wait(350);tap("b");check(!isCharacterSheetVisible(),"B closes sheet");
        tap("cw");tap("cw");tap("a");
        check(gameState==GAME_DUNGEON && dungeon.runActive && dungeon.roomCount>1,"Production dungeon generation");
        auto* p=getActiveMapPlayer();check(p && p->x==2 && p->y==6,"Original entrance spawn");
        face(DIR_NORTH);tap("encoder");check(p->y==5,"Tile movement");tap("encoder");check(p->y==5,"Real wall collision");
        face(DIR_SOUTH);tap("encoder");face(DIR_EAST);capture("play-dungeon.rgb565");
        tap("a");check(menuState.isOpen,"Original map menu opens");tap("cw");tap("b");check(!menuState.isOpen,"Original menu navigation and cancel");
        for(int i=0;i<12 && dungeon.currentRoom==0;i++)tap("encoder");
        check(dungeon.currentRoom==1,"Existing door/room transition");
        check(monsterHP()>0,"Actual generated monsters");
        wait(18000);check(combat.active,"Real monster awareness and initiative");capture("play-combat.rgb565");
        const int initialMonsters=monsterHP();const int initialPlayer=getActiveMapPlayer()->character.health.currentHP;
        bool attacked=false,hpChanged=false,enemyActed=false;
        auto confirmTarget=[&]() {
            for(int turn=0;turn<8 && !getSelectedAttackTarget();turn++)tap("cw");
            if(getSelectedAttackTarget()) { tap("a"); attacked=true; return true; }
            tap("b"); return false;
        };
        for(int turn=0;turn<80 && combat.active;turn++) {
            wait(16000);
            auto* active=getActiveMapPlayer();
            if(!active || active->character.health.currentHP<=0)break;
            if(isPlayerTurn() && combat.waitingForPlayer) {
                if(isPlayerTargetingAttack())confirmTarget();
                else {
                    if(menuState.isOpen)tap("b");
                    tap("a");
                    if(select(MENU_ATTACK)) {
                        bool hit=false;
                        if(select(MENU_MELEE_ATTACK) && isPlayerTargetingAttack())hit=confirmTarget();
                        if(!hit) {
                            if(!menuState.isOpen)tap("a");
                            if(select(MENU_ATTACK) && select(MENU_RANGED_ATTACK) && isPlayerTargetingAttack())confirmTarget();
                        }
                    }
                    if(menuState.isOpen)tap("b");
                }
                wait(10000);
                if(isPlayerTurn() && combat.waitingForPlayer)tap("b");
            }
            hpChanged |= monsterHP()<initialMonsters;
            enemyActed |= active->character.health.currentHP<initialPlayer;
            std::cout<<"turn="<<turn<<" monster_hp="<<monsterHP()<<" "<<pfme::runtime::diagnostic()<<'\n';
        }
        check(attacked,"Player attack via original menus");check(hpChanged,"Production attack applies monster HP change");
        check(enemyActed,"Production enemy turn applies player HP change");
        check(!combat.active && getActiveMapPlayer() && getActiveMapPlayer()->character.health.currentHP>0,"Survived and completed first combat");
        capture("play-after-combat.rgb565");
        const int beforeX=getActiveMapPlayer()->x,beforeY=getActiveMapPlayer()->y;
        bool moved=false;
        for(int d=0;d<8 && !moved;d++) {face(static_cast<Direction>(d));tap("encoder");auto* e=getActiveMapPlayer();moved=e && (e->x!=beforeX || e->y!=beforeY);}
        check(moved,"Continue exploring after combat");
        pfme::runtime::releaseInputs();
        std::cout<<"Production playthrough passed: title, creation, town, long press, dungeon, collision, menu, door, initiative, attack, HP, enemy AI, combat completion, continued exploration. Enemy damaged player="<<enemyActed<<'\n';
    } catch(const std::exception& error) {std::cerr<<"FAILED: "<<error.what()<<"; "<<pfme::runtime::diagnostic()<<'\n';return 1;}
}
