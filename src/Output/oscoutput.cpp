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

#include <cstdarg>
#include <unistd.h>
#include <csignal>
#include <rtosc/thread-link.h>
#include <rtosc/rtosc.h>

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


void ZynOscPlugin::runSynth(float* outl, float* outr, unsigned long sample_count)
{
    master->GetAudioOutSamples(sample_count,
			       (int)sampleRate,
			       outl, outr);
}

void ZynOscPlugin::sendOsc(const char* port, const char* args, ...)
{
    if(!strcmp(port, "/save-master"))
    {
        va_list ap;
        va_start(ap, args);
        middleware->messageAnywhere("/save_xmz", "s", va_arg(ap, const char*));
        va_end(ap);
    }
    else if(!strcmp(port, "/load-master"))
    {
        va_list ap;
        va_start(ap, args);
        middleware->messageAnywhere("/load_xmz", "s", va_arg(ap, const char*));
        va_end(ap);
    }
    else if(!strcmp(port, "/show-ui"))
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
				execlp("zyn-fusion", "zyn-fusion", addr, "--builtin", "--no-hotload",  0);
				execlp("./zyn-fusion", "zyn-fusion", addr, "--builtin", "--no-hotload",  0);

				perror("Failed to launch Zyn-Fusion");
			}
		}
	}
    else if(!strcmp(port, "/hide-ui"))
	{
        hide_ui();
	}
	else
	{
		va_list ap;
		va_start(ap, args);
		char buf[1024];

		rtosc_vmessage(buf, 1024, port, args, ap);
        master->uToB->raw_write(buf);
		va_end(ap);
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

unsigned long ZynOscPlugin::buffersize() const
{
    return master->synth.buffersize;
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
const OscDescriptor* osc_descriptor(unsigned long )
{
    return new ZynOscDescriptor;
}
}

ZynOscPlugin::ZynOscPlugin(unsigned long sampleRate)
{
    zyn::SYNTH_T synth;
    synth.samplerate = sampleRate;

    this->sampleRate  = sampleRate;

    config.init();

    zyn::sprng(time(NULL));

    synth.alias();
    middleware = new zyn::MiddleWare(std::move(synth), &config);
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

const char* ZynOscDescriptor::label() const { return "ZASF"; }
const char* ZynOscDescriptor::name() const { return "ZynAddSubFX"; }
const char* ZynOscDescriptor::maker() const {
    return "Nasca Octavian Paul <zynaddsubfx@yahoo.com>";
}
const char* ZynOscDescriptor::copyright() const {
    return "GNU General Public License v2 or later";
}

ZynOscPlugin* ZynOscDescriptor::instantiate(unsigned long srate) const
{
	return new ZynOscPlugin(srate);
}
