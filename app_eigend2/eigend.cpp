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

#include <piw/piw_tsd.h>
#include <piw/piw_thing.h>
#include <picross/pic_power.h>
#include <picross/pic_thread.h>
#include <picross/pic_time.h>
#include <picross/pic_tool.h>
#include <picross/pic_resources.h>

#include "eigend.h"

#include <lib_juce/ejuce.h>
#include <lib_juce/ejuce_laf.h>
#include <lib_juce/epython.h>

#include "jucer/StatusComponent.h"
#include "jucer/LoadDialogComponent.h"
#include "jucer/LoadProgressComponent.h"
#include "jucer/SaveDialogComponent.h"
#include "jucer/EditDialogComponent.h"
#include "jucer/AboutComponent.h"
#include "jucer/BugComponent.h"
#include "jucer/AlertComponent1.h"
#include "jucer/AlertComponent2.h"
#include "jucer/HelpComponent.h"
#include "jucer/InfoComponent.h"

#include <algorithm>

static const char *save_help_label = "Help With Setups";
static const char *save_help_text = ""
"About Setups\n"
"\n"
"EigenD Setups contain everything about your current playing setup, including recordings that you have made using the built in recorders, which scales and instruments you have selected or loaded (including AU/VST instruments) and the settings of those instruments. \n"
"\n"
"EigenD Setups are not stored using filenames but instead as a Belcanto name. This is so that you can recall and save setups directly from the instrument using Belcanto, from the Commander, keys on the instrument or preprogrammed Talker keys to do this for you. The standard MIDI IN mapping includes a Talker that recalls User Setups 1 to 10 for you, mapped to notes from C0 to A0.\n"
"\n"
"You will find that when you load setups, the amount of time this takes depends on how different they are. Setups that only differ in a few ways, for example some recordings or scale changes, will only take a few seconds to load. Setups that have different AU or VST instruments loaded or different Soundfonts in the samplers will take longer. Setups that have different key layouts or arrangements of instruments will normally take the longest. Loading individual Setups for the first time after a software upgrade may take a little longer as the Setup may need to be updated to run with the new software.\n"
"\n"
"\n"
"Using Basic Setups\n"
"\n"
"The simplest way to use Setups is to store them as a User Setup number. You can do this by using the top option on the Save panel. Pressing 'New' finds the next available unused number and assigns it for you.\n"
"\n"
"You can add text tags to your saved Setup to help you find it again later. You can, for example, use the name of the song you are playing as a tag. You can add a long description as well.\n"
"\n"
"\n"
"Using Advanced Setups\n"
"\n"
"If you want to use a full Belcanto name to save a setup you can use the second and third options in the Save panel. \n"
"\n"
"Words typed in as text must exist in the Belcanto Dictionary (which can be seen in the EigenD Commander). If you want to save a Setup by using Belcanto notes, these must be from 1 to 8. If you are saving a Setup using notes you are not confined to Belcanto words in the dictionary. You can use any sequence of notes in the major scale (typed in a numbers 1-8) as words. If they appear in the Dictionary, you will see the text translation appear in the text input box. Words that have no translation into text will appear with a '!' in front of them to let you know that they are not a number but a new Belcanto word. \n"
"\n"
"When you go to load a Setup you will see that all your saved Setups are arranged in a hierarchy which uses each part of the Belcanto name as a level. This, along with the tags, helps you to organise and remember large collections of Setups.\n";


#ifdef PI_MACOSX
#define TOOL_BROWSER "EigenBrowser"
#define TOOL_COMMANDER "EigenCommander"
#define TOOL_SCANNER "EigenScanner"
#define TOOL_STAGE "Stage"
#endif

#ifdef PI_WINDOWS
#define TOOL_BROWSER "EigenBrowser"
#define TOOL_COMMANDER "EigenCommander"
#define TOOL_SCANNER "Scanner"
#define TOOL_STAGE "Stage"
#endif

#ifdef PI_LINUX
#define TOOL_BROWSER "eigenbrowser"
#define TOOL_COMMANDER "eigencommander"
#define TOOL_SCANNER "scanner"
#define TOOL_STAGE "Stage"
#endif

class EigenD;
class EigenLoadComponent;

class EigenBugComponent;
class EigenMainWindow;

#define c2w(t) ((juce_wchar *)(juce::String((t))))

enum EigenCommands
{
    commandAbout = 1000,
    commandStartStatus,
    commandStartSave,
    commandStartLoad,
    commandStartBug,
    commandStartBrowser,
    commandStartCommander,
    commandStartScanner,
    commandStartStage,
    commandMainWindow,
    commandQuit,
    commandDownload,
    commandWindow // must be last
};

class EigenAlertDelegate
{
    public:
        virtual ~EigenAlertDelegate() {}
        virtual void alert_ok() {}
        virtual void alert_cancel() {}
};

class EigenAlertComponent2: public AlertComponent2
{
    public:
        EigenAlertComponent2(EigenMainWindow *main,const String &klass,const String &label, const String &text, EigenAlertDelegate *listener);
        ~EigenAlertComponent2();
        void set_listener(EigenAlertDelegate *l);
        void buttonClicked (Button* b);

    private:
        EigenMainWindow *main_;
        EigenAlertDelegate *listener_;
};

class EigenAlertComponent1: public AlertComponent1
{
    public:
        EigenAlertComponent1(EigenMainWindow *main,const String &klass,const String &label, const String &text);
        ~EigenAlertComponent1();
        void buttonClicked (Button* buttonThatWasClicked);

    private:
        EigenMainWindow *main_;
};

class EigenInfoComponent: public InfoComponent
{
    public:
        EigenInfoComponent(EigenMainWindow *main,const String &klass,const String &label, const String &text);
        ~EigenInfoComponent();
        void buttonClicked (Button* buttonThatWasClicked);

    private:
        EigenMainWindow *main_;
};

class EigenHelpComponent: public HelpComponent
{
    public:
        EigenHelpComponent(EigenMainWindow *main,const String &klass,const String &label, const String &text);
        ~EigenHelpComponent();
        void buttonClicked (Button* buttonThatWasClicked);

    private:
        EigenMainWindow *main_;
};

class EigenLogger
{
    public:
        EigenLogger(const char *prefix, const pic::f_string_t &logger);
        EigenLogger(const EigenLogger &l);
        EigenLogger &operator=(const EigenLogger &l);
        bool operator==(const EigenLogger &l) const;
        static pic::f_string_t create(const char *prefix, const pic::f_string_t &logger);
        void operator()(const char *msg) const;

    private:
        juce::String prefix_;
        pic::f_string_t logger_;
};

class EigenDialog: public DocumentWindow
{
    public:
        EigenDialog(EigenMainWindow *main,Component *content,int,int,int,int,int,int,Component *position = 0);
        void closeButtonPressed();
        ~EigenDialog();
    private:
        EigenMainWindow *main_;
};

