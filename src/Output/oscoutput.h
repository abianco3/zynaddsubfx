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

#include "./oscplugin.h"

#include "../globals.h"
#include "../Misc/Config.h"
#include "../Misc/MiddleWare.h"

class ZynOscPlugin : public OscPlugin
{
public:
	static const OscDescriptor* getOscDescriptor(unsigned long index);
	static const ZynOscPlugin* instantiate(const OscDescriptor*, unsigned long sampleRate);

	void runSynth(float* outl, float* outr, unsigned long sample_count) override;
	void sendOsc(const char *port, const char *args, ...) override;
	unsigned long buffersize() const override;

public:	// FEATURE: make these private?
	/**
	 * The sole constructor.
	 * @param sampleRate [in] the sample rate to be used by the synth.
	 */
	ZynOscPlugin(unsigned long sampleRate);
	~ZynOscPlugin();
private:
	pid_t ui_pid = 0;
	long sampleRate;
	MiddleWare *middleware;
	std::thread *middlewareThread;

	Config config;
};

class ZynOscDescriptor : public OscDescriptor
{
	virtual const char* label() const;
	virtual const char* name() const;
	virtual const char* maker() const;
	virtual const char* copyright() const;
	virtual int id() const;
	virtual ZynOscPlugin* instantiate(unsigned long srate) const;
};

#endif
