#include "rg/mixed.hpp"
#include "rg/motion.hpp"
#include "rg/panel_scale.hpp"
#include <thread>
#include <atomic>
#include <iostream>
#include <stdexcept>
using rg::InputEventKind;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(){try{
    check(rg::panelScale(0,96,1080)==1,"1080p scale");
    check(rg::panelScale(0,96,2160)==2,"4K at 100 percent scales 2x");
    check(rg::panelScale(0,192,2160)==2,"4K DPI does not double-apply scale");
    check(rg::panelScale(0,144,1080)==1.5f,"DPI honored");
    check(rg::panelScale(2.5f,96,1080)==2.5f,"user override honored");
    check(rg::panelScale(0,500,4320)==3,"auto scale bounded");
    for(auto event:{InputEventKind::MouseMotion,InputEventKind::KeyboardOrMouseButton,InputEventKind::Controller})check(!rg::suppressPresentation(false,true,1,event),"disabled is pass-through");
    check(rg::suppressPresentation(true,true,0,InputEventKind::MouseMotion),"auto suppresses mouse motion");
    check(!rg::suppressPresentation(true,true,0,InputEventKind::KeyboardOrMouseButton),"auto allows real KB/M");
    check(!rg::suppressPresentation(true,true,0,InputEventKind::Controller),"auto allows controller");
    check(!rg::suppressPresentation(true,false,0,InputEventKind::MouseMotion),"mouse opt-out honored");
    check(rg::suppressPresentation(true,true,1,InputEventKind::KeyboardOrMouseButton),"controller lock");
    check(rg::suppressPresentation(true,true,2,InputEventKind::Controller),"KBM lock");
    // Model-only A/B/C regression: events always reach gameplay; presentation cannot reset held state.
    // This deliberately does not substitute for Returnal's physical Alt-Fire/trigger tests.
    for(int scenario=0;scenario<3;++scenario){
        bool controllerPresentation=true,held=true;int delivered=0;
        for(int tick=0;tick<60000;++tick){auto event=(scenario==1&&tick%20==0)?InputEventKind::Controller:InputEventKind::MouseMotion;
            ++delivered;
            if(!rg::suppressPresentation(true,true,0,event)){bool next=event==InputEventKind::Controller;if(next!=controllerPresentation)held=false;controllerPresentation=next;}
        }
        check(held&&controllerPresentation&&delivered==60000,"held-state model survives mixed stream");
    }
    rg::MotionQueue<8> generations;
    check(generations.push({{10,0},1,1}),"old activation interval queued");
    check(generations.push({{2,0},2,2}),"new activation interval queued");
    check(generations.consume(3,true,2).yawDegrees==2,"ratchet/reconnect epoch discards earlier queued rotation");
    rg::MotionQueue<64> queue;std::atomic<bool> done{};constexpr int count=100000;double yaw=0,pitch=0;
    std::thread producer([&]{for(int i=0;i<count;++i)while(!queue.push({{1,-2},1}))std::this_thread::yield();done.store(true,std::memory_order_release);});
    while(!done.load(std::memory_order_acquire)){auto value=queue.consume(2,true);yaw+=value.yawDegrees;pitch+=value.pitchDegrees;}
    producer.join();auto rest=queue.consume(2,true);yaw+=rest.yawDegrees;pitch+=rest.pitchDegrees;
    check(yaw==count&&pitch==-2*count,"concurrent SPSC preserves every accepted delta");
    std::cout<<"PASS: mixed-input policy, modeled held-state cases, concurrent queue, and 4K/DPI scaling\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
