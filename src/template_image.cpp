/*
 *    Copyright 2012 Thomas Schöps
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


#include "template_image.h"

#if QT_VERSION < 0x050000
#include <QtGui>
#else
#include <QtWidgets>
#endif
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "map.h"
#include "util.h"
#include "georeferencing.h"
#include "georeferencing_dialog.h"

TemplateImage::TemplateImage(const QString& path, Map* map) : Template(path, map)
{
	image = NULL;
	georef.reset(new Georeferencing());
	
	const Georeferencing& georef = map->getGeoreferencing();
	connect(&georef, SIGNAL(projectionChanged()), this, SLOT(updateGeoreferencing()));
	connect(&georef, SIGNAL(transformationChanged()), this, SLOT(updateGeoreferencing()));
}

TemplateImage::~TemplateImage()
{
	if (template_state == Loaded)
		unloadTemplateFile();
}

bool TemplateImage::saveTemplateFile()
{
	return image->save(template_path);
}

void TemplateImage::saveTypeSpecificTemplateConfiguration(QIODevice* stream)
{
	if (is_georeferenced)
		saveString(stream, temp_crs_spec);
}

bool TemplateImage::loadTypeSpecificTemplateConfiguration(QIODevice* stream, int version)
{
	if (is_georeferenced)
	{
		loadString(stream, temp_crs_spec);
	}
	
	return true;
}

void TemplateImage::saveTypeSpecificTemplateConfiguration(QXmlStreamWriter& xml)
{
	if (is_georeferenced)
	{
		// Follow map georeferencing XML structure
		xml.writeStartElement("crs_spec");
// TODO: xml.writeAttribute("language", "PROJ.4");
		xml.writeCharacters(temp_crs_spec);
		xml.writeEndElement(/*crs_spec*/);
	}
}

bool TemplateImage::loadTypeSpecificTemplateConfiguration(QXmlStreamReader& xml)
{
	if (is_georeferenced && xml.name() == "crs_spec")
	{
		// TODO: check specification language
		temp_crs_spec = xml.readElementText();
	}
	else
		xml.skipCurrentElement(); // unsupported
	
	return true;
}

bool TemplateImage::loadTemplateFileImpl(bool configuring)
{
	image = new QImage(template_path);
	if (image->isNull())
	{
		delete image;
		image = NULL;
		return false;
	}
	
	// Check if georeferencing information is available
	available_georef = Georeferencing_None;
	
	// TODO: GeoTiff
	
	WorldFile world_file;
	if (available_georef == Georeferencing_None && world_file.tryToLoadForImage(template_path))
		available_georef = Georeferencing_WorldFile;
	
	if (!configuring && is_georeferenced)
	{
		if (available_georef == Georeferencing_None)
		{
			// Image was georeferenced, but georeferencing info is gone -> deny to load template
			return false;
		}
		else
			calculateGeoreferencing();
	}
	
	return true;
}
bool TemplateImage::postLoadConfiguration(QWidget* dialog_parent)
{
	if (getTemplateFilename().endsWith(".gif", Qt::CaseInsensitive))
		QMessageBox::warning(dialog_parent, tr("Warning"), tr("Loading a GIF image template.\nSaving GIF files is not supported. This means that drawings on this template won't be saved!\nIf you do not intend to draw on this template however, that is no problem."));
	
	TemplateImageOpenDialog open_dialog(this, dialog_parent);
	open_dialog.setWindowModality(Qt::WindowModal);
	while (true)
	{
		if (open_dialog.exec() == QDialog::Rejected)
			return false;
		
		if (open_dialog.isGeorefRadioChecked() && map->getGeoreferencing().isLocal())
		{
			// Make sure that map is georeferenced
			GeoreferencingDialog dialog(dialog_parent, map);
			if (dialog.exec() == QDialog::Rejected)
				continue;
		}
		
		if (open_dialog.isGeorefRadioChecked() && available_georef == Georeferencing_WorldFile)
		{
			// Let user select the coordinate reference system, as this is not specified in world files
			SelectCRSDialog dialog(map, dialog_parent, true, true, tr("Select the coordinate reference system of the coordinates in the world file"));
			if (dialog.exec() == QDialog::Rejected)
				continue;
			temp_crs_spec = dialog.getCRSSpec();
		}
		
		break;
	}
	
	is_georeferenced = open_dialog.isGeorefRadioChecked();	
	if (is_georeferenced)
		calculateGeoreferencing();
	else
	{
		transform.template_scale_x = open_dialog.getMpp() * 1000.0 / map->getScaleDenominator();
		transform.template_scale_y = transform.template_scale_x;
		
		//transform.template_x = main_view->getPositionX();
		//transform.template_y = main_view->getPositionY();
		
		updateTransformationMatrices();
	}
	
	return true;
}

