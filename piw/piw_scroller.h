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

#ifndef __PIW_SCROLLER__
#define __PIW_SCROLLER__
#include "piw_exports.h"
#include "piw_bundle.h"

namespace piw
{
    struct PIW_DECLSPEC_CLASS scrolldelegate_t
    {
        virtual ~scrolldelegate_t() {}
        virtual void scroll(float,float) = 0;
        virtual void tap() = 0;
    };

    class PIW_DECLSPEC_CLASS scroller_t
    {
        public:
            class impl_t;
        public:
            scroller_t(scrolldelegate_t *,float,float,unsigned long);
            ~scroller_t();
            cookie_t cookie();
            void reset(float,float);
            void reset_h(float);
            void reset_v(float);
            void set_scheme(unsigned);
            void disable();
            void enable();
        private:
            impl_t *impl_;
    };
};

#endif
