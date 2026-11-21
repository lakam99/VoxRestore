#include "Updater.h"

namespace {
std::array<int,3> stableVersion(juce::String text){
    if(text.startsWithChar('v'))text=text.substring(1);auto parts=juce::StringArray::fromTokens(text,".",{});
    if(parts.size()!=3)throw std::runtime_error("Invalid stable release version.");std::array<int,3> version{};
    for(int i=0;i<3;++i){if(parts[i].isEmpty()||parts[i].length()>6||!parts[i].containsOnly("0123456789")||(parts[i].length()>1&&parts[i].startsWithChar('0')))throw std::runtime_error("Invalid stable release version.");version[(size_t)i]=parts[i].getIntValue();}return version;
}
}
void VoxRestoreUpdater::Worker::run(){
    ownedState->started.store(true);int delay=ownedState->testDelay.exchange(0);if(delay>0)sleep(delay);if(threadShouldExit())return;
    auto* object=new juce::DynamicObject;juce::var result(object);
    try{
        auto stream=juce::URL("https://api.github.com/repos/lakam99/VoxRestore/releases/latest").createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(20000).withExtraHeaders("User-Agent: VoxRestore/" VOX_VERSION "\r\nAccept: application/vnd.github+json\r\n"));
        if(!stream)throw std::runtime_error("Could not reach GitHub Releases.");juce::MemoryBlock bytes;stream->readIntoMemoryBlock(bytes,2097153);if(bytes.getSize()>2097152)throw std::runtime_error("Release metadata is too large.");
        auto release=juce::JSON::parse(bytes.toString());auto tag=release["tag_name"].toString();auto version=stableVersion(tag);auto current=stableVersion(VOX_VERSION);
        if(!release.isObject()||(bool)release["draft"]||(bool)release["prerelease"])throw std::runtime_error("Expected a stable published release.");
        juce::String name="VoxRestore-"+tag.substring(1)+"-macOS-arm64.zip";bool found=false;
        if(auto* assets=release["assets"].getArray())for(auto& asset:*assets)if(asset["name"].toString()==name&&asset["state"].toString()=="uploaded"){
            auto expected="https://github.com/lakam99/VoxRestore/releases/download/"+tag+"/"+name;
            if(asset["browser_download_url"].toString()!=expected)throw std::runtime_error("Unexpected release download URL.");found=true;
        }
        if(version>current&&!found)throw std::runtime_error("The newer release has no Apple Silicon package yet.");
        object->setProperty("passed",true);object->setProperty("available",version>current&&found);object->setProperty("version",tag.substring(1));object->setProperty("releaseUrl","https://github.com/lakam99/VoxRestore/releases/tag/"+tag);object->setProperty("manual_install",true);
    }catch(const std::exception& error){object->setProperty("passed",false);object->setProperty("error",juce::String(error.what()));}
    if(!threadShouldExit())juce::MessageManager::callAsync([completion=callback,result]{if(completion)completion(result);});
}
bool VoxRestoreUpdater::installAndRestart(const juce::var&,juce::String& error){error="Install the Apple Silicon package from GitHub Releases after closing VoxRestore and its host.";return false;}
