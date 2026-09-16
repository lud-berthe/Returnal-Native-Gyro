#pragma once
#include "backend.hpp"
#include <windows.h>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <fstream>
#include <string>
namespace rg {
struct Runtime {
    std::filesystem::path directory;
    std::filesystem::path previewOutput; // Development preview only; empty in the game.
    std::mutex settingsMutex, diagnosticsMutex, logMutex;
    Settings settings;
    std::atomic<unsigned> revision{1}, calibrationCommand{}, gameFlags{}, blockedReasons{}, mixedFlags{3};
    std::atomic<std::uint64_t> gameTime{}, sampleTime{}, motionEpoch{1}, suppressed{}, inputCalls{}, cameraCalls{}, queueDrops{};
    DWORD externalHostPid{};
    std::atomic<std::uint64_t> panelFrames{};
    std::atomic<bool> panelActuallyVisible{},panelExit{},panelOpen{}, suspended{}, nativeReady{}, mixedReady{}, sensorActive{};
    std::atomic<float> appliedYaw{}, appliedPitch{}, controlYaw{}, controlPitch{};
    std::atomic<int> presentation{-1};
    MotionQueue<> queue;
    MotionDiagnostics diagnostics;
    std::string device="Waiting for a motion controller", status="Validating installed build";
    std::ofstream logFile;
    std::atomic<bool> toggleEnabled{true};
    std::atomic<float> flickYaw{},flickX{},flickY{};
    std::atomic<unsigned> flickSuppressed{};
    std::atomic<ControllerLayout> controllerLayout{ControllerLayout::Sony};
    std::atomic<bool> controllerTouchpad{};
    std::atomic<std::uint32_t> availableButtons{};
    std::atomic<bool> externalCalibration{};
    std::atomic<std::uint32_t> controllerButtons{};
    Settings snapshot();
    void update(const Settings&);
    void log(const std::string&);
};
// Process-lifetime runtime: hooked modules must never be unloaded while a game call is in flight.
extern Runtime* runtime;
bool installBindings(Runtime&);
bool installCanvasPanel(Runtime&);
bool installNativeMenu(Runtime&);
void sensorLoop(Runtime&);
void panelLoop(Runtime&,HINSTANCE);
void controlLoop(Runtime&);
}
