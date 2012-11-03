/*
 *    Copyright 2012 Pete Curtis, Kai Pastor
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

#include "file_format_xml.h"

#include <QDebug>
#include <QFile>
#include <QPrinter>
#include <QStringBuilder>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "georeferencing.h"
#include "map.h"
#include "map_color.h"
#include "map_grid.h"
#include "object.h"
#include "object_text.h"
#include "symbol_point.h"
#include "symbol_line.h"
#include "symbol_area.h"
#include "symbol_text.h"
#include "symbol_combined.h"
#include "template.h"
#include "map_grid.h"

// ### XMLFileExporter declaration ###

class XMLFileExporter : public Exporter
{
public:
	XMLFileExporter(QIODevice* stream, Map *map, MapView *view);
	virtual ~XMLFileExporter() {}
	
	virtual void doExport() throw (FormatException);
	
	void exportGeoreferencing();
	void exportColors();
	void exportSymbols();
	void exportMapParts();
	void exportTemplates();
	void exportView();
	void exportPrint();
	void exportUndo();
	void exportRedo();

protected:
	QXmlStreamWriter xml;
};


// ### XMLFileImporter declaration ###

class XMLFileImporter : public Importer
{
public:
	XMLFileImporter(QIODevice* stream, Map *map, MapView *view);
	virtual ~XMLFileImporter() {}

protected:
	virtual void import(bool load_symbols_only) throw (FormatException);
	
	void addWarningUnsupportedElement();
	void importGeoreferencing();
	void importColors();
	void importSymbols();
	void importMapParts();
	void importTemplates();
	void importView();
	void importPrint();
	void importUndo();
	void importRedo();
	
	QXmlStreamReader xml;
	SymbolDictionary symbol_dict;
};



// ### XMLFileFormat definition ###

const int XMLFileFormat::minimum_version = 2;
const int XMLFileFormat::current_version = 2;
const QString XMLFileFormat::magic_string = "<?xml ";
const QString XMLFileFormat::mapper_namespace = "http://oorienteering.sourceforge.net/mapper/xml/v2";

XMLFileFormat::XMLFileFormat()
: Format("XML", QObject::tr("OpenOrienteering Mapper XML"), "xmap", true, true, false) 
{
	// NOP
}

bool XMLFileFormat::understands(const unsigned char *buffer, size_t sz) const
{
	uint len = magic_string.length();
	if (sz >= len && memcmp(buffer, magic_string.toUtf8().data(), len) == 0) 
		return true;
	return false;
}

Importer *XMLFileFormat::createImporter(QIODevice* stream, Map *map, MapView *view) const throw (FormatException)
{
	return new XMLFileImporter(stream, map, view);
}

Exporter *XMLFileFormat::createExporter(QIODevice* stream, Map *map, MapView *view) const throw (FormatException)
{
	return new XMLFileExporter(stream, map, view);
}



// ### XMLFileExporter definition ###

XMLFileExporter::XMLFileExporter(QIODevice* stream, Map *map, MapView *view)
: Exporter(stream, map, view),
  xml(stream)
{
	xml.setAutoFormatting(true);
}

void XMLFileExporter::doExport() throw (FormatException)
{
	xml.writeDefaultNamespace(XMLFileFormat::mapper_namespace);
	xml.writeStartDocument();
	xml.writeStartElement("map");
	xml.writeAttribute("version", QString::number(XMLFileFormat::current_version));
	
	xml.writeTextElement("notes", map->getMapNotes());
	
	exportGeoreferencing();
	exportColors();
	exportSymbols();
	exportMapParts();
	exportTemplates();
	exportView();
	exportPrint();
	exportUndo();
	exportRedo();
	
	xml.writeEndElement(/*document*/);
	xml.writeEndDocument();
}

void XMLFileExporter::exportGeoreferencing()
{
	map->getGeoreferencing().save(xml);
}

void XMLFileExporter::exportColors()
{
	xml.writeStartElement("colors");
	int num_colors = (int)map->color_set->colors.size();
	xml.writeAttribute("count", QString::number(num_colors));
	for (int i = 0; i < num_colors; ++i)
	{
		MapColor* color = map->color_set->colors[i];
		xml.writeEmptyElement("color");
		xml.writeAttribute("priority", QString::number(color->priority));
		xml.writeAttribute("c", QString::number(color->c));
		xml.writeAttribute("m", QString::number(color->m));
		xml.writeAttribute("y", QString::number(color->y));
		xml.writeAttribute("k", QString::number(color->k));
		xml.writeAttribute("opacity", QString::number(color->opacity));
		xml.writeAttribute("name", color->name);
	}
	xml.writeEndElement(/*colors*/); 
}