class EigenMainWindow: public DocumentWindow, public MenuBarModel, public piw::thing_t, public eigend::p2c_t, public ApplicationCommandTarget
{
    public:
        EigenMainWindow(ApplicationCommandManager *mgr, pia::scaffold_gui_t *scaffold, eigend::c2p_t *backend, const pic::f_string_t &logger);
        ~EigenMainWindow();
        void getAllCommands (Array <CommandID>& commands);
        void getCommandInfo (const CommandID commandID, ApplicationCommandInfo& result);
        ApplicationCommandTarget *getNextCommandTarget();
        const PopupMenu getMenuForIndex (int topLevelMenuIndex, const String& /*menuName*/);
        bool perform (const InvocationInfo& info);
        void thing_timer_slow();
        void set_cpu(unsigned cpu);
        void load_started(const char *setup);
        void load_status(const char *msg,unsigned progress);
        void load_ended();
        pic::f_string_t make_logger(const char *prefix);
        void closeButtonPressed();
        void windowClosed(Component *window);
        ApplicationCommandManager *manager() { return manager_; }
        eigend::c2p_t *backend() { return backend_; }
        void init_menu();
        const StringArray getMenuBarNames();
        void menuItemSelected  (int menuItemID, int topLevelMenuIndex);
        void setups_changed(const char *file);
        EigenDialog *alert1(const String &klass, const String &label, const String &text);
        EigenDialog *alert2(const String &klass, const String &label, const String &text, EigenAlertDelegate *listener);
        EigenDialog *info(const String &klass, const String &label, const String &text);
        void ignore_klass(const juce::String &klass);
        bool select_setup(const char *setup);
        void save_dialog(const std::string &current);
        void edit_dialog(const std::string &current);
        void alert_dialog(const char *klass, const char *label, const char *text);
        void info_dialog(const char *klass, const char *label, const char *text);
        void dialog_dead(EigenDialog *dialog);
        void show_save_help(bool);
        void set_latest_release(const char *);
        std::string get_cookie();
        std::string get_os();

    private:
        EigenLoadComponent *component_;
        ApplicationCommandManager *manager_;
        pia::scaffold_gui_t *scaffold_;
        eigend::c2p_t *backend_;
        EigenDialog *status_;
        EigenDialog *saving_;
        EigenDialog *editing_;
        EigenDialog *about_;
        pic::f_string_t logger_;
        EigenDialog *bug_;
        pic::tool_t browser_;
        pic::tool_t commander_;
        pic::tool_t scanner_;
        pic::tool_t stage_;
        std::set<std::string> ignores_;
        std::string new_version_;
        std::string current_setup_;
        std::map<std::string,EigenDialog *> alert_dialogs_;
        std::map<std::string,EigenDialog *> info_dialogs_;
        EigenDialog *progress_;
        EigenDialog *help_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EigenMainWindow);
};

class EigenD : public ejuce::Application, virtual public pic::tracked_t
{
    public:
        EigenD();
        ~EigenD();
        const char *get_user();
        void initialise (const String& commandLine);
        void shutdown();
        const String getApplicationName();
        const String getApplicationVersion();
        bool moreThanOneInstanceAllowed();
        void anotherInstanceStarted (const String& commandLine);
        void log(const char *msg);
        void handleWinch(const std::string &msg);

    private:
        EigenMainWindow *main_window_;
        epython::PythonInterface *python_;
        pia::context_t context_;
        FILE *logfile_;
};

class EigenSaveComponent: public SaveDialogComponent, public EigenAlertDelegate
{
    public:
        EigenSaveComponent(EigenMainWindow *mediator, const std::string &current);
        virtual ~EigenSaveComponent();
        void buttonClicked (Button* buttonThatWasClicked);
        void comboBoxChanged (ComboBox* comboBoxThatHasChanged);
        void alert_ok();
        void alert_cancel();
        void cancel_confirm();
        unsigned getUserNumber(const juce::String &tag);

    private:
        EigenMainWindow *mediator_;
        EigenDialog *confirm_;
        piw::term_t term_;
        int max_;
};

class EigenEditComponent: public EditDialogComponent
{
    public:
        EigenEditComponent(EigenMainWindow *mediator, const std::string &current);
        virtual ~EigenEditComponent();
        void buttonClicked (Button* buttonThatWasClicked);

    private:
        EigenMainWindow *mediator_;
        piw::term_t term_;
        std::string orig_;
};

class EigenBugComponent: public BugComponent
{
    public:
        EigenBugComponent(EigenMainWindow *mediator);
        virtual ~EigenBugComponent();
        void buttonClicked (Button* buttonThatWasClicked);

    private:
        EigenMainWindow *mediator_;
};

class EigenLoadComponent;

class EigenTreeItem: public TreeViewItem
{
    public:
        EigenTreeItem (EigenLoadComponent *view, const piw::term_t &term);
        ~EigenTreeItem();
        void itemSelectionChanged(bool isNowSelected);
        void itemDoubleClicked(const MouseEvent &);
        int getItemWidth() const;
        bool mightContainSubItems();
        void paintItem (Graphics& g, int width, int height);
        void itemOpennessChanged (bool isNowOpen);
        void tree_changed(const piw::term_t &term);
        bool select_setup(const char *setup);
        piw::data_t name();
        juce::String getSlot();

    private:
        piw::term_t term_;
        EigenLoadComponent *view_;
};

class EigenLoadComponent: public LoadDialogComponent, public EigenAlertDelegate
{
    public:
        EigenLoadComponent(EigenMainWindow *mediator);
        void updateUpgradeToggle(bool enabled);
        void updateSetupButtons(bool term, bool user);
        void selected(const piw::term_t &term, bool dbl);
        void buttonClicked(Button *b);
        void tree_changed();
        virtual ~EigenLoadComponent();
        EigenMainWindow *mediator() { return mediator_; }
        bool select_setup(const char *setup);
        void alert_ok();
        void alert_cancel();
        void cancel_confirm();

    private:
        EigenMainWindow *mediator_;
        EigenDialog *confirm_;
        EigenTreeItem *model_;
        piw::data_t slot_;
        piw::data_t selected_;
};

void EigenTreeItem::itemSelectionChanged(bool isNowSelected)
{
    if(isNowSelected)
    {
        pic::logmsg() << "item selected" << term_.render();
        if(term_.arity()>2)
        {
            view_->selected(term_,false);
        }
        else
        {
            view_->getDefaultToggle()->setToggleState(false,false);
            view_->updateSetupButtons(false, false);
            view_->updateUpgradeToggle(false);
        }
    }
}

void EigenTreeItem::itemDoubleClicked(const MouseEvent &)
{
    pic::logmsg() << "item dbl clicked " << term_.render();
    if(term_.arity()>2)
    {
        view_->selected(term_,true);
    }
    else
    {
        view_->updateSetupButtons(false, false);
        view_->updateUpgradeToggle(false);
    }
}

piw::data_t EigenTreeItem::name()
{
    return term_.arg(0).value();
}

class EigenStatusComponent: public StatusComponent
{
    public:
        EigenStatusComponent(EigenMainWindow *mediator);
        virtual ~EigenStatusComponent();

    private:
        EigenMainWindow *mediator_;
};

class EigenAboutComponent: public AboutComponent
{
    public:
        EigenAboutComponent(EigenMainWindow *mediator);
        virtual ~EigenAboutComponent();

    private:
        EigenMainWindow *mediator_;
};

bool EigenMainWindow::perform (const InvocationInfo& info)
{
    if(info.commandID>=commandWindow)
    {
        unsigned w = info.commandID-commandWindow+1;
        scaffold_->set_window_state(w-1,!scaffold_->window_state(w-1));
        return true;
    }

    switch (info.commandID)
    {
        case commandAbout:
            if(!about_)
            {
                about_ = new EigenDialog(this,new EigenAboutComponent(this),300,400,0,0,0,0,this);
            }
            break;

        case commandStartStatus:
            if(!status_)
            {
                status_ = new EigenDialog(this,new EigenStatusComponent(this),250,24,250,50,2000,50);
            }
            else
            {
                delete status_;
                status_=0;
            }
            menuItemsChanged();
            break;

        case commandStartCommander:
            commander_.start();
            break;

        case commandStartBrowser:
            browser_.start();
            break;

        case commandStartScanner:
            scanner_.start();
            break;

        case commandStartStage:
            stage_.start();
            break;
            
        case commandStartBug:
            if(!bug_)
            {
                bug_ = new EigenDialog(this,new EigenBugComponent(this),700,600,700,600,2000,2000,this);
            }
            break;

        case commandStartSave:
            save_dialog(current_setup_);
            break;

        case commandStartLoad:
            if(!isVisible())
            {
                setVisible(true);
                menuItemsChanged();
            }
            break;

        case commandMainWindow:
#ifdef JUCE_MAC
            setVisible(!isVisible());
            menuItemsChanged();
#else
            setMinimised(!isMinimised());
#endif
            break;

        case commandQuit:
            backend_->prepare_quit();
//#ifdef JUCE_WINDOWS
//            TerminateProcess(GetCurrentProcess(),0);
//#else
            JUCEApplication::quit();
//#endif
            break;

        case commandDownload:
            URL u(T("http://www.eigenlabs.com/downloads/releases/"));
            u.launchInDefaultBrowser();
            break;
    }

    return true;
}

