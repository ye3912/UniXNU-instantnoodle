"""Apply the IN2020 display policy to the pinned plugin, retaining its runtime."""
from pathlib import Path
import shutil
HERE=Path(__file__).resolve().parent
root=Path.cwd();p=root/'drivers/AdrenoMetal/NativeAdrenoMetalPlugin.mm'
text=p.read_text()
anchor='// `site` is the slid address of that branch in the running QuartzCore; any other word there is left alone (0x6E41).'
assert text.count(anchor)==1
helper='''#include "In2020FramebufferPolicy.h"
static unsigned applyIn2020FramebufferPolicy() {
#if !defined(__arm64e__) && !TARGET_OS_OSX
    if (strcmp(getprogname(), "backboardd") || !bootArgSet("sm8250-fb-bgra=1")) return in2020fb::Disabled;
    void *start = dlsym(RTLD_DEFAULT, "CARenderServerStart");
    Dl_info info = {};
    if (!start || !dladdr(start, &info) || !info.dli_fbase) return in2020fb::Refused;
    return in2020fb::apply(true, reinterpret_cast<uintptr_t>(info.dli_fbase),
        [](uintptr_t p, void *out, size_t n) {return readTask(p,out,n);},
        [](uintptr_t p, const void *in, size_t n) {return writeTask(p,in,n);},
        [] {
            CFPreferencesSetAppValue(CFSTR("CAEnableDeepFramebuffer"), kCFBooleanFalse, kCFPreferencesCurrentApplication);
            Boolean valid = false;
            const Boolean value = CFPreferencesGetAppBooleanValue(CFSTR("CAEnableDeepFramebuffer"), kCFPreferencesCurrentApplication, &valid);
            return valid && !value;
        });
#else
    return in2020fb::Disabled;
#endif
}

'''
text=text.replace(anchor,helper+anchor)
anchor='''            unsigned bits = 0;
            if (CFTypeRef value = CFPreferencesCopyAppValue(CFSTR("CAEnableDeepFramebuffer"), kCFPreferencesCurrentApplication)) {'''
replacement='''            unsigned bits = 0;
            const unsigned framebufferPolicy = applyIn2020FramebufferPolicy();
            if (framebufferPolicy == in2020fb::PreferenceSet || framebufferPolicy == in2020fb::CachedFlagClear) bits |= 0x40;
            if (framebufferPolicy == in2020fb::CachedFlagClear) bits |= 0x80;
            if (CFTypeRef value = CFPreferencesCopyAppValue(CFSTR("CAEnableDeepFramebuffer"), kCFPreferencesCurrentApplication)) {'''
assert text.count(anchor)==1
p.write_text(text.replace(anchor,replacement))
shutil.copy2(HERE/'In2020FramebufferPolicy.h',p.parent/'In2020FramebufferPolicy.h')
print('Applied opt-in IN2020/21H461 BGRA policy; runtime and scanout validation unchanged')
