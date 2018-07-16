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

#include <cassert>
#include <cstdarg>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <rtosc/thread-link.h>
#include <rtosc/rtosc.h>

#include "icon.h"
#include "oscoutput.h"
#include "../Misc/Master.h"
#include "../Misc/Util.h"

using std::set;
using std::string;
using std::vector;

//Dummy variables and functions for linking purposes
namespace zyn {
    const char *instance_name = 0;
    class WavFile;
    namespace Nio {
        bool start(void){return 1;};
        void stop(void){};
        void masterSwap(Master *){};
        void waveNew(WavFile *){}
        void waveStart(void){}
        void waveStop(void){}
        void waveEnd(void){}
        bool setSource(string){return true;}
        bool setSink(string){return true;}
        set<string> getSources(void){return set<string>();}
        set<string> getSinks(void){return set<string>();}
        string getSource(void){return "";}
        string getSink(void){return "";}
    }
}


void ZynOscPlugin::run(/*float* outl, float* outr, unsigned long sample_count*/)
{
    check_osc();
    master->GetAudioOutSamples(p_buffersize,
                   (int)sampleRate,
                               p_out.left, p_out.right);
}

void ZynOscPlugin::ui_ext_show(bool show)
{
    if(show)
    {
        if(ui_pid)
        {
            // ui is already running
        }
        else
        {
            const char *addr = middleware->getServerAddress();
            ui_pid = fork();
            if(ui_pid < 0) {
                perror("fork() UI");
            }
            else if(ui_pid == 0)
            {
                // TODO: common func "launch fusion" (common with main.cpp)
                auto fusion_exec = [&](const char* path) {
                    execlp(path, "zyn-fusion", addr,
                           "--builtin", "--no-hotload",  0);
                };

                const char* fusion_path = getenv("ZYN_FUSION_PATH");
                printf("fusion_path: %s\n", fusion_path);
                if(fusion_path) {

                    const char* ld_library_path = getenv("LD_LIBRARY_PATH");
                    if(ld_library_path)
                    {
                        std::string new_path = ld_library_path;
                        new_path += ":";
                        new_path += fusion_path;
                        setenv("LD_LIBRARY_PATH", new_path.c_str(), 1);
                    }

                    std::string fusion_exe = fusion_path;
                    fusion_exe += "/zest";
                    printf("launching: %s\n", fusion_exe.c_str());
                    fusion_exec(fusion_exe.c_str());
                }
                fusion_exec("./zyn-fusion"); // whatever LD_LIBRARY_PATH...
                fusion_exec("zyn-fusion");

                perror("Failed to launch Zyn-Fusion");
            }
        }
    }
    else
    {
        hide_ui();
    }
}

const char** ZynOscPlugin::xpm_load() const
{
    return get_icon();
}

void ZynOscPlugin::check_osc()
{
    while(p_osc_in.read_msg())
    {
        const char* path = p_osc_in.path();
        // TODO: if (osc_in == match_path("/save_master", "s")
        if(!strcmp(path, "/save-master") ||
           !strcmp(path, "/load-master"))
        {
            spa::audio::assert_types_are(path, "s", p_osc_in.types());
            middleware->messageAnywhere((path[0] == 'l')
                                        ? "/load_xmz"
                                        : "/save_xmz", "s", p_osc_in.arg(0).s);
        }
        else
        {
            master->uToB->raw_write(path);
        }
    }
}

void ZynOscPlugin::hide_ui()
{
    if(ui_pid)
    {
        kill(ui_pid, SIGTERM);
        ui_pid = 0;
    }
    else
    {
        // zyn-fusion is not running
    }
}

unsigned ZynOscPlugin::net_port() const {
    const char* addr = middleware->getServerAddress();
    const char* port_str = strrchr(addr, ':');
    assert(port_str);
    return atoi(port_str + 1);
}

void ZynOscPlugin::masterChangedCallback(zyn::Master *m)
{
    master = m;
    master->setMasterChangedCallback(_masterChangedCallback, this);
}

void ZynOscPlugin::_masterChangedCallback(void *ptr, zyn::Master *m)
{
    ((ZynOscPlugin*)ptr)->masterChangedCallback(m);
}

extern "C" {
//! the main entry point
const spa::descriptor* spa_descriptor(unsigned long )
{
    return new ZynOscDescriptor;
}
}

ZynOscPlugin::ZynOscPlugin(unsigned long sampleRate)
    : p_osc_in(16384, 2048)
{
    zyn::SYNTH_T synth;
    synth.samplerate = sampleRate;

    this->sampleRate  = sampleRate;

    config.init();
    // disable compression for being conform with LMMS' format
    config.cfg.GzipCompression = 0;

    zyn::sprng(time(NULL));

    synth.alias();
    middleware = new zyn::MiddleWare(std::move(synth), &config);
    middleware->setUiCallback(_uiCallback, this);
    masterChangedCallback(middleware->spawnMaster());

    middlewareThread = new std::thread([this]() {
            while(middleware) {
            middleware->tick();
            usleep(1000);
            }});
}

ZynOscPlugin::~ZynOscPlugin()
{
    // usually, this is not our job...
    hide_ui();

    auto *tmp = middleware;
    middleware = 0;
    middlewareThread->join();
    delete tmp;
    delete middlewareThread;
}

void ZynOscPlugin::uiCallback(const char *msg)
{
    if(!strcmp(msg, "/save_osc") || !strcmp(msg, "/load_xmz"))
    {
        mutex_guard guard(cb_mutex);
#ifdef SAVE_OSC_DEBUG
        fprintf(stderr, "Received message \"%s\".\n", msg);
#endif
        recent.operation = msg;
        recent.file = rtosc_argument(msg, 0).s;
        recent.stamp = rtosc_argument(msg, 1).t;
        recent.status = rtosc_argument(msg, 2).T;
    }
    else if(!strcmp(msg, "/damage"))
    {
        // (ignore)
    }
    else if(!strcmp(msg, "/connection-remove"))
    {
        // TODO: save this info somewhere
    }
//    else
//        fprintf(stderr, "Unknown message \"%s\", ignoring...\n", msg);
}

spa::descriptor::hoster_t ZynOscDescriptor::hoster() const
{
    return hoster_t::github;
}

const char *ZynOscDescriptor::organization_url() const
{
    return nullptr; // none on SF
}

const char *ZynOscDescriptor::project_url() const
{
    return "zynaddsubfx";
}

const char* ZynOscDescriptor::label() const {
    return "ZASF"; /* conforming to zyn's DSSI plugin */ }

const char* ZynOscDescriptor::name() const { return "ZynAddSubFX"; }

const char* ZynOscDescriptor::authors() const {
    return "JohannesLorenz";
}

spa::descriptor::license_type ZynOscDescriptor::license() const {
    return license_type::gpl_2_0;
}

const char* ZynOscDescriptor::project() const { return "TODO"; }
ZynOscPlugin* ZynOscDescriptor::instantiate() const
{
    return new ZynOscPlugin(44100 /*TODO*/);
}