void XMLFileExporter::exportSymbols()
{
	xml.writeStartElement("symbols");
	int num_symbols = map->getNumSymbols();
	xml.writeAttribute("count", QString::number(num_symbols));
	for (int i = 0; i < num_symbols; ++i)
	{
		map->getSymbol(i)->save(xml, *map);
	}
	xml.writeEndElement(/*symbols*/); 
}

void XMLFileExporter::exportMapParts()
{
	xml.writeStartElement("parts");
	int num_parts = map->getNumParts();
	xml.writeAttribute("count", QString::number(num_parts));
	xml.writeAttribute("current", QString::number(map->current_part_index));
	for (int i = 0; i < num_parts; ++i)
		map->getPart(i)->save(xml, *map);
	xml.writeEndElement(/*parts*/); 
}

void XMLFileExporter::exportTemplates()
{
	xml.writeStartElement("templates");
	
	int num_templates = map->getNumTemplates() + map->getNumClosedTemplates();
	xml.writeAttribute("count", QString::number(num_templates));
	xml.writeAttribute("first_front_template", QString::number(map->first_front_template));
	for (int i = 0; i < map->getNumTemplates(); ++i)
		map->getTemplate(i)->saveTemplateConfiguration(xml, true);
	for (int i = 0; i < map->getNumClosedTemplates(); ++i)
		map->getClosedTemplate(i)->saveTemplateConfiguration(xml, false);
	
	xml.writeEmptyElement("defaults");
	xml.writeAttribute("use_meters_per_pixel", map->image_template_use_meters_per_pixel ? "true" : "false");
	xml.writeAttribute("meters_per_pixel", QString::number(map->image_template_meters_per_pixel));
	xml.writeAttribute("dpi", QString::number(map->image_template_dpi));
	xml.writeAttribute("scale", QString::number(map->image_template_scale));
	
	xml.writeEndElement(/*templates*/); 
}

void XMLFileExporter::exportView()
{
	xml.writeStartElement("view");
	if (map->area_hatching_enabled)
		xml.writeAttribute("area_hatching_enabled", "true");
	if (map->baseline_view_enabled)
		xml.writeAttribute("baseline_view_enabled", "true");
	
	map->getGrid().save(xml);
	
	if (view)
		view->save(xml);
	
	xml.writeEndElement(/*view*/);
}

void XMLFileExporter::exportPrint()
{
	if (map->print_params_set)
	{
		xml.writeStartElement("print");
		xml.writeAttribute("orientation",
		  (map->print_orientation == QPrinter::Portrait) ? "portrait" : "landscape" );
		xml.writeAttribute("QPrinter_PaperSize", QString::number(map->print_format)); // FIXME: use readable names
		xml.writeAttribute("dpi", QString::number(map->print_dpi));
		if (map->print_show_templates)
			xml.writeAttribute("templates_visible", "true");
		if (map->print_show_grid)
			xml.writeAttribute("grid_visible", "true");
		if (map->print_center)
			xml.writeAttribute("center", "true");
		xml.writeAttribute("area_left", QString::number(map->print_area_left));
		xml.writeAttribute("area_top", QString::number(map->print_area_top));
		xml.writeAttribute("area_width", QString::number(map->print_area_width));
		xml.writeAttribute("area_height", QString::number(map->print_area_height));
		if (map->print_different_scale_enabled)
			xml.writeAttribute("alternative_scale_enabled", "true");
		xml.writeAttribute("alternative_scale", QString::number(map->print_different_scale));
		
		xml.writeEndElement(/*print*/);
    }
}

void XMLFileExporter::exportUndo()
{
	map->object_undo_manager.saveUndo(xml);
}

void XMLFileExporter::exportRedo()
{
	map->object_undo_manager.saveRedo(xml);
}



// ### XMLFileImporter definition ###

XMLFileImporter::XMLFileImporter(QIODevice* stream, Map *map, MapView *view)
: Importer(stream, map, view),
  xml(stream)
{
	//NOP
}

