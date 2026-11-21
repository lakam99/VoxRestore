#include "Updater.h"
#include "AsyncCleanup.h"
VoxRestoreUpdater::VoxRestoreUpdater():state(std::make_shared<State>()),progress(state->progress),phase(state->phase){}
VoxRestoreUpdater::~VoxRestoreUpdater(){
    if(!worker)return;worker->signalThreadShouldExit();worker->notify();
    if(!worker->waitForThreadToExit(100)){auto* remaining=worker.release();deferVoxRestoreCleanup([remaining]{remaining->waitForThreadToExit(-1);delete remaining;});}
}
bool VoxRestoreUpdater::request(bool prepare,std::function<void(juce::var)> completion){
    if(worker&&worker->isThreadRunning())return false;worker.reset();progress.store(-1);phase.store(0);state->started.store(false);worker=std::make_unique<Worker>(state,prepare,std::move(completion));worker->startThread();return true;
}