void TemplateImage::unloadTemplateFileImpl()
{
	delete image;
	image = NULL;
}

void TemplateImage::drawTemplate(QPainter* painter, QRectF& clip_rect, double scale, float opacity)
{
	applyTemplateTransform(painter);
	
	painter->setRenderHint(QPainter::SmoothPixmapTransform);
	painter->setOpacity(opacity);
	painter->drawImage(QPointF(-image->width() * 0.5, -image->height() * 0.5), *image);
	painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
}
QRectF TemplateImage::getTemplateExtent()
{
    // If the image is invalid, the extent is an empty rectangle.
    if (!image) return QRectF();
	return QRectF(-image->width() * 0.5, -image->height() * 0.5, image->width(), image->height());
}

QPointF TemplateImage::calcCenterOfGravity(QRgb background_color)
{
	int num_points = 0;
	QPointF center = QPointF(0, 0);
	int width = image->width();
	int height = image->height();
	
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			QRgb pixel = image->pixel(x, y);
			if (qAlpha(pixel) < 127 || pixel == background_color)
				continue;
			
			center += QPointF(x, y);
			++num_points;
		}
	}
	
	if (num_points > 0)
		center = QPointF(center.x() / num_points, center.y() / num_points);
	center -= QPointF(image->width() * 0.5 - 0.5, image->height() * 0.5 - 0.5);
	
	return center;
}

void TemplateImage::updateGeoreferencing()
{
	if (is_georeferenced && template_state == Template::Loaded)
		updatePosFromGeoreferencing();
}

Template* TemplateImage::duplicateImpl()
{
	TemplateImage* new_template = new TemplateImage(template_path, map);
	new_template->image = new QImage(*image);
	new_template->available_georef = available_georef;
	return new_template;
}

void TemplateImage::drawOntoTemplateImpl(MapCoordF* coords, int num_coords, QColor color, float width)
{
	QPointF* points = new QPointF[num_coords];
	
	for (int i = 0; i < num_coords; ++i)
		points[i] = mapToTemplateQPoint(coords[i]) + QPointF(image->width() * 0.5, image->height() * 0.5);
	
	// This conversion is to prevent a very strange bug where the behavior of the
	// default QPainter composition mode seems to be incorrect for images which are
	// loaded from a file without alpha and then painted over with the eraser
	if (color.alpha() == 0 && image->format() != QImage::Format_ARGB32_Premultiplied)
		*image = image->convertToFormat(QImage::Format_ARGB32_Premultiplied);
	
    QPainter painter;
	painter.begin(image);
	if (color.alpha() == 0)
		painter.setCompositionMode(QPainter::CompositionMode_Clear);
	else
		painter.setOpacity(color.alphaF());
	
	QPen pen(color);
	pen.setWidthF(width);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	painter.setPen(pen);
	painter.setRenderHint(QPainter::Antialiasing);
	
	painter.drawPolyline(points, num_coords);
	
	painter.end();
}

void TemplateImage::calculateGeoreferencing()
{
	// Calculate georeferencing of image coordinates where the coordinate (0, 0)
	// is mapped to the world position of the middle of the top-left pixel
	georef.reset(new Georeferencing());
	if (!temp_crs_spec.isEmpty())
		georef->setProjectedCRS("", temp_crs_spec);
	
	if (available_georef == Georeferencing_WorldFile)
	{
		WorldFile world_file;
		if (!world_file.tryToLoadForImage(template_path))
		{
			// TODO: world file lost, disable georeferencing or unload template
			return;
		}
		if (!temp_crs_spec.isEmpty())
			georef->setProjectedCRS("", temp_crs_spec);
		georef->setTransformationDirectly(world_file.pixel_to_world);
	}
	else if (available_georef == Georeferencing_GeoTiff)
	{
		// TODO: GeoTiff
	}
	
	updatePosFromGeoreferencing();
}

