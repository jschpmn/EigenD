
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

#include "pia_window.h"
#include "pia_glue.h"
#include "pia_error.h"

#include <picross/pic_strbase.h>
#include <picross/pic_ilist.h>
#include <picross/pic_flipflop.h>
#include <picross/pic_log.h>
#include <picross/pic_mlock.h>

#include <stdlib.h>

namespace
{
    struct wnode_t: pic::element_t<>, virtual public pic::lckobject_t
    {
        wnode_t(pia_windowlist_t::impl_t *tl,const pia_ctx_t &e, bct_window_t *s);
        ~wnode_t();

        static void api_window_close(bct_window_host_ops_t **);
        static void api_window_state(bct_window_host_ops_t **, int);
        static void api_window_title(bct_window_host_ops_t **, const char *);

        static void close_callback(void *t_, const pia_data_t & d);
        static void state_callback(void *t_, const pia_data_t & d);

        void detach(int);
        void close();
        void title(const pia_data_t &);
        void state(bool);

        std::string get_title() 
        {
            if(title_.type()==BCTVTYPE_STRING)
            {
                return title_.asstring();
            }

            return "";
        }

        void set_state(bool o)
        {
            job_state_.idle(entity_->appq(),state_callback,this,entity_->glue()->allocate_bool(o));
        }

        bct_window_host_ops_t *host_ops_;

        pia_windowlist_t::impl_t *list_;
        bct_window_t *window_;

        pia_ctx_t entity_;

        pic::flipflop_t<bool> open_;
        pia_job_t job_close_;
        pia_job_t job_state_;
        pia_cref_t cpoint_;

        bool state_;
        pia_data_t title_;


        static bct_window_host_ops_t dispatch__;
    };
};

struct pia_windowlist_t::impl_t
{
    impl_t(): count_(0) {}
    pic::ilist_t<wnode_t> windows_;
    unsigned count_;
};

void wnode_t::state_callback(void *t_, const pia_data_t & d)
{
    wnode_t *t = (wnode_t *)t_;
    bct_window_plug_state(t->window_,t->entity_->api(),d.asbool()?1:0);
}

void wnode_t::close_callback(void *t_, const pia_data_t & d)
{
    wnode_t *t = (wnode_t *)t_;
    bct_window_plug_closed(t->window_,t->entity_->api());
}

void wnode_t::detach(int e)
{
    if(!open_.current())
    {
        return;
    }

    open_.set(false);

    job_close_.cancel();
    job_state_.cancel();
    cpoint_->disable();

    remove();
    list_->count_--;
    window_->plg_state = PLG_STATE_DETACHED;

    if(e)
    {
        job_close_.idle(entity_->appq(),close_callback,this,pia_data_t());
    }
}

wnode_t::~wnode_t()
{
}

void wnode_t::close()
{
    detach(0);
    window_->plg_state = PLG_STATE_CLOSED;
    delete this;
}

void wnode_t::api_window_close(bct_window_host_ops_t **t_)
{
    wnode_t *t = PIC_STRBASE(wnode_t,t_,host_ops_);

    try
    {
        pia_mainguard_t guard(t->entity_->glue());

        t->close();
    }
    PIA_CATCHLOG_EREF(t->entity_)
}

void wnode_t::state(bool open)
{
    state_=open;
    entity_->glue()->winch("");
}

void wnode_t::title(const pia_data_t &t)
{
    title_=t;
    entity_->glue()->winch("");
}

void wnode_t::api_window_state(bct_window_host_ops_t **t_, int open)
{
    wnode_t *t = PIC_STRBASE(wnode_t,t_,host_ops_);

    try
    {
        pia_mainguard_t guard(t->entity_->glue());

        t->state(open?true:false);
    }
    PIA_CATCHLOG_EREF(t->entity_)
}

void wnode_t::api_window_title(bct_window_host_ops_t **t_, const char *ti)
{
    wnode_t *t = PIC_STRBASE(wnode_t,t_,host_ops_);

    try
    {
        pia_mainguard_t guard(t->entity_->glue());
        pia_data_t tid = t->entity_->glue()->allocate_string(ti,strlen(ti));
        t->title(tid);
    }
    PIA_CATCHLOG_EREF(t->entity_)
}

PIC_FASTDATA bct_window_host_ops_t wnode_t::dispatch__ =
{
    api_window_close,
    api_window_state,
    api_window_title
};

pia_windowlist_t::pia_windowlist_t()
{
    impl_ = new impl_t;
}

pia_windowlist_t::~pia_windowlist_t()
{
    try
    {
        kill(0);
    }
    PIA_CATCHLOG_PRINT()

    delete impl_;
}

unsigned pia_windowlist_t::window_count()
{
    return impl_->count_;
}

bool pia_windowlist_t::window_state(unsigned w)
{
    unsigned c=0;
    wnode_t *n = impl_->windows_.head();

    while(n && c<w)
    {
        c++;
        n = impl_->windows_.next(n);
    }

    if(n)
    {
        return n->state_;
    }

    return false;
}

void pia_windowlist_t::set_window_state(unsigned w, bool o)
{
    unsigned c=0;
    wnode_t *n = impl_->windows_.head();

    while(n && c<w)
    {
        c++;
        n = impl_->windows_.next(n);
    }

    if(n)
    {
        n->set_state(o);
    }
}

std::string pia_windowlist_t::window_title(unsigned w)
{
    unsigned c=0;
    wnode_t *n = impl_->windows_.head();

    while(n && c<w)
    {
        c++;
        n = impl_->windows_.next(n);
    }

    if(n)
    {
        return n->get_title();
    }

    return "";
}

void pia_windowlist_t::window(const pia_ctx_t &e, bct_window_t *s)
{
    new wnode_t(impl_,e,s);
}

wnode_t::wnode_t(pia_windowlist_t::impl_t *tl,const pia_ctx_t &e, bct_window_t *s): list_(tl), window_(s), entity_(e)
{
    open_.set(true);

    cpoint_ = pia_make_cpoint();

    host_ops_=&dispatch__;
    window_->host_ops=&host_ops_;
    window_->plg_state = PLG_STATE_OPENED;

    list_->windows_.append(this);
    list_->count_++;
    state_=false;
}

void pia_windowlist_t::dump(const pia_ctx_t &e)
{
    wnode_t *i;

    i = impl_->windows_.head();

    while(i)
    {
        if(i->entity_.matches(e))
        {
            pic::logmsg() << "window " << i->entity_->tag();
        }

        i = impl_->windows_.next(i);
    }
}

void pia_windowlist_t::kill(const pia_ctx_t &e)
{
    wnode_t *i,*n;

    i = impl_->windows_.head();

    while(i)
    {
        n = impl_->windows_.next(i);

        if(i->entity_.matches(e))
        {
            i->detach(1);
        }

        i=n;
    }
}
