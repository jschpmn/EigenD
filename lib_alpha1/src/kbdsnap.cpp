
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

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <picross/pic_config.h>
#ifndef PI_WINDOWS
#include <unistd.h>
#else
#include <io.h>
#define sleep Sleep
#endif


#include <lib_alpha1/alpha1_usb.h>
#include <lib_alpha1/alpha1_active.h>
#include <picross/pic_usb.h>
#include <picross/pic_time.h>

#define KEY 0

struct printer_t: public  alpha1::active_t::delegate_t
{
    printer_t(): count(0),pressed(0) {}

    void kbd_dead()
    {
        printf("keyboard disconnected\n");
        exit(0);
    }

    void kbd_raw(unsigned long long t, unsigned key, unsigned c1, unsigned c2, unsigned c3, unsigned c4)
    {
        //if(key==119) printf("raw: %04d %04d %04d %04d %04d %04d %llu\n", key, c, c1, c2, c3, c4, t);
    }

    void kbd_key(unsigned long long t, unsigned key, unsigned p, int r, int y)
    {
        if(key!=KEY) return;

        if(pressed==0)
        {
            file=fopen("kbdsnap.out","w");
            pressed=t;
            previous=t;
            printf("starting capture\n");
        }

        if(file)
        {
            fprintf(file,"%llu %04d %04d %04d %llu\n", t-pressed, p, r, y, 1000000ULL/(t-previous));
        }

        previous=t;
    }

    void kbd_keydown(unsigned long long t, const unsigned short *bitmap)
    {
        //if(++count==0) printf("bitmap t=%llu\n",t);

        if(pressed && !alpha1::active_t::keydown(KEY,bitmap))
        {
            printf("stopping capture\n");

            if(file)
            {
                fclose(file);
                file=0;
            }

            pressed=0;
        }
    }

    unsigned char count;
    unsigned long long pressed, previous;
    FILE *file;
};

int main(int ac, char **av)
{
    const char *usbdev = 0;

    if(ac==2)
    {
        usbdev=av[1];
    }
    else
    {
        try
        {
            usbdev = pic::usbenumerator_t::find(BCTALPHA1_USBVENDOR,BCTALPHA1_USBPRODUCT).c_str();
        }
        catch(...)
        {
            fprintf(stderr,"can't find a keyboard\n");   
            exit(-1);
        }
    }

    printer_t printer;
    alpha1::active_t loop(usbdev, &printer);
    loop.set_raw(false);

    sleep(1);
    loop.start();
    sleep(1);

    while(1)
    {
        pic_microsleep(2000);
        loop.poll(0);
    }

    return 0;
}
