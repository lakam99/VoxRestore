#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class VoxRestoreUpdater final {
    struct State {std::atomic<double> progress{-1};std::atomic<int> phase{0},testDelay{0};std::atomic<bool> started{false};};
    std::shared_ptr<State> state;
public:
    VoxRestoreUpdater();
    ~VoxRestoreUpdater();
    bool request(bool prepare, std::function<void(juce::var)> completion);
    static bool installAndRestart(const juce::var& prepared, juce::String& error);
    void delayNextRequestForTest(int milliseconds){state->testDelay.store(milliseconds);}
    bool startedForTest() const{return state->started.load();}
    std::atomic<double>& progress;
    std::atomic<int>& phase;
    juce::String progressStage() const {static constexpr const char* stages[]{"CHECKING","DOWNLOADING","VERIFYING","EXTRACTING","CHECKING FILES","READY"};return stages[juce::jlimit(0,5,phase.load())];}
private:
    struct Worker final:juce::Thread {
        Worker(std::shared_ptr<State> shared,bool prepare,std::function<void(juce::var)> completion):Thread("VoxRestore release checker"),ownedState(std::move(shared)),progress(ownedState->progress),phase(ownedState->phase),preparing(prepare),callback(std::move(completion)){}
        void run() override;
        std::shared_ptr<State> ownedState;
        std::atomic<double>& progress;
        std::atomic<int>& phase;
        bool preparing=false;
        std::function<void(juce::var)> callback;
    };
    std::unique_ptr<Worker> worker;
};
