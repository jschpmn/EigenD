
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

from pi import agent,atom,domain,errors,action,logic,async,index,utils,bundles,resource,paths,node,container,upgrade,timeout
from pi.logic.shortcuts import *
from plg_language import interpreter,database,feedback,noun,builtin_misc,builtin_conn,controller,context,variable,script,stage_server,widget,help_manager
from plg_language import interpreter_version as version

import piw
import time
import os
import xmlrpclib

def read_script(filename):
    filename = os.path.abspath(filename)
    dirname = os.path.dirname(filename)

    print 'started',filename
    script = open(filename,'r')

    for line in script:
        line = line.strip()

        if not line:
            continue

        if not line.startswith('#'):
            yield line
            continue

        if not line.startswith('#include'):
            continue

        include = line[8:].strip()
        if not os.path.isabs(include):
            include = os.path.join(dirname,include)

        for line in read_script(include):
            yield line

    print 'finished',filename


class Agent(agent.Agent):

    def __init__(self, address, ordinal):
        agent.Agent.__init__(self,signature=version, names='interpreter',protocols='langagent has_subsys',container=10,ordinal=ordinal)
        self.__transcript = resource.open_logfile('transcript')

        self.database = database.Database()
        self.help_manager = help_manager.HelpManager()

        self.__plumber = builtin_conn.Plumber(self,self.database,ordinal)
        self.__builtin = builtin_misc.Builtins(self,self.database)
        self[12] = script.ScriptManager(self,self.__runner)

        self.__ctxmgr = context.ContextManager(self)
        self.__statemgr = node.Server(value=piw.makestring('',0), change=self.__setstatemgr)

        self.__state = node.Server()
        self.__state[1] = self.__statemgr
        self.__state[2] = container.PersistentFactory(asserted=self.__ctlassert,retracted=self.__ctlretract)
        self.__state[3] = self.__ctxmgr
        
        self.set_private(self.__state)

        self.__interpreters = {}
        self.interpreter = interpreter.Interpreter(self, self.database, self, ctx=self.__ctxmgr.default_context())
        self.register_interpreter(self.interpreter)
        self.queue = interpreter.Queue(self.interpreter)

        self[11] = atom.Atom(rtransient=True)
        self.__history = feedback.History(self)
        self[11].set_private(self.__history)

        self.domain = piw.clockdomain_ctl()
        self.domain.set_source(piw.makestring('*',0))

        self.cloner = piw.clone(True)
        self.input = bundles.VectorInput(self.cloner.cookie(),self.domain,signals=(1,2,3,4),filter=piw.last_filter())

        self[1] = atom.Atom()

        self[1][1] = atom.Atom(domain=domain.BoundedFloat(0,1),policy=self.input.vector_policy(1,False),names='activation input')
        self[1][2] = atom.Atom(domain=domain.BoundedFloat(-1,1),policy=self.input.vector_policy(2,False),names='roll input')
        self[1][3] = atom.Atom(domain=domain.BoundedFloat(-1,1),policy=self.input.vector_policy(3,False),names='yaw input')
        self[1][4] = atom.Atom(domain=domain.BoundedFloat(0,1),policy=self.input.vector_policy(4,False),names='pressure input')

        self.cloner.set_output(1,self.__history.cookie())

        self[2] = atom.Atom(init='',names='word output',domain=domain.String())

        self[5] = atom.Atom(names='main kgroup')
        self[5][1] = bundles.Output(1,False, names='activation output')
        self[5][2] = bundles.Output(2,False, names='roll output')
        self[5][3] = bundles.Output(3,False, names='yaw output')
        self[5][4] = bundles.Output(4,False, names='pressure output')

        self[9] = atom.Atom(names='auxilliary kgroup')
        self[9][1] = bundles.Output(1,False, names='activation output')
        self[9][2] = bundles.Output(2,False, names='roll output')
        self[9][3] = bundles.Output(3,False, names='yaw output')
        self[9][4] = bundles.Output(4,False, names='pressure output')

        self.output1 = bundles.Splitter(self.domain,*self[5].values())
        self.output2 = bundles.Splitter(self.domain,*self[9].values())

        self.cloner.set_filtered_output(3,self.output1.cookie(), piw.last_lt_filter(9))
        self.cloner.set_filtered_output(4,self.output2.cookie(), piw.last_gt_filter(10))

        self.add_verb2(100,'create([],None,role(None,[matches([controller])]))',self.__ctlcreate)
        self.add_verb2(101,'create([un],None,role(None,[concrete,singular,issubject(create,[role(by,[cnc(~self)])])]))', self.__ctluncreate)
        self.add_verb2(30,'message([],None,role(None,[abstract]))', self.__message)
        self.add_verb2(150,'execute([],None,role(None,[abstract]))',self.__runscript)
        self.add_verb2(151,'load([],None,role(None,[matches([help])]))',self.__loadhelp)

        self.add_builtins(self.__builtin)
        self.add_builtins(self.__ctxmgr)

        self.database.add_module(builtin_conn)
        self.database.add_module(context)
        self.database.add_module(controller)

        self.__messages_on = True
        self[4] = atom.Atom(names='messages',policy=atom.default_policy(self.__messages),domain=domain.Bool(),init=True)
        self[3] = variable.VariableManager(self)

        self.add_builtins(self[3])

        self.add_subsystem('plumber',self.__plumber)

        # TODO: how should the port number be chosen to avoid clashes with multiple eigenDs?
        # perhaps could get a free socket like this:
        # import socket
        #s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        #s.bind(('',0))
        #port = s.getsockname()[1]
        #s.close()

        self.xmlrpc_server_port = 55553;

        self.widgets = widget.WidgetManager(self, self.xmlrpc_server_port)
        self.database.set_widget_manager(self.widgets)

        # Stage tab data
        self[14] = stage_server.TabList(self)
        self.tabs = self[14]

        self[15] = self.widgets

        self.add_verb2(102,'create([],None,role(None,[matches([widget])]),role(for,[concrete,singular]))',callback=self.__add_widget)
        self.add_verb2(103,'create([un],None,role(None,[matches([widget])]),role(for,[concrete,singular]))',callback=self.__del_widget)


    def __loadhelp(self,subject,dummy):
        self.help_manager.update()
        self.database.update_all_agents()


    def interpret(self,interp,scope,words):
        return noun.interpret(interp,scope,words)

    @async.coroutine('internal error')
    def __runscript(self,subject,arg):
        name = action.abstract_string(arg)
        print 'script',name
        r = self[12].run_script(name)

        if not r:
            yield async.Coroutine.success(errors.doesnt_exist(name,'execute'))

        yield r

    @async.coroutine('internal error')
    def rpc_canonical(self,arg):
        arg = arg.strip()
        spl = arg.split('->')

        if len(spl)==2:
            out = spl[1]
            arg = spl[0]
        else:
            out = None

        words = arg.strip().split()
        r = self.interpret(self.interpreter,None,words)

        yield r
        if not r.status():
            yield async.Coroutine.failure('not found')

        (objs,) = r.args()
        ids = objs.concrete_ids()
        rv = []
        db = self.database

        def bits(id):
            ids = set([id])
            ids.update(db.find_descendants(id))
            for did in db.find_joined_slaves(id):
                ids.add(did)
                ids.update(db.find_descendants(did))

            return ids

        for id in ids:
            for did in bits(id):
                if db.find_canonical(did):
                    d = '/'.join(db.find_full_canonical(did,as_string=False))
                    rv.append(d)

        rv.sort()
        text = '\n'.join(rv)

        if out:
            f=open(out,'w')
            f.write(text)
            f.close()
            text = out

        yield async.Coroutine.success(text)

    @async.coroutine('internal error')
    def rpc_identify(self,arg):
        words = arg.strip().split()
        r = self.interpret(self.interpreter,None,words)
        yield r
        if not r.status():
            yield async.Coroutine.failure('not found')

        (objs,) = r.args()
        ids = objs.concrete_ids()
        rv = []
        for id in ids:
            d = "'%s' %s" % (id,self.database.find_full_desc(id))
            rv.append(d)

        yield async.Coroutine.success('\n'.join(rv))

    @async.coroutine('internal error')
    def rpc_dump(self,arg):
        words = arg.strip().split()
        r = self.interpret(self.interpreter,None,words)
        yield r
        if not r.status():
            yield async.Coroutine.failure('not found')

        (objs,) = r.args()
        ids = objs.concrete_ids()
        rv = []

        for id in ids:
            rv.append(id)
            d = self.__plumber.get_connections(id)
            rv.extend(d)

        yield async.Coroutine.success('\n'.join(rv))


    def find_help(self,id):
        canonical_name = self.database.find_full_canonical(id,False)
        local_help = self.database.find_help(id)
        return (self.help_manager.find_help(canonical_name),local_help)

    @async.coroutine('internal error')
    def rpc_help(self,arg):
        words = arg.strip().split()
        r = self.interpret(self.interpreter,None,words)
        yield r
        if not r.status():
            yield async.Coroutine.failure('not found')

        (objs,) = r.args()
        ids = objs.concrete_ids()
        rv = []

        for id in ids:
            rv.append("help for %s" % id)
            canonical_name = self.database.find_full_canonical(id,False)
            rv.append(" canonical name: %s" % canonical_name)
            help_node = self.help_manager.find_help(canonical_name)
            if help_node:
                rv.append(" tool tip: %s" % help_node.get_tooltip())
                rv.append(" help text; %s" % help_node.get_helptext())

        yield async.Coroutine.success('\n'.join(rv))


    def __add_widget(self,subject,dummy,target):
        target = action.concrete_object(target)
        self.widgets.create_widget(target)

    def __del_widget(self,subject,dummy,target):
        target = action.concrete_object(target)
        self.widgets.destroy_widget(target)

    def __runner(self,name,script):

        class SubDelegate(interpreter.Delegate):
            def error_message(dself,err):
                return self.error_message(err)
            def buffer_done(dself,*a,**kw):
                return 

        words = script.split()
        interp = interpreter.Interpreter(self,self.database,SubDelegate())

        print 'doing',words
        self.register_interpreter(interp)
        r = interp.process_block(words)
        r2 = async.Deferred()

        def ok(*a,**k):
            self.unregister_interpreter(interp)
            r2.succeeded()

        def not_ok(*a,**k):
            self.unregister_interpreter(interp)
            r2.failed()

        r.setCallback(ok).setErrback(not_ok)
        return r2

    def get_variable(self,name):
        return self[3].get_var(name)

    def get_interpreter(self,interpid):
        return self.__interpreters.get(interpid)

    def register_interpreter(self,interp):
        interpid = str(id(interp))
        print 'register interpreter',interpid
        self.__interpreters[interpid] = interp

    def unregister_interpreter(self,interp):
        interpid = str(id(interp))
        print 'unregister interpreter',interpid
        try: del self.__interpreters[interpid]
        except: pass

    def __messages(self,v):
        print 'messages:',self.__messages_on,'->',v
        self.__messages_on=v

    def __message(self,subject,arg):
        if self.__messages_on:
            words = action.abstract_wordlist(arg)
            print 'message',words
            self.message(words)

    def create_echo(self,trigger,text):
        print 'create echo for',text,'trigger',trigger
        self.verb_defer(30,trigger,trigger,None,(logic.make_term('abstract',tuple(text.split(' '))),))

    def cancel_echo(self,triggers):
        for (id,ctx,cookie) in self.verb_events(lambda l,i: i==30):
            if ctx in triggers:
                print 'cancelling echo',id
                self.verb_cancel(id)

    def __ctlassert(self,index,value):
        c = controller.Controller(self.database,self,int(value))
        self.add_subsystem(index,c)
        return c

    def __ctlretract(self,index,value,state):
        self.remove_subsystem(index)
        state.disconnect()

    def __ctlcreate(self,subj,dummy):
        index=0
        for ss in self.iter_subsystem():
            if ss != 'plumber':
                index=max(index,int(ss))
        index=index+1
        s = self.__state[2].assert_state(str(index))
        return action.created_return(s.id())

    def __ctluncreate(self,subj,c):
        a = action.concrete_object(c)
        for (i,p) in self.iter_subsys_items():
            if p.id()==a:
                self.__state[2].retract_state(lambda v,s: v==str(i))
                return action.removed_return(a)
        return async.failure('not found')

    def __setstatemgr(self,arg):
        if arg.is_string():
            self.set_statemgr(arg.as_string())

    def set_statemgr(self,sm):
        self.interpreter.set_statemgr(sm)
        self.__statemgr.set_data(piw.makestring(sm,0))

    def checkpoint_undo(self):
        return self.interpreter.checkpoint_undo()

    def clear_history(self):
        print 'clear_history'
        self.__history.clear_history()

    def __timestamp(self):
        return time.strftime('%Y-%m-%d-%H-%M-%S', time.gmtime(time.time()))

    def __find_builtins(self,provider,prefix,act):
        for k in dir(provider):
            x = k.split('_',2)

            if len(x) < 2 or x[0] != prefix:
                continue

            try: l=int(x[1])
            except: continue

            act(l,getattr(provider,k))

    def add_builtins(self,provider):
        self.database.add_module(provider)

        def verb(label,func): self.add_verb2(label,func.__doc__.strip(),callback=func)
        def iverb(label,func): self.add_verb2(label,func.__doc__.strip(),callback=func,need_interp=True)
        def mode(label,func): self.add_mode2(label,func.__doc__.strip(),*func())

        self.__find_builtins(provider,'iverb2',iverb)
        self.__find_builtins(provider,'verb2',verb)
        self.__find_builtins(provider,'mode2',mode)

    def message(self,words,desc='message',speaker=''):
        print 'language_plg:message words=',words,'desc=',desc,'speaker=',speaker
        if speaker:
            speaker=self.database.find_desc(speaker)
        m=[]
        if words[0]=='*':
            for w in words:
                m.append(w)
        else:
            for w in words:
                music=self.database.translate_to_music2(w) 
                if music:
                    m.extend(music)
                else:
                    m.extend(w)
        self.__history.message(m,desc,speaker)

    def error_message(self,err):
        speaker='' 
        print 'language_plg:error_message',err
        msg=errors.render_message(self.database,err[0])
        if len(err)>1:
            speaker=err[1]
        self.message(msg,'err_msg',speaker)    

    def rpc_inject(self,msg):
        for word in msg.split():
            m = self.database.translate_to_music2(word)

            for music in m:
                if music.startswith('!'): music=music[1:]
                self.__history.inject(music)

    def inject(self,word):
        music = self.database.translate_to_music(word)
        if music.startswith('!'): music=music[1:]
        self.__history.inject(music)

    def update_timestamp(self,t):
        self[11].set_property_string('timestamp',str(t))

    def word_in(self,word):
        print 'word input:',word
        english = self.database.translate_to_english(word) or ''
        music = self.database.translate_to_music(word) or ''
        self.__log(english,music)
        self.queue.interpret(word)

    def word_out(self):
        w = self.interpreter.undo()
        m = self.database.translate_to_music(w) if w else w
        return m

    def line_clear(self):
        self.__log('undo')
        return self.interpreter.line_clear()

    def __log(self,*words):
        print >> self.__transcript,' '.join(words)
        self.__transcript.flush()

    @async.coroutine('internal error')
    def rpc_execfile(self,filename):
        if not os.path.exists(filename):
            print 'script:',filename,'doesnt exist'
            yield async.Coroutine.success()

        interp = interpreter.Interpreter(self,self.database,interpreter.Delegate())

        self.register_interpreter(interp)
        count_good = 0
        count_bad = 0

        try:
            for line in read_script(filename):
                words = line.split()
                print 'running',line
                r = interp.process_block(words)
                yield r

                if r.status():
                    count_good+=1
                else:
                    count_bad+=1

        finally:
            self.unregister_interpreter(interp)

        print 'finished',filename,'good lines',count_good,'bad lines',count_bad
        yield async.Coroutine.success()

    def rpc_upgrade(self,arg):
        filename = resource.find_release_resource('upgrades',arg)

        if not filename:
            return async.failure('upgrade script %s doesnt exist' % arg)

        return self.rpc_execfile(filename)

    @async.coroutine('internal error')
    def rpc_script(self,arg):
        words = arg.strip().split()
        interp = interpreter.Interpreter(self,self.database, interpreter.Delegate(), ctx=self.__ctxmgr.default_context())

        self.register_interpreter(interp)

        try:
            print 'running',words
            r = interp.process_block(words)
            yield r
            yield async.Coroutine.completion(r.status(),*r.args(),**r.kwds())

        finally:
            self.unregister_interpreter(interp)

    def rpc_exec(self,arg):
        words = arg.strip().split()
        self.__log(*words)
        return self.interpreter.process_block(words)

    def server_opened(self):
        agent.Agent.server_opened(self)
        self.advertise('<language>')
        self.database.start('<main>')

        # startup stage server
        print "starting up stage server"

        self.snapshot = piw.tsd_snapshot()

        self.stageServer = stage_server.stageXMLRPCServer(self, self.snapshot, self.xmlrpc_server_port)
        self.stageServer.start()

        print "server proxy","http://0.0.0.0:"+str(self.xmlrpc_server_port)
        self.dummyStageClient = xmlrpclib.ServerProxy( "http://0.0.0.0:"+str(self.xmlrpc_server_port) )


    def close_server(self):
        self.database.stop()
        agent.Agent.close_server(self)
        
        # shutdown stage server
        print "shutting down stage server"
        self.stageServer.stop()

        try:
            # dummy call to unlock the socket deadlock
            print self.dummyStageClient.ping()
        except:
            print 'caught xmlrpc server shutdown exception'


    def buffer_done(self,status,msg,repeat):
        self.__history.buffer_done(status,msg,repeat)






