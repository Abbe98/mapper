/*
 *    Copyright 2012, 2013 Thomas Schöps
 *    Copyright 2012-2015 Kai Pastor
 *
 *    This file is part of OpenOrienteering.
 *
 *    OpenOrienteering is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    OpenOrienteering is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "home_screen_widget.h"

#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QCommandLinkButton>
#include <QDirIterator>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QProcessEnvironment>
#include <QScroller>
#include <QStackedLayout>

#include "../home_screen_controller.h"
#include "../main_window.h"
#include "../../file_format_registry.h"
#include "../../mapper_resource.h"


//### AbstractHomeScreenWidget ###

AbstractHomeScreenWidget::AbstractHomeScreenWidget(HomeScreenController* controller, QWidget* parent)
: QWidget(parent),
  controller(controller)
{
	Q_ASSERT(controller->getWindow() != NULL);
}

AbstractHomeScreenWidget::~AbstractHomeScreenWidget()
{
	// nothing
}

QLabel* AbstractHomeScreenWidget::makeHeadline(const QString& text, QWidget* parent) const
{
	QLabel* title_label = new QLabel(text, parent);
	QFont title_font = title_label->font();
	int pixel_size = title_font.pixelSize();
	if (pixel_size > 0)
	{
		title_font.setPixelSize(pixel_size * 2);
	}
	else
	{
		pixel_size = title_font.pointSize();
		title_font.setPointSize(pixel_size * 2);
	}
	title_font.setBold(true);
	title_label->setFont(title_font);
	title_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	return title_label;
}

QAbstractButton* AbstractHomeScreenWidget::makeButton(const QString& text, QWidget* parent) const
{
	QAbstractButton* button = new QCommandLinkButton(text, parent);
	QFont button_font = button->font();
	int pixel_size = button_font.pixelSize();
	if (pixel_size > 0)
	{
		button_font.setPixelSize(pixel_size * 3 / 2);
	}
	else
	{
		pixel_size = button_font.pointSize();
		button_font.setPointSize(pixel_size * 3 / 2);
	}
	button->setFont(button_font);
	return button;
}

QAbstractButton* AbstractHomeScreenWidget::makeButton(const QString& text, const QIcon& icon, QWidget* parent) const
{
	QAbstractButton* button = makeButton(text, parent);
	button->setIcon(icon);
	return button;
}



//### HomeScreenWidgetDesktop ###

HomeScreenWidgetDesktop::HomeScreenWidgetDesktop(HomeScreenController* controller, QWidget* parent)
: AbstractHomeScreenWidget(controller, parent)
{
	QLabel* title_label = new QLabel(QString("<img src=\":/images/title.png\"/>"));
	title_label->setAlignment(Qt::AlignCenter);
	QWidget* menu_widget = makeMenuWidget(controller, parent);
	QWidget* recent_files_widget = makeRecentFilesWidget(controller, parent);
	QWidget* tips_widget = makeTipsWidget(controller, parent);
	
	QGridLayout* layout = new QGridLayout();
	layout->setSpacing(2 * layout->spacing());
	layout->addWidget(title_label, 0, 0, 1, 2);
	layout->addWidget(menu_widget, 1, 0, 2, 1);
	layout->addWidget(recent_files_widget, 1, 1);
	layout->setRowStretch(1, 4);
	layout->addWidget(tips_widget, 2, 1);
	layout->setRowStretch(2, 3);
	setLayout(layout);
	
	setAutoFillBackground(false);
}

HomeScreenWidgetDesktop::~HomeScreenWidgetDesktop()
{
	// nothing
}

QWidget* HomeScreenWidgetDesktop::makeMenuWidget(HomeScreenController* controller, QWidget* parent)
{
	MainWindow* window = controller->getWindow();
	
	QVBoxLayout* menu_layout = new QVBoxLayout();
	
	QLabel* menu_headline = makeHeadline(tr("Activities"));
	menu_layout->addWidget(menu_headline);
	QAbstractButton* button_new_map = makeButton(
	  tr("Create a new map ..."), QIcon(":/images/new.png"));
	menu_layout->addWidget(button_new_map);
	QAbstractButton* button_open_map = makeButton(
	  tr("Open map ..."), QIcon(":/images/open.png"));
	menu_layout->addWidget(button_open_map);
	
	menu_layout->addStretch(1);
	
	QAbstractButton* button_settings = makeButton(
	  tr("Settings"), QIcon(":/images/settings.png"));
	menu_layout->addWidget(button_settings);
	QAbstractButton* button_about = makeButton(
	  tr("About %1", "As in 'About OpenOrienteering Mapper'").arg(window->appName()), QIcon(":/images/about.png"));
	menu_layout->addWidget(button_about);
	QAbstractButton* button_help = makeButton(
	  tr("Help"), QIcon(":/images/help.png"));
	menu_layout->addWidget(button_help);
	QAbstractButton* button_exit = makeButton(
	  tr("Exit"), QIcon(":/qt-project.org/styles/commonstyle/images/standardbutton-close-32.png")); // From Qt5
	menu_layout->addWidget(button_exit);
	
	connect(button_new_map, SIGNAL(clicked(bool)), window, SLOT(showNewMapWizard()));
	connect(button_open_map, SIGNAL(clicked(bool)), window, SLOT(showOpenDialog()));
	connect(button_settings, SIGNAL(clicked(bool)), window, SLOT(showSettings()));
	connect(button_about, SIGNAL(clicked(bool)), window, SLOT(showAbout()));
	connect(button_help, SIGNAL(clicked(bool)), window, SLOT(showHelp()));
	connect(button_exit, SIGNAL(clicked(bool)), qApp, SLOT(closeAllWindows()));
	
	QWidget* menu_widget = new QWidget(parent);
	menu_widget->setLayout(menu_layout);
	menu_widget->setAutoFillBackground(true);
	return menu_widget;
}

QWidget* HomeScreenWidgetDesktop::makeRecentFilesWidget(HomeScreenController* controller, QWidget* parent)
{
	QGridLayout* recent_files_layout = new QGridLayout();
	
	QLabel* recent_files_headline = makeHeadline(tr("Recent maps"));
	recent_files_layout->addWidget(recent_files_headline, 0, 0, 1, 2);
	
	recent_files_list = new QListWidget();
	QFont list_font = recent_files_list->font();
	int pixel_size = list_font.pixelSize();
	if (pixel_size > 0)
	{
		list_font.setPixelSize(pixel_size * 3 / 2);
	}
	else
	{
		pixel_size = list_font.pointSize();
		list_font.setPointSize(pixel_size * 3 / 2);
	}
	recent_files_list->setFont(list_font);
	recent_files_list->setSpacing(pixel_size/2);
	recent_files_list->setCursor(Qt::PointingHandCursor);
	recent_files_list->setStyleSheet(" \
	  QListWidget::item:hover { \
	    color: palette(highlighted-text); \
	    background: palette(highlight); \
	  } ");
	recent_files_layout->addWidget(recent_files_list, 1, 0, 1, 2);
	
	open_mru_file_check = new QCheckBox(tr("Open most recently used file on start"));
	recent_files_layout->addWidget(open_mru_file_check, 2, 0, 1, 1);
	
	QPushButton* clear_list_button = new QPushButton(tr("Clear list"));
	recent_files_layout->addWidget(clear_list_button, 2, 1, 1, 1);
	
	recent_files_layout->setRowStretch(1, 1);
	recent_files_layout->setColumnStretch(0, 1);
	
	connect(recent_files_list, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(recentFileClicked(QListWidgetItem*)));
	connect(open_mru_file_check, SIGNAL(clicked(bool)), controller, SLOT(setOpenMRUFile(bool)));
	connect(clear_list_button, SIGNAL(clicked(bool)), controller, SLOT(clearRecentFiles()));
	
	QWidget* recent_files_widget = new QWidget(parent);
	recent_files_widget->setLayout(recent_files_layout);
	recent_files_widget->setAutoFillBackground(true);
	return recent_files_widget;
}

QWidget* HomeScreenWidgetDesktop::makeTipsWidget(HomeScreenController* controller, QWidget* parent)
{
	QGridLayout* tips_layout = new QGridLayout();
	QWidget* tips_headline = makeHeadline(tr("Tip of the day"));
	tips_layout->addWidget(tips_headline, 0, 0, 1, 3);
	tips_label = new QLabel();
	tips_label->setWordWrap(true);
	tips_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	tips_check = new QCheckBox(tr("Show tip of the day"));
	tips_check->setChecked(true);
	tips_layout->addWidget(tips_check, 2, 0, 1, 1);
	tips_layout->addWidget(tips_label, 1, 0, 1, 3);
	QPushButton* prev_button = new QPushButton(QIcon(":/images/arrow-left.png"), tr("Previous"));
	tips_layout->addWidget(prev_button, 2, 1, 1, 1);
	QPushButton* next_button = new QPushButton(QIcon(":/images/arrow-right.png"), tr("Next"));
	tips_layout->addWidget(next_button, 2, 2, 1, 1);
	
	tips_layout->setRowStretch(1, 1);
	tips_layout->setColumnStretch(0, 1);
	
	tips_children.reserve(4);
	tips_children.push_back(tips_headline);
	tips_children.push_back(tips_label);
	tips_children.push_back(prev_button);
	tips_children.push_back(next_button);
	
	MainWindow* window = controller->getWindow();
	connect(tips_label, SIGNAL(linkActivated(QString)), window, SLOT(linkClicked(QString)));
	connect(tips_check, SIGNAL(clicked(bool)), controller, SLOT(setTipsVisible(bool)));
	connect(prev_button, SIGNAL(clicked(bool)), controller, SLOT(goToPreviousTip()));
	connect(next_button, SIGNAL(clicked(bool)), controller, SLOT(goToNextTip()));
	
	QWidget* tips_widget = new QWidget(parent);
	tips_widget->setLayout(tips_layout);
	tips_widget->setAutoFillBackground(true);
	return tips_widget;
}

void HomeScreenWidgetDesktop::setRecentFiles(const QStringList& files)
{
	recent_files_list->clear();
	for (auto&& file : files)
	{
		QListWidgetItem* new_item = new QListWidgetItem(QFileInfo(file).fileName());
		new_item->setData(Qt::UserRole, file);
		new_item->setToolTip(file);
		recent_files_list->addItem(new_item);
	}
}

void HomeScreenWidgetDesktop::recentFileClicked(QListWidgetItem* item)
{
	QString path = item->data(Qt::UserRole).toString();
	controller->getWindow()->openPath(path);
}

void HomeScreenWidgetDesktop::paintEvent(QPaintEvent*)
{
	// Background
	QPainter p(this);
	p.setPen(Qt::NoPen);
	p.setBrush(Qt::gray);
	p.drawRect(rect());
}

void HomeScreenWidgetDesktop::setOpenMRUFileChecked(bool state)
{
	open_mru_file_check->setChecked(state);
}

void HomeScreenWidgetDesktop::setTipOfTheDay(const QString& text)
{
	tips_label->setText(text);
}

void HomeScreenWidgetDesktop::setTipsVisible(bool state)
{
	QGridLayout* layout = qobject_cast<QGridLayout*>(this->layout());
	for (auto widget : tips_children)
	{
		widget->setVisible(state);
	}
	if (layout != NULL)
		layout->setRowStretch(2, state ? 3 : 0);
	
	tips_check->setChecked(state);
}



//### HomeScreenWidgetMobile ###

HomeScreenWidgetMobile::HomeScreenWidgetMobile(HomeScreenController* controller, QWidget* parent)
: AbstractHomeScreenWidget(controller, parent)
{
	title_pixmap = QPixmap::fromImage(QImage(":/images/title.png"));
	
	title_label = new QLabel();
	title_label->setPixmap(title_pixmap);
	title_label->setAlignment(Qt::AlignCenter);
	
	QWidget* file_list_widget = makeFileListWidget(controller, parent);
	
	examples_button = new QPushButton(tr("Examples"));
	connect(examples_button, &QPushButton::clicked, this, &HomeScreenWidgetMobile::showExamples);
	QPushButton* help_button = new QPushButton(HomeScreenWidgetDesktop::tr("Help"));
	connect(help_button, &QPushButton::clicked, controller->getWindow(), &MainWindow::showHelp);
	QPushButton* about_button = new QPushButton(tr("About Mapper"));
	connect(about_button, &QPushButton::clicked, controller->getWindow(), &MainWindow::showAbout);
	QHBoxLayout* buttons_layout = new QHBoxLayout();
	buttons_layout->setContentsMargins(0, 0, 0, 0);
	buttons_layout->addWidget(examples_button);
	buttons_layout->addStretch(1);
	buttons_layout->addWidget(help_button);
	buttons_layout->addWidget(about_button);
	
	QGridLayout* layout = new QGridLayout();
	layout->setSpacing(2 * layout->spacing());
	layout->addWidget(title_label, 0, 0);
 	layout->addWidget(file_list_widget, 1, 0);
	layout->addLayout(buttons_layout, 2, 0);
	setLayout(layout);
	
	setAutoFillBackground(false);
}

HomeScreenWidgetMobile::~HomeScreenWidgetMobile()
{
	// nothing
}

void HomeScreenWidgetMobile::resizeEvent(QResizeEvent* event)
{
	Q_UNUSED(event);
	adjustTitlePixmapSize();
}

void HomeScreenWidgetMobile::adjustTitlePixmapSize()
{
	QSize label_size = title_label->size();
	int scaled_width = qRound(title_pixmap.devicePixelRatio() * label_size.width());
	if (title_pixmap.width() > scaled_width)
	{
		if (title_label->pixmap()->width() != scaled_width)
		{
			label_size.setHeight(title_pixmap.height());
			title_label->setPixmap(title_pixmap.scaled(label_size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		}
	}
	else if (title_label->pixmap()->width() != title_pixmap.width())
	{
		title_label->setPixmap(title_pixmap);
	}
}

void HomeScreenWidgetMobile::setRecentFiles(const QStringList& files)
{
	Q_UNUSED(files);
	// nothing
}

void HomeScreenWidgetMobile::setOpenMRUFileChecked(bool state)
{
	Q_UNUSED(state);
	// nothing
}

void HomeScreenWidgetMobile::setTipOfTheDay(const QString& text)
{
	Q_UNUSED(text);
	// nothing
}

void HomeScreenWidgetMobile::setTipsVisible(bool state)
{
	Q_UNUSED(state);
	// nothing
}

// slot
void HomeScreenWidgetMobile::showExamples()
{
	int old_size = file_list->count();
	
	QStringList paths = MapperResource::getLocations(MapperResource::EXAMPLE);
	while (!paths.isEmpty())
	{
		addFilesToFileList(file_list, paths.takeFirst());
	}
	
	if (file_list->count() > old_size)
	{
		file_list_stack->setCurrentIndex(0);
		file_list->setCurrentRow(old_size, QItemSelectionModel::ClearAndSelect);
		examples_button->setEnabled(false);
	}
}

void HomeScreenWidgetMobile::fileClicked(QListWidgetItem* item)
{
	QString path = item->data(Qt::UserRole).toString();
	controller->getWindow()->openPath(path);
}

QWidget* HomeScreenWidgetMobile::makeFileListWidget(HomeScreenController* controller, QWidget* parent)
{
	Q_UNUSED(controller);
	Q_UNUSED(parent);
	
	QGridLayout* file_list_layout = new QGridLayout();
	
	QLabel* file_list_headline = makeHeadline(tr("File list"));
	file_list_layout->addWidget(file_list_headline, 0, 0, 1, 2);
	
	file_list_stack = new QStackedLayout();
	file_list_layout->addLayout(file_list_stack, 1, 0, 1, 2);
	file_list_layout->setRowStretch(1, 1);
	
	file_list = new QListWidget();
	QScroller::grabGesture(file_list->viewport(), QScroller::TouchGesture);
	QFont list_font = file_list->font();
	int pixel_size = list_font.pixelSize();
	if (pixel_size > 0)
	{
		list_font.setPixelSize(pixel_size * 3 / 2);
	}
	else
	{
		pixel_size = list_font.pointSize();
		list_font.setPointSize(pixel_size * 3 / 2);
	}
	file_list->setFont(list_font);
	file_list->setSpacing(pixel_size/2);
	file_list->setCursor(Qt::PointingHandCursor);
	file_list->setStyleSheet(" \
	  QListWidget::item:hover { \
	    color: palette(highlighted-text); \
	    background: palette(highlight); \
	  } ");
	file_list_stack->addWidget(file_list);
	
	// Look for map files at device-specific locations
#ifdef Q_OS_ANDROID
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	// Actual SD card mount points may be found in secondary storage
	QStringList folder_list = env.value("SECONDARY_STORAGE").split(':');
	// The primary volume that can be accessed externally (from PC)
	QString external_storage = env.value("EXTERNAL_STORAGE");
	if (!external_storage.isEmpty())
		folder_list.insert(0, external_storage);
	// Optional fallback locations
	folder_list << "/" << "/mnt" << "/mnt/sdcard" << "/sdcard" << "/storage";
	
	bool oomapper_found = false;
	for (auto&& path : folder_list)
	{
		if (oomapper_found && path == "/")
			// Don't need fallback (avoid duplicate entries)
			break;
		
		QDir dir(path);
		if (dir.exists("OOMapper"))
		{
			oomapper_found = true;
			addFilesToFileList(file_list, dir.filePath("OOMapper"));
		}
	}
#endif
	
	file_list->sortItems();
	if (file_list->count() == 0)
	{
		QLabel* message_label = new QLabel(tr("No map files found!<br/><br/>Copy map files to a top-level folder named 'OOMapper' on the device or a memory card."));
		message_label->setWordWrap(true);
		file_list_stack->addWidget(message_label);
		file_list_stack->setCurrentWidget(message_label);
	}
	
	connect(file_list, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(fileClicked(QListWidgetItem*)));
	
	QWidget* file_list_widget = new QWidget();
	file_list_widget->setLayout(file_list_layout);
	file_list_widget->setAutoFillBackground(true);
	return file_list_widget;
}

void HomeScreenWidgetMobile::addFilesToFileList(QListWidget* file_list, const QString& path)
{
	QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		it.next();
		if (FileFormats.findFormatForFilename(it.filePath()) == NULL)
			continue;
		
		QListWidgetItem* new_item = new QListWidgetItem(it.fileInfo().fileName());
		new_item->setData(Qt::UserRole, it.filePath());
		new_item->setToolTip(it.filePath());
		file_list->addItem(new_item);
	}
}
