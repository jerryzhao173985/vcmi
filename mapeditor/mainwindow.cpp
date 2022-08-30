#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication.h>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QFileInfo>

#include "../lib/VCMIDirs.h"
#include "../lib/VCMI_Lib.h"
#include "../lib/logging/CBasicLogConfigurator.h"
#include "../lib/CConfigHandler.h"
#include "../lib/filesystem/Filesystem.h"
#include "../lib/GameConstants.h"
#include "../lib/mapping/CMapService.h"
#include "../lib/mapping/CMap.h"


#include "CGameInfo.h"
#include "maphandler.h"
#include "graphics.h"
#include "windownewmap.h"

static CBasicLogConfigurator * logConfig;

void init()
{
	loadDLLClasses();
	const_cast<CGameInfo*>(CGI)->setFromLib();
	logGlobal->info("Initializing VCMI_Lib");
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

	//configure logging
	const boost::filesystem::path logPath = VCMIDirs::get().userCachePath() / "VCMI_Editor_log.txt";
	console = new CConsoleHandler();
	logConfig = new CBasicLogConfigurator(logPath, console);
	logConfig->configureDefault();
	logGlobal->info("The log file will be saved to %s", logPath);
	
	//init
	preinitDLL(::console);
	settings.init();
	
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
	
	if(!testFile("DATA/HELP.TXT", "Heroes III data") ||
	   !testFile("MODS/VCMI/MOD.JSON", "VCMI data"))
	{
		QApplication::quit();
	}
	
	conf.init();
	logGlobal->info("Loading settings");
	
	CGI = new CGameInfo(); //contains all global informations about game (texts, lodHandlers, map handler etc.)
	init();
	
	graphics = new Graphics(); // should be before curh->init()
	graphics->load();//must be after Content loading but should be in main thread
	
	
	if(!testFile("DATA/new-menu/Background.png", "Cannot find file"))
	{
		QApplication::quit();
	}
	
	//now let's try to draw
	auto resPath = *CResourceHandler::get()->getResourceName(ResourceID("DATA/new-menu/Background.png"));
	
	scene = new QGraphicsScene(this);
	ui->graphicsView->setScene(scene);
	
	sceneMini = new QGraphicsScene(this);
	ui->minimapView->setScene(sceneMini);

	scene->addPixmap(QPixmap(QString::fromStdString(resPath.native())));

	show();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::reloadMap()
{
	MapHandler mapHandler(map.get());

	for(int j = 0; j < map->height; ++j)
	{
		for(int i = 0; i < map->width; ++i)
		{
			mapHandler.drawTerrainTile(i, j, 0);
			mapHandler.drawObjects(i, j, 0);
		}
	}

	auto mapSizePx = mapHandler.surface.rect();
	float ratio = std::fmin(mapSizePx.width() / 256., mapSizePx.height() / 256.);
	minimap = mapHandler.surface;
	minimap.setDevicePixelRatio(ratio);

	scene->clear();
	scene->addPixmap(mapHandler.surface);
	//ui->graphicsView->setSceneRect(mapHandler.surface.rect());

	sceneMini->clear();
	sceneMini->addPixmap(minimap);
}

void MainWindow::setMap(std::unique_ptr<CMap> cmap)
{
	map = std::move(cmap);
	unsaved = true;
	filename.clear();
	setWindowTitle(filename + " - VCMI Map Editor");

	reloadMap();
}

void MainWindow::on_actionOpen_triggered()
{
	auto filenameSelect = QFileDialog::getOpenFileName(this, tr("Open Image"), QString::fromStdString(VCMIDirs::get().userCachePath().native()), tr("Homm3 Files (*.vmap *.h3m)"));
	
	if(filenameSelect.isNull())
		return;
	
	QFileInfo fi(filenameSelect);
	std::string fname = fi.fileName().toStdString();
	std::string fdir = fi.dir().path().toStdString();
	ResourceID resId("MAPS/" + fname, EResType::MAP);
	//ResourceID resId("MAPS/SomeMap.vmap", EResType::MAP);
	
	if(!CResourceHandler::get()->existsResource(resId))
	{
		QMessageBox::information(this, "Failed to open map", "Only map folder is supported");
		return;
	}
	
	CMapService mapService;
	try
	{
		map = mapService.loadMap(resId);
	}
	catch(const std::exception & e)
	{
		QMessageBox::critical(this, "Failed to open map", e.what());
	}

	unsaved = false;
	filename = filenameSelect;
	setWindowTitle(filename + " - VCMI Map Editor");

	reloadMap();
}


void MainWindow::on_actionSave_as_triggered()
{
	if(!map)
		return;

	auto filenameSelect = QFileDialog::getSaveFileName(this, tr("Save map"), "", tr("VCMI maps (*.vmap)"));

	if(filenameSelect.isNull())
		return;

	if(!unsaved && filenameSelect == filename)
		return;

	CMapService mapService;
	try
	{
		mapService.saveMap(map, filenameSelect.toStdString());
	}
	catch(const std::exception & e)
	{
		QMessageBox::critical(this, "Failed to save map", e.what());
	}

	filename = filenameSelect;
	unsaved = false;
	setWindowTitle(filename + " - VCMI Map Editor");
}


void MainWindow::on_actionNew_triggered()
{
	auto newMapDialog = new WindowNewMap(this);
}