void TemplateImage::updatePosFromGeoreferencing()
{
	// Determine map coords of three image corner points
	// by transforming the points from one Georeferencing into the other
	bool ok;
	MapCoordF top_left = map->getGeoreferencing().toMapCoordF(georef.data(), MapCoordF(-0.5, -0.5), &ok);
	if (!ok)
	{
		qDebug() << "updatePosFromGeoreferencing() failed";
		return; // TODO: proper error message?
	}
	MapCoordF top_right = map->getGeoreferencing().toMapCoordF(georef.data(), MapCoordF(image->width() - 0.5, -0.5), &ok);
	if (!ok)
	{
		qDebug() << "updatePosFromGeoreferencing() failed";
		return; // TODO: proper error message?
	}
	MapCoordF bottom_left = map->getGeoreferencing().toMapCoordF(georef.data(), MapCoordF(-0.5, image->height() - 0.5), &ok);
	if (!ok)
	{
		qDebug() << "updatePosFromGeoreferencing() failed";
		return; // TODO: proper error message?
	}
	
	// Calculate template transform as similarity transform from pixels to map coordinates
	PassPointList pp_list;
	
	PassPoint pp;
	pp.src_coords = MapCoordF(-0.5 * image->width(), -0.5 * image->height());
	pp.dest_coords = top_left;
	pp_list.push_back(pp);
	pp.src_coords = MapCoordF(0.5 * image->width(), -0.5 * image->height());
	pp.dest_coords = top_right;
	pp_list.push_back(pp);
	pp.src_coords = MapCoordF(-0.5 * image->width(), 0.5 * image->height());
	pp.dest_coords = bottom_left;
	pp_list.push_back(pp);
	
	QTransform q_transform;
	if (!pp_list.estimateNonIsometricSimilarityTransform(&q_transform))
	{
		qDebug() << "updatePosFromGeoreferencing() failed";
		return; // TODO: proper error message?
	}
	qTransformToTemplateTransform(q_transform, &transform);
	updateTransformationMatrices();
}


TemplateImage::WorldFile::WorldFile()
{
	loaded = false;
}

bool TemplateImage::WorldFile::load(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		loaded = false;
		return false;
	}
	QTextStream text_stream(&file);
	
	bool ok = false;
	double numbers[6];
	for (int i = 0; i < 6; ++i)
	{
		double number = text_stream.readLine().toDouble(&ok);
		if (!ok)
		{
			file.close();
			loaded = false;
			return false;
		}
		numbers[i] = number;
	}
	
	file.close();
	
	pixel_to_world.setMatrix(
		numbers[0], numbers[2], numbers[4],
		numbers[1], numbers[3], numbers[5],
		0, 0, 1);
	pixel_to_world = pixel_to_world.transposed();
	loaded = true;
	return true;
}

bool TemplateImage::WorldFile::tryToLoadForImage(const QString image_path)
{
	int last_dot_index = image_path.lastIndexOf('.');
	if (last_dot_index < 0)
		return false;
	QString path_without_ext = image_path.left(last_dot_index + 1);
	QString ext = image_path.right(image_path.size() - (last_dot_index + 1));
	if (ext.size() <= 2)
		return false;
	
	// Possibility 1: use first and last character from image filename extension and 'w'
	QString test_path = path_without_ext + ext[0] + ext[ext.size() - 1] + 'w';
	if (load(test_path))
		return true;
	
	// Possibility 2: append 'w' to original extension
	test_path = image_path + 'w';
	if (load(test_path))
		return true;
	
	// Possibility 3: replace original extension by 'wld'
	test_path = path_without_ext + "wld";
	if (load(test_path))
		return true;
	
	return false;
}


// ### TemplateImageOpenDialog ###

