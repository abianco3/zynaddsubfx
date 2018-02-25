/*
  ZynAddSubFX - a software synthesizer

  oscoutput.h - Audio output for OSC plugins
  Copyright (C) 2017 Johannes Lorenz
  Author: Johannes Lorenz

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
*/

#ifndef OSC_AUDIO_OUTPUT_PLUGIN_H
#define OSC_AUDIO_OUTPUT_PLUGIN_H

#include <thread>
#include <mutex>
// #include <queue>

#include "./osc-interface.h" // TODO: path

#include "../globals.h"
#include "../Misc/Config.h"
#include "../Misc/MiddleWare.h"

class ZynOscPlugin : public OscConsumer
{
public:
    static const OscDescriptor* getOscDescriptor(unsigned long index);
    static const ZynOscPlugin* instantiate(const OscDescriptor*, unsigned long sampleRate);

    void runSynth(float* outl, float* outr, unsigned long sample_count) override;
    void sendOsc(const char *port, const char *args, ...) override;
    bool recvOsc(const char** msg) override;
    unsigned long buffersize() const override;

    // TODO: handle UI callback?

    void masterChangedCallback(zyn::Master* m);
    static void _masterChangedCallback(void* ptr, zyn::Master* m);

    std::mutex cb_mutex;
    using mutex_guard = std::lock_guard<std::mutex>;

    struct {
        std::string operation;
        std::string file;
        uint64_t stamp;
        bool status;
    } recent;

public:	// FEATURE: make these private?
    /**
     * The sole constructor.
     * @param sampleRate the sample rate to be used by the synth.
     */
    ZynOscPlugin(unsigned long sampleRate);
    ~ZynOscPlugin();

    static void _uiCallback(void* ptr, const char* msg)
    {
        ((ZynOscPlugin*)ptr)->uiCallback(msg);
    }

    void uiCallback(const char* msg);
private:
    void hide_ui();

    pid_t ui_pid = 0;
    long sampleRate;
    zyn::MiddleWare *middleware;
    std::thread *middlewareThread;

    zyn::Config config;
    zyn::Master* master;
};

class ZynOscDescriptor : public OscDescriptor
{
// virtual functions
    const char* label() const;
    const char* name() const;
    const char* maker() const;
    license_type license() const;
//	int id() const;
    ZynOscPlugin* instantiate(unsigned long srate) const;
};

#endif
