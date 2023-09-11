/*
 * rumorsettings.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "rumorsettings.h"
#include "ui_rumorsettings.h"

RumorSettings::RumorSettings(QWidget *parent) :
	AbstractSettings(parent),
	ui(new Ui::RumorSettings)
{
	ui->setupUi(this);
}

RumorSettings::~RumorSettings()
{
	delete ui;
}

void RumorSettings::initialize(const CMap & map)
{
	for(auto & rumor : map.rumors)
	{
		auto * item = new QListWidgetItem(QString::fromStdString(rumor.name));
		item->setData(Qt::UserRole, QVariant(QString::fromStdString(rumor.text)));
		item->setFlags(item->flags() | Qt::ItemIsEditable);
		ui->rumors->addItem(item);
	}
}

void RumorSettings::update(CMap & map)
{
	map.rumors.clear();
	for(int i = 0; i < ui->rumors->count(); ++i)
	{
		Rumor rumor;
		rumor.name = ui->rumors->item(i)->text().toStdString();
		rumor.text = ui->rumors->item(i)->data(Qt::UserRole).toString().toStdString();
		map.rumors.push_back(rumor);
	}
}

void RumorSettings::on_message_textChanged()
{
	if(auto item = ui->rumors->currentItem())
		item->setData(Qt::UserRole, QVariant(ui->message->toPlainText()));
}

void RumorSettings::on_add_clicked()
{
	auto * item = new QListWidgetItem(tr("New rumor"));
	item->setData(Qt::UserRole, QVariant(""));
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	ui->rumors->addItem(item);
	emit ui->rumors->itemActivated(item);
}

void RumorSettings::on_remove_clicked()
{
	if(auto item = ui->rumors->currentItem())
	{
		ui->message->setPlainText("");
		ui->rumors->takeItem(ui->rumors->row(item));
	}
}


void RumorSettings::on_rumors_itemSelectionChanged()
{
	if(auto item = ui->rumors->currentItem())
		ui->message->setPlainText(item->data(Qt::UserRole).toString());
}


