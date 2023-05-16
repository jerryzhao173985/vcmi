/*
 * CMT.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

// CMT.cpp : Defines the entry point for the console application.
#include "StdInc.h"
#include "CMT.h"

#include "CGameInfo.h"
#include "mainmenu/CMainMenu.h"
#include "mainmenu/CPrologEpilogVideo.h"
#include "gui/CursorHandler.h"
#include "CPlayerInterface.h"
#include "CVideoHandler.h"
#include "CMusicHandler.h"
#include "gui/CGuiHandler.h"
#include "gui/WindowHandler.h"
#include "CServerHandler.h"
#include "gui/NotificationHandler.h"
#include "ClientCommandManager.h"
#include "windows/CMessage.h"
#include "render/IScreenHandler.h"

#include "../lib/filesystem/Filesystem.h"
#include "../lib/CConsoleHandler.h"
#include "../lib/CGeneralTextHandler.h"
#include "../lib/VCMIDirs.h"
#include "../lib/mapping/CCampaignHandler.h"

#include "../lib/logging/CBasicLogConfigurator.h"

#include <boost/program_options.hpp>
#include <vstd/StringUtils.h>
#include <SDL_events.h>
#include <SDL_hints.h>
#include <SDL_main.h>

#ifdef VCMI_WINDOWS
#include <SDL_syswm.h>
#endif

#ifdef VCMI_ANDROID
#include "../lib/CAndroidVMHelper.h"
#include <SDL_system.h>
#endif

#if __MINGW32__
#undef main
#endif

namespace po = boost::program_options;
namespace po_style = boost::program_options::command_line_style;
namespace bfs = boost::filesystem;

extern boost::thread_specific_ptr<bool> inGuiThread;

std::queue<SDL_Event> SDLEventsQueue;
boost::mutex eventsM;

static po::variables_map vm;

#ifndef VCMI_IOS
void processCommand(const std::string &message);
#endif
void playIntro();
static void mainLoop();

static CBasicLogConfigurator *logConfig;

void init()
{
	CStopWatch tmh;

	loadDLLClasses();
	const_cast<CGameInfo*>(CGI)->setFromLib();

	logGlobal->info("Initializing VCMI_Lib: %d ms", tmh.getDiff());

	// Debug code to load all maps on start
	//ClientCommandManager commandController;
	//commandController.processCommand("convert txt", false);
}

static void prog_version()
{
	printf("%s\n", GameConstants::VCMI_VERSION.c_str());
	std::cout << VCMIDirs::get().genHelpString();
}

static void prog_help(const po::options_description &opts)
{
	auto time = std::time(0);
	printf("%s - A Heroes of Might and Magic 3 clone\n", GameConstants::VCMI_VERSION.c_str());
	printf("Copyright (C) 2007-%d VCMI dev team - see AUTHORS file\n", std::localtime(&time)->tm_year + 1900);
	printf("This is free software; see the source for copying conditions. There is NO\n");
	printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	printf("\n");
	std::cout << opts;
}

#if defined(VCMI_WINDOWS) && !defined(__GNUC__) && defined(VCMI_WITH_DEBUG_CONSOLE)
int wmain(int argc, wchar_t* argv[])
#elif defined(VCMI_MOBILE)
int SDL_main(int argc, char *argv[])
#else
int main(int argc, char * argv[])
#endif
{
#ifdef VCMI_ANDROID
	CAndroidVMHelper::initClassloader(SDL_AndroidGetJNIEnv());
	// boost will crash without this
	setenv("LANG", "C", 1);
#endif

#if !defined(VCMI_MOBILE)
	// Correct working dir executable folder (not bundle folder) so we can use executable relative paths
	boost::filesystem::current_path(boost::filesystem::system_complete(argv[0]).parent_path());
#endif
	std::cout << "Starting... " << std::endl;
	po::options_description opts("Allowed options");
	opts.add_options()
		("help,h", "display help and exit")
		("version,v", "display version information and exit")
		("disable-shm", "force disable shared memory usage")
		("enable-shm-uuid", "use UUID for shared memory identifier")
		("testmap", po::value<std::string>(), "")
		("testsave", po::value<std::string>(), "")
		("spectate,s", "enable spectator interface for AI-only games")
		("spectate-ignore-hero", "wont follow heroes on adventure map")
		("spectate-hero-speed", po::value<int>(), "hero movement speed on adventure map")
		("spectate-battle-speed", po::value<int>(), "battle animation speed for spectator")
		("spectate-skip-battle", "skip battles in spectator view")
		("spectate-skip-battle-result", "skip battle result window")
		("onlyAI", "allow to run without human player, all players will be default AI")
		("headless", "runs without GUI, implies --onlyAI")
		("ai", po::value<std::vector<std::string>>(), "AI to be used for the player, can be specified several times for the consecutive players")
		("oneGoodAI", "puts one default AI and the rest will be EmptyAI")
		("autoSkip", "automatically skip turns in GUI")
		("disable-video", "disable video player")
		("nointro,i", "skips intro movies")
		("donotstartserver,d","do not attempt to start server and just connect to it instead server")
		("serverport", po::value<si64>(), "override port specified in config file")
		("saveprefix", po::value<std::string>(), "prefix for auto save files")
		("savefrequency", po::value<si64>(), "limit auto save creation to each N days")
		("lobby", "parameters address, port, uuid to connect ro remote lobby session")
		("lobby-address", po::value<std::string>(), "address to remote lobby")
		("lobby-port", po::value<ui16>(), "port to remote lobby")
		("lobby-host", "if this client hosts session")
		("lobby-uuid", po::value<std::string>(), "uuid to the server")
		("lobby-connections", po::value<ui16>(), "connections of server")
		("lobby-username", po::value<std::string>(), "player name")
		("lobby-gamemode", po::value<ui16>(), "use 0 for new game and 1 for load game")
		("uuid", po::value<std::string>(), "uuid for the client");

	if(argc > 1)
	{
		try
		{
			po::store(po::parse_command_line(argc, argv, opts, po_style::unix_style|po_style::case_insensitive), vm);
		}
		catch(std::exception &e)
		{
			std::cerr << "Failure during parsing command-line options:\n" << e.what() << std::endl;
		}
	}

	po::notify(vm);
	if(vm.count("help"))
	{
		prog_help(opts);
#ifdef VCMI_IOS
		exit(0);
#else
		return 0;
#endif
	}
	if(vm.count("version"))
	{
		prog_version();
#ifdef VCMI_IOS
		exit(0);
#else
		return 0;
#endif
	}

	// Init old logging system and new (temporary) logging system
	CStopWatch total, pomtime;
	std::cout.flags(std::ios::unitbuf);
#ifndef VCMI_IOS
	console = new CConsoleHandler();

	auto callbackFunction = [](std::string buffer, bool calledFromIngameConsole)
	{
		ClientCommandManager commandController;
		commandController.processCommand(buffer, calledFromIngameConsole);
	};

	*console->cb = callbackFunction;
	console->start();
#endif

	const bfs::path logPath = VCMIDirs::get().userLogsPath() / "VCMI_Client_log.txt";
	logConfig = new CBasicLogConfigurator(logPath, console);
	logConfig->configureDefault();
	logGlobal->info("Starting client of '%s'", GameConstants::VCMI_VERSION);
	logGlobal->info("Creating console and configuring logger: %d ms", pomtime.getDiff());
	logGlobal->info("The log file will be saved to %s", logPath);

	// Init filesystem and settings
	preinitDLL(::console);

	Settings session = settings.write["session"];
	auto setSettingBool = [](std::string key, std::string arg) {
		Settings s = settings.write(vstd::split(key, "/"));
		if(::vm.count(arg))
			s->Bool() = true;
		else if(s->isNull())
			s->Bool() = false;
	};
	auto setSettingInteger = [](std::string key, std::string arg, si64 defaultValue) {
		Settings s = settings.write(vstd::split(key, "/"));
		if(::vm.count(arg))
			s->Integer() = ::vm[arg].as<si64>();
		else if(s->isNull())
			s->Integer() = defaultValue;
	};
	auto setSettingString = [](std::string key, std::string arg, std::string defaultValue) {
		Settings s = settings.write(vstd::split(key, "/"));
		if(::vm.count(arg))
			s->String() = ::vm[arg].as<std::string>();
		else if(s->isNull())
			s->String() = defaultValue;
	};

	setSettingBool("session/onlyai", "onlyAI");
	if(vm.count("headless"))
	{
		session["headless"].Bool() = true;
		session["onlyai"].Bool() = true;
	}
	else if(vm.count("spectate"))
	{
		session["spectate"].Bool() = true;
		session["spectate-ignore-hero"].Bool() = vm.count("spectate-ignore-hero");
		session["spectate-skip-battle"].Bool() = vm.count("spectate-skip-battle");
		session["spectate-skip-battle-result"].Bool() = vm.count("spectate-skip-battle-result");
		if(vm.count("spectate-hero-speed"))
			session["spectate-hero-speed"].Integer() = vm["spectate-hero-speed"].as<int>();
		if(vm.count("spectate-battle-speed"))
			session["spectate-battle-speed"].Float() = vm["spectate-battle-speed"].as<int>();
	}
	// Server settings
	setSettingBool("session/donotstartserver", "donotstartserver");

	// Shared memory options
	setSettingBool("session/disable-shm", "disable-shm");
	setSettingBool("session/enable-shm-uuid", "enable-shm-uuid");

	// Init special testing settings
	setSettingInteger("session/serverport", "serverport", 0);
	setSettingString("session/saveprefix", "saveprefix", "");
	setSettingInteger("general/saveFrequency", "savefrequency", 1);

	// Initialize logging based on settings
	logConfig->configure();
	logGlobal->debug("settings = %s", settings.toJsonNode().toJson());

	// Some basic data validation to produce better error messages in cases of incorrect install
	auto testFile = [](std::string filename, std::string message) -> bool
	{
		if (CResourceHandler::get()->existsResource(ResourceID(filename)))
			return true;

		logGlobal->error("Error: %s was not found!", message);
		return false;
	};

	if (!testFile("DATA/HELP.TXT", "Heroes III data") ||
		!testFile("MODS/VCMI/MOD.JSON", "VCMI data"))
	{
		exit(1); // These are unrecoverable errors
	}

	// these two are optional + some installs have them on CD and not in data directory
	testFile("VIDEO/GOOD1A.SMK", "campaign movies");
	testFile("SOUNDS/G1A.WAV", "campaign music"); //technically not a music but voiced intro sounds

	srand ( (unsigned int)time(nullptr) );

	if(!settings["session"]["headless"].Bool())
		GH.init();

	CCS = new CClientState();
	CGI = new CGameInfo(); //contains all global informations about game (texts, lodHandlers, map handler etc.)
	CSH = new CServerHandler();
	
	// Initialize video
#ifdef DISABLE_VIDEO
	CCS->videoh = new CEmptyVideoPlayer();
#else
	if (!settings["session"]["headless"].Bool() && !vm.count("disable-video"))
		CCS->videoh = new CVideoPlayer();
	else
		CCS->videoh = new CEmptyVideoPlayer();
#endif

	logGlobal->info("\tInitializing video: %d ms", pomtime.getDiff());

	if(!settings["session"]["headless"].Bool())
	{
		//initializing audio
		CCS->soundh = new CSoundHandler();
		CCS->soundh->init();
		CCS->soundh->setVolume((ui32)settings["general"]["sound"].Float());
		CCS->musich = new CMusicHandler();
		CCS->musich->init();
		CCS->musich->setVolume((ui32)settings["general"]["music"].Float());
		logGlobal->info("Initializing screen and sound handling: %d ms", pomtime.getDiff());
	}

#ifdef VCMI_MAC
	// Ctrl+click should be treated as a right click on Mac OS X
	SDL_SetHint(SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK, "1");
#endif

#ifdef SDL_HINT_MOUSE_TOUCH_EVENTS
	if(GH.isPointerRelativeMode)
	{
		SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
		SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
	}
#endif

#ifndef VCMI_NO_THREADED_LOAD
	//we can properly play intro only in the main thread, so we have to move loading to the separate thread
	boost::thread loading(init);
#else
	init();
#endif

	if(!settings["session"]["headless"].Bool())
	{
		if(!vm.count("battle") && !vm.count("nointro") && settings["video"]["showIntro"].Bool())
			playIntro();
		GH.screenHandler().clearScreen();
	}


#ifndef VCMI_NO_THREADED_LOAD
	#ifdef VCMI_ANDROID // android loads the data quite slowly so we display native progressbar to prevent having only black screen for few seconds
	{
		CAndroidVMHelper vmHelper;
		vmHelper.callStaticVoidMethod(CAndroidVMHelper::NATIVE_METHODS_DEFAULT_CLASS, "showProgress");
	#endif // ANDROID
		loading.join();
	#ifdef VCMI_ANDROID
		vmHelper.callStaticVoidMethod(CAndroidVMHelper::NATIVE_METHODS_DEFAULT_CLASS, "hideProgress");
	}
	#endif // ANDROID
#endif // THREADED

	if(!settings["session"]["headless"].Bool())
	{
		pomtime.getDiff();
		graphics = new Graphics(); // should be before curh

		CCS->curh = new CursorHandler();
		logGlobal->info("Screen handler: %d ms", pomtime.getDiff());

		CMessage::init();
		logGlobal->info("Message handler: %d ms", pomtime.getDiff());

		CCS->curh->show();
	}

	logGlobal->info("Initialization of VCMI (together): %d ms", total.getDiff());

	session["autoSkip"].Bool()  = vm.count("autoSkip");
	session["oneGoodAI"].Bool() = vm.count("oneGoodAI");
	session["aiSolo"].Bool() = false;
	std::shared_ptr<CMainMenu> mmenu;
	
	if(vm.count("testmap"))
	{
		session["testmap"].String() = vm["testmap"].as<std::string>();
		session["onlyai"].Bool() = true;
		boost::thread(&CServerHandler::debugStartTest, CSH, session["testmap"].String(), false);
	}
	else if(vm.count("testsave"))
	{
		session["testsave"].String() = vm["testsave"].as<std::string>();
		session["onlyai"].Bool() = true;
		boost::thread(&CServerHandler::debugStartTest, CSH, session["testsave"].String(), true);
	}
	else
	{
		mmenu = CMainMenu::create();
		GH.curInt = mmenu.get();
	}
	
	std::vector<std::string> names;
	session["lobby"].Bool() = false;
	if(vm.count("lobby"))
	{
		session["lobby"].Bool() = true;
		session["host"].Bool() = false;
		session["address"].String() = vm["lobby-address"].as<std::string>();
		if(vm.count("lobby-username"))
			session["username"].String() = vm["lobby-username"].as<std::string>();
		else
			session["username"].String() = settings["launcher"]["lobbyUsername"].String();
		if(vm.count("lobby-gamemode"))
			session["gamemode"].Integer() = vm["lobby-gamemode"].as<ui16>();
		else
			session["gamemode"].Integer() = 0;
		CSH->uuid = vm["uuid"].as<std::string>();
		session["port"].Integer() = vm["lobby-port"].as<ui16>();
		logGlobal->info("Remote lobby mode at %s:%d, uuid is %s", session["address"].String(), session["port"].Integer(), CSH->uuid);
		if(vm.count("lobby-host"))
		{
			session["host"].Bool() = true;
			session["hostConnections"].String() = std::to_string(vm["lobby-connections"].as<ui16>());
			session["hostUuid"].String() = vm["lobby-uuid"].as<std::string>();
			logGlobal->info("This client will host session, server uuid is %s", session["hostUuid"].String());
		}
		
		//we should not reconnect to previous game in online mode
		Settings saveSession = settings.write["server"]["reconnect"];
		saveSession->Bool() = false;
		
		//start lobby immediately
		names.push_back(session["username"].String());
		ESelectionScreen sscreen = session["gamemode"].Integer() == 0 ? ESelectionScreen::newGame : ESelectionScreen::loadGame;
		mmenu->openLobby(sscreen, session["host"].Bool(), &names, ELoadMode::MULTI);
	}
	
	// Restore remote session - start game immediately
	if(settings["server"]["reconnect"].Bool())
	{
		CSH->restoreLastSession();
	}

	if(!settings["session"]["headless"].Bool())
	{
		mainLoop();
	}
	else
	{
		while(true)
			boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
	}

	return 0;
}

//plays intro, ends when intro is over or button has been pressed (handles events)
void playIntro()
{
	if(CCS->videoh->openAndPlayVideo("3DOLOGO.SMK", 0, 1, true, true))
	{
		CCS->videoh->openAndPlayVideo("AZVS.SMK", 0, 1, true, true);
	}
}

static void handleEvent(SDL_Event & ev)
{
	if((ev.type==SDL_QUIT) ||(ev.type == SDL_KEYDOWN && ev.key.keysym.sym==SDLK_F4 && (ev.key.keysym.mod & KMOD_ALT)))
	{
#ifdef VCMI_ANDROID
		handleQuit(false);
#else
		handleQuit();
#endif
		return;
	}
#ifdef VCMI_ANDROID
	else if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_AC_BACK)
	{
		handleQuit(true);
	}
#endif
	else if(ev.type == SDL_KEYDOWN && ev.key.keysym.sym==SDLK_F4)
	{
		Settings full = settings.write["video"]["fullscreen"];
		full->Bool() = !full->Bool();
		return;
	}
	else if(ev.type == SDL_USEREVENT)
	{
		switch(static_cast<EUserEvent>(ev.user.code))
		{
		case EUserEvent::FORCE_QUIT:
			{
				handleQuit(false);
				return;
			}
			break;
		case EUserEvent::RETURN_TO_MAIN_MENU:
			{
				CSH->endGameplay();
				GH.defActionsDef = 63;
				CMM->menu->switchToTab("main");
			}
			break;
		case EUserEvent::RESTART_GAME:
			{
				CSH->sendRestartGame();
			}
			break;
		case EUserEvent::CAMPAIGN_START_SCENARIO:
			{
				CSH->campaignServerRestartLock.set(true);
				CSH->endGameplay();
				auto ourCampaign = std::shared_ptr<CCampaignState>(reinterpret_cast<CCampaignState *>(ev.user.data1));
				auto & epilogue = ourCampaign->camp->scenarios[ourCampaign->mapsConquered.back()].epilog;
				auto finisher = [=]()
				{
					if(ourCampaign->mapsRemaining.size())
					{
						GH.windows().pushInt(CMM);
						GH.windows().pushInt(CMM->menu);
						CMM->openCampaignLobby(ourCampaign);
					}
				};
				if(epilogue.hasPrologEpilog)
				{
					GH.windows().pushIntT<CPrologEpilogVideo>(epilogue, finisher);
				}
				else
				{
					CSH->campaignServerRestartLock.waitUntil(false);
					finisher();
				}
			}
			break;
		case EUserEvent::RETURN_TO_MENU_LOAD:
			CSH->endGameplay();
			GH.defActionsDef = 63;
			CMM->menu->switchToTab("load");
			break;
		case EUserEvent::FULLSCREEN_TOGGLED:
			{
				boost::unique_lock<boost::recursive_mutex> lock(*CPlayerInterface::pim);
				GH.onScreenResize();
				break;
			}
		default:
			logGlobal->error("Unknown user event. Code %d", ev.user.code);
			break;
		}

		return;
	}
	else if(ev.type == SDL_WINDOWEVENT)
	{
		switch (ev.window.event) {
		case SDL_WINDOWEVENT_RESTORED:
#ifndef VCMI_IOS
			{
				boost::unique_lock<boost::recursive_mutex> lock(*CPlayerInterface::pim);
				GH.onScreenResize();
			}
#endif
			break;
		}
		return;
	}
	else if(ev.type == SDL_SYSWMEVENT)
	{
		if(!settings["session"]["headless"].Bool() && settings["general"]["notifications"].Bool())
		{
			NotificationHandler::handleSdlEvent(ev);
		}
	}

	//preprocessing
	if(ev.type == SDL_MOUSEMOTION)
	{
		CCS->curh->cursorMove(ev.motion.x, ev.motion.y);
	}

	{
		boost::unique_lock<boost::mutex> lock(eventsM);
		SDLEventsQueue.push(ev);
	}
}

static void mainLoop()
{
	SettingsListener resChanged = settings.listen["video"]["resolution"];
	SettingsListener fsChanged = settings.listen["video"]["fullscreen"];
	resChanged([](const JsonNode &newState){  CGuiHandler::pushUserEvent(EUserEvent::FULLSCREEN_TOGGLED); });
	fsChanged([](const JsonNode &newState){  CGuiHandler::pushUserEvent(EUserEvent::FULLSCREEN_TOGGLED); });

	inGuiThread.reset(new bool(true));

	while(1) //main SDL events loop
	{
		SDL_Event ev;

		while(1 == SDL_PollEvent(&ev))
		{
			handleEvent(ev);
		}

		CSH->applyPacksOnLobbyScreen();
		GH.renderFrame();
	}
}

static void quitApplication()
{
	if(!settings["session"]["headless"].Bool())
	{
		if(CSH->client)
			CSH->endGameplay();
	}

	GH.windows().listInt.clear();
	GH.windows().objsToBlit.clear();

	CMM.reset();

	if(!settings["session"]["headless"].Bool())
	{
		// cleanup, mostly to remove false leaks from analyzer
		if(CCS)
		{
			CCS->musich->release();
			CCS->soundh->release();

			vstd::clear_pointer(CCS);
		}
		CMessage::dispose();

		vstd::clear_pointer(graphics);
	}

	vstd::clear_pointer(VLC);

	vstd::clear_pointer(console);// should be removed after everything else since used by logging

	boost::this_thread::sleep(boost::posix_time::milliseconds(750));//???

	if(!settings["session"]["headless"].Bool())
		GH.screenHandler().close();

	if(logConfig != nullptr)
	{
		logConfig->deconfigure();
		delete logConfig;
		logConfig = nullptr;
	}

	std::cout << "Ending...\n";
	exit(0);
}

void handleQuit(bool ask)
{
	if(CSH->client && LOCPLINT && ask)
	{
		CCS->curh->set(Cursor::Map::POINTER);
		LOCPLINT->showYesNoDialog(CGI->generaltexth->allTexts[69], [](){
			// Workaround for assertion failure on exit:
			// handleQuit() is alway called during SDL event processing
			// during which, eventsM is kept locked
			// this leads to assertion failure if boost::mutex is in locked state
			eventsM.unlock();

			quitApplication();
		}, nullptr);
	}
	else
	{
		quitApplication();
	}
}
