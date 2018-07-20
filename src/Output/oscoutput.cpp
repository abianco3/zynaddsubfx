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

/*// TODO: make ports with full zyn metadata
struct zyn_port
{
    bool linear; //!< vs log

};*/

spa::port_ref_base* port_from_osc_args(const char* args)
{
    if(!args)
        return nullptr;
    for(; *args == ':'; ++args) ;
    if(strchr(args, 'S'))
        return new spa::audio::control_in<int>; // TODO: separate port type
    else switch(*args)
    {
        case 'T':
        case 'F':
            return new spa::audio::control_in<bool>;
        case 'i': return new spa::audio::control_in<int>;
        case 'h': return new spa::audio::control_in<long long int>;
        case 'f': return new spa::audio::control_in<float>;
        case 'd': return new spa::audio::control_in<double>;
        default: return nullptr;
    }
}

struct set_min : public spa::audio::visitor
{
    const char* min = nullptr;
    template<class T> using ci = spa::audio::control_in<T>;
    void visit(ci<int> &p) override { p.min = atoi(min); }
    void visit(ci<long long int> &p) override { p.min = atoll(min); }
    void visit(ci<float> &p) override { p.min = atof(min); }
    void visit(ci<double> &p) override { p.min = atof(min); }
};

struct set_max : public spa::audio::visitor
{
    const char* max = nullptr;
    template<class T> using ci = spa::audio::control_in<T>;
    void visit(ci<int> &p) override { p.max = atoi(max); }
    void visit(ci<long long int> &p) override { p.min = atoll(max); }
    void visit(ci<float> &p) override { p.min = atof(max); }
    void visit(ci<double> &p) override { p.min = atof(max); }
};

struct is_bool_t : public spa::audio::visitor
{
    bool res = false;
    template<class T> using ci = spa::audio::control_in<T>;
    void visit(ci<bool> &) override { res = true; }
};

// TODO: bad design of control_in<T>
struct set_scale_type : public spa::audio::visitor
{
    template<class T> using ci = spa::audio::control_in<T>;
    spa::audio::scale_type_t scale_type = spa::audio::scale_type_t::linear;
    void visit(ci<int> &p) override { p.scale_type = scale_type; }
    void visit(ci<long long int> &p) override { p.scale_type = scale_type; }
    void visit(ci<float> &p) override { p.scale_type = scale_type; }
    void visit(ci<double> &p) override { p.scale_type = scale_type; }
};

//! class for capturing numerics (not pointers to string/blob memory involved)
class capture : public rtosc::RtData
{
    rtosc_arg_val_t res;
    void replyArray(const char*, const char *args,
                    rtosc_arg_t *vals) override
    {
        assert(*args && args[1] == 0); // reply only one arg for numeric ports
        res.type = args[0];
        res.val = vals[0];
    }

    void reply_va(const char *args, va_list va)
    {
        rtosc_v2argvals(&res, 1, args, va);
    }

    void reply(const char *, const char *args, ...) override
    {
        va_list va;
        va_start(va,args);
        reply_va(args, va);
        va_end(va);
    }
public:
    const rtosc_arg_val_t& val() { return res; }
};

class set_init : public spa::audio::visitor
{
    template<class T> using ci = spa::audio::control_in<T>;
    const rtosc_arg_val_t& av;
    void visit(ci<int> &p) override { assert(av.type == 'i'); p.def = av.val.i; }
    void visit(ci<long long int> &p) override { assert(av.type == 'h'); p.def = av.val.h; }
    void visit(ci<float> &p) override { assert(av.type == 'f'); p.def = av.val.f; }
    void visit(ci<double> &p) override { assert(av.type == 'd'); p.def = av.val.d; }
    void visit(ci<bool> &p) override { assert(av.type == 'T' || av.type == 'F'); p.def = av.val.T; }
public:
    set_init(const rtosc_arg_val_t& av) : av(av) {}
};

