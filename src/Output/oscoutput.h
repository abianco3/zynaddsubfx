/*
  ZynAddSubFX - a software synthesizer

  oscoutput.h - Audio output for OSC plugins
  Copyright (C) 2017-2018 Johannes Lorenz
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

#include <spa/audio.h>

#include "../globals.h"
#include "../Misc/Config.h"
#include "../Misc/MiddleWare.h"

class ZynOscPlugin : public spa::plugin
{
    void check_osc();
public:
    static const spa::descriptor* getOscDescriptor(unsigned long index);
    static const ZynOscPlugin* instantiate(const spa::descriptor*, unsigned long sampleRate);

    void run() override;

    bool ui_ext() const override { return true; } // TODO...
    void ui_ext_show(bool show) override;

    const char** xpm_load() const override;

    std::map<std::string, port_ref_base*> ports;
    spa::port_ref_base& port(const char* pname) override;

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

    spa::audio::stereo::out p_out;
    spa::audio::stereo::in p_in;
    spa::audio::buffersize p_buffersize;
    spa::audio::osc_ringbuffer_in p_osc_in;

private:
    void hide_ui();

    pid_t ui_pid = 0;
    long sampleRate;
    zyn::MiddleWare *middleware;
    std::thread *middlewareThread;

    zyn::Config config;
    zyn::Master* master;

    virtual unsigned net_port() const;
};

class ZynOscDescriptor : public spa::descriptor
{
    hoster_t hoster() const override;
    const char* organization_url() const override;
    const char* project_url() const override;
    const char* label() const override;

    const char* project() const override;
    const char* name() const override;
    const char* authors() const override;

    license_type license() const override;

    virtual const char* description_line() const { return "ZynAddSubFX"; }
    virtual const char* description_full() const {
        return "The ZynAddSubFX synthesizer"; }

//	int id() const;

    ZynOscPlugin* instantiate() const;

    virtual spa::simple_vec<spa::simple_str> port_names() const override;
};

#endif
