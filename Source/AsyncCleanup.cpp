#include "AsyncCleanup.h"
#include <thread>
#if JUCE_WINDOWS
#include <windows.h>
#elif JUCE_MAC
#include <dlfcn.h>
#endif
namespace {
std::atomic<int> pending{0};
bool keepModuleMapped(){
#if JUCE_WINDOWS
    HMODULE module=nullptr;return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&keepModuleMapped),&module)!=0;
#elif JUCE_MAC
    Dl_info info{};if(!dladdr(reinterpret_cast<void*>(&keepModuleMapped),&info)||!info.dli_fname)return false;
    if(juce::File(juce::String::fromUTF8(info.dli_fname))==juce::File::getSpecialLocation(juce::File::currentExecutableFile))return true;
    // Deliberately retain this reference: a host may unload a removed VST3
    // while an uninterruptible third-party operation is still unwinding.
    return dlopen(info.dli_fname,RTLD_NOW|RTLD_LOCAL)!=nullptr;
#else
    return false;
#endif
}
}
void deferVoxRestoreCleanup(std::function<void()> cleanup){
    if(!keepModuleMapped()){cleanup();return;}
    auto function=std::make_shared<std::function<void()>>(std::move(cleanup));
    ++pending;
    try{std::thread([function]{(*function)();--pending;}).detach();}
    catch(...){--pending;(*function)();}
}
int pendingVoxRestoreCleanups(){return pending.load();}