spa::port_ref_base* new_port(const char* metadata, const char* args)
{
    assert(args);
    rtosc::Port::MetaContainer meta(metadata);
    bool is_enumerated = false;
    (void)is_enumerated; // TODO
    bool is_parameter = false;
    set_min min_setter;
    set_max max_setter;
    set_scale_type scale_type_setter;

    for(const auto x : meta)
    {
        if(!strcmp(x.title, "parameter"))
            is_parameter = true;
        else if(!strcmp(x.title, "enumerated"))
            is_enumerated = true;
        else if(!strcmp(x.title, "min"))
            min_setter.min = x.value;
        else if(!strcmp(x.title, "max"))
            max_setter.max = x.value;
        else if(!strcmp(x.title, "scale")) {
            if(!strcmp(x.value, "linear"))
            {}
            else if(!strcmp(x.value, "logarithmic"))
                scale_type_setter.scale_type =
                    spa::audio::scale_type_t::logartihmic;
            else throw std::runtime_error("unknown scale type");
        }

        /*else if(!strncmp(x.title, "map ", 4)) {
            ++mappings[atoi(x.title + 4)];
            ++mapping_values[x.value];
        }*/
    }
    if(is_parameter)
    {
        spa::port_ref_base* res = port_from_osc_args(args);
        is_bool_t is_bool;
        res->accept(is_bool);
        if(min_setter.min) res->accept(min_setter); else if(!is_bool.res) throw std::runtime_error("Port has no minimum value");
        if(max_setter.max) res->accept(max_setter); else if(!is_bool.res) throw std::runtime_error("Port has no maximum value");
        if(scale_type_setter.scale_type != spa::audio::scale_type_t::logartihmic)
            res->accept(scale_type_setter);
        return res;
    }
    else return nullptr;
}

spa::port_ref_base &ZynOscPlugin::port(const char *pname) {
    // TODO: add those to map?
    if(!strcmp(pname, "buffersize")) return p_buffersize; // TODO: use slashes?
    else if(!strcmp(pname, "osc")) return p_osc_in;
    else if(!strcmp(pname, "out")) return p_out;
    else
    {
        spa::port_ref_base* new_ref = nullptr;
        char types[2+1];
        rtosc_arg_t args[2];
        rtosc::path_search(zyn::Master::ports, pname, nullptr,
                           types, sizeof(types), args, sizeof(args));
        if(!strcmp(types, "sb"))
        {
            const char* metadata = reinterpret_cast<char*>(args[1].b.data);
            if(!metadata)
                metadata = "";
            new_ref = new_port(metadata, strchr(args[0].s, ':'));
        }

        if(new_ref) {
            capture cap;
            char loc[1024];
            cap.obj = master;
            cap.loc = loc;
            cap.loc_size = sizeof(loc);
            char msgbuf[1024];
            std::size_t length =
                rtosc_message(msgbuf, sizeof(msgbuf), pname, "");
            if(!length)
                throw std::runtime_error("Could not build rtosc message");
            zyn::Master::ports.dispatch(msgbuf, cap, true);
            if(cap.matches != 1)
                throw std::runtime_error("Could not find port"); // TODO...
            set_init init_setter(cap.val());
            new_ref->accept(init_setter);
            ports.emplace(pname, new_ref);
            return *new_ref;
        } else
            throw spa::port_not_found_error(pname);
    }
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
#if 0
            printf("received: %s %s\n", path, p_osc_in.types());
            if(p_osc_in.types()[0] == 'i')
            {
                printf("arg0: %d\n", +p_osc_in.arg(0).i);
            }
#endif
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
//        fprintf(stderr, "OSC Plugin: Unknown message \"%s\" from "
//            "MiddleWare, ignoring...\n", msg);
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

spa::simple_vec<spa::simple_str> ZynOscDescriptor::port_names() const
{
    return { "out", "buffersize", "osc" };
}
