#
# libpri: An implementation of Primary Rate ISDN
#
# Written by Mark Spencer <markster@linux-support.net>
#
# Copyright (C) 2001, Linux Support Services, Inc.
# All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. 
#
#
# Uncomment if you want libpri not send PROGRESS_INDICATOR w/ALERTING
#ALERTING=-DALERTING_NO_PROGRESS

TOBJS=testpri.o
STATIC_LIBRARY=libpri.a
DYNAMIC_LIBRARY=libpri.so.1.0
STATIC_OBJS=pri.o q921.o prisched.o q931.o
DYNAMIC_OBJS=pri.lo q921.lo prisched.lo q931.lo
CFLAGS=-Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -g $(ALERTING)
INSTALL_PREFIX=

all: $(STATIC_LIBRARY) $(DYNAMIC_LIBRARY)

install: $(STATIC_LIBRARY) $(DYNAMIC_LIBRARY)
	mkdir -p $(INSTALL_PREFIX)/usr/lib
	mkdir -p $(INSTALL_PREFIX)/usr/include
	install -m 644 libpri.h $(INSTALL_PREFIX)/usr/include
	install -m 755 $(DYNAMIC_LIBRARY) $(INSTALL_PREFIX)/usr/lib
	( cd $(INSTALL_PREFIX)/usr/lib ; ln -sf libpri.so.1 libpri.so )
	install -m 644 $(STATIC_LIBRARY) $(INSTALL_PREFIX)/usr/lib
	/sbin/ldconfig

pritest: pritest.o
	$(CC) -o pritest pritest.o -L. -lpri -lzap

pridump: pridump.o
	$(CC) -o pridump pridump.o -L. -lpri -lzap

%.lo : %.c
	$(CC) -fPIC $(CFLAGS) -o $@ -c $<

$(STATIC_LIBRARY): $(STATIC_OBJS)
	ar rcs $(STATIC_LIBRARY) $(STATIC_OBJS)
	ranlib $(STATIC_LIBRARY)

$(DYNAMIC_LIBRARY): $(DYNAMIC_OBJS)
	$(CC) -shared -Wl,-soname,libpri.so.1 -o $@ $(DYNAMIC_OBJS)
	/sbin/ldconfig -n .
	ln -sf libpri.so.1 libpri.so

clean:
	rm -f *.o *.so *.lo 
	rm -f testpri $(STATIC_LIBRARY) $(DYNAMIC_LIBRARY)
	rm -f pritest pridump
