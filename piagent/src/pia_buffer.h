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

#ifndef __PIA_SRC_BUFFER__
#define __PIA_SRC_BUFFER__

#include <pibelcanto/link.h>
#include <pibelcanto/plugin.h>
#include <piagent/pia_network.h>
#include <picross/pic_fastalloc.h>

#include "pia_glue.h"
#include "pia_network.h"

class pia_buffer_t: pic::nocopy_t
{
    public:
        pia_buffer_t(pia::manager_t::impl_t *, const pia_data_t &addr, unsigned hlen);
        virtual ~pia_buffer_t();

        void buffer_enable();
        void buffer_disable();

        unsigned char *buffer_transmit_fast(unsigned space,bool audoflush);
        virtual void buffer_receive_fast(const unsigned char *data, unsigned len) = 0;
        virtual void buffer_fixup_fast(unsigned char *data, unsigned len) = 0;
        void buffer_flush_fast();

        unsigned char *buffer_transmit_slow(unsigned space);
        virtual void buffer_receive_slow(const unsigned char *data, unsigned len) = 0;
        virtual void buffer_fixup_slow(unsigned char *data, unsigned len) = 0;
        void buffer_flush_slow(bool force=false);

    private:
        static void buffer_receive_fast_thunk(void *, const pia_data_t &);
        static void buffer_transmit_fast_thunk(void *, const pia_data_t &);
        static void buffer_receive_slow_thunk(void *, const pia_data_t &);
        static void buffer_transmit_slow_thunk(void *, const pia_data_t &);

    private:
        unsigned header_;

        pia_eventq_t *slowloop_;
        unsigned char *slowbuffer_;
        unsigned slowused_;
        pia_sockref_t slowsocket_;
        pia_job_t slowjob_transmit_;

        pia_eventq_t *fastloop_;
        unsigned char *fastbuffer_;
        unsigned fastused_;
        pia_sockref_t fastsocket_;
        pia_job_t fastjob_transmit_;

        pia::manager_t::impl_t *glue_;
};

#endif