class Upgrader(upgrade.Upgrader):
    def upgrade_1_0_0_to_1_0_1(self,tools,address):
        root = tools.get_root(address)
        print 'upgrade Stage widget atoms'
        root.ensure_node(14)
        root.ensure_node(15)

    def upgrade_7_0_to_8_0(self,tools,address):
        root = tools.root(address)
        for ss in root.get_node(255,6,2).iter():
            addr = '<interpreter/%d>' % ss.path[-1]
            print 'upgrade',addr
            if not controller.upgrade_7_0_to_8_0(tools,address,addr):
                return False
        return True

    def upgrade_6_0_to_7_0(self,tools,address):
        root = tools.root(address)
        old = root.get_node(10,253)
        if old is not None:
            oldid = old.id()
            tmp = root.ensure_node(15)
            tmp.copy(old)
            old.erase_all_children()
            rep = old.ensure_node(1)
            rep.copy(tmp)
            tools.substitute_connection(oldid,rep.id())
            tmp.erase()
        return True

    def upgrade_5_0_to_6_0(self,tools,address):
        root = tools.root(address)
        root.ensure_node(10).erase_children()
        for ss in tools.get_subsystems(address):
            if not controller.upgrade_5_0_to_6_0(tools,address,ss):
                return False
        return True

    def upgrade_4_0_to_5_0(self,tools,address):
        root = tools.root(address)
        root.get_node(8).erase()
        return True

    def upgrade_2_0_to_3_0(self,tools,address):
        root = tools.root(address)
        fb = root.get_node(6)
        if fb is not None: fb.erase()
        return True

    def upgrade_3_0_to_4_0(self,tools,address):
        for ss in tools.get_subsystems(address):
            if not controller.upgrade_3_0_to_4_0(tools,address,ss):
                return False
        return True

agent.main(Agent,Upgrader)