void XMLFileImporter::addWarningUnsupportedElement()
{
	addWarning(QObject::tr("Unsupported element: %1 (line %2 column %3)").
	  arg(xml.name().toString()).
	  arg(xml.lineNumber()).
	  arg(xml.columnNumber())
	);
}

void XMLFileImporter::import(bool load_symbols_only) throw (FormatException)
{
	/*while (!xml.atEnd())
	{
		if (xml.readNextStartElement())
			qDebug() << xml.qualifiedName();
		else
			qDebug() << "FALSE";
	}*/
	
	if (xml.readNextStartElement())
	{
		if (xml.name() != "map")
			xml.raiseError(QObject::tr("Unsupport file format."));
		else
		{
			int version = xml.attributes().value("version").toString().toInt();
			if (version < 1)
				xml.raiseError(QObject::tr("Invalid file format version."));
			else if (version < XMLFileFormat::minimum_version)
				xml.raiseError(QObject::tr("Unsupported file format version. Please use an older program version to load and update the file."));
			else if (version > XMLFileFormat::current_version)
				addWarning(QObject::tr("New file format version detected. Some features will be not be supported by this version of the program."));
		}
	}
	
	while (xml.readNextStartElement())
	{
		const QStringRef name(xml.name());
		
		if (name == "georeferencing")
			importGeoreferencing();
		else if (name == "colors")
			importColors();
		else if (name == "symbols")
			importSymbols();
		else if (load_symbols_only)
			xml.skipCurrentElement();
		else if (name == "notes")
			map->setMapNotes(xml.readElementText());
		else if (name == "parts")
			importMapParts();
		else if (name == "templates")
			importTemplates();
		else if (name == "view")
			importView();
		else if (name == "print")
			importPrint();
		else if (name == "undo")
			importUndo();
		else if (name == "redo")
			importRedo();
		else
		{
			addWarningUnsupportedElement();
			xml.skipCurrentElement();
		}
	}
	
	if (xml.error())
		throw FormatException(xml.errorString());
}

void XMLFileImporter::importGeoreferencing()
{
	Q_ASSERT(xml.name() == "georeferencing");
	Georeferencing georef;
	georef.load(xml);
	map->setGeoreferencing(georef);
}

void XMLFileImporter::importColors()
{
	int num_colors = xml.attributes().value("count").toString().toInt();
	Map::ColorVector& colors(map->color_set->colors);
	colors.reserve(qMin(num_colors, 100)); // 100 is not a limit
	while (xml.readNextStartElement())
	{
		if (xml.name() == "color")
		{
			QXmlStreamAttributes attributes = xml.attributes();
			MapColor* color = new MapColor();
			color->name = attributes.value("name").toString();
			color->priority = attributes.value("priority").toString().toInt();
			color->c = attributes.value("c").toString().toFloat();
			color->m = attributes.value("m").toString().toFloat();
			color->y = attributes.value("y").toString().toFloat();
			color->k = attributes.value("k").toString().toFloat();
			color->opacity = attributes.value("opacity").toString().toFloat();
			color->updateFromCMYK();
			colors.push_back(color);
		}
		else
			addWarningUnsupportedElement();
		xml.skipCurrentElement();
	}
	
	if (num_colors > 0 && num_colors != (int)colors.size())
		addWarning(QObject::tr("Expected %1 colors, found %2.").
		  arg(num_colors).
		  arg(colors.size())
		);
}

void XMLFileImporter::importSymbols()
{
	int num_symbols = xml.attributes().value("count").toString().toInt();
	map->symbols.reserve(qMin(num_symbols, 1000)); // 1000 is not a limit
	
	symbol_dict[QString::number(map->findSymbolIndex(map->getUndefinedPoint()))] = map->getUndefinedPoint();
	symbol_dict[QString::number(map->findSymbolIndex(map->getUndefinedLine()))] = map->getUndefinedLine();
	
	while (xml.readNextStartElement())
	{
		if (xml.name() == "symbol")
		{
			map->symbols.push_back(Symbol::load(xml, *map, symbol_dict));
		}
		else
		{
			addWarningUnsupportedElement();
			xml.skipCurrentElement();
		}
	}
	
	if (num_symbols > 0 && num_symbols != map->getNumSymbols())
		addWarning(QObject::tr("Expected %1 symbols, found %2.").
		  arg(num_symbols).
		  arg(map->getNumSymbols())
		);
}