void EigenLoadComponent::updateSetupButtons(bool term, bool user)
{
    getDefaultToggle()->setEnabled(term);
    getLoadButton()->setEnabled(term);
    getEditButton()->setEnabled(term && user);
    getDeleteButton()->setEnabled(term && user);
}

void EigenLoadComponent::updateUpgradeToggle(bool enabled)
{
    getUpgradeToggle()->setEnabled(enabled);
    getUpgradeToggle()->setToggleState(enabled,false);
}

void EigenLoadComponent::selected(const piw::term_t &term,bool dbl)
{
    slot_=term.arg(3).value();
    selected_=term.arg(4).value();
    bool upg=term.arg(5).value().as_bool();

    pic::logmsg() << "selected " << selected_ << " " << upg;
    std::string d = mediator_->backend()->get_description(selected_.as_string());
    getLabel()->setText(d.c_str(),false);

    updateSetupButtons(true,term.arg(6).value().as_bool());
    updateUpgradeToggle(upg);

    std::string default_setup = mediator_->backend()->get_default_setup(true);
    getDefaultToggle()->setToggleState(!default_setup.compare(selected_.as_string()),false);

    pic::logmsg() << "selected2 " << selected_ << " " << upg;

    if(dbl)
    {
	pic::logmsg() << "selected3 " << selected_ << " " << upg;
        buttonClicked(getLoadButton());
	pic::logmsg() << "selected4 " << selected_ << " " << upg;
    }
}

void EigenMainWindow::show_save_help(bool show)
{
    if(show)
    {
        if(!help_)
        {
            help_ = new EigenDialog(this,new EigenHelpComponent(this,save_help_label,save_help_label,save_help_text),500,600,500,600,2000,2000,this);
        }
    }
    else
    {
        if(help_)
        {
            delete help_;
            help_=0;
        }
    }
}

void EigenLoadComponent::buttonClicked(Button *b)
{
    if(b==getLoadButton())
    {
        pic::logmsg() << "load button " << selected_;

        if(selected_.is_string())
        {
            bool upg = false;

            if(getUpgradeToggle()->isEnabled())
            {
                upg = getUpgradeToggle()->getToggleState();
            }

            pic::logmsg() << "loading " << selected_;
            mediator_->backend()->load_setup(selected_.as_string(),upg);
        }
    }
    else if (b==getEditButton())
    {
        mediator_->edit_dialog(selected_.as_string());
    }
    else if (b==getDeleteButton())
    {
        juce::String msg = T("This will delete a setup named\n\n");
        msg += slot_.as_string();
        confirm_ = mediator_->alert2("Delete Setup","Delete Setup",msg,this);
    }
    else if (b==getDefaultToggle())
    {
        if(b->getToggleState())
        {
            mediator_->backend()->set_default_setup(selected_.as_string());
        }
        else
        {
            mediator_->backend()->set_default_setup("");
        }
    }
}

EigenLoadComponent::~EigenLoadComponent()
{
    cancel_confirm();
}

void EigenLoadComponent::cancel_confirm()
{
    if(confirm_)
    {
        ((EigenAlertComponent2 *)(confirm_->getContentComponent()))->set_listener(0);
        delete confirm_;
        confirm_=0;
    }
}

EigenLoadComponent::EigenLoadComponent(EigenMainWindow *mediator): mediator_(mediator), confirm_(0)
{
    setName(T("Load"));
    model_ = new EigenTreeItem(this,mediator_->backend()->get_setups());
    model_->setOpen(true);
    getTreeView()->setMultiSelectEnabled(false);
    getTreeView()->setRootItem(model_);
    getLabel()->setText("",false);
}

void EigenLoadComponent::alert_ok()
{
    mediator_->backend()->delete_setup(slot_.as_string());
    confirm_ = 0;
}

void EigenLoadComponent::alert_cancel()
{
    confirm_ = 0;
}

bool EigenLoadComponent::select_setup(const char *setup)
{
    bool rv = model_->select_setup(setup);
    model_->setOpen(true);

    getSetupLabel()->setText(T(""),0);

    if(rv)
    {
        EigenTreeItem *i = (EigenTreeItem *)(getTreeView()->getSelectedItem(0));

        if(i)
        {
            getSetupLabel()->setText(i->getSlot(),false);
        }
    }

    return rv;
}

void EigenLoadComponent::tree_changed()
{
    typedef std::pair<TreeViewItem*, std::string> ItemStackEntry;

    // traverse the tree to collect the opened branches
    std::vector<ItemStackEntry> items;
    std::vector<std::string> opened;

    items.push_back(ItemStackEntry(model_,""));
    while(!items.empty())
    {
        ItemStackEntry current=items.back();
        items.pop_back();
        for(int i=0;i<current.first->getNumSubItems();i++)
        {
            TreeViewItem *item=current.first->getSubItem(i);
            std::string name=current.second+"\t"+std::string(((EigenTreeItem*)item)->name().as_string());
            if(item->isOpen())
            {
                opened.push_back(name);
            }
            items.push_back(ItemStackEntry(item,name));
        }
    }

    model_->tree_changed(mediator_->backend()->get_setups());

    // traverse the tree to restore the opened branches
    std::vector<std::string>::iterator it;
    items.clear();

    items.push_back(ItemStackEntry(model_,""));
    while(!items.empty())
    {
        ItemStackEntry current=items.back();
        items.pop_back();
        for(int i=0;i<current.first->getNumSubItems();i++)
        {
            TreeViewItem *item=current.first->getSubItem(i);
            std::string name=current.second+"\t"+std::string(((EigenTreeItem*)item)->name().as_string());
            if(opened.end()!=std::find(opened.begin(),opened.end(),name))
            {
                item->setOpen(true);
            }
            items.push_back(ItemStackEntry(item,name));
        }
    }
}

std::string EigenMainWindow::get_os()
{
    String os_name = juce::SystemStats::getOperatingSystemName();

#ifdef JUCE_MAC

    os_name = os_name+T(" 10.")+String(juce::PlatformUtilities::getOSXMinorVersionNumber());

#endif

    if(juce::SystemStats::isOperatingSystem64Bit())
    {
        os_name = os_name+T(" (64)");
    }

    std::string rv = os_name.toUTF8();
    return rv;
}

std::string EigenMainWindow::get_cookie()
{
	juce::Array<MACAddress> mac_addresses;
	juce::MACAddress::findAllAddresses(mac_addresses);

    if(mac_addresses.size()>0)
    {
        std::string rv = mac_addresses[0].toString().toUTF8();
        return rv;
    }

    return "";

}

void EigenMainWindow::set_latest_release(const char *release)
{
    JUCE_AUTORELEASEPOOL
    new_version_ = release;
    menuItemsChanged();
}

