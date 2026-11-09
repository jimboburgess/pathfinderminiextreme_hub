#include "runtime.h"
#include "town/town.h"
#include "data/game.h"
#include "characters/characters.h"
#include "characters/conditions.h"
#include "characters/items.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#define check(v) do { if(!(v)) throw std::runtime_error("recovery invariant failed at line "+std::to_string(__LINE__)); } while(false)
int main(int argc,char** argv){try{
 pfme::runtime::initialize(19);
 player=Character{};player.state=STATE_ALIVE;player.characterClass=argc>2?CLASS_WIZARD:CLASS_CLERIC;
 player.level=argc>1?std::atoi(argv[1]):1;
 player.health.maxHP=50;player.health.currentHP=1;
 player.magic.maxMP=40;player.magic.currentMP=0;
 player.abilities.constitution=12;player.abilities.charisma=18;player.abilities.intelligence=16;
 addCondition(player,CONDITION_POISONED,1,10);applySlowingVenom(player,15);
 gameState=GAME_TOWN;beginTownRest();for(unsigned i=0;i<28;++i)pfme::runtime::advance(100);updateTownRest();
 check(player.health.currentHP==(player.level==1?50:1+player.level+1));
 check(player.magic.currentMP==(player.level==1?40:player.level+(player.characterClass==CLASS_CLERIC?4:3)));
 check(getPoisonAffliction(player)==nullptr);
 for(unsigned i=0;i<player.conditions.count;++i)check(player.conditions.conditions[i].type!=CONDITION_POISONED&&player.conditions.conditions[i].type!=CONDITION_SLOWING_VENOM);
 auto hp=player.health.currentHP,mp=player.magic.currentMP;updateTownRest();check(player.health.currentHP==hp&&player.magic.currentMP==mp);
 for(int roll=1;roll<=8;++roll)check(calculateCureLightWoundsPotionHealing(roll)==roll+1);
 player.health.currentHP=49;addItem(player,ITEM_POTION_CURE_LIGHT_WOUNDS,1);int restored=0;
 check(useCureLightWoundsPotion(player,restored)&&restored==1&&player.health.currentHP==50);
 std::cout<<"PASS home rest HP/MP, poison cure, no repeated healing, potion d8+1 and HP cap\n";
 }catch(const std::exception& e){std::cerr<<e.what();return 1;}}