TemplateImageOpenDialog::TemplateImageOpenDialog(TemplateImage* image, QWidget* parent)
 : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint), image(image)
{
	setWindowTitle(tr("Opening %1").arg(image->getTemplateFilename()));
	
	QLabel* size_label = new QLabel("<b>" + tr("Image size:") + QString("</b> %1 x %2")
		.arg(image->getQImage()->width()).arg(image->getQImage()->height()));
	
	QLabel* desc_label = new QLabel(tr("Specify how to position or scale the image:"));
	
	bool use_meters_per_pixel;
	double meters_per_pixel;
	double dpi;
	double scale;
	image->getMap()->getImageTemplateDefaults(use_meters_per_pixel, meters_per_pixel, dpi, scale);
	
	QString georef_type_string;
	if (image->getAvailableGeoreferencing() == TemplateImage::Georeferencing_WorldFile)
		georef_type_string = tr("World file");
	else if (image->getAvailableGeoreferencing() == TemplateImage::Georeferencing_GeoTiff)
		georef_type_string = tr("GeoTiff");
	else if (image->getAvailableGeoreferencing() == TemplateImage::Georeferencing_None)
		georef_type_string = tr("no georeferencing information");
	
	georef_radio = new QRadioButton(tr("Georeferenced") +
		(georef_type_string.isEmpty() ? "" : (" (" + georef_type_string + ")")));
	georef_radio->setEnabled(image->getAvailableGeoreferencing() != TemplateImage::Georeferencing_None);
	
	mpp_radio = new QRadioButton(tr("Meters per pixel:"));
	mpp_edit = new QLineEdit((meters_per_pixel > 0) ? QString::number(meters_per_pixel) : "");
	mpp_edit->setValidator(new DoubleValidator(0, 999999, mpp_edit));
	
	dpi_radio = new QRadioButton(tr("Scanned with"));
	dpi_edit = new QLineEdit((dpi > 0) ? QString::number(dpi) : "");
	dpi_edit->setValidator(new DoubleValidator(1, 999999, dpi_edit));
	QLabel* dpi_label = new QLabel(tr("dpi"));
	
	QLabel* scale_label = new QLabel(tr("Template scale:  1 :"));
	scale_edit = new QLineEdit((scale > 0) ? QString::number(scale) : "");
	scale_edit->setValidator(new QIntValidator(1, 999999, scale_edit));
	
	if (georef_radio->isEnabled())
		georef_radio->setChecked(true);
	else if (use_meters_per_pixel)
		mpp_radio->setChecked(true);
	else
		dpi_radio->setChecked(true);
	
	QHBoxLayout* mpp_layout = new QHBoxLayout();
	mpp_layout->addWidget(mpp_radio);
	mpp_layout->addWidget(mpp_edit);
	mpp_layout->addStretch(1);
	QHBoxLayout* dpi_layout = new QHBoxLayout();
	dpi_layout->addWidget(dpi_radio);
	dpi_layout->addWidget(dpi_edit);
	dpi_layout->addWidget(dpi_label);
	dpi_layout->addStretch(1);
	QHBoxLayout* scale_layout = new QHBoxLayout();
	scale_layout->addSpacing(16);
	scale_layout->addWidget(scale_label);
	scale_layout->addWidget(scale_edit);
	scale_layout->addStretch(1);
	
	QPushButton* cancel_button = new QPushButton(tr("Cancel"));
	open_button = new QPushButton(QIcon(":/images/arrow-right.png"), tr("Open"));
	open_button->setDefault(true);
	
	QHBoxLayout* buttons_layout = new QHBoxLayout();
	buttons_layout->addWidget(cancel_button);
	buttons_layout->addStretch(1);
	buttons_layout->addWidget(open_button);
	
	QVBoxLayout* layout = new QVBoxLayout();
	layout->addWidget(size_label);
	layout->addSpacing(16);
	layout->addWidget(desc_label);
	layout->addWidget(georef_radio);
	layout->addLayout(mpp_layout);
	layout->addLayout(dpi_layout);
	layout->addLayout(scale_layout);
	layout->addSpacing(16);
	layout->addLayout(buttons_layout);
	setLayout(layout);
	
	connect(mpp_edit, SIGNAL(textEdited(QString)), this, SLOT(setOpenEnabled()));
	connect(dpi_edit, SIGNAL(textEdited(QString)), this, SLOT(setOpenEnabled()));
	connect(scale_edit, SIGNAL(textEdited(QString)), this, SLOT(setOpenEnabled()));
	connect(cancel_button, SIGNAL(clicked(bool)), this, SLOT(reject()));
	connect(open_button, SIGNAL(clicked(bool)), this, SLOT(doAccept()));
	connect(georef_radio, SIGNAL(clicked(bool)), this, SLOT(radioClicked()));
	connect(mpp_radio, SIGNAL(clicked(bool)), this, SLOT(radioClicked()));
	connect(dpi_radio, SIGNAL(clicked(bool)), this, SLOT(radioClicked()));
	
	radioClicked();
}
double TemplateImageOpenDialog::getMpp() const
{
	if (mpp_radio->isChecked())
		return mpp_edit->text().toDouble();
	else
	{
		double dpi = dpi_edit->text().toDouble();			// dots/pixels per inch(on map)
		double ipd = 1 / dpi;								// inch(on map) per pixel
		double mpd = ipd * 0.0254;							// meters(on map) per pixel	
		double mpp = mpd * scale_edit->text().toDouble();	// meters(in reality) per pixel
		return mpp;
	}
}
bool TemplateImageOpenDialog::isGeorefRadioChecked() const
{
	return georef_radio->isChecked();
}
void TemplateImageOpenDialog::radioClicked()
{
	bool mpp_checked = mpp_radio->isChecked();
	bool dpi_checked = dpi_radio->isChecked();
	dpi_edit->setEnabled(dpi_checked);
	scale_edit->setEnabled(dpi_checked);
	mpp_edit->setEnabled(mpp_checked);
	setOpenEnabled();
}
void TemplateImageOpenDialog::setOpenEnabled()
{
	if (mpp_radio->isChecked())
		open_button->setEnabled(!mpp_edit->text().isEmpty());
	else if (dpi_radio->isChecked())
		open_button->setEnabled(!scale_edit->text().isEmpty() && !dpi_edit->text().isEmpty());
	else //if (georef_radio->isChecked())
		open_button->setEnabled(true);
}
void TemplateImageOpenDialog::doAccept()
{
	image->getMap()->setImageTemplateDefaults(mpp_radio->isChecked(), mpp_edit->text().toDouble(), dpi_edit->text().toDouble(), scale_edit->text().toDouble());
	accept();
}
