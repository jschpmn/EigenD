
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

#ifndef __PIW_SCALER__
#define __PIW_SCALER__
#include "piw_exports.h"
#include "piw_bundle.h"

// inputs
#define SCALER_TONIC 5
#define SCALER_BASE 6
#define SCALER_SCALE 7
#define SCALER_KBEND 9
#define SCALER_GBEND 10
#define SCALER_KRANGE 11
#define SCALER_GRANGE 12
#define SCALER_CONTROL 13
#define SCALER_OVERRIDE 14
#define SCALER_OCTAVE 15
#define SCALER_KEY 16
#define SCALER_ROCTAVE 17
#define SCALER_IN_MASK SIG12(SCALER_TONIC,SCALER_BASE,SCALER_SCALE,SCALER_KBEND,SCALER_GBEND,SCALER_KRANGE,SCALER_GRANGE,SCALER_CONTROL,SCALER_OVERRIDE,SCALER_OCTAVE,SCALER_KEY,SCALER_ROCTAVE)

// outputs
#define SCALER_SCALENOTE 5
#define SCALER_FREQUENCY 6
#define SCALER_OUT_MASK  SIG2(SCALER_SCALENOTE,SCALER_FREQUENCY)

#define BTONIC (1<<0)
#define BBASE (1<<1)
#define BOCT (1<<2)
#define BSCALE (1<<3)
#define BLAYOUT (1<<4)

namespace piw
{
    struct PIW_DECLSPEC_CLASS scaler_subscriber_t
    {
        virtual ~scaler_subscriber_t() {}
        virtual void control_changed(unsigned,const unsigned char *) {}
    };

    class PIW_DECLSPEC_CLASS scaler_controller_t
    {
        public:
            class impl_t;

            static void parsevector(const std::string &s, pic::lckvector_t<float>::nbtype *v, unsigned sizehint);

            struct scale_t: pic::atomic_counted_t, pic::lckobject_t
            {
                scale_t(const std::string &s) { parsevector(s,&notes_,13); }

                unsigned size()
                {
                    return notes_.size();
                }

                float at(unsigned i)
                {
                    return notes_.at(i);
                }

                pic::lckvector_t<float>::nbtype notes_;
            };

            typedef pic::ref_t<scale_t> sref_t;

            struct layout_t: pic::atomic_counted_t, pic::lckobject_t
            {
                layout_t(const std::string &offsets,const std::string &lengths)
                {
                    pic::lckvector_t<float>::nbtype co;
                    parsevector(offsets,&co,5);
                    parsevector(lengths,&lengths_,5);

                    ncourses_ = std::min(co.size(),lengths_.size());
                    if(ncourses_==0)
                    {
                        return;
                    }

                    notes_.reserve(ncourses_*(int)lengths_.at(0));

                    int number = 0;
                    float note = 0.f;

                    for(unsigned c=0; c<ncourses_; ++c)
                    {
                        float adj = co.at(c);
                        if(adj>500.f)
                        {
                            adj -= 10000.f;
                            number += (int)adj;
                        }
                        else
                        {
                            note += adj;
                        }

                        unsigned nk = (unsigned)lengths_.at(c);
                        for(unsigned k=0; k<nk; ++k)
                        {
                            notes_.push_back(std::make_pair(number+k,note));
                        }
                    }
                }

                void geometry(unsigned knum, unsigned &course, unsigned &key)
                {
                    if(ncourses_==0)
                    {
                        key=knum;
                        course=1;
                        return;
                    }

                    unsigned k=knum;

                    for(unsigned c=0;c<ncourses_; ++c)
                    {
#if SCALER_DEBUG>0
                        pic::logmsg() << "course " << c << " len " << lengths_[c];
#endif // SCALER_DEBUG>0
                        if(k <= lengths_[c])
                        {
                            course=c+1;
                            key=k;
                            return;
                        }

                        k -= (int)lengths_[c];
                    }

                    course=ncourses_+1;
                    key=k;
                    return;
                }

                unsigned size()
                {
                    return notes_.size();
                }

                int knum(unsigned i)
                {
                    return notes_.at(i).first;
                }

                float note(unsigned i)
                {
                    return notes_.at(i).second;
                }

                unsigned ncourses_;
                pic::lckvector_t<std::pair<int,float> >::nbtype notes_;
                pic::lckvector_t<float>::nbtype lengths_;
            };

            typedef pic::ref_t<layout_t> lref_t;

            struct bits_t
            {
                bits_t(): bits(0) {}
                bits_t(const bits_t &b): tonic(b.tonic), base(b.base), oct(b.oct), scale(b.scale), layout(b.layout), bits(b.bits) {}
                float tonic;
                float base;
                float oct;
                sref_t scale;
                lref_t layout;
                unsigned bits;
            };

        public:
            scaler_controller_t();
            ~scaler_controller_t();
            cookie_t cookie();

            void add_subscriber(scaler_subscriber_t *);
            void del_subscriber(scaler_subscriber_t *);
            unsigned common_scale_count();
            sref_t common_scale_at(unsigned);
            bits_t bits(const piw::data_nb_t &id);

        private:
            impl_t *impl_;
    };

    class PIW_DECLSPEC_CLASS scaler_t
    {
        public:
            class impl_t;

        public:
            scaler_t(scaler_controller_t *,const cookie_t &c, const pic::f2f_t &b);
            ~scaler_t();

            void set_bend_curve(const pic::f2f_t &b);
            int gc_traverse(void *v,void *a) const;
            int gc_clear();
            cookie_t cookie();

        private:
            impl_t *ctl_;
    };

    // -1 if -1.0 <= x <= -0.5
    // 0  if -0.5 <  x <  +0.5
    // 1  if +0.5 <= x <= +1.0
    PIW_DECLSPEC_FUNC(pic::f2f_t) step_bucket();
};

#endif
