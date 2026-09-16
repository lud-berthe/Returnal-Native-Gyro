#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace rg {
// The Steam handle is the identity. Neither an XInput slot nor a product/name
// match is sufficient to reuse orientation from a previous controller.
class MotionSession {
 std::string identity_;bool external_{};std::uint64_t lastSample_{};
public:
 bool reconnect(std::string_view identity,bool external,std::uint64_t now){
  bool resume=external&&external_&&identity.starts_with("steam:")&&identity.size()>6&&identity==identity_&&lastSample_&&now>=lastSample_&&now-lastSample_<=2'000'000'000;
  identity_=identity;external_=external;lastSample_=0;return resume;
 }
 void sample(std::uint64_t now){lastSample_=now;}
};
}
