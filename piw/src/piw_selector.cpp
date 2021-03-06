
/*
 Copyright 2009 Eigenlabs Ltd.  http://www.eigenlabs.com

 This file is part of EigenD.

 EigenD is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 EigenD is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with EigenD.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <piw/piw_selector.h>
#include <piw/piw_tsd.h>
#include <piw/piw_bundle.h>
#include <piw/piw_status.h>
#include <picross/pic_log.h>
#include <picross/pic_functor.h>

#include <map>

#define MODE_RUNNING 0
#define MODE_SELECTING 1
#define MODE_CHOOSING 2

namespace
{
    struct gatesink_t: pic::sink_t<void(const piw::data_nb_t &)>
    {
        gatesink_t(piw::selector_t::impl_t *i, unsigned s);
        void invoke(const piw::data_nb_t &p1) const;
        bool iscallable() const { return i_.isvalid(); }
        bool compare(const pic::sink_t<void(const piw::data_nb_t &)> *s) const
        {
            const gatesink_t *c = dynamic_cast<const gatesink_t *>(s);
            if(c && c->i_==i_ && c->s_==s_) return true;
            return false;
        }

        pic::weak_t<piw::selector_t::impl_t> i_;
        unsigned s_;
        mutable bool state_;
    };

    struct slot_t: virtual public pic::lckobject_t
    {
        slot_t(unsigned char sl): slot_(sl), state_(false), current_(false)
        {
        }

        ~slot_t()
        {
        }

        void gate(unsigned sel)
        {
            bool v(false);

            switch(sel)
            {
                case MODE_RUNNING:      v=state_; break;
                case MODE_SELECTING:    v=false; break;
                case MODE_CHOOSING:     v=false; break;
            }

            if(v!=current_)
            {
                unsigned long long t(piw::tsd_time());
                piw::data_nb_t d(piw::makefloat_bounded_nb(1,0,0,v?1:0,t));
                gate_(d);
                current_=v;
            }

            if(sel==MODE_RUNNING)
            {
                unsigned long long t(piw::tsd_time());
                piw::data_nb_t d(piw::makefloat_bounded_nb(1,0,0,current_?1:0,t));
                selected_(d);
            }
        }

        void source_ended(unsigned seq)
        {
        }

        void change_status(piw::statusbuffer_t *buffer, unsigned sel)
        {
            unsigned st(BCTSTATUS_OFF);

            if (MODE_SELECTING == sel)
            {
                st = state_ ? BCTSTATUS_SELECTOR_ON : BCTSTATUS_SELECTOR_OFF;
            }

            buffer->set_status(slot_,st);
        }

        int gc_traverse(void *v, void *a) const
        {
            int r;
            if((r=gate_.gc_traverse(v,a))!=0) return r;
            if((r=selected_.gc_traverse(v,a))!=0) return r;
            return 0;
        }

        int gc_clear()
        {
            gate_.gc_clear();
            selected_.gc_clear();
            return 0;
        }

        unsigned slot_;
        bool state_;
        pic::flipflop_functor_t<piw::change_nb_t> gate_;
        pic::flipflop_functor_t<piw::change_nb_t> selected_;
        bool current_;
    };
};

struct piw::selector_t::impl_t: virtual pic::lckobject_t, virtual pic::tracked_t
{
    impl_t(const cookie_t &lo, const change_nb_t &ls, unsigned n, bool initial): selecting_(MODE_RUNNING),initial_(initial),lightswitch_(ls),lightchannel_(n),statusbuffer_(piw::change_nb_t(),0,lo)
    {
        statusbuffer_.autosend(false);
    }

    ~impl_t()
    {
        tracked_invalidate();

        for(;;)
        {
            pic::lckmap_t<unsigned,slot_t *>::lcktype::iterator ci = slots_.alternate().begin();

            if(ci==slots_.alternate().end())
            {
                break;
            }

            slot_t *sl = ci->second;
            slots_.alternate().erase(ci);
            slots_.exchange();
            delete sl;
        }
    }

    int gc_traverse(void *v, void *a) const
    {
        int r;

        if((r=lightswitch_.gc_traverse(v,a))!=0)
        {
            return r;
        }

        const pic::lckmap_t<unsigned,slot_t *>::lcktype &c = slots_.current();
        pic::lckmap_t<unsigned,slot_t *>::lcktype::const_iterator ci;

        for(ci=c.begin(); ci!=c.end(); ++ci)
        {
            if((r=ci->second->gc_traverse(v,a))!=0)
            {
                return r;
            }
        }

        return 0;
    }

    int gc_clear()
    {
        lightswitch_.gc_clear();

        const pic::lckmap_t<unsigned,slot_t *>::lcktype &c = slots_.current();
        pic::lckmap_t<unsigned,slot_t *>::lcktype::const_iterator ci;

        for(ci=c.begin(); ci!=c.end(); ++ci)
        {
            ci->second->gc_clear();
        }

        return 0;
    }

    slot_t *plumb_slot(unsigned s)
    {
        pic::lckmap_t<unsigned,slot_t *>::lcktype::iterator si=slots_.alternate().find(s);

        if(si!=slots_.alternate().end())
            return si->second;

        slot_t *sl = new slot_t(s+1);

        slots_.alternate().insert(std::make_pair(s,sl));
        slots_.exchange();

        return sl;
    }

    void unplumb_slot(unsigned s)
    {
        pic::lckmap_t<unsigned,slot_t *>::lcktype::iterator si=slots_.alternate().find(s);

        if(si==slots_.alternate().end())
            return;

        slot_t *sl = si->second;
        slots_.alternate().erase(si);
        slots_.exchange();
        delete sl;
    }

    static int __setgate(void *self_, void *sl_, void *func_, void *selected_)
    {
        impl_t *self = (impl_t *)self_;
        slot_t *sl = (slot_t *)sl_;

        const piw::change_nb_t *func = (piw::change_nb_t *)func_;
        const piw::change_nb_t *selected = (piw::change_nb_t *)selected_;

        sl->gate_ = *func;
        sl->selected_ = *selected;
        sl->current_ = self->initial_;
        sl->gate(self->selecting_);

        return 0;
    }

    void gate_output(unsigned s, const piw::change_nb_t &f, const piw::change_nb_t &e)
    {
        slot_t *sl = plumb_slot(s);
        piw::tsd_fastcall4(__setgate,this,sl,(void *)&f,(void *)&e);
    }

    void clear_output(unsigned s)
    {
        unplumb_slot(s);
    }

    void activate(unsigned idx)
    {
        pic::flipflop_t<pic::lckmap_t<unsigned,slot_t *>::lcktype>::guard_t g(slots_);
        pic::lckmap_t<unsigned,slot_t *>::lcktype::const_iterator ci;

        for(ci=g.value().begin(); ci!=g.value().end(); ++ci)
        {
            ci->second->state_ = (ci->first==idx);
        }
    }

    void gate_input(unsigned idx, const piw::data_nb_t &d)
    {
        if(selecting_==MODE_SELECTING)
        {
            bool s = d.as_bool();

            if(s)
            {
                pic::flipflop_t<pic::lckmap_t<unsigned,slot_t *>::lcktype>::guard_t g(slots_);
                pic::lckmap_t<unsigned,slot_t *>::lcktype::const_iterator ci;

                if(counter_==0)
                {
                    for(ci=g.value().begin(); ci!=g.value().end(); ++ci)
                    {
                        ci->second->state_ = (ci->first==idx);
                        ci->second->change_status(&statusbuffer_,selecting_);
                    }
                    statusbuffer_.send();
                }
                else
                {
                    ci=g.value().find(idx);
                    if(ci!=g.value().end())
                    {
                        ci->second->state_ = !ci->second->state_;
                        ci->second->change_status(&statusbuffer_,selecting_);
                        statusbuffer_.send();
                    }
                }
                counter_++;
            }
            else
            {
                counter_--;
            }
        }
    }

    void choose(bool ch)
    {
        if(ch)
        {
            if(selecting_!=MODE_CHOOSING)
            {
                selecting_=MODE_CHOOSING;
                pic::logmsg() << "choose mode on";

                pic::flipflop_t<pic::lckmap_t<unsigned,slot_t *>::lcktype>::guard_t g(slots_);
                pic::lckmap_t<unsigned,slot_t *>::lcktype::const_iterator ci;
				
                for(ci=g.value().begin(); ci!=g.value().end(); ++ci)
                {
                    ci->second->change_status(&statusbuffer_,selecting_);
                    ci->second->gate(selecting_);
                }
                statusbuffer_.send();
                lightswitch_(piw::makelong_nb(lightchannel_,piw::tsd_time()));
            }
        }
        else
        {
            if(selecting_==MODE_CHOOSING)
            {
                selecting_=MODE_RUNNING;
                pic::logmsg() << "choose mode off";

                pic::flipflop_t<pic::lckmap_t<unsigned,slot_t *>::lcktype>::guard_t g(slots_);
                pic::lckmap_t<unsigned,slot_t *>::lcktype::const_iterator ci;
                unsigned long long t = piw::tsd_time();

                for(ci=g.value().begin(); ci!=g.value().end(); ++ci)
                {
                    if(ci->second->state_)
                    {
                        lightswitch_(piw::makelong_nb(ci->first+1,t));
                        break;
                    }
                }

                for(ci=g.value().begin(); ci!=g.value().end(); ++ci)
                {
                    ci->second->change_status(&statusbuffer_,selecting_);
                    ci->second->gate(selecting_);
                }
                statusbuffer_.send();
            }
        }
    }

    void mode_input(const piw::data_nb_t &d)
    {
        if(selecting_==MODE_CHOOSING)
        {
            return;
        }

        unsigned s = (d.as_norm()>=0.5)?MODE_SELECTING:MODE_RUNNING;

        if(s!=selecting_)
        {
            selecting_ = s;
            unsigned long long t = d.time();

            pic::flipflop_t<pic::lckmap_t<unsigned,slot_t *>::lcktype>::guard_t g(slots_);
            pic::lckmap_t<unsigned,slot_t *>::lcktype::const_iterator ci;

            if(selecting_)
            {
                lightswitch_(piw::makelong_nb(lightchannel_,t));
                counter_=0;
            }
            else
            {
                for(ci=g.value().begin(); ci!=g.value().end(); ++ci)
                {
                    if(ci->second->state_)
                    {
                        lightswitch_(piw::makelong_nb(ci->first+1,t));
                        break;
                    }
                }
			}
            for(ci=g.value().begin(); ci!=g.value().end(); ++ci)
            {
                ci->second->change_status(&statusbuffer_,selecting_);
                ci->second->gate(selecting_);
            }
            statusbuffer_.send();
        }
    }

    unsigned counter_;
    unsigned selecting_;
    bool initial_;
    pic::flipflop_t<pic::lckmap_t<unsigned,slot_t *>::lcktype> slots_;
    change_nb_t lightswitch_;
    unsigned lightchannel_;
    piw::statusbuffer_t statusbuffer_;
};

void gatesink_t::invoke(const piw::data_nb_t &d) const
{
    bool s = (d.as_renorm(0,1,0)>=0.5);
    if(s==state_)
        return;

    state_=s;
    i_->gate_input(s_,piw::makebool_nb(s,d.time()));
}

piw::selector_t::selector_t(const cookie_t &lo, const change_nb_t &ls, unsigned n, bool initial): impl_(new impl_t(lo,ls,n,initial))
{
}

piw::selector_t::~selector_t()
{
    delete impl_;
}

static int __chooser(void *i_, void *ch_)
{
    piw::selector_t::impl_t *i = (piw::selector_t::impl_t *)i_;
    bool ch = *(bool *)ch_;
    i->choose(ch);
    return 0;
}

int piw::selector_t::gc_traverse(void *v, void *a) const
{
    return impl_->gc_traverse(v,a);
}

int piw::selector_t::gc_clear()
{
    return impl_->gc_clear();
}

void piw::selector_t::choose(bool ch)
{
    tsd_fastcall(__chooser,impl_,&ch);
}

void piw::selector_t::gate_output(unsigned s, const piw::change_nb_t &f, const piw::change_nb_t &e)
{
    impl_->gate_output(s,f,e);
}

void piw::selector_t::clear_output(unsigned s)
{
    impl_->clear_output(s);
}

piw::change_nb_t piw::selector_t::gate_input(unsigned s)
{
    return piw::change_nb_t(pic::ref(new gatesink_t(impl_,s))); 
}

piw::change_nb_t piw::selector_t::mode_input()
{
    return piw::change_nb_t::method(impl_,&impl_t::mode_input);
}

static int __activate(void *i_, void *s_)
{
    piw::selector_t::impl_t *i = (piw::selector_t::impl_t *)i_;
    unsigned s = *(unsigned *)s_;
    i->activate(s);
    return 0;
}

void piw::selector_t::activate(unsigned s)
{
    tsd_fastcall(__activate,impl_,&s);
}

static int __select(void *i_, void *s_, void *e_)
{
    piw::selector_t::impl_t *i = (piw::selector_t::impl_t *)i_;
    unsigned s = *(unsigned *)s_;
    bool e = *(bool *)e_;
    unsigned long long t  = piw::tsd_time();

    pic::flipflop_t<pic::lckmap_t<unsigned,slot_t *>::lcktype>::guard_t g(i->slots_);
    pic::lckmap_t<unsigned,slot_t *>::lcktype::const_iterator ci;

    ci=g.value().find(s);
    if(ci!=g.value().end())
    {
        ci->second->state_ = e;
        ci->second->gate(MODE_RUNNING);
        ci->second->change_status(&i->statusbuffer_,i->selecting_);
        i->statusbuffer_.send();
        if(e)
            i->lightswitch_(piw::makelong_nb(ci->first+1,t));
    }

    return 0;
}

void piw::selector_t::select(unsigned s, bool e)
{
    tsd_fastcall3(__select,impl_,&s,&e);
}

gatesink_t::gatesink_t(piw::selector_t::impl_t *i, unsigned s): i_(i), s_(s), state_(i->initial_) {}
