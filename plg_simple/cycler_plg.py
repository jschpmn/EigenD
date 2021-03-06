
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

import piw
from pi import const,agent,atom,bundles,domain,upgrade,paths,utils,node
from plg_simple import cycler_version as version

class Agent(agent.Agent):

    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version,names='cycler',container=10,ordinal=ordinal)

        self.set_private(node.Server())
        self.get_private()[1]=node.Server(change=self.__cycle_set,value=piw.makebool(True,0))
        self.get_private()[2]=node.Server(change=self.__invert_set,value=piw.makebool(False,0))

        self[2] = atom.Atom()
        self[2][1] = bundles.Output(1,False,names='activation output')
        self[2][2] = bundles.Output(2,False,names='pressure output')
        self[2][3] = bundles.Output(3,False,names='roll output')
        self[2][4] = bundles.Output(4,False,names='yaw output')
        self[2][5] = bundles.Output(5,False,names='scale note output')
        self[2][6] = bundles.Output(6,False,names='frequency output')
        self[2][7] = bundles.Output(16,False,names='damper output')

        self.add_verb2(1,'cycle([],None)',self.__cycle_on)
        self.add_verb2(2,'cycle([un],None)',self.__cycle_off)

        self.domain = piw.clockdomain_ctl()
        self.domain.set_source(piw.makestring('*',0))
        self.main_output = bundles.Splitter(self.domain,self[2][1],self[2][2],self[2][3],self[2][4],self[2][5],self[2][6])
        self.damp_output = bundles.Splitter(self.domain,self[2][7])
        self.cycler = piw.cycler(self.domain,32,self.main_output.cookie(),self.damp_output.cookie(),False)
        self.input = bundles.VectorInput(self.cycler.main_cookie(),self.domain,signals=(1,2,3,4,5,6,16,17),threshold=5)
        self.feedback = bundles.VectorInput(self.cycler.feedback_cookie(),self.domain,signals=(1,),threshold=5)

        self.cycler.set_cycle(True)
        self.cycler.set_maxdamp(1.0)
        self.cycler.set_invert(False)
        self.cycler.set_curve(1.0)

        self[1] = atom.Atom()
        self[1][1]=atom.Atom(domain=domain.BoundedFloat(0,1),policy=self.input.vector_policy(1,False),names='activation input')
        self[1][2]=atom.Atom(domain=domain.BoundedFloat(0,1),policy=self.input.vector_policy(2,False),names='pressure input')
        self[1][3]=atom.Atom(domain=domain.BoundedFloat(-1,1),policy=self.input.vector_policy(3,False),names='roll input')
        self[1][4]=atom.Atom(domain=domain.BoundedFloat(-1,1),policy=self.input.vector_policy(4,False),names='yaw input')
        self[1][5]=atom.Atom(domain=domain.BoundedFloat(0,1000),policy=self.input.vector_policy(5,False),names='scale note input')
        self[1][6]=atom.Atom(domain=domain.BoundedFloat(0,96000),policy=self.input.vector_policy(6,False),names='frequency input')
        self[1][7]=atom.Atom(domain=domain.BoundedFloat(0,1),policy=self.feedback.vector_policy(1,False,clocked=False),names='feedback input')
        self[1][8]=atom.Atom(domain=domain.BoundedFloat(0,1),init=0.0,policy=self.input.linger_policy(16,False),names='damper pedal input',container=(None,'damper',self.verb_container()))
        self[1][10]=atom.Atom(domain=domain.BoundedFloat(0,1),init=1.0,policy=atom.default_policy(self.__setmaxdamp),names='damper maximum input')
        self[1][12]=atom.Atom(domain=domain.BoundedFloat(0,1),policy=self.input.latch_policy(17,False),names='hold pedal input')
        self[1][13]=atom.Atom(domain=domain.BoundedFloat(0.1,10),init=1,names="damper curve",policy=atom.default_policy(self.__setdcurve))

        self[1][8].add_verb2(1,'invert([],None)',self.__invert_on)
        self[1][8].add_verb2(2,'invert([un],None)',self.__invert_off)

    def __setmaxdamp(self,v):
        self.cycler.set_maxdamp(v)
        return True

    def __invert_on(self,subj):
        self.cycler.set_invert(True)
        self.get_private()[2].set_data(piw.makebool(True,0))

    def __invert_off(self,subj):
        self.cycler.set_invert(False)
        self.get_private()[2].set_data(piw.makebool(False,0))

    def __cycle_on(self,subj):
        self.cycler.set_cycle(True)
        self.get_private()[1].set_data(piw.makebool(True,0))

    def __cycle_off(self,subj):
        self.cycler.set_cycle(False)
        self.get_private()[1].set_data(piw.makebool(False,0))

    def __invert_set(self,d):
        if d.is_bool():
            self.get_private()[2].set_data(d)
            self.cycler.set_invert(d.as_bool())

    def __cycle_set(self,d):
        if d.is_bool():
            self.get_private()[1].set_data(d)
            self.cycler.set_cycle(d.as_bool())

    def __setdcurve(self,x):
        self.cycler.set_curve(x)
        return True

class Upgrader(upgrade.Upgrader):
    def upgrade_3_0_to_4_0(self,tools,address):
        root = tools.root(address)
        #root.ensure_node(10).erase_children()
        return True
        
    def upgrade_2_0_to_3_0(self,tools,address):
        root = tools.root(address)
        root.ensure_node(255,17)
        p = root.ensure_node(255,6)
        oc = p.get_data()
        if not oc.is_bool(): oc=piw.makebool(False,0)
        p.set_data(piw.data())
        p.ensure_node(1).set_data(oc)
        p.ensure_node(2).set_data(piw.makebool(False,0))
        return True

agent.main(Agent,Upgrader)
