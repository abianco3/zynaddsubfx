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


void ZynOscPlugin::runSynth(float* outl, float* outr, unsigned long sample_count)
{
    Master *master = middleware->spawnMaster();
    master->GetAudioOutSamples(sample_count,
			       (int)sampleRate,
			       outl, outr);
}

void ZynOscPlugin::sendOsc(const char* port, const char* args, ...)
{
	if(!strcmp(port, "/show-ui"))
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
	else if(!strcmp(port, "/close-ui"))
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
	else
	{
		va_list ap;
		va_start(ap, args);
		char buf[1024];

		rtosc_vmessage(buf, 1024, port, args, ap);
		middleware->spawnMaster()->uToB->raw_write(buf);
		va_end(ap);
	}
}

unsigned long ZynOscPlugin::buffersize() const
{
	return middleware->spawnMaster()->synth.buffersize;
}

//! the main entry point
const OscDescriptor* osc_descriptor(unsigned long )
{
    return new ZynOscDescriptor;
}

ZynOscPlugin::ZynOscPlugin(unsigned long sampleRate)
{
    SYNTH_T synth;
    synth.samplerate = sampleRate;

    this->sampleRate  = sampleRate;

    config.init();

    sprng(time(NULL));

    synth.alias();
    middleware = new MiddleWare(std::move(synth), &config);
    middlewareThread = new std::thread([this]() {
            while(middleware) {
            middleware->tick();
            usleep(1000);
            }});
}

ZynOscPlugin::~ZynOscPlugin()
{
    auto *tmp = middleware;
    middleware = 0;
    middlewareThread->join();
    delete tmp;
    delete middlewareThread;
}

ZynOscPlugin* ZynOscDescriptor::instantiate(unsigned long srate) const
{
	return new ZynOscPlugin(srate);
}
