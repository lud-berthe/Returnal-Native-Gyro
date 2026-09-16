#include "rg/haptic_gate.hpp"
#include "rg/mixed.hpp"
#include <vector>
#include <iostream>
#include <cstdlib>
void check(bool v){if(!v){std::cerr<<"Haptic gate check failed\n";std::exit(1);}}
template<class T> void put(std::vector<unsigned char>& b,size_t at,T v){std::memcpy(b.data()+at,&v,sizeof(v));}
int main(){
 std::vector<unsigned char> graph(13232),axis(36);std::uint64_t name=0x30000002aULL;std::uintptr_t object=0x12345678;
 for(auto [at,on]:{std::pair{rg::mouseRumbleCall,false},std::pair{rg::controllerRumbleCall,true}}){graph[at]=0x45;put(graph,at+1,std::uint32_t(name));put(graph,at+5,std::uint32_t(name));put(graph,at+9,std::uint32_t(name>>32));graph[at+13]=on?0x27:0x28;graph[at+14]=0x16;}
 axis[18]=0x46;put(axis,19,object);axis[27]=0x1d;put(axis,28,std::uint32_t(rg::mouseAxisEntry));axis[32]=0x16;axis[33]=4;axis[34]=0xb;axis[35]=0x53;
 auto valid=[&]{return rg::validMouseRumbleBranch(graph,axis,object,name);};check(valid());
 // Reject a different function, branch, argument or cooked asset; never skip unknown bytecode.
 check(!rg::validMouseRumbleBranch(graph,axis,object+1,name));check(!rg::validMouseRumbleBranch(graph,axis,object,name+1));
 graph[rg::mouseRumbleCall+13]=0x27;check(!valid());graph[rg::mouseRumbleCall+13]=0x28;
 axis[28]++;check(!valid());axis[28]--;graph.pop_back();check(!valid());graph.push_back(0);check(valid());
 check(!rg::rumbleCallMatches({},0,name,false));check(!rg::rumbleCallMatches(graph,SIZE_MAX,name,false));
 check(rg::suppressPresentation(true,true,0,rg::InputEventKind::MouseMotion));
 check(!rg::suppressPresentation(false,true,0,rg::InputEventKind::MouseMotion));check(!rg::suppressPresentation(true,false,0,rg::InputEventKind::MouseMotion));check(!rg::suppressPresentation(true,true,2,rg::InputEventKind::MouseMotion));
 std::cout<<"Mouse rumble branch validation and policy passed\n";
}
