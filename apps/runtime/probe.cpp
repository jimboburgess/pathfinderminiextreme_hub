#include "runtime.h"
#include <iostream>
#include <fstream>
#include "dungeon/dungeon.h"
#include "map/activemap.h"
#include "input/menu.h"
#include "dungeon/combat.h"
int main(int argc,char** argv) {
    pfme::runtime::initialize(argc>1?static_cast<uint32_t>(std::stoul(argv[1])):12345);
    std::string command;
    while(std::cin>>command) {
        if(command=="wait") { unsigned ms; std::cin>>ms; for(unsigned i=0;i<ms;i+=10) pfme::runtime::advance(10); }
        else if(command=="map") {
            for(int y=0;y<14;y++){for(int x=0;x<15;x++)std::cout<<int(getActiveMapTile(x,y))<<',';std::cout<<'\n';}
            for(int i=0;i<dungeon.entityCount;i++){const auto& e=dungeon.entities[i];std::cout<<"entity "<<i<<" type="<<int(e.type)<<" x="<<int(e.x)<<" y="<<int(e.y)<<" hp="<<e.character.health.currentHP<<" active="<<e.active<<'\n';}
            std::cout<<"turn="<<int(combat.currentTurnIndex)<<" waiting="<<combat.waitingForPlayer<<" phase="<<int(combat.phase)<<'\n';
        }
        else if(command=="frame") {
            std::ofstream f("runtime.rgb565",std::ios::binary);
            const auto* p=pfme::runtime::pixels();
            for(int i=0;i<57600;i++) { f.put(char(p[i]&255)); f.put(char(p[i]>>8)); }
        } else if(command=="quit") break;
        else { pfme::runtime::input(command,true); pfme::runtime::advance(30); pfme::runtime::input(command,false); for(int i=0;i<32;i++) pfme::runtime::advance(10); }
        std::cout<<pfme::runtime::diagnostic()<<'\n';
    }
}
