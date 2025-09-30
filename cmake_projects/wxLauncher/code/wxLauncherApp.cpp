/*
Copyright (C) 2009-2010 wxLauncher Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "generated/configure_launcher.h"
#include <wx/wx.h>
#include <wx/cmdline.h>
#include <wx/image.h>
#include <wx/filesys.h>
#include <wx/fs_arc.h>
#include <wx/fs_inet.h>
#include <wx/splash.h>

#if HAS_SDL == 1
#include "SDL.h"
#endif

// prevents SDL_main.h (included by SDL.h) from redefining main
#ifdef main
#undef main
#endif

#include "wxLauncherApp.h"
#include "MainWindow.h"
#include "apis/SkinManager.h"
#include "controls/Logger.h"
#include "global/version.h"
#include "apis/TCManager.h"
#include "apis/ProfileManager.h"
#include "apis/HelpManager.h"
#include "apis/FlagListManager.h"
#include "apis/ProfileProxy.h"

#include "global/MemoryDebugging.h" // Last include for memory debugging

#if IS_LINUX
// main needs to be handled by us on non-Mac *nix as SDL interferes with wxWidgets
IMPLEMENT_APP_NO_MAIN(wxLauncher);
#else
// Windows is fine and also needs special WinMain treatment
// OS X can't have SDL2 and wxWidgets 3 both initializing at startup
IMPLEMENT_APP(wxLauncher);
#endif

void wxLauncher::OnInitCmdLine(wxCmdLineParser& parser)
{
	static const char addprofiledesc[] =
		"Add profile PROFILE from FILE. If PROFILE "
		"already exists it will not be overwritten. *Operator*";
	static const char selectprofiledesc[] =
		"Make PROFILE the that wxLauncher will use "
		"on next run. *Operator*";
	static const char profiledesc[] =
		"The name of a profile to operate on. Operand PROFILE.";
	static const char filedesc[] =
		"The path to a file to operate on. Operand FILE.";
	static const char sessiononlydesc[] =
		"Do not remember the profile that is selected at exit";

	/* Operators */
	parser.AddSwitch(wxEmptyString, wxT_2("add-profile"),
		wxGetTranslation(wxString::FromUTF8(addprofiledesc)));
	parser.AddSwitch(wxEmptyString, wxT_2("select-profile"),
		wxGetTranslation(wxString::FromUTF8(selectprofiledesc)));

	/* Operands */
	parser.AddOption(wxEmptyString, wxT_2("profile"),
		wxGetTranslation(wxString::FromUTF8(profiledesc)),
		wxCMD_LINE_VAL_STRING);
	parser.AddOption(wxEmptyString, wxT_2("file"),
		wxGetTranslation(wxString::FromUTF8(filedesc)),
		wxCMD_LINE_VAL_STRING);

	/* Other */
	parser.AddSwitch(wxEmptyString, wxT_2("session-only"),
		wxGetTranslation(wxString::FromUTF8(sessiononlydesc)));

	parser.SetSwitchChars(wxT_2("-")); // always use -, even on windows

	wxApp::OnInitCmdLine(parser);
}

bool wxLauncher::OnCmdLineParsed(wxCmdLineParser& parser)
{
	if (!wxApp::OnCmdLineParsed(parser))
		return false;

	if (parser.Found(wxT_2("session-only")))
	{
		mKeepForSessionOnly = true;
	}
	
	if (parser.Found(wxT_2("add-profile")))
	{
		mProfileOperator = ProManOperator::add;
		if (!parser.Found(wxT_2("profile"), &mProfileOperand))
		{
			wxLogError(_("No profile specified to add"));
			return false;
		}
		if (!parser.Found(wxT_2("file"), &mFileOperand))
		{
			wxLogError(_("No file specified to add as profile"));
			return false;
		}
	}
	else if(parser.Found(wxT_2("select-profile")))
	{
		mProfileOperator = ProManOperator::select;
		if (!parser.Found(wxT_2("profile"), &mProfileOperand))
		{
			wxLogError(_("No profile specified to select"));
			return false;
		}
	}

	return true;
}

wxLauncher::wxLauncher()
	:mProfileOperator(ProManOperator::none),
	mKeepForSessionOnly(false),
	mShowGUI(false)
	// The strings init themselves sanely
{
}

static const char* DISPLAY_SPLASH_FAIL_TEXT =
	"Unable to load splash image."
	" This normally means that you are running the Launcher from a folder"
	" that the launcher does not know how to find the resource folder from."
	"\n\nThe launcher is expecting (%s) to contain the resource images.";

