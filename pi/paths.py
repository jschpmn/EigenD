
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

import struct
import piw
from pi import utils

def valid_id(id):
    id = id2server(id)
    return True if len(id)>2 and id.startswith('<') and id.endswith('>') else False

def make_subst(id):
    server = id2server(id)
    subst = { 'self':id, 'server':server, 'a':server, 's':id }

    id = id2parent(id)

    if id is not None:
        subst['parent']=id
        subst['p']=id
        id = id2parent(id)
        if id is not None:
            subst['grandparent']=id
            subst['pp']=id

    return subst

def id2parent(a):
    (a,p) = breakid_list(a)
    if len(p)==0:
        return None
    return makeid_list(a,*p[0:-1])

def makeid_list(server,*path):
    if not path:
        return server
    return server+'#'+'.'.join([str(p) for p in path])

def id2server(id):
    if '#' not in id:
        return id
    (a,p) = id.split('#')
    return a

def path2grist(path):
    if ':' not in path:
        p = path.split('.')
        if len(p)>0:
            return int(p[-1])
        return None

    (p1,p2) = path.split(':')
    p = p2.split('.')
    if len(p)>0:
        return int(p[-1])
    return None

def breakid_list(path):
    if '#' not in path:
        return (path,[])
    (a,p) = path.split('#')
    return (a,[int(c) for c in p.split('.') if c])

def breakid(path):
    if '#' not in path:
        return (piw.makestring(path,0),piw.pathnull(0))
    (a,p) = path.split('#')
    return (piw.makestring(a,0),piw.parsepath(p,0))

def id2child(id,*c):
    (s,p) = breakid_list(id)
    p.extend(c)
    return makeid_list(s,*p)