void EigenMainWindow::setups_changed(const char *file)
{
    JUCE_AUTORELEASEPOOL
    component_->tree_changed();
    if(file && file[0])
    {
        current_setup_ = file;
        select_setup(file);
    }
}

File getPluginsDir()
{
    return File(pic::global_library_dir().c_str()).getChildFile("Plugins");
}

EigenMainWindow::EigenMainWindow(ApplicationCommandManager *mgr, pia::scaffold_gui_t *scaffold, eigend::c2p_t *backend, const pic::f_string_t &log):
    DocumentWindow (T("eigenD"), Colours::black, DocumentWindow::allButtons, true),
    manager_(mgr), scaffold_(scaffold), backend_(backend), status_(0), saving_(0), editing_(0), about_(0), logger_(log), bug_(0),
    browser_(pic::private_tools_dir(),TOOL_BROWSER), commander_(pic::private_tools_dir(),TOOL_COMMANDER), scanner_(pic::private_tools_dir(),TOOL_SCANNER), 
    stage_(pic::public_tools_dir(),TOOL_STAGE), progress_(0), help_(0)
{
    backend->upgrade_setups();

    component_ = new EigenLoadComponent(this);
    setContentComponent (component_, true, true);
    centreWithSize (getWidth(), getHeight());
    setUsingNativeTitleBar(true);
    setResizable(true,true);
    setResizeLimits(600,500,2000,2000);
    setVisible (true);
    pic::to_front();
    toFront(true);
    component_->getSetupLabel()->setText(T(""),false);

    piw::tsd_thing(this);
    timer_slow(500);

    pic::logmsg() << "latest version: " << new_version_;

    init_menu();

    manager_->registerAllCommandsForTarget(this);
    manager_->getKeyMappings()->resetToDefaultMappings();
    addKeyListener(manager_->getKeyMappings());
    mgr->setFirstCommandTarget(this);

    backend->initialise(this,scaffold,get_cookie(),get_os());

    std::string setup = backend->get_default_setup(false);

    if(setup.length())
    {
        if(select_setup(setup.c_str()))
        {
            pic::printmsg() << "default setup: " << setup;
            backend->load_setup(setup.c_str(),false);
        }
    }

    juce::File f(getPluginsDir().getChildFile(T("plugins_cache")));
    if(!f.exists())
    {
        pic::logmsg() << "starting plugin scan..";
        scanner_.start();

        while(scanner_.isrunning())
        {
            juce::Thread::sleep(250);
        }

        pic::logmsg() << "plugin scan finished";
    }
}

EigenMainWindow::~EigenMainWindow()
{
    browser_.quit();
    commander_.quit();
    scanner_.quit();
    stage_.quit();
    backend_->quit();

    cancel_timer_slow();

    if (status_ != 0) delete status_;
    if (about_ != 0) delete about_;
    if (saving_ != 0) delete saving_;
    if (editing_ != 0) delete editing_;
    if (bug_ != 0) delete bug_;
}

void EigenMainWindow::getAllCommands (Array <CommandID>& commands)
{
    const CommandID ids[] = { 
        commandAbout,
        commandStartStatus,
        commandStartSave,
        commandStartLoad,
        commandStartBug,
        commandStartBrowser,
        commandStartCommander,
        commandStartScanner,
        commandStartStage,
        commandMainWindow,
        commandQuit,
        commandDownload,
        commandWindow,
        commandWindow+1,
        commandWindow+2,
        commandWindow+3,
        commandWindow+4,
        commandWindow+5,
        commandWindow+6,
        commandWindow+7,
        commandWindow+8,
        commandWindow+9,
        commandWindow+10,
        commandWindow+11,
        commandWindow+12,
        commandWindow+13,
        commandWindow+14,
        commandWindow+15,
        commandWindow+16,
        commandWindow+17,
        commandWindow+18,
        commandWindow+19
    };

    commands.addArray (ids, numElementsInArray (ids));
}

void EigenMainWindow::getCommandInfo (const CommandID commandID, ApplicationCommandInfo& result)
{
    const String generalCategory (T("General"));

    if(commandID>=commandWindow)
    {
        unsigned w = commandID-commandWindow+1;
        String j = String(T("Plugin Window "));
        j += w;
        result.setInfo (j,j, generalCategory, 0);
        if(w<10)
        {
            result.addDefaultKeypress (((char)(w+'0')), ModifierKeys::commandModifier);
            result.setTicked(scaffold_->window_state(w-1));
        }
        return;
    }

    switch (commandID)
    {
        case commandAbout:
            result.setInfo (T("About eigenD"), T("Program Information"), generalCategory, 0);
            break;

        case commandStartStatus:
            result.setInfo (T("System Usage Meter"), T("System Usage Meter"), generalCategory, 0);
            result.addDefaultKeypress (T('x'), ModifierKeys::commandModifier);
            result.setTicked(status_ && status_->isVisible());
            break;

        case commandStartCommander:
            result.setInfo (T("EigenCommander"), T("EigenCommander"), generalCategory, 0);
            result.addDefaultKeypress (T('c'), ModifierKeys::commandModifier);
            break;

        case commandStartBrowser:
            result.setInfo (T("EigenBrowser"), T("EigenBrowser"), generalCategory, 0);
            result.addDefaultKeypress (T('b'), ModifierKeys::commandModifier);
            break;

        case commandStartScanner:
            result.setInfo (T("Plugin Scanner"), T("Plugin Scanner"), generalCategory, 0);
            //result.addDefaultKeypress (T('S'), ModifierKeys::commandModifier);
            break;

        case commandStartStage:
            result.setInfo (T("Stage"), T("Stage"), generalCategory, 0);
            result.addDefaultKeypress (T('g'), ModifierKeys::commandModifier);
            break;
            
        case commandStartBug:
            result.setInfo (T("Report Bug"), T("Report Bug"), generalCategory, 0);
            result.addDefaultKeypress (T('u'), ModifierKeys::commandModifier);
            break;

        case commandStartLoad:
            result.setInfo (T("Load Setup"), T("Load Setup"), generalCategory, 0);
            result.addDefaultKeypress (T('l'), ModifierKeys::commandModifier);
            break;

        case commandStartSave:
            result.setInfo (T("Save Setup"), T("Save Setup"), generalCategory, 0);
            result.addDefaultKeypress (T('s'), ModifierKeys::commandModifier);
            break;

        case commandMainWindow:
            result.setInfo (T("Load Window"), T("Load Window"), generalCategory, 0);
            result.addDefaultKeypress (T('w'), ModifierKeys::commandModifier);
            result.setTicked(isVisible());
            break;

        case commandQuit:
            result.setInfo (T("Quit"), T("Quit eigenD"), generalCategory, 0);
            result.addDefaultKeypress (T('q'), ModifierKeys::commandModifier);
            break;

        case commandDownload:
            juce::String v = T("Download "); v += juce::String(new_version_.c_str());
            result.setInfo (v, T("Download New Version"), generalCategory, 0);
            result.addDefaultKeypress (T('d'), ModifierKeys::commandModifier);
            break;
    }
}

ApplicationCommandTarget *EigenMainWindow::getNextCommandTarget()
{
    return 0;
}

