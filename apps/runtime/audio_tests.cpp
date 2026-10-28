#include "runtime.h"
#include "controller_slots.h"
#include "native_audio.h"
#include "native_rooms.h"
#include "audio/audio.h"
#include "data/game.h"
#include "input/menu.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>
using namespace pfme::runtime;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void wait(unsigned ms){while(ms){auto step=std::min(ms,10u);advance(step);ms-=step;}}
void tap(unsigned id,const char* action){check(controllerInput(id,action,true),"input");wait(30);check(controllerInput(id,action,false),"release");wait(310);}
void quiet(unsigned id){selectController(id);stopSound();resetAudioStream(id);}
}
int main() {
 try {
  pfme::GameSession session;check(session.initialize(2)==pfme::SessionError::Ok,"session");
  ControllerSlots slots(session);pfme::PlayerHandle handles[2];ControllerState* states[2]{};
  initializeControllers(2);
  for(unsigned id=0;id<2;++id) {
   check(session.join(id?"Audio1":"Audio0",handles[id])==pfme::SessionError::Ok,"join");
   check(slots.attach(handles[id],id+1,states[id])==ControllerError::Ok,"lease");
   check(bindController(id,*states[id]),"bind");quiet(id);
  }
  selectController(0);const auto start=audioTime();playSound(SoundEffect::MENU_MOVE);wait(25);
  auto click=readAudioStream(0,0,false);
  check(!click.reset&&click.count==2,"original menu tone/end events");
  check(click.events[0].frequency==1200&&click.events[0].duty==256&&click.events[0].at==start,"original tone and duty");
  check(click.events[1].frequency==0&&click.events[1].at-start==25,"original 25 ms tone duration");
  check(readAudioStream(1,0,false).count==0,"private UI audio isolation");
  quiet(0);const uint16_t notes[]={1234,2345,3456};const auto bellStart=audioTime();
  check(playToneSequence(notes,3,15,4,AudioDuty::DUTY_75),"production dynamic bell commands");wait(53);
  auto bells=readAudioStream(0,0,false);const unsigned times[]={0,15,19,34,38,53};
  const unsigned frequencies[]={1234,0,2345,0,3456,0};
  check(!bells.reset&&bells.count==6,"three original bell tones and pauses");
  for(unsigned i=0;i<6;++i)check(bells.events[i].at-bellStart==times[i]&&bells.events[i].frequency==frequencies[i]&&bells.events[i].duty==768,"exact C++ dynamic tone trace");
  const auto beforeAudio=audioTime(),beforeWorld=worldTime();delay(500);
  check(audioTime()==beforeAudio&&worldTime()==beforeWorld+500,"virtual rendering delay does not rush hardware audio timeline");

  std::ofstream trace("native-audio-trace.json");trace<<"{\"cases\":[";unsigned cases=0;
  for(unsigned value=1;value<static_cast<unsigned>(SoundEffect::COUNT);++value) {
   quiet(0);const auto began=audioTime();playSound(static_cast<SoundEffect>(value));uint64_t cursor=0;
   std::vector<AudioEvent> events;
   auto collect=[&](){auto batch=readAudioStream(0,cursor,false);check(!batch.reset,"bounded audio polling");cursor=batch.cursor;for(unsigned i=0;i<batch.count;++i)events.push_back(batch.events[i]);};
   collect();
   for(unsigned elapsed=0;isSoundPlaying()&&elapsed<10000;elapsed+=10){wait(10);collect();}
   check(!isSoundPlaying()&&!events.empty()&&events.back().frequency==0,"original sound terminates in silence");
   if(cases++)trace<<',';trace<<"{\"sound\":"<<value<<",\"start_ms\":"<<began<<",\"events\":[";
   for(unsigned i=0;i<events.size();++i){if(i)trace<<',';const auto& e=events[i];trace<<"[\""<<e.sequence<<"\","<<e.at<<','<<e.frequency<<','<<e.duty<<']';}
   trace<<"]}";
  }
  trace<<"]}\n";trace.close();
  for(unsigned id=0;id<2;++id) {
   for(unsigned press=0;press<5;++press)tap(id,"a");
   check(gameState==GAME_TOWN,"original character creation for audio audiences");
  }
  for(unsigned id=0;id<2;++id){tap(id,"cw");tap(id,"cw");tap(id,"a");if(menuState.isOpen)tap(id,"a");}
  quiet(0);quiet(1);selectController(0);playSound(SoundEffect::WALK);
  auto first=readAudioStream(0,0,false),second=readAudioStream(1,0,false);
  check(first.count&&second.count&&first.events[0].frequency>0&&second.events[0].frequency>0&&first.events[0].duty==second.events[0].duty,"same-room world audio reaches both listeners");
  const auto otherCursor=second.cursor;playSound(SoundEffect::MENU_SELECT);
  check(readAudioStream(1,otherCursor,false).count==0,"private menu does not interrupt another listener");
  for(unsigned step=0;nativeActorRoom(0)==0&&step<16;++step)tap(0,"encoder");
  check(nativeActorRoom(0)==1&&nativeActorRoom(1)==0,"separate room audience fixture");
  quiet(0);quiet(1);selectController(0);playSound(SoundEffect::SPELL_CAST);
  check(readAudioStream(0,0,false).count>0&&readAudioStream(1,0,false).count==0,"audio does not reveal another room");
  setAudioMuted(true);quiet(0);playSound(SoundEffect::MENU_SELECT);check(readAudioStream(0,0,false).frequency==0,"original mute semantics");
  selectController(1);playSound(SoundEffect::MENU_SELECT);check(readAudioStream(1,0,false).frequency>0,"mute is local to its controller");
  resetAudioStream(0);for(unsigned i=0;i<600;++i)emitNativeAudio(0,static_cast<uint16_t>(1000+i),256);
  const auto resync=readAudioStream(0,0,false);check(resync.reset&&resync.count==0&&resync.cursor==600&&resync.frequency==1599,"bounded history resynchronizes instead of replaying old notes");
  std::cout<<"PASS original_audio_cases="<<cases<<" exact_dynamic_bells=true private_ui=true room_audiences=true per_listener_mute=true bounded_resync=true\n";
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
