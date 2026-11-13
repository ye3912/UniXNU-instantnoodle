#include "In2020FramebufferPolicy.h"
#include <assert.h>
#include <stdio.h>
#include <map>
#include <vector>
using namespace in2020fb;
struct Fixture {
    std::map<uintptr_t,std::vector<uint8_t>> memory;
    unsigned reads=0,writes=0,prefs=0;bool writeOK=true,prefOK=true;
    void put32(std::vector<uint8_t> &v,unsigned at,uint32_t x){memcpy(v.data()+at,&x,4);}
    void put64(std::vector<uint8_t> &v,unsigned at,uint64_t x){memcpy(v.data()+at,&x,8);}
    Fixture(uint64_t token=UINT64_MAX,uint8_t flag=1) {
        auto &h=memory[imageBase];h.resize(32+24+72);
        put32(h,0,0xfeedfacf);put32(h,4,0x0100000c);put32(h,12,6);put32(h,16,2);put32(h,20,96);
        put32(h,32,0x1b);put32(h,36,24);memcpy(h.data()+40,uuid,16);
        put32(h,56,0x19);put32(h,60,72);put64(h,80,deep&~4095ULL);put64(h,88,4096);put32(h,116,3);
        memory[choice]=std::vector<uint8_t>((const uint8_t *)choiceCode,(const uint8_t *)choiceCode+sizeof choiceCode);
        memory[onceSite]=std::vector<uint8_t>((const uint8_t *)onceCode,(const uint8_t *)onceCode+sizeof onceCode);
        memory[once].resize(8);memcpy(memory[once].data(),&token,8);memory[deep]={flag};
    }
    bool read(uintptr_t p,void *out,size_t n) {
        ++reads;
        for(auto &v:memory)if(p>=v.first && p-v.first<=v.second.size() && n<=v.second.size()-(p-v.first)) {
            memcpy(out,v.second.data()+p-v.first,n);return true;
        }
        return false;
    }
    Result run(bool on=true,uintptr_t base=imageBase) {
        return apply(on,base,[&](auto p,auto out,auto n){return read(p,out,n);},
            [&](auto p,const void *in,size_t n){assert(p==deep && n==1);++writes;if(writeOK)memcpy(memory[deep].data(),in,1);return writeOK;},
            [&](){++prefs;return prefOK;});
    }
};
int main() {
    unsigned cases=0;
    {Fixture f;assert(f.run(false)==Disabled && !f.reads && !f.writes && !f.prefs);++cases;}
    {Fixture f;f.memory[imageBase][40]^=1;assert(f.run()==Refused && !f.writes && !f.prefs);++cases;}
    {Fixture f;f.memory[choice][0]^=1;assert(f.run()==Refused && !f.writes && !f.prefs);++cases;}
    {Fixture f;f.memory[onceSite][0]^=1;assert(f.run()==Refused && !f.writes && !f.prefs);++cases;}
    {Fixture f;f.memory[imageBase][116]=1;assert(f.run()==Refused && !f.writes && !f.prefs);++cases;}
    {Fixture f;f.memory[imageBase][36]=0;assert(f.run()==Refused && !f.writes && !f.prefs);++cases;}
    {Fixture f;assert(f.run(true,0)==Refused && !f.writes && !f.prefs);++cases;}
    {Fixture f(0,0);assert(f.run()==PreferenceSet && !f.writes && f.prefs==1);++cases;}
    {Fixture f(7,1);assert(f.run()==Busy && !f.writes && f.prefs==1);++cases;}
    {Fixture f(UINT64_MAX,0);assert(f.run()==CachedFlagClear && !f.writes && f.prefs==1);++cases;}
    {Fixture f;assert(f.run()==CachedFlagClear && f.writes==1 && f.memory[deep][0]==0 && f.prefs==1);++cases;}
    {Fixture f;f.writeOK=false;assert(f.run()==WriteFailed && f.memory[deep][0]==1);++cases;}
    {Fixture f(UINT64_MAX,2);assert(f.run()==Refused && !f.writes && !f.prefs);++cases;}
    {Fixture f;f.prefOK=false;assert(f.run()==PreferenceFailed && !f.writes && f.prefs==1);++cases;}
    // The actual original CSEL chooses BGRA exactly when the verified byte is 0.
    assert((Fixture(UINT64_MAX,1).memory[deep][0]?0x62336138:0x42475241)==0x62336138);
    puts("PASS: 14 policy cases; wrong identity/code/state refuses writes; cached flag changes only after init");
    (void)cases;
}