const PopupMenu EigenMainWindow::getMenuForIndex (int topLevelMenuIndex, const String& /*menuName*/)
{
    PopupMenu menu;

    if (topLevelMenuIndex == 0)
    {
        menu.addCommandItem (manager(),commandStartLoad);
        menu.addCommandItem (manager(),commandStartSave);
        menu.addSeparator();
        menu.addCommandItem (manager(),commandStartBug);
        menu.addCommandItem (manager(),commandAbout);
#ifndef PI_MACOSX
        menu.addSeparator();
        menu.addCommandItem (manager(),commandQuit);
#endif
    }

    if (topLevelMenuIndex == 1)
    {
        menu.addCommandItem (manager(),commandStartStatus);
        menu.addCommandItem (manager(),commandMainWindow);

        menu.addSeparator();
        unsigned n = scaffold_->window_count();

        for(unsigned i=0;i<n;i++)
        {
            std::string t = scaffold_->window_title(i);
            if(t.length()>0)
                menu.addCommandItem(manager(),commandWindow+i,t.c_str());
        }
    }

    if (topLevelMenuIndex == 2)
    {
        menu.addCommandItem (manager(),commandStartBrowser);
        menu.addCommandItem (manager(),commandStartCommander);
        menu.addCommandItem (manager(),commandStartStage);
        menu.addSeparator();
        menu.addCommandItem (manager(),commandStartScanner);
    }

    if (topLevelMenuIndex == 3)
    {
        menu.addCommandItem (manager(),commandDownload);
    }

    return menu;
}


void EigenMainWindow::thing_timer_slow()
{
    set_cpu(scaffold_->cpu_usage());
}

void EigenMainWindow::set_cpu(unsigned cpu)
{
    if(status_)
    {
        ((StatusComponent *)(status_->getContentComponent()))->set_cpu(cpu);
        status_->getPeer()->performAnyPendingRepaintsNow();
    }
}

void EigenMainWindow::load_started(const char *setup)
{
    JUCE_AUTORELEASEPOOL

#ifdef DISABLE_FAST_THREAD_AT_LOAD
    scaffold_->fast_pause();
#endif

    if(progress_)
    {
        delete progress_;
        progress_ = 0;
    }

    LoadProgressComponent *c = new LoadProgressComponent();
    juce::String title = T("Loading ");
    title += setup;
    c->setName(title);
    c->getMessageLabel()->setText(T(""),false);
    c->getProgressSlider()->setValue(0,false,false);
    progress_ = new EigenDialog(this,c,400,60,0,0,0,0,this);
    progress_->getPeer()->performAnyPendingRepaintsNow();
}

void EigenMainWindow::load_status(const char *msg,unsigned progress)
{
    JUCE_AUTORELEASEPOOL

    if(progress_)
    {
        LoadProgressComponent *c = (LoadProgressComponent *)(progress_->getContentComponent());
        c->getMessageLabel()->setText(msg,false);
        c->getProgressSlider()->setValue(progress,false,false);
        progress_->getPeer()->performAnyPendingRepaintsNow();
    }

}

void EigenMainWindow::load_ended()
{
    JUCE_AUTORELEASEPOOL

#ifdef DISABLE_FAST_THREAD_AT_LOAD
    scaffold_->fast_resume();
#endif

    if(progress_)
    {
        delete progress_;
        progress_ = 0;
    }
}

void EigenMainWindow::closeButtonPressed()
{
#ifdef JUCE_MAC
    setVisible(false);
#else
    setMinimised(!isMinimised());
#endif
}


void EigenMainWindow::init_menu()
{
#ifdef JUCE_MAC
    PopupMenu extra;
    extra.addCommandItem (manager(),commandAbout);
    MenuBarModel::setMacMainMenu(this,&extra);
#else
    setMenuBar(this);
#endif
}

const StringArray EigenMainWindow::getMenuBarNames()
{
    if(new_version_.length()>0)
    {
        const tchar* const names[] = { T("File"), T("Window"), T("Tools"), T("Update Available"),0 };
        return StringArray ((const tchar**) names);
    }
    else
    {
        const tchar* const names[] = { T("File"), T("Window"), T("Tools"), 0 };
        return StringArray ((const tchar**) names);
    }
}

void EigenMainWindow::menuItemSelected  (int menuItemID, int topLevelMenuIndex)
{
}

EigenTreeItem::EigenTreeItem (EigenLoadComponent *view, const piw::term_t &term): term_(term), view_(view)
{
}

bool EigenTreeItem::select_setup(const char *setup)
{
    piw::term_t ch = term_.arg(1);

    if(ch.arity()>0)
    {
        setOpen(true);

        unsigned n = getNumSubItems();

        for(unsigned i=0;i<n;i++)
        {
            EigenTreeItem *et = (EigenTreeItem *)getSubItem(i);

            if(et->select_setup(setup))
            {
                return true;
            }
        }

        setOpen(false);
    }

    if(term_.arity()>2)
    {
        if(!strcmp(term_.arg(4).value().as_string(),setup))
        {
            setSelected(true,true);
            return true;
        }
    }

    return false;
}

void EigenTreeItem::tree_changed(const piw::term_t &term)
{
    term_ = term;
    clearSubItems();
    treeHasChanged();
    setOpen(false);
    setOpen(true);
}

EigenTreeItem::~EigenTreeItem()
{
}

int EigenTreeItem::getItemWidth() const
{
    return -1;
}

bool EigenTreeItem::mightContainSubItems()
{
    piw::term_t ch = term_.arg(1);

    if(ch.arity()>0)
    {
        return true;
    }

    return false;
}

juce::String EigenTreeItem::getSlot()
{
    if(term_.arity()>2)
    {
        return term_.arg(3).value().as_string();
    }

    return T("");
}

void EigenTreeItem::paintItem (Graphics& g, int width, int height)
{
    juce::String s;

    s = term_.arg(0).value().as_string();

    if(term_.arity()>2)
    {
        piw::data_t tag  = term_.arg(2).value();
        if(tag.is_string() && strlen(tag.as_string())>0)
        {
            s+=" ("; s+=String::fromUTF8(tag.as_string()); s+=")";
        }
    }

    if (isSelected()) g.fillAll (Colours::black.withAlpha (0.2f));

    g.setColour (Colours::black);
    g.setFont (height * 0.7f);
    g.drawText (s,4, 0, width - 4, height, Justification::centredLeft, true);
}

void EigenTreeItem::itemOpennessChanged (bool isNowOpen)
{
    if (isNowOpen)
    {
        if (getNumSubItems() == 0)
        {
            piw::term_t ch = term_.arg(1);

            for(unsigned i=0;i<ch.arity();i++)
            {
                addSubItem(new EigenTreeItem(view_,ch.arg(i)));
            }
        }
    }
}

EigenBugComponent::EigenBugComponent(EigenMainWindow *mediator): mediator_(mediator)
{
    setName(T("Bug"));
    pic::msg_t bug_report;

    bug_report << "OS: " << SystemStats::getOperatingSystemName().toUTF8() << " "

#if JUCE_MAC
               << " 10." << PlatformUtilities::getOSXMinorVersionNumber()
#endif
               << "64 bit: " << (SystemStats::isOperatingSystem64Bit()?"yes":"no") << "\n"
               << "CPU: " << SystemStats::getCpuVendor() << " "
               << "Cores: " << SystemStats::getNumCpus() << " "
               << "Speed: " << SystemStats::getCpuSpeedInMegaherz() << " mhz\n"
               << "Memory: " << SystemStats::getMemorySizeInMegabytes() << " mb\n"
               << "Version: " << pic::release() << "\n" << "\n";

    name_editor()->setText(mediator->backend()->get_username().c_str(),false);
    email_editor()->setText(mediator->backend()->get_email().c_str(),false);
    subject_editor()->setText(mediator->backend()->get_subject().c_str(),false);
    description_editor()->setText(bug_report.str().c_str());

    if(name_editor()->getText().length()==0)
    {
        name_editor()->grabKeyboardFocus();
        return;
    }

    if(email_editor()->getText().length()==0)
    {
        email_editor()->grabKeyboardFocus();
        return;
    }

    if(subject_editor()->getText().length()==0)
    {
        subject_editor()->grabKeyboardFocus();
        return;
    }

    description_editor()->grabKeyboardFocus();
}

