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

#ifndef __PIW_CONTROL_PARAMS__
#define __PIW_CONTROL_PARAMS__

#include "piw_exports.h"

#include <picross/pic_fastalloc.h>
#include <picross/pic_flipflop.h>
#include <picross/pic_stl.h>
#include <piw/piw_bundle.h>
#include <piw/piw_data.h>
#include <piw/piw_control_mapping.h>

namespace piw
{
    class param_wire_t;

    typedef pic::lckmap_t<piw::data_t,param_wire_t *,piw::path_less>::lcktype host_wire_map_t;
    typedef pic::flipflop_t<host_wire_map_t> host_wire_map_flipflop_t;

    class PIW_DECLSPEC_CLASS clocking_delegate_t
    {
        public:
            virtual ~clocking_delegate_t() {};
            virtual void remove_upstream_clock(bct_clocksink_t *) = 0;
            virtual void add_upstream_clock(bct_clocksink_t *) = 0;
    };

    struct PIW_DECLSPEC_CLASS param_data_t
    {
        param_data_t(unsigned p, float v): param_(p), value_(v) {};

        unsigned param_;
        float value_;
    };

    struct PIW_DECLSPEC_CLASS midi_data_t
    {
        midi_data_t(unsigned long long time, unsigned char mcc, unsigned char lcc, unsigned value, unsigned scope, unsigned channel, piw::data_nb_t id, bool continuous): time_(time), mcc_(mcc), lcc_(lcc), value_(value), scope_(scope), channel_(channel), id_(id), continuous_(continuous) {};

        unsigned long long time_;
        unsigned char mcc_;
        unsigned char lcc_;
        unsigned value_;
        unsigned scope_;
        unsigned channel_;
        piw::data_nb_t id_;
        bool continuous_;
    };

    class PIW_DECLSPEC_CLASS params_delegate_t: public clocking_delegate_t
    {
        public:
            virtual ~params_delegate_t() {};
            virtual void update_origins(piw::control_mapping_t &) {};
            virtual void update_mapping(piw::control_mapping_t &) {};
            virtual void set_parameters(pic::lckvector_t<param_data_t>::nbtype &) {};
            virtual void set_midi(pic::lckvector_t<midi_data_t>::nbtype &) {};
    };

    class PIW_DECLSPEC_CLASS input_root_t: public piw::root_t, virtual public pic::lckobject_t
    {
        public:
            input_root_t(clocking_delegate_t *);
            ~input_root_t();
            virtual void root_opened() { root_clock(); root_latency(); }
            virtual void root_closed() {}
            virtual void root_latency() {}
            virtual void root_clock();
            virtual piw::wire_t *root_wire(const piw::event_data_source_t &es);
            virtual void started(param_wire_t *) {}
            virtual void ending(param_wire_t *, unsigned long long) {}
            virtual void ended(param_wire_t *) {}

        protected:
            friend class param_wire_t;

            clocking_delegate_t *clocking_delegate_;
            host_wire_map_flipflop_t wires_;
            bct_clocksink_t *clk_;
            pic::ilist_t<param_wire_t, 0> active_;
            pic::ilist_t<param_wire_t, 1> rotating_active_;
    };
    
    class PIW_DECLSPEC_CLASS param_input_t: public input_root_t
    {
        public:
            param_input_t(params_delegate_t *, unsigned);

            virtual float calculate_param_value(const piw::data_nb_t &, const float, const piw::mapping_data_t&, const float);
            virtual long calculate_midi_value(const piw::data_nb_t &, const float, const piw::mapping_data_t&);
            virtual bool wiredata_processed(param_wire_t *, const piw::data_nb_t &) { return true; };

            void started(param_wire_t *);
            void ending(param_wire_t *, unsigned long long);
            void ended(param_wire_t *);
            unsigned char extract_keynum(const piw::data_nb_t &);
            void schedule(unsigned long long from, unsigned long long to);
            void resend_current(const piw::data_nb_t &);
            void update_origins();
            void update_mapping();

        private:
            void process_wire(param_wire_t *, pic::lckvector_t<param_data_t>::nbtype &, pic::lckvector_t<midi_data_t>::nbtype &, unsigned long long, bool, bool);
            bool process_wire_data(param_wire_t *, pic::lckvector_t<param_data_t>::nbtype &, pic::lckvector_t<midi_data_t>::nbtype &, bool, bool);
            void process_params(pic::lckvector_t<param_data_t>::nbtype &, const piw::data_nb_t &, const piw::data_nb_t &, bool);
            void process_midi(pic::lckvector_t<midi_data_t>::nbtype &, const piw::data_nb_t &, const piw::data_nb_t &, bool, bool, bool);
            void end_with_origins();
            params_delegate_t *params_delegate_;
            piw::control_mapping_t control_mapping_;
            piw::dataholder_nb_t current_id_;
            piw::dataholder_nb_t current_data_;
    };
    
    class PIW_DECLSPEC_CLASS keynum_input_t: public param_input_t
    {
        public:
            keynum_input_t(params_delegate_t *, unsigned);
            float calculate_param_value(const piw::data_nb_t &, const float, const piw::mapping_data_t&, const float);
            long calculate_midi_value(const piw::data_nb_t &, const float, const piw::mapping_data_t&);
            bool wiredata_processed(param_wire_t *, const piw::data_nb_t &);
    };

    class PIW_DECLSPEC_CLASS param_wire_t: public piw::wire_t, public piw::event_data_sink_t, public pic::element_t<0>, public pic::element_t<1>
    {
        public:
            param_wire_t(input_root_t *, const piw::event_data_source_t &);
            ~param_wire_t();
            void wire_closed();
            void invalidate();
            static int __clear(void *, void *);
            void event_start(unsigned, const piw::data_nb_t &, const piw::xevent_data_buffer_t &);
            bool event_end(unsigned long long);
            void event_buffer_reset(unsigned, unsigned long long, const piw::dataqueue_t &, const piw::dataqueue_t &);

            const piw::data_nb_t get_id() const { return id_; };

            piw::xevent_data_buffer_t::iter_t iterator_;
        private:
            friend class keynum_input_t;
            friend class param_input_t;

            input_root_t *root_;
            piw::data_t path_;
            piw::dataholder_nb_t id_;
            bool ended_;
            bool processed_data_;
    };
}

#endif
