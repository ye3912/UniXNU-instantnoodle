// SPDX-License-Identifier: MIT
// IN2020/21H461: negotiate the BGRA format implemented by the board presenter.
// No code pages or display validation rules are changed.
#pragma once
#include <stdint.h>
#include <string.h>
namespace in2020fb {
constexpr uintptr_t imageBase=0x189429000ULL;
constexpr uintptr_t choice=0x18966ea18ULL,onceSite=0x189538eacULL;
constexpr uintptr_t deep=0x1f491ae10ULL,once=0x1f491ae68ULL;
constexpr uint8_t uuid[16]={0xbc,0xa7,0x4c,0xf2,0x23,0x6a,0x30,0xac,0xbd,0x05,0x88,0x72,0xb2,0xc2,0x26,0x2d};
constexpr uint32_t choiceCode[8]={0x90359568,0x39784108,0x7100011f,0x528c2708,
                                0x72ac4668,0x528a4829,0x72a848e9,0x1a88013c};
constexpr uint32_t onceCode[3]={0xf9473768,0xb100051f,0x54000201};
enum Result:unsigned {Disabled=0,Refused=1,PreferenceSet=2,CachedFlagClear=3,Busy=4,WriteFailed=5,PreferenceFailed=6};
inline uint32_t u32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
inline uint64_t u64(const uint8_t *p){uint64_t v;memcpy(&v,p,8);return v;}
template<class Read,class Write,class Preference>
Result apply(bool enabled,uintptr_t base,Read read,Write write,Preference preference) {
    if(!enabled)return Disabled;
    if(base<imageBase)return Refused;
    const uintptr_t slide=base-imageBase;
    if(slide>UINTPTR_MAX-once-8)return Refused;
    uint8_t h[32];
    if(!read(base,h,sizeof h) || u32(h)!=0xfeedfacf || u32(h+4)!=0x0100000c ||
       (u32(h+8)&0xffffff)!=0 || u32(h+12)!=6)return Refused;
    const uint32_t count=u32(h+16),bytes=u32(h+20);
    if(!count || count>256 || bytes<24 || bytes>65536)return Refused;
    if(base>UINTPTR_MAX-32-bytes)return Refused;
    uintptr_t at=base+32,end=at+bytes;
    bool identity=false,flagWritable=false,onceWritable=false;
    for(uint32_t i=0;i<count;i++) {
        uint8_t c[72]={};
        if(at>end || end-at<8 || !read(at,c,8))return Refused;
        const uint32_t command=u32(c),size=u32(c+4);
        if(size<8 || size%8 || size>end-at)return Refused;
        if(command==0x1b) {
            if(size!=24 || !read(at,c,24) || memcmp(c+8,uuid,sizeof uuid))return Refused;
            identity=true;
        } else if(command==0x19) {
            if(size<72 || !read(at,c,72))return Refused;
            const uint64_t start=u64(c+24),length=u64(c+32);
            if(length>UINT64_MAX-start)return Refused;
            if((u32(c+60)&3)==3) {
                flagWritable|=start<=deep && deep-start<length;
                onceWritable|=start<=once && once-start<=length && length-(once-start)>=8;
            }
        }
        at+=size;
    }
    if(at!=end || !identity || !flagWritable || !onceWritable)return Refused;
    uint32_t code[8];
    if(!read(choice+slide,code,sizeof choiceCode) || memcmp(code,choiceCode,sizeof choiceCode) ||
       !read(onceSite+slide,code,sizeof onceCode) || memcmp(code,onceCode,sizeof onceCode))return Refused;
    uint64_t token=0;uint8_t flag=0;
    if(!read(once+slide,&token,8) || !read(deep+slide,&flag,1) || flag>1)return Refused;
    // The original initializer reads this exact application preference. Set it
    // in the process cache; do not synchronize or replace its preference file.
    if(!preference())return PreferenceFailed;
    if(token==0)return PreferenceSet;
    if(token!=UINT64_MAX)return Busy;
    // Initialization completed: the original choice uses the already-cached byte.
    // Its sole writer is that once initializer; changing the preference alone
    // cannot update it. Only this verified writable byte may be changed.
    if(flag) {
        const uint8_t off=0;
        if(!write(deep+slide,&off,1) || !read(deep+slide,&flag,1) || flag)return WriteFailed;
    }
    return CachedFlagClear;
}
}