EigenBugComponent::~EigenBugComponent()
{
}

void EigenBugComponent::buttonClicked (Button* buttonThatWasClicked)
{
    mediator_->backend()->file_bug(
          name_editor()->getText().trim().toUTF8(),
          email_editor()->getText().trim().toUTF8(),
          subject_editor()->getText().trim().toUTF8(),
          description_editor()->getText().trim().toUTF8()
    );

    delete getTopLevelComponent();
}

unsigned EigenSaveComponent::getUserNumber(const juce::String &tag)
{
    juce::String tag2 = tag.trim();

    if(!tag2.startsWith(T("user ")))
    {
        return 0;
    }

    juce::String tag3 = tag.substring(5).trim();
    juce::String tag4 = tag3.removeCharacters(T("0123456789"));

    if(tag3.length()==0 || tag4.length()>0)
    {
        return 0;
    }

    return tag3.getIntValue();
}

EigenSaveComponent::EigenSaveComponent(EigenMainWindow *mediator, const std::string &current): mediator_(mediator), confirm_(0)
{
    pic::logmsg() << "save component " << current;
    setName(T("Save"));
    term_ = mediator->backend()->get_user_setups();
    max_ = 0;
    bool c = false;
    bool cun = false;

    getUserButton()->setToggleState(true,false);
    getErrorLabel()->setVisible(false);

    for(unsigned i=1;i<term_.arity();i++)
    {
        juce::String sltt = term_.arg(i).arg(1).value().as_string();
        juce::String sltt2 = mediator_->backend()->words_to_notes(sltt.toUTF8()).c_str();

        getWordsChooser()->addItem(sltt,i);
        getNotesChooser()->addItem(sltt2,i);

        int un = getUserNumber(sltt);

        if(!strcmp(current.c_str(),term_.arg(i).arg(2).value().as_string()))
        {
            getWordsChooser()->setText(sltt);
            c = true;
            if(un) cun=true;
        }

        if(!un)
        {
            continue;
        }

        juce::String usr = sltt;
        if(term_.arg(i).arg(0).value().is_string())
        {
            usr = sltt + " (" + term_.arg(i).arg(0).value().as_string() + ")";
        }
        getUserChooser()->addItem(usr,i);

        if(un>max_) max_=un;
    }

    max_+=1;


    if(!c)
    {
        //buttonClicked(newButton());
        getWordsChooser()->setEnabled(false);
        getNotesChooser()->setEnabled(false);
        getUserChooser()->setEnabled(true);
        getWordsLabel()->setEnabled(false);
        getNotesLabel()->setEnabled(false);
        getUserLabel()->setEnabled(true);
        getDescription()->setText(T(""),false);
    }
    else
    {
        comboBoxChanged(getWordsChooser());
        getNotesLabel()->setEnabled(false);
        getNotesChooser()->setEnabled(false);

        getWordsChooser()->setEnabled(!cun);
        getWordsLabel()->setEnabled(!cun);
        getWordsButton()->setToggleState(!cun,false);

        getUserChooser()->setEnabled(cun);
        getUserLabel()->setEnabled(cun);
        getUserButton()->setToggleState(cun,false);

        juce::String d = mediator_->backend()->get_description(current.c_str()).c_str();
        getDescription()->setText(d,false);
    }
}

EigenSaveComponent::~EigenSaveComponent()
{
    mediator_->show_save_help(false);
    cancel_confirm();
}

void EigenSaveComponent::cancel_confirm()
{
    if(confirm_)
    {
        ((EigenAlertComponent2 *)(confirm_->getContentComponent()))->set_listener(0);
        delete confirm_;
        confirm_=0;
    }
}

void EigenSaveComponent::comboBoxChanged (ComboBox* comboBoxThatHasChanged)
{
    cancel_confirm();

    if(comboBoxThatHasChanged == getUserChooser())
    {
        juce::String t(term_.arg(getUserChooser()->getSelectedId()).arg(1).value().as_string());
        getWordsChooser()->setText(t,false);
        comboBoxChanged(getWordsChooser());
        return;
    }

    if(comboBoxThatHasChanged == getWordsChooser())
    {
        juce::String t = getWordsChooser()->getText().trim();
        int un = getUserNumber(t);

        if(un>0)
        {
            getUserChooser()->setText(t,true);
        }
        else
        {
            getUserChooser()->setText(T(""),true);
        }

        for(unsigned i=1;i<term_.arity();i++)
        {
            juce::String sltt = term_.arg(i).arg(1).value().as_string();
            sltt.trim();

            if(sltt.compare(t)==0)
            {
                if(term_.arg(i).arg(0).value().is_string())
                {
                    juce::String desc = term_.arg(i).arg(0).value().as_string();
                    getSummary()->setText(desc,false);
                }
                else
                {
                    getSummary()->setText(T(""),false);
                }

                juce::String d = mediator_->backend()->get_description(term_.arg(i).arg(2).value().as_string()).c_str();
                getDescription()->setText(d,false);
            }
        }

        getWordsChooser()->setText(t,true);
        std::string n = mediator_->backend()->words_to_notes(t.toUTF8());
        getErrorLabel()->setVisible(false);

        if(n.length()==0)
        {
            getSaveButton()->setEnabled(false);

            if(t.length()>0 || getNotesChooser()->getText().length()>0)
            {
                getErrorLabel()->setVisible(true);
            }
        }
        else
        {
            getNotesChooser()->setText(n.c_str(),true);
            getSaveButton()->setEnabled(true);
        }
        
        return;
    }

    if(comboBoxThatHasChanged == getNotesChooser())
    {
        juce::String t = getNotesChooser()->getText();
        std::string n = mediator_->backend()->notes_to_words(t.toUTF8());
        juce::String tn(n.c_str());
        getWordsChooser()->setText(tn,true);
        comboBoxChanged(getWordsChooser());
        return;
    }

}

void EigenSaveComponent::buttonClicked (Button* buttonThatWasClicked)
{
    cancel_confirm();

    if(buttonThatWasClicked == getUserButton())
    {
        getWordsChooser()->setEnabled(false);
        getNotesChooser()->setEnabled(false);
        getUserChooser()->setEnabled(true);
        getWordsLabel()->setEnabled(false);
        getNotesLabel()->setEnabled(false);
        getUserLabel()->setEnabled(true);
        return;
    }

    if(buttonThatWasClicked == getWordsButton())
    {
        getWordsChooser()->setEnabled(true);
        getNotesChooser()->setEnabled(false);
        getUserChooser()->setEnabled(false);
        getWordsLabel()->setEnabled(true);
        getNotesLabel()->setEnabled(false);
        getUserLabel()->setEnabled(false);
        comboBoxChanged(getWordsChooser());
        return;
    }

    if(buttonThatWasClicked == getNotesButton())
    {
        getWordsChooser()->setEnabled(false);
        getNotesChooser()->setEnabled(true);
        getUserChooser()->setEnabled(false);
        getWordsLabel()->setEnabled(false);
        getNotesLabel()->setEnabled(true);
        getUserLabel()->setEnabled(false);
        comboBoxChanged(getWordsChooser());
        return;
    }

    if(buttonThatWasClicked == getNewButton())
    {
        juce::String slot = juce::String::formatted(T("user %d"),max_);
        getUserChooser()->setText(slot,true);
        getWordsChooser()->setText(slot,true);

        getWordsChooser()->setEnabled(false);
        getNotesChooser()->setEnabled(false);
        getUserChooser()->setEnabled(true);
        getWordsLabel()->setEnabled(false);
        getNotesLabel()->setEnabled(false);
        getUserLabel()->setEnabled(true);

        getUserButton()->setToggleState(true,false);
        getNotesButton()->setToggleState(false,false);
        getWordsButton()->setToggleState(false,false);
        comboBoxChanged(getWordsChooser());
        return;
    }

    if(buttonThatWasClicked == getHelpButton())
    {
        mediator_->show_save_help(true);
        return;
    }

    if(buttonThatWasClicked == getSaveButton())
    {
        juce::String slot = getWordsChooser()->getText().trim();

        if(slot.length()==0)
        {
            return;
        }

        std::string old_setup = mediator_->backend()->get_setup_slot(slot.toUTF8());
        getSaveButton()->setEnabled(false);

        if(old_setup.length()==0)
        {
            alert_ok();
            return;
        }

        juce::String msg = T("This will overwrite an existing setup named\n\n");
        msg += slot;

        if(strcmp(old_setup.c_str(),"none"))
        {
            msg += T("\n\nand tagged\n\n");
            msg += juce::String(old_setup.c_str());
        }

        confirm_ = mediator_->alert2("Overwrite Setup","Overwrite Setup",msg,this);
    }
}

