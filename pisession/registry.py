
#
# Copyright 2009 Eigenlabs Ltd.  http://www.eigenlabs.com
#
# This file is part of EigenD.
#
# EigenD is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# EigenD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with EigenD.  If not, see <http://www.gnu.org/licenses/>.
#

import glob,zipfile,os
import sys

def iscompatible(mod_version, state_version):
    mod_version = mod_version.split('.')
    state_version = state_version.split('.')

    if len(state_version)==2:
        state_version.insert(0,'0')

    if len(mod_version)==2:
        mod_version.insert(0,'0')

    if mod_version[0] != state_version[0]:
        return False

    if mod_version[1] < state_version[1]:
        return False

    if mod_version[1] > state_version[1]:
        return True

    if mod_version[2] < state_version[2]:
        return False

    return True

class Registry:
    def __init__(self):
        self.__registry={}

    def dump(self,dumper):
        for (mname,vlist) in self.__registry.iteritems():
            for (version,(cversion,module)) in vlist.iteritems():
                print '%s:%s:%s %s' % (mname,version,cversion,dumper(module))

    def modules(self):
        return self.__registry.keys()

    def get_module(self,name):
        versions = self.__registry.get(name)

        if not versions:
            return None

        vkeys = versions.keys()
        vkeys.sort(reverse=True)
        (cversion,module) = versions[vkeys[0]]
        return module

    def get_compatible_module(self,name,state_cversion):
        actual_module = None
        actual_cversion = None

        for (mod_version,mod_cversion,mod) in self.iter_versions(name):
            if iscompatible(mod_cversion,state_cversion):
                if actual_module is None or mod_cversion>actual_cversion:
                    actual_module = mod
                    actual_cversion = mod_cversion

        return actual_module

    def iter_versions(self,name):
        mlist = self.__registry.get(name)
        if mlist:
            for (version,(cversion,module)) in mlist.iteritems():
                    yield (version,cversion,module)

    def add_module(self,name,version,cversion,module):
        r = self.__registry

        if name not in r:
            r[name] = {}

        if version in r[name]:
            raise RuntimeError('module %s:%s already defined' % (name,version))

        r[name][version] = (cversion,module)

    def scan_path(self,directory,klass):
        for p in glob.glob(os.path.join(directory,'*')):
            try:
                m = open(p,'r').read()
            except:
                continue

            for a in m.splitlines():
                a = a.split(':')
                (name,module,cversion,version) = a[0:4]
                self.add_module(name,version,cversion,klass(name,version,cversion,p,module))
                for e in a[4:]:
                    self.add_module(e,version,cversion,klass(name,version,cversion,p,module))