void XMLFileImporter::importMapParts()
{
	int num_parts = xml.attributes().value("count").toString().toInt();
	int current_part_index = xml.attributes().value("current").toString().toInt();
	map->parts.clear();
	map->parts.reserve(qMin(num_parts, 20)); // 20 is not a limit
	
	while (xml.readNextStartElement())
	{
		if (xml.name() == "part")
		{
			map->parts.push_back(MapPart::load(xml, *map, symbol_dict));
		}
		else
		{
			addWarningUnsupportedElement();
			xml.skipCurrentElement();
		}
	}
	
	if (0 <= current_part_index && current_part_index < map->getNumParts())
		map->current_part_index = current_part_index;
	
	if (num_parts > 0 && num_parts != map->getNumParts())
		addWarning(QObject::tr("Expected %1 map parts, found %2.").
		  arg(num_parts).
		  arg(map->getNumParts())
		);
}

void XMLFileImporter::importTemplates()
{
	Q_ASSERT(xml.name() == "templates");
	
	int first_front_template = xml.attributes().value("first_front_template").toString().toInt();
	
	int num_templates = xml.attributes().value("count").toString().toInt();
	map->templates.reserve(qMin(num_templates, 20)); // 20 is not a limit
	map->closed_templates.reserve(num_templates % 20);
	
	while (xml.readNextStartElement())
	{
		if (xml.name() == "template")
		{
			bool opened = true;
			Template* temp = Template::loadTemplateConfiguration(xml, *map, opened);
			if (opened)
				map->templates.push_back(temp);
			else
				map->closed_templates.push_back(temp);
		}
		else if (xml.name() == "defaults")
		{
			QXmlStreamAttributes attributes = xml.attributes();
			map->image_template_use_meters_per_pixel = (attributes.value("use_meters_per_pixel") == "true");
			map->image_template_meters_per_pixel = attributes.value("meters_per_pixel").toString().toDouble();
			map->image_template_dpi = attributes.value("dpi").toString().toDouble();
			map->image_template_scale = attributes.value("scale").toString().toDouble();
			xml.skipCurrentElement();
		}
		else
			xml.skipCurrentElement();
	}
	
	map->first_front_template = qMax(0, qMin(map->getNumTemplates(), first_front_template));
}

void XMLFileImporter::importView()
{
	Q_ASSERT(xml.name() == "view");
	
	map->area_hatching_enabled = (xml.attributes().value("area_hatching_enabled") == "true");
	map->baseline_view_enabled = (xml.attributes().value("baseline_view_enabled") == "true");
	
	while (xml.readNextStartElement())
	{
		if (xml.name() == "grid")
			map->getGrid().load(xml);
		else if (xml.name() == "map_view")
			view->load(xml);
		else
			xml.skipCurrentElement(); // unsupported
	}
}

void XMLFileImporter::importPrint()
{
	Q_ASSERT(xml.name() == "print");
	
	map->print_params_set = true;
	QXmlStreamAttributes attributes = xml.attributes();
	map->print_orientation = 
	  (attributes.value("orientation") == "portrait") ? QPrinter::Portrait : QPrinter::Landscape;
	map->print_format = (QPrinter::PaperSize) attributes.value("QPrinter_PaperSize").toString().toInt();
	map->print_dpi = attributes.value("dpi").toString().toFloat();
	map->print_show_templates = (attributes.value("templates_visible") == "true");
	map->print_show_grid = (attributes.value("grid_visible") == "true");
	map->print_center = (attributes.value("center") == "true");
	map->print_area_left = attributes.value("area_left").toString().toFloat();
	map->print_area_top = attributes.value("area_top").toString().toFloat();
	map->print_area_width = attributes.value("area_width").toString().toFloat();
	map->print_area_height = attributes.value("area_height").toString().toFloat();
	map->print_different_scale_enabled = (attributes.value("alternative_scale_enabled") == "true");
	map->print_different_scale = attributes.value("alternative_scale").toString().toInt();
    xml.skipCurrentElement();
}

void XMLFileImporter::importUndo()
{
	map->object_undo_manager.loadUndo(xml, symbol_dict);
}

void XMLFileImporter::importRedo()
{
	map->object_undo_manager.loadRedo(xml, symbol_dict);
}