void EigenSaveComponent::alert_ok()
{
    EigenMainWindow *m = mediator_;
    juce::String slot = getWordsChooser()->getText().trim();
    juce::String name = getSummary()->getText().trim();
    juce::String desc = getDescription()->getText().trim();
    m->backend()->save_setup(slot.toUTF8(),name.toUTF8(),desc.toUTF8(),getDefaultButton()->getToggleState());
    confirm_ = 0;
    delete getTopLevelComponent();
}

void EigenSaveComponent::alert_cancel()
{
    confirm_ = 0;
    getSaveButton()->setEnabled(true);
}

EigenEditComponent::EigenEditComponent(EigenMainWindow *mediator, const std::string &current): mediator_(mediator)
{
    pic::logmsg() << "edit component " << current;

    term_ = mediator->backend()->get_user_setups();

    for(unsigned i=1;i<term_.arity();i++)
    {
        if(!strcmp(current.c_str(),term_.arg(i).arg(2).value().as_string()))
        {
            orig_=term_.arg(i).arg(2).value().as_string();

            juce::String slot=term_.arg(i).arg(1).value().as_string();
            getSlot()->setText(slot,false);

            if(term_.arg(i).arg(0).value().is_string())
            {
                juce::String desc=term_.arg(i).arg(0).value().as_string();
                getSummary()->setText(desc,false);
            }
            else
            {
                getSummary()->setText(T(""),false);
            }

            break;
        }
    }

    juce::String d=mediator_->backend()->get_description(current.c_str()).c_str();
    getDescription()->setText(d,false);
}

EigenEditComponent::~EigenEditComponent()
{
    mediator_->show_save_help(false);
}

void EigenEditComponent::buttonClicked (Button* buttonThatWasClicked)
{
    if(buttonThatWasClicked == helpButton())
    {
        mediator_->show_save_help(true);
        return;
    }

    getButton()->setEnabled(false);

    juce::String slot = getSlot()->getText().trim();
    juce::String name = getSummary()->getText().trim();
    juce::String desc = getDescription()->getText().trim();
    mediator_->backend()->edit_setup(orig_.c_str(),slot.toUTF8(),name.toUTF8(),desc.toUTF8());

    delete getTopLevelComponent();
}

bool EigenMainWindow::select_setup(const char *setup)
{
    return component_->select_setup(setup);
}

EigenDialog::EigenDialog(EigenMainWindow *main,Component *content,int w,int h,int mw,int mh,int xw,int xh,Component *position): DocumentWindow(content->getName(), Colours::black, DocumentWindow::closeButton, true), main_(main)
{
    setContentComponent (content, true, true);
    setSize(w,h);

    if(position)
    {
        centreAroundComponent (position, getWidth(), getHeight());
    }
    else
    {
        setCentrePosition (160,80);
    }

    setUsingNativeTitleBar(true);
    setVisible (true);
    toFront(true);

    if(mw>0)
    {
        setResizeLimits(mw,mh,xw,xh);
        setResizable (true, true);
    }

}

void EigenDialog::closeButtonPressed()
{
    delete this;
}

EigenDialog::~EigenDialog()
{
    main_->windowClosed(this);
    main_->dialog_dead(this);
}

EigenD::EigenD(): main_window_ (0), python_(0), logfile_(0)
{
}

EigenD::~EigenD()
{
}

const char *EigenD::get_user()
{
#ifdef JUCE_WINDOWS
    return getenv("USERNAME");
#else
    return getenv("USER");
#endif
}

void EigenD::initialise (const String& commandLine)
{
    pic_set_interrupt();
    pic_mlock_code();
    pic_init_time();

    printf("release root: %s\n",pic::release_root_dir().c_str());

    pic::f_string_t primary_logger = pic::f_string_t::method(this,&EigenD::log);
    pic::f_string_t eigend_logger = EigenLogger::create("eigend",primary_logger);

    ejuce::Application::initialise(commandLine,get_user(),eigend_logger,false,true);
    LookAndFeel::setDefaultLookAndFeel(new ejuce::EJuceLookandFeel);
    ApplicationCommandManager *manager = new ApplicationCommandManager();

    python_ = new epython::PythonInterface();
    context_ = scaffold()->context(pic::status_t(),eigend_logger,"eigend");

    piw::tsd_setcontext(context_.entity());
    python_->py_startup();

    if(python_->init_python("app_eigend2.backend","main"))
    {
        eigend::c2p_t *backend = (eigend::c2p_t *)python_->mediator();
        if(backend)
        {
            backend->set_args(commandLine.toUTF8());
            std::string logfile = backend->get_logfile();

            if(logfile.length()>0)
            {
                logfile_ = fopen(logfile.c_str(),"w");
            }

            main_window_ = new EigenMainWindow(manager,scaffold(),backend,primary_logger);
        }
        else
        {
            juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "An unexpected error occurred ...", python_->last_error().c_str());
        }
    }
    else
    {
        juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "An unexpected error occurred ...", python_->last_error().c_str());
    }
}

void EigenD::shutdown()
{
    if (main_window_ != 0) delete main_window_;

    ejuce::Application::shutdown();
}

const String EigenD::getApplicationName()
{
    return T("eigenD");
}

const String EigenD::getApplicationVersion()
{
    return pic::release().c_str();
}

bool EigenD::moreThanOneInstanceAllowed()
{
    return false;
}

void EigenD::log(const char *msg)
{
    if(logfile_)
    {
        fprintf(logfile_,"%s\n",msg);
        fflush(logfile_);
    }
    else
    {
        printf("%s\n",msg);
        fflush(stdout);
    }
}

void EigenD::anotherInstanceStarted (const String& commandLine)
{
    pic::logmsg() << "new instance";
}

EigenStatusComponent::EigenStatusComponent(EigenMainWindow *mediator): mediator_(mediator)
{
    setName(T("System Usage Meter"));
}

EigenStatusComponent::~EigenStatusComponent()
{
}


EigenAboutComponent::EigenAboutComponent(EigenMainWindow *mediator): mediator_(mediator)
{
    setName(T("About"));
}

EigenAboutComponent::~EigenAboutComponent()
{
}

void EigenMainWindow::windowClosed(Component *window)
{
    if(window==about_) { about_=0; }
    if(window==status_) { status_=0; }
    if(window==saving_) { saving_=0; }
    if(window==editing_) { editing_=0; }
    if(window==bug_) { bug_=0; }
    if(window==progress_) { progress_=0; }
    if(window==help_) { help_=0; }
}

