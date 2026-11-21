#include "VoiceDetector.h"
#include "AsyncCleanup.h"
#include <juce_dsp/juce_dsp.h>
#include "BinaryData.h"
#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>
#if JUCE_WINDOWS
 #define NOMINMAX
 #include <windows.h>
#endif
#include <mutex>
#if JUCE_MAC
#include <dlfcn.h>
#endif

namespace {
std::once_flag initFlag;
void initialiseRuntime() {
    std::call_once(initFlag,[]{
       #if JUCE_WINDOWS
        HMODULE own=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&initialiseRuntime),&own);
        wchar_t path[32768]{}; GetModuleFileNameW(own,path,32768);
        juce::File dll=juce::File(juce::String(path)).getSiblingFile("onnxruntime.dll");
        auto module=LoadLibraryExW(dll.getFullPathName().toWideCharPointer(),nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!module) throw std::runtime_error("onnxruntime.dll must stay beside VoxRestore.");
        using GetBase=const OrtApiBase* (ORT_API_CALL*)();
        auto getBase=reinterpret_cast<GetBase>(GetProcAddress(module,"OrtGetApiBase"));
        if(!getBase) throw std::runtime_error("Incompatible ONNX Runtime.");
        auto api=getBase()->GetApi(ORT_API_VERSION);
        if(!api) throw std::runtime_error("ONNX Runtime API version mismatch.");
        Ort::InitApi(api);
       #elif JUCE_MAC
        Dl_info location{};
        if(!dladdr(reinterpret_cast<void*>(&initialiseRuntime),&location)||!location.dli_fname)throw std::runtime_error("Could not locate the VoxRestore bundle.");
        auto library=juce::File(juce::String::fromUTF8(location.dli_fname)).getParentDirectory().getSiblingFile("Frameworks").getChildFile("libonnxruntime.dylib");
        auto module=dlopen(library.getFullPathName().toRawUTF8(),RTLD_NOW|RTLD_LOCAL);
        if(!module)throw std::runtime_error("The bundled Apple Silicon speech runtime could not be loaded.");
        using GetBase=const OrtApiBase* (*)();auto getBase=reinterpret_cast<GetBase>(dlsym(module,"OrtGetApiBase"));
        if(!getBase)throw std::runtime_error("Incompatible speech runtime.");
        auto api=getBase()->GetApi(ORT_API_VERSION);if(!api)throw std::runtime_error("Speech runtime API version mismatch.");Ort::InitApi(api);
       #endif
    });
}
}

struct VoiceDetector::Impl {
    // Release sessions and the environment on their owning analysis worker,
    // never from a plugin module's global destructors under the loader lock.
    Ort::Env env{nullptr};
    Ort::Session session{nullptr};
    Ort::MemoryInfo memory{nullptr};
    std::array<float,576> input{};
    std::array<float,256> state{};
    std::array<float,64> context{};
    Impl() {
        initialiseRuntime();env=Ort::Env(ORT_LOGGING_LEVEL_ERROR,"VoxRestore");env.DisableTelemetryEvents();
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(1); options.SetInterOpNumThreads(1);
        options.AddConfigEntry("session.intra_op.allow_spinning","0");
        options.AddConfigEntry("session.inter_op.allow_spinning","0");
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        session=Ort::Session(env,BinaryData::silero_vad_onnx,(size_t)BinaryData::silero_vad_onnxSize,options);
        memory=Ort::MemoryInfo::CreateCpu(OrtArenaAllocator,OrtMemTypeDefault);
    }
};
VoiceDetector::VoiceDetector():impl(std::make_unique<Impl>()) {}
VoiceDetector::~VoiceDetector()=default;
void VoiceDetector::reset() { impl->state.fill(0); impl->context.fill(0); }
float VoiceDetector::analyse(const float* frame) {
    auto& p=*impl;
    std::copy(p.context.begin(),p.context.end(),p.input.begin());
    for(int i=0;i<512;++i) p.input[(size_t)i+64]=juce::jlimit(-1.0f,1.0f,frame[i]*4);
    std::array<int64_t,2> inputShape{1,576};
    std::array<int64_t,3> stateShape{2,1,128};
    int64_t rate=16000;
    std::array<Ort::Value,3> values{
        Ort::Value::CreateTensor<float>(p.memory,p.input.data(),p.input.size(),inputShape.data(),inputShape.size()),
        Ort::Value::CreateTensor<float>(p.memory,p.state.data(),p.state.size(),stateShape.data(),stateShape.size()),
        Ort::Value::CreateTensor<int64_t>(p.memory,&rate,1,nullptr,0)};
    const char* names[]{"input","state","sr"}; const char* outputs[]{"output","stateN"};
    auto result=p.session.Run(Ort::RunOptions{nullptr},names,values.data(),3,outputs,2);
    std::copy_n(result[1].GetTensorData<float>(),256,p.state.begin());
    std::copy(p.input.end()-64,p.input.end(),p.context.begin());
    return juce::jlimit(0.0f,1.0f,result[0].GetTensorData<float>()[0]);
}