/** Display the splash screen.

\param splashWindow Out. Point to splash window if created. NULL otherwise.
\param returns false if an error occurented while loading spalsh; a
false return value indicates that this function has already informed the user
of the failue.  Returns true otherwise.
*/
bool displaySplash(wxSplashScreen **splashWindow)
{
	wxBitmap splash;
	// splash image location is fixed so that it's known at compile time
	wxFileName splashFile(wxT_2(RESOURCES_PATH), wxT_2("wxL_Splash.png"));
	if (splash.LoadFile(splashFile.GetFullPath(), wxBITMAP_TYPE_ANY)) {
#if NDEBUG
		(*splashWindow) = new wxSplashScreen(splash, wxSPLASH_CENTRE_ON_SCREEN, 0, NULL, wxID_ANY);
#else
		(*splashWindow) = NULL;//new wxSplashScreen(splash, wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_TIMEOUT, 1000, NULL, wxID_ANY);
#endif
		wxYield();
	} else {
		wxFileName expectedDir;
		if (wxFileName(wxT_2(RESOURCES_PATH)).IsAbsolute()) {
			expectedDir = wxFileName(wxT_2(RESOURCES_PATH));
		} else {
			expectedDir = wxFileName(::wxGetCwd(), wxT_2(RESOURCES_PATH));	
		}
		wxString errmsg(DISPLAY_SPLASH_FAIL_TEXT, wxMBConvUTF8());
		wxLogFatalError(wxString::Format(wxGetTranslation(errmsg),
			expectedDir.GetFullPath().c_str()).c_str());
		return false;
	}
	return true;
}

int wxLauncher::OnRun() {
	if (mProfileOperator == ProManOperator::none)
	{
		return wxApp::OnRun();
	}
	else
	{
		return ProManOperator::RunProfileOperator(mProfileOperator);
	}
}

bool wxLauncher::OnInit() {
	wxInitAllImageHandlers();

	// call base class OnInit so that cmdline stuff works.
	if (!wxApp::OnInit())
		return false; // base said abort so abort

	wxLog::SetActiveTarget(new Logger());
	wxLogInfo(wxT_2("wxLauncher Version %d.%d.%d"), MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
	wxLogInfo(wxT_2("Build \"%s\" committed on (%s)"), GITVersion, GITDate);
	wxLogInfo(wxDateTime(time(NULL)).Format(wxT_2("%c")));

#if MSCRTMEMORY
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
	// Little hack to deal with people starting the launcher from the bin folder
	if ( !wxFileName::DirExists(wxT_2(RESOURCES_PATH)) ) {
		wxFileName resourceDir;
		resourceDir.AssignDir(::wxGetCwd());
		resourceDir.AppendDir(wxT_2(".."));
		resourceDir.AppendDir(wxT_2(RESOURCES_PATH));
		if ( resourceDir.DirExists() ) {
			wxFileName newWorkingDir(::wxGetCwd(), wxT_2(".."));
			::wxSetWorkingDirectory(newWorkingDir.GetFullPath());
		}
	}

	wxSplashScreen* splashWindow = NULL;
	if (!displaySplash(&splashWindow))
		return false;

	wxLogInfo(wxT_2("Initializing profiles..."));
	ProMan::Flags promanFlags = ProMan::None;
	if (mKeepForSessionOnly)
		promanFlags = promanFlags | ProMan::NoUpdateLastProfile;
	if ( !ProMan::Initialize(promanFlags) ) {
		wxLogFatalError(wxT_2("ProfileManager failed to initialize. Aborting! See log file for more details."));
		return false;
	}
	if (mProfileOperator != ProManOperator::none)
	{
		// We are not to create a GUI so we are done init now
		return true;
	}

	wxFileSystem::AddHandler(new wxArchiveFSHandler);
	wxFileSystem::AddHandler(new wxInternetFSHandler);

	wxLogInfo(wxT_2("Initializing SkinSystem..."));
	SkinSystem::Initialize();

	wxLogInfo(wxT_2("Initializing HelpManager..."));
	HelpManager::Initialize();
	
	wxLogInfo(wxT_2("Initializing FlagListManager..."));
	FlagListManager::Initialize();
	
	wxLogInfo(wxT_2("Initializing ProfileProxy..."));
	ProfileProxy::Initialize();

	wxLogInfo(wxT_2("wxLauncher starting up."));


	MainWindow* window = new MainWindow();
	wxLogStatus(_("MainWindow is complete"));
	window->Show(true);
#if NDEBUG // will autodelete when timout runs out in debug
	splashWindow->Show(false);
	splashWindow->Destroy();
#endif

	// must call TCManager::CurrentProfileChanged() manually on startup,
	// since initial profile switch takes place before TCManager has been initialized
	// calling it here to ensure that AdvSettingsPage is set up by the time events are triggered
	wxCommandEvent tcMgrInitEvent;
	TCManager::Get()->CurrentProfileChanged(tcMgrInitEvent);

	wxLogStatus(_("Ready."));
	return true;
}

int wxLauncher::OnExit() {

	ProMan::DeInitialize();

	if (mProfileOperator == ProManOperator::none)
	{

		// deinitialize subsystems in the opposite order of initialization
		ProfileProxy::DeInitialize();
		FlagListManager::DeInitialize();
		HelpManager::DeInitialize();
		SkinSystem::DeInitialize();

#if HAS_SDL
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif

	}

	wxLogInfo(_("wxLogger shutdown complete."));

	return wxApp::OnExit();
}

#if IS_LINUX
int main(int argc, char** argv)
{
#if HAS_SDL == 1
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		wxLogFatalError(wxT_2("SDL_Init failed"));
		return 1;
	}
#endif

	return wxEntry(argc, argv);
}
#endif