pic::f_string_t EigenMainWindow::make_logger(const char *prefix)
{
    JUCE_AUTORELEASEPOOL
    return EigenLogger::create(prefix,logger_);
}

void EigenMainWindow::save_dialog(const std::string &current)
{
    if(!saving_)
    {
        saving_ = new EigenDialog(this,new EigenSaveComponent(this,current),600,600,600,600,2000,2000,this);
    }
}

void EigenMainWindow::edit_dialog(const std::string &current)
{
    if(!editing_)
    {
        editing_ = new EigenDialog(this,new EigenEditComponent(this,current),600,500,600,500,2000,2000,this);
    }
}

void EigenMainWindow::alert_dialog(const char *klass, const char *label, const char *text)
{
    JUCE_AUTORELEASEPOOL
    alert1(klass,label,text);
}

EigenDialog *EigenMainWindow::alert1(const String &klass, const String &label, const String &text)
{
    std::string cklass = klass.toUTF8();

    if(ignores_.find(cklass) != ignores_.end())
    {
        return 0;
    }

    EigenAlertComponent1 *c = new EigenAlertComponent1(this,klass,label,text);
    EigenDialog *e = new EigenDialog(this,c,400,600,400,600,2000,2000,this);

    std::map<std::string,EigenDialog *>::iterator i;

    if((i=alert_dialogs_.find(cklass)) != alert_dialogs_.end())
    {
        e->setBounds(i->second->getBounds());
        delete i->second;
    }

    alert_dialogs_.insert(std::make_pair(cklass,e));
    return e;
}

EigenDialog *EigenMainWindow::alert2(const String &klass, const String &label, const String &text, EigenAlertDelegate *l)
{
    std::string cklass = klass.toUTF8();

    if(ignores_.find(cklass) != ignores_.end())
    {
        l->alert_ok();
        return 0;
    }

    EigenAlertComponent2 *c = new EigenAlertComponent2(this,klass,label,text,l);
    EigenDialog *e = new EigenDialog(this,c,400,600,400,600,2000,2000,this);

    std::map<std::string,EigenDialog *>::iterator i;

    if((i=alert_dialogs_.find(cklass)) != alert_dialogs_.end())
    {
        delete i->second;
    }

    alert_dialogs_.insert(std::make_pair(cklass,e));
    return e;
}

void EigenMainWindow::info_dialog(const char *klass, const char *label, const char *text)
{
    JUCE_AUTORELEASEPOOL
    info(klass,label,text);
}

EigenDialog *EigenMainWindow::info(const String &klass, const String &label, const String &text)
{
    std::string cklass = klass.toUTF8();

    EigenInfoComponent *c = new EigenInfoComponent(this,klass,label,text);
    EigenDialog *e = new EigenDialog(this,c,400,300,400,300,2000,2000,this);

    std::map<std::string,EigenDialog *>::iterator i;

    if((i=info_dialogs_.find(cklass)) != info_dialogs_.end())
    {
        delete i->second;
    }

    info_dialogs_.insert(std::make_pair(cklass,e));
    return e;
}

void EigenMainWindow::dialog_dead(EigenDialog *e)
{
    std::map<std::string,EigenDialog *>::iterator i;

    if((i=alert_dialogs_.find(std::string(e->getName().toUTF8()))) != alert_dialogs_.end())
    {
        if(e==i->second)
        {
            alert_dialogs_.erase(i);
        }
    }

    if((i=info_dialogs_.find(std::string(e->getName().toUTF8()))) != info_dialogs_.end())
    {
        if(e==i->second)
        {
            info_dialogs_.erase(i);
        }
    }
}

EigenAlertComponent2::EigenAlertComponent2(EigenMainWindow *main,const String &klass,const String &label, const String &text, EigenAlertDelegate *listener): main_(main), listener_(listener)
{
    setName(klass);
    set_title(label);
    set_text(text);
}

EigenAlertComponent2::~EigenAlertComponent2()
{
    if(listener_)
    {
        listener_->alert_cancel();
    }
}

void EigenAlertComponent2::set_listener(EigenAlertDelegate *l)
{
    listener_ = l;
}

void EigenAlertComponent2::buttonClicked (Button* b)
{
    if(get_toggle_button()->getToggleState())
    {
        main_->ignore_klass(getName());
    }

    if(b == get_ok_button())
    {
        EigenAlertDelegate *listener = listener_;
        listener_ = 0;

        delete getTopLevelComponent();

        if(listener)
        {
            listener->alert_ok();
        }
    }
    else
    {
        delete getTopLevelComponent();
    }

}

EigenHelpComponent::EigenHelpComponent(EigenMainWindow *main,const String &klass,const String &label, const String &text): main_(main)
{
    setName(klass);

    set_title(label);
    set_text(text);
}

EigenHelpComponent::~EigenHelpComponent()
{
}

void EigenHelpComponent::buttonClicked (Button* buttonThatWasClicked)
{
    delete getTopLevelComponent();
}

EigenAlertComponent1::EigenAlertComponent1(EigenMainWindow *main,const String &klass,const String &label, const String &text): main_(main)
{
    setName(klass);

    set_title(label);
    set_text(text);
}

EigenAlertComponent1::~EigenAlertComponent1()
{
}

void EigenAlertComponent1::buttonClicked (Button* buttonThatWasClicked)
{
    if(get_toggle_button()->getToggleState())
    {
        main_->ignore_klass(getName());
    }

    delete getTopLevelComponent();
}

EigenInfoComponent::EigenInfoComponent(EigenMainWindow *main,const String &klass,const String &label, const String &text): main_(main)
{
    setName(klass);

    set_title(label);
    set_text(text);
}

EigenInfoComponent::~EigenInfoComponent()
{
}

void EigenInfoComponent::buttonClicked (Button* buttonThatWasClicked)
{
    delete getTopLevelComponent();
}

EigenLogger::EigenLogger(const char *prefix, const pic::f_string_t &logger): prefix_(prefix), logger_(logger)
{
}

EigenLogger::EigenLogger(const EigenLogger &l): prefix_(l.prefix_), logger_(l.logger_)
{
}

EigenLogger &EigenLogger::operator=(const EigenLogger &l)
{
    prefix_=l.prefix_;
    logger_=l.logger_;
    return *this;
}

bool EigenLogger::operator==(const EigenLogger &l) const
{
    return (logger_==l.logger_) && (prefix_.compare(l.prefix_)==0);
}

pic::f_string_t EigenLogger::create(const char *prefix, const pic::f_string_t &logger)
{
    return pic::f_string_t::callable(EigenLogger(prefix,logger));
}

void EigenLogger::operator()(const char *msg) const
{
    juce::String buffer(prefix_);
    buffer += T(": "); buffer+=msg;
    logger_(buffer.toCString());
}

void EigenMainWindow::ignore_klass(const juce::String &klass)
{
    std::string cklass = klass.toUTF8();
    ignores_.insert(cklass);
}

void EigenD::handleWinch(const std::string &msg)
{
    JUCE_AUTORELEASEPOOL

    if(msg.length()==0)
    {
        main_window_->menuItemsChanged();
        return;
    }

    juce::String jmsg = juce::String::fromUTF8(msg.c_str());
    int c1 = jmsg.indexOf(0,T(":"));

    if(c1<0)
    {
        return;
    }

    int c2 = jmsg.indexOf(c1+1,T(":"));

    if(c2<0)
    {
        return;
    }

    juce::String alert_klass = jmsg.substring(0,c1);
    juce::String alert_title = jmsg.substring(c1+1,c2);
    juce::String alert_msg = jmsg.substring(c2+1);
    
    main_window_->alert1(alert_klass,alert_title,alert_msg);
}

START_JUCE_APPLICATION (EigenD)
