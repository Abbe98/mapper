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


#include "object.h"

#include <qmath.h>
#include <QDebug>
#include <QIODevice>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "util.h"
#include "symbol.h"
#include "symbol_point.h"
#include "symbol_line.h"
#include "symbol_text.h"
#include "map_editor.h"
#include "object_text.h"
#include "renderable.h"
#include "qbezier_p.h"
#include "settings.h"

Object::Object(Object::Type type, Symbol* symbol)
: type(type),
  symbol(symbol),
  map(NULL),
  output_dirty(true),
  extent(),
  output(this, extent)
{
}
Object::~Object()
{
}

bool Object::equals(Object* other, bool compare_symbol)
{
	if (type != other->type)
		return false;
	if (compare_symbol)
	{
		if ((symbol == NULL && other->symbol != NULL) ||
			(symbol != NULL && other->symbol == NULL))
			return false;
		if (symbol && !symbol->equals(other->symbol))
			return false;
	}
	
	if (coords.size() != other->coords.size())
		return false;
	for (size_t i = 0, end = coords.size(); i < end; ++i)
	{
		if (coords[i] != other->coords[i])
			return false;
	}
	
	if (type == Point)
	{
		PointObject* point_this = static_cast<PointObject*>(this);
		PointObject* point_other = static_cast<PointObject*>(other);
		
		// Make sure that compared values are greater than 1, see qFuzzyCompare
		float rotation_a = point_this->getRotation();
		float rotation_b = point_other->getRotation();
		while (rotation_a < 1)
		{
			rotation_a += 100;
			rotation_b += 100;
		}
		if (!qFuzzyCompare(rotation_a, rotation_b))
			return false;
	}
	else if (type == Path)
	{
		PathObject* path_this = static_cast<PathObject*>(this);
		PathObject* path_other = static_cast<PathObject*>(other);
		
		// Make sure that compared values are greater than 1, see qFuzzyCompare
		float rotation_a = path_this->getPatternRotation();
		float rotation_b = path_other->getPatternRotation();
		while (rotation_a < 1)
		{
			rotation_a += 100;
			rotation_b += 100;
		}
		if (!qFuzzyCompare(rotation_a, rotation_b))
			return false;
		
		if (path_this->getPatternOrigin() != path_other->getPatternOrigin())
			return false;
	}
	else if (type == Text)
	{
		TextObject* text_this = static_cast<TextObject*>(this);
		TextObject* text_other = static_cast<TextObject*>(other);
		
		if (text_this->getText().compare(text_other->getText(), Qt::CaseSensitive) != 0)
			return false;
		if (text_this->getHorizontalAlignment() != text_other->getHorizontalAlignment())
			return false;
		if (text_this->getVerticalAlignment() != text_other->getVerticalAlignment())
			return false;
		
		// Make sure that compared values are greater than 1, see qFuzzyCompare
		float rotation_a = text_this->getRotation();
		float rotation_b = text_other->getRotation();
		while (rotation_a < 1)
		{
			rotation_a += 100;
			rotation_b += 100;
		}
		if (!qFuzzyCompare(rotation_a, rotation_b))
			return false;
	}
	
	return true;
}

Object& Object::operator=(const Object& other)
{
	assert(type == other.type);
	symbol = other.symbol;
	coords = other.coords;
	
	if (type == Point)
	{
		PointObject* point_this = static_cast<PointObject*>(this);
		const PointObject* point_other = static_cast<const PointObject*>(&other);
		
		PointSymbol* point_symbol = static_cast<PointSymbol*>(point_this->getSymbol());
		if (point_symbol && point_symbol->isRotatable())
			point_this->setRotation(point_other->getRotation());
	}
	else if (type == Path)
	{
		PathObject* path_this = static_cast<PathObject*>(this);
		const PathObject* path_other = static_cast<const PathObject*>(&other);
		
		path_this->setPatternRotation(path_other->getPatternRotation());
		path_this->setPatternOrigin(path_other->getPatternOrigin());
	}
	else if (type == Text)
	{
		TextObject* text_this = static_cast<TextObject*>(this);
		const TextObject* text_other = static_cast<const TextObject*>(&other);
		
		text_this->setText(text_other->getText());
		text_this->setHorizontalAlignment(text_other->getHorizontalAlignment());
		text_this->setVerticalAlignment(text_other->getVerticalAlignment());
		text_this->setRotation(text_other->getRotation());
	}
	
	return *this;
}

PointObject* Object::asPoint()
{
	assert(type == Point);
	return static_cast<PointObject*>(this);
}
PathObject* Object::asPath()
{
	assert(type == Path);
	return static_cast<PathObject*>(this);
}
TextObject* Object::asText()
{
	assert(type == Text);
	return static_cast<TextObject*>(this);
}

void Object::save(QIODevice* file)
{
	int symbol_index = -1;
	if (map)
		symbol_index = map->findSymbolIndex(symbol);
	file->write((const char*)&symbol_index, sizeof(int));
	
	int num_coords = (int)coords.size();
	file->write((const char*)&num_coords, sizeof(int));
	file->write((const char*)&coords[0], num_coords * sizeof(MapCoord));
	
	// Central handling of sub-types here to avoid virtual methods
	if (type == Point)
	{
		PointObject* point = reinterpret_cast<PointObject*>(this);
		PointSymbol* point_symbol = reinterpret_cast<PointSymbol*>(point->getSymbol());
		if (point_symbol->isRotatable())
		{
			float rotation = point->getRotation();
			file->write((const char*)&rotation, sizeof(float));
		}
	}
	else if (type == Path)
	{
		PathObject* path = reinterpret_cast<PathObject*>(this);
		float rotation = path->getPatternRotation();
		file->write((const char*)&rotation, sizeof(float));
		MapCoord origin = path->getPatternOrigin();
		file->write((const char*)&origin, sizeof(MapCoord));
	}
	else if (type == Text)
	{
		TextObject* text = reinterpret_cast<TextObject*>(this);
		float rotation = text->getRotation();
		file->write((const char*)&rotation, sizeof(float));
		int temp = (int)text->getHorizontalAlignment();
		file->write((const char*)&temp, sizeof(int));
		temp = (int)text->getVerticalAlignment();
		file->write((const char*)&temp, sizeof(int));
		saveString(file, text->getText());
	}
}

void Object::load(QIODevice* file, int version, Map* map)
{
	this->map = map;
	
	int symbol_index;
	file->read((char*)&symbol_index, sizeof(int));
	Symbol* read_symbol = map->getSymbol(symbol_index);
	if (read_symbol)
		symbol = read_symbol;
	
	int num_coords;
	file->read((char*)&num_coords, sizeof(int));
	coords.resize(num_coords);
	file->read((char*)&coords[0], num_coords * sizeof(MapCoord));
	
	if (version <= 8)
	{
		bool path_closed;
		file->read((char*)&path_closed, sizeof(bool));
		if (path_closed)
			coords[coords.size() - 1].setClosePoint(true);
	}
	
	if (version >= 8)
	{
		if (type == Point)
		{
			PointObject* point = reinterpret_cast<PointObject*>(this);
			PointSymbol* point_symbol = reinterpret_cast<PointSymbol*>(point->getSymbol());
			if (point_symbol->isRotatable())
			{
				float rotation;
				file->read((char*)&rotation, sizeof(float));
				point->setRotation(rotation);
			}
		}
		else if (type == Path)
		{
			PathObject* path = reinterpret_cast<PathObject*>(this);
			if (version >= 21)
			{
				float rotation;
				file->read((char*)&rotation, sizeof(float));
				path->setPatternRotation(rotation);
				MapCoord origin;
				file->read((char*)&origin, sizeof(MapCoord));
				path->setPatternOrigin(origin);
			}
		}
		else if (type == Text)
		{
			TextObject* text = reinterpret_cast<TextObject*>(this);
			float rotation;
			file->read((char*)&rotation, sizeof(float));
			text->setRotation(rotation);
			
			int temp;
			file->read((char*)&temp, sizeof(int));
			text->setHorizontalAlignment((TextObject::HorizontalAlignment)temp);
			file->read((char*)&temp, sizeof(int));
			text->setVerticalAlignment((TextObject::VerticalAlignment)temp);
			
			QString str;
			loadString(file, str);
			text->setText(str);
		}
	}
	
	if (type == Path)
	{
		PathObject* path = reinterpret_cast<PathObject*>(this);
		path->recalculateParts();
	}
	
	output_dirty = true;
}

void Object::save(QXmlStreamWriter& xml) const
{
	xml.writeStartElement("object");
	xml.writeAttribute("type", QString::number(type));
	int symbol_index = -1;
	if (map)
		symbol_index = map->findSymbolIndex(symbol);
	if (symbol_index != -1)
		xml.writeAttribute("symbol", QString::number(symbol_index));
	
	if (type == Point)
	{
		const PointObject* point = reinterpret_cast<const PointObject*>(this);
		const PointSymbol* point_symbol = reinterpret_cast<const PointSymbol*>(point->getSymbol());
		if (point_symbol->isRotatable())
			xml.writeAttribute("rotation", QString::number(point->getRotation()));
	}
	else if (type == Text)
	{
		const TextObject* text = reinterpret_cast<const TextObject*>(this);
		xml.writeAttribute("rotation", QString::number(text->getRotation()));
		xml.writeAttribute("h_align", QString::number(text->getHorizontalAlignment()));
		xml.writeAttribute("v_align", QString::number(text->getVerticalAlignment()));
	}
	
	xml.writeStartElement("coords");
	int num_coords = (int)coords.size();
	xml.writeAttribute("count", QString::number(num_coords));
	for (int i = 0; i < num_coords; i++)
		coords[i].save(xml);
	xml.writeEndElement(/*coords*/);
	
	if (type == Path)
	{
		const PathObject* path = reinterpret_cast<const PathObject*>(this);
		xml.writeStartElement("pattern");
		xml.writeAttribute("rotation", QString::number(path->getPatternRotation()));
		path->getPatternOrigin().save(xml);
		xml.writeEndElement(/*pattern*/);
	}
	else if (type == Text)
	{
		const TextObject* text = reinterpret_cast<const TextObject*>(this);
		xml.writeTextElement("text", text->getText());
	}
	
	xml.writeEndElement(/*object*/);
}

Object* Object::load(QXmlStreamReader& xml, Map& map, const SymbolDictionary& symbol_dict, Symbol* symbol) throw (FormatException)
{
	Q_ASSERT(xml.name() == "object");
	
	QXmlStreamAttributes attributes(xml.attributes());
	
	int object_type = attributes.value("type").toString().toInt();
	Object* object = Object::getObjectForType(static_cast<Object::Type>(object_type));
	if (!object)
		throw FormatException(QObject::tr("Error while loading an object of type %1.").arg(object_type));
	
	object->map = &map;
	
	if (symbol)
		object->symbol = symbol;
	else
	{
		QString symbol_id = attributes.value("symbol").toString();
		object->symbol = symbol_dict[symbol_id]; // FIXME: cannot work for forward references
		if (!object->symbol)
			throw FormatException(QObject::tr("Unable to find symbol for object at %1:%2.").arg(xml.lineNumber()).arg(xml.columnNumber()));
	}
	
	if (object_type == Point)
	{
		PointObject* point = reinterpret_cast<PointObject*>(object);
		PointSymbol* point_symbol = reinterpret_cast<PointSymbol*>(point->getSymbol());
		if (point_symbol && point_symbol->isRotatable())
			point->setRotation(attributes.value("rotation").toString().toFloat());
		else if (!point_symbol)
			throw FormatException(QObject::tr("Point object with undefined or wrong symbol at %1:%2.").arg(xml.lineNumber()).arg(xml.columnNumber()));
	}
	else if (object_type == Text)
	{
		TextObject* text = reinterpret_cast<TextObject*>(object);
		text->setRotation(attributes.value("rotation").toString().toFloat());
		text->setHorizontalAlignment(static_cast<TextObject::HorizontalAlignment>(attributes.value("h_align").toString().toInt()));
		text->setVerticalAlignment(static_cast<TextObject::VerticalAlignment>(attributes.value("v_align").toString().toInt()));
	}
	
	while (xml.readNextStartElement())
	{
		if (xml.name() == "coords")
		{
			int num_coords = xml.attributes().value("count").toString().toInt();
			object->coords.clear();
			object->coords.reserve(qMin(num_coords, 20)); // 20 is not a limit
			for (int i = 0; xml.readNextStartElement(); i++)
			{
				if (xml.name() == "coord")
					object->coords.push_back(MapCoord::load(xml));
				else
					xml.skipCurrentElement();
			}
		}
		else if (xml.name() == "pattern" && object_type == Path)
		{
			PathObject* path = reinterpret_cast<PathObject*>(object);
			path->setPatternRotation(xml.attributes().value("rotation").toString().toFloat());
			while (xml.readNextStartElement())
			{
				if (xml.name() == "coord")
					path->setPatternOrigin(MapCoord::load(xml));
				else
					xml.skipCurrentElement(); // unknown
			}
		}
		else if (xml.name() == "text" && object_type == Text)
		{
			TextObject* text = reinterpret_cast<TextObject*>(object);
			text->setText(xml.readElementText());
		}
		else
			xml.skipCurrentElement(); // unknown
	}
	
	if (object_type == Path)
	{
		PathObject* path = reinterpret_cast<PathObject*>(object);
		path->recalculateParts();
	}
	object->output_dirty = true;
	
	return object;
}

bool Object::update(bool force, bool insert_new_renderables)
{
	if (!force && !output_dirty)
		return false;
	
	if (map && extent.isValid())
		map->setObjectAreaDirty(extent);
	
	output.deleteRenderables();
	
	// Calculate float coordinates
	MapCoordVectorF coordsF;
	mapCoordVectorToF(coords, coordsF);
	
	// If the symbol contains a line or area symbol, calculate path coordinates
	if (symbol->getContainedTypes() & (Symbol::Area | Symbol::Line))
	{
		PathObject* path = reinterpret_cast<PathObject*>(this);
		path->updatePathCoords(coordsF);
	}
	
	// Create renderables
	extent = QRectF();
	
	if (map && map->isBaselineViewEnabled())
		Symbol::createBaselineRenderables(this, symbol, coords, coordsF, output, map->isAreaHatchingEnabled());
	else
		symbol->createRenderables(this, coords, coordsF, output);
	
	assert(extent.right() < 999999);	// assert if bogus values are returned
	output_dirty = false;
	
	if (map)
	{
		if (insert_new_renderables)
			map->insertRenderablesOfObject(this);
		if (extent.isValid())
			map->setObjectAreaDirty(extent);
	}
	
	return true;
}

void Object::move(qint64 dx, qint64 dy)
{
	int coords_size = coords.size();
	
	if (type == Text && coords_size == 2)
		coords_size = 1;	// don't touch box width / height for box texts
	
	for (int c = 0; c < coords_size; ++c)
	{
		MapCoord coord = coords[c];
		coord.setRawX(dx + coord.rawX());
		coord.setRawY(dy + coord.rawY());
		coords[c] = coord;
	}
	
	setOutputDirty();
}

void Object::scale(double factor)
{
	int coords_size = coords.size();
	for (int c = 0; c < coords_size; ++c)
	{
		coords[c].setX(coords[c].xd() * factor);
		coords[c].setY(coords[c].yd() * factor);
	}
	
	setOutputDirty();
}

void Object::rotateAround(MapCoordF center, double angle)
{
	double sin_angle = sin(angle);
	double cos_angle = cos(angle);
	
	int coords_size = coords.size();
	if (type == Text && coords_size == 2)
		coords_size = 1;	// don't touch box width / height for box texts
	for (int c = 0; c < coords_size; ++c)
	{
		MapCoordF center_to_coord = MapCoordF(coords[c].xd() - center.getX(), coords[c].yd() - center.getY());
		coords[c].setX(center.getX() + cos_angle * center_to_coord.getX() + sin_angle * center_to_coord.getY());
		coords[c].setY(center.getY() - sin_angle * center_to_coord.getX() + cos_angle * center_to_coord.getY());
	}
	
	if (type == Point)
	{
		PointObject* point = reinterpret_cast<PointObject*>(this);
		PointSymbol* point_symbol = reinterpret_cast<PointSymbol*>(point->getSymbol());
		if (point_symbol->isRotatable())
			point->setRotation(point->getRotation() + angle);
	}
	else if (type == Text)
	{
		TextObject* text = reinterpret_cast<TextObject*>(this);
		text->setRotation(text->getRotation() + angle);
	}
}

int Object::isPointOnObject(MapCoordF coord, float tolerance, bool treat_areas_as_paths, bool extended_selection)
{
	Symbol::Type type = symbol->getType();
	Symbol::Type contained_types = symbol->getContainedTypes();
	
	// Points
	if (type == Symbol::Point)
	{
		if (!extended_selection)
			return (coord.lengthToSquared(MapCoordF(coords[0])) <= tolerance) ? Symbol::Point : Symbol::NoSymbol;
		else
			return extent.contains(coord.toQPointF()) ? Symbol::Point : Symbol::NoSymbol;
	}
	
	// First check using extent
	float extent_extension = ((contained_types & Symbol::Line) || treat_areas_as_paths) ? tolerance : 0;
	if (coord.getX() < extent.left() - extent_extension) return Symbol::NoSymbol;
	if (coord.getY() < extent.top() - extent_extension) return Symbol::NoSymbol;
	if (coord.getX() > extent.right() + extent_extension) return Symbol::NoSymbol;
	if (coord.getY() > extent.bottom() + extent_extension) return Symbol::NoSymbol;
	
	if (type == Symbol::Text)
	{
		// Texts
		TextObject* text_object = reinterpret_cast<TextObject*>(this);
		return (text_object->calcTextPositionAt(coord, true) != -1) ? Symbol::Text : Symbol::NoSymbol;
	}
	else
	{
		// Path objects
		PathObject* path = reinterpret_cast<PathObject*>(this);
		return path->isPointOnPath(coord, tolerance, treat_areas_as_paths);
	}
}
bool Object::intersectsBox(QRectF box)
{
	if (type == Text)
		return extent.intersects(box);
	else if (type == Point)
		return box.contains(coords[0].toQPointF());
	else if (type == Path)
	{
		PathObject* path = reinterpret_cast<PathObject*>(this);
		const PathCoordVector& path_coords = path->getPathCoordinateVector();
		
		// Check path coords for an intersection with box
		int size = (int)path_coords.size();
		for (int i = 1; i < size; ++i)
		{
			if (path_coords[i].clen < path_coords[i-1].clen)
				continue;
			if (lineIntersectsRect(box, path_coords[i].pos.toQPointF(), path_coords[i-1].pos.toQPointF()))
				return true;
		}
		
		// If this is an area, additionally check if the area contains the box
		if (getSymbol()->getContainedTypes() & Symbol::Area)
			return isPointOnObject(MapCoordF(box.center()), 0, false, false);
	}
	else
		assert(false);
	
	return false;
}

void Object::takeRenderables()
{
	output.takeRenderables();
}

void Object::clearRenderables()
{
	output.deleteRenderables();
	extent = QRectF();
	if (getType() == Object::Path)
	{
		PathObject* path = reinterpret_cast<PathObject*>(this);
		path->clearPathCoordinates();
	}
}

bool Object::setSymbol(Symbol* new_symbol, bool no_checks)
{
	if (!no_checks && new_symbol)
	{
		if (!new_symbol->isTypeCompatibleTo(this))
			return false;
	}
	
	symbol = new_symbol;
	setOutputDirty();
	return true;
}

Object* Object::getObjectForType(Object::Type type, Symbol* symbol)
{
	if (type == Point)
		return new PointObject(symbol);
	else if (type == Path)
		return new PathObject(symbol);
	else if (type == Text)
		return new TextObject(symbol);
	else
	{
		assert(false);
		return NULL;
	}
}

// ### PathObject::PathPart ###

void PathObject::PathPart::setClosed(bool closed, bool may_use_existing_close_point)
{
	if (!isClosed() && closed)
	{
		if (getNumCoords() == 1 || !may_use_existing_close_point ||
			path->coords[start_index].rawX() != path->coords[end_index].rawX() || path->coords[start_index].rawY() != path->coords[end_index].rawY())
		{
			path->coords[end_index].setHolePoint(false);
			path->coords.insert(path->coords.begin() + (end_index + 1), path->coords[start_index]);
			++end_index;
			
			int part_index = this - &path->parts[0];
			for (int i = part_index + 1; i < (int)path->parts.size(); ++i)
			{
				++path->parts[i].start_index;
				++path->parts[i].end_index;
			}
		}
		path->setClosingPoint(end_index, path->coords[start_index]);
		
		path->setOutputDirty();
	}
	else if (isClosed() && !closed)
	{
		path->coords[end_index].setClosePoint(false);
		
		path->setOutputDirty();
	}
}
void PathObject::PathPart::connectEnds()
{
	if (isClosed())
		return;
	
	path->coords[start_index].setRawX(qRound64((path->coords[end_index].rawX() + path->coords[start_index].rawX()) / 2.0));
	path->coords[start_index].setRawY(qRound64((path->coords[end_index].rawY() + path->coords[start_index].rawY()) / 2.0));
	path->setClosingPoint(end_index, path->coords[start_index]);
	path->setOutputDirty();
}
int PathObject::PathPart::calcNumRegularPoints()
{
	int num_regular_points = 0;
	for (int i = start_index; i <= end_index; ++i)
	{
		++num_regular_points;
		if (path->coords[i].isCurveStart())
			i += 2;
	}
	if (isClosed())
		--num_regular_points;
	return num_regular_points;
}

double PathObject::PathPart::getLength()
{
	return path->path_coords[path_coord_end_index].clen;
}
double PathObject::PathPart::calculateArea()
{
	double area = 0;
	int j = path_coord_end_index;  // The last vertex is the 'previous' one to the first
	
	for (int i = path_coord_start_index; i <= path_coord_end_index; ++i)
	{
		area += (path->path_coords[j].pos.getX() + path->path_coords[i].pos.getX()) * (path->path_coords[j].pos.getY() - path->path_coords[i].pos.getY()); 
		j = i;  // j is previous vertex to i
	}
	return qAbs(area) / 2;
}

// ### PathObject ###

PathObject::PathObject(Symbol* symbol) : Object(Object::Path, symbol)
{
	assert(!symbol || (symbol->getType() == Symbol::Line || symbol->getType() == Symbol::Area || symbol->getType() == Symbol::Combined));
	pattern_rotation = 0;
	pattern_origin = MapCoord(0, 0);
}
PathObject::PathObject(Symbol* symbol, const MapCoordVector& coords, Map* map) : Object(Object::Path, symbol)
{
	assert(!symbol || (symbol->getType() == Symbol::Line || symbol->getType() == Symbol::Area || symbol->getType() == Symbol::Combined));
	pattern_rotation = 0;
	pattern_origin = MapCoord(0, 0);
	this->coords = coords;
	recalculateParts();
	if (map)
		setMap(map);
}
Object* PathObject::duplicate()
{
	PathObject* new_path = new PathObject(symbol);
	new_path->pattern_rotation = pattern_rotation;
	new_path->pattern_origin = pattern_origin;
	new_path->coords = coords;
	new_path->parts = parts;
	int parts_size = parts.size();
	for (int i = 0; i < parts_size; ++i)
		new_path->parts[i].path = new_path;
	return new_path;
}
PathObject* PathObject::duplicatePart(int part_index)
{
	PathObject* new_path = new PathObject(symbol);
	new_path->pattern_rotation = pattern_rotation;
	new_path->pattern_origin = pattern_origin;
	new_path->coords.assign(coords.begin() + parts[part_index].start_index, coords.begin() + (parts[part_index].end_index + 1));
	new_path->parts.push_back(parts[part_index]);
	new_path->parts[0].path = new_path;
	new_path->parts[0].end_index -= new_path->parts[0].start_index;
	new_path->parts[0].start_index = 0;
	new_path->parts[0].path_coord_end_index -= new_path->parts[0].path_coord_start_index;
	new_path->parts[0].path_coord_start_index = 0;
	return new_path;
}

MapCoord& PathObject::shiftedCoord(int base_index, int offset, PathObject::PathPart& part)
{
	int index = shiftedCoordIndex(base_index, offset, part);
	assert(index >= 0);
	return coords[index];
}
int PathObject::shiftedCoordIndex(int base_index, int offset, PathObject::PathPart& part)
{
	if (part.isClosed())
	{
		if (offset >= part.getNumCoords() - 1)
			return -1;
		return ((base_index + offset - part.start_index + (part.getNumCoords() - 1)) % (part.getNumCoords() - 1)) + part.start_index;
	}
	else
	{
		base_index += offset;
		return (base_index < 0 || base_index > part.end_index) ? -1 : base_index;
	}
}
PathObject::PathPart& PathObject::findPartForIndex(int coords_index)
{
	int num_parts = (int)parts.size();
	for (int i = 0; i < num_parts; ++i)
	{
		if (coords_index >= parts[i].start_index && coords_index <= parts[i].end_index)
			return parts[i];
	}
	assert(false);
	return parts[0];
}
int PathObject::findPartIndexForIndex(int coords_index)
{
	int num_parts = (int)parts.size();
	for (int i = 0; i < num_parts; ++i)
	{
		if (coords_index >= parts[i].start_index && coords_index <= parts[i].end_index)
			return i;
	}
	assert(false);
	return 0;
}
void PathObject::deletePart(int part_index)
{
	coords.erase(coords.begin() + parts[part_index].start_index, coords.begin() + (parts[part_index].end_index + 1));
	int num_part_coords = parts[part_index].end_index - parts[part_index].start_index + 1;
	parts.erase(parts.begin() + part_index);
	for (int i = part_index; i < (int)parts.size(); ++i)
	{
		parts[i].start_index -= num_part_coords;
		parts[i].end_index -= num_part_coords;
	}
}
void PathObject::partSizeChanged(int part_index, int change)
{
	parts[part_index].end_index += change;
	for (int i = part_index + 1; i < (int)parts.size(); ++i)
	{
		parts[i].start_index += change;
		parts[i].end_index += change;
	}
}

void PathObject::calcClosestPointOnPath(MapCoordF coord, float& out_distance_sq, PathCoord& out_path_coord, int part_index)
{
	update(false);
	
	int coords_size = (int)coords.size();
	if (coords_size == 0)
	{
		out_distance_sq = -1;
		return;	
	}
	else if (coords_size == 1)
	{
		out_distance_sq = coord.lengthToSquared(MapCoordF(coords[0]));
		out_path_coord.clen = 0;
		out_path_coord.index = 0;
		out_path_coord.param = 0;
		out_path_coord.pos = MapCoordF(coords[0]);
		return;
	}
	
	out_distance_sq = 999999;
	int start = 0, end = (int)path_coords.size();
	if (part_index >= 0 && part_index < (int)parts.size())
	{
		start = parts[part_index].path_coord_start_index;
		end = parts[part_index].path_coord_end_index + 1;
	}
	for (int i = start; i < end - 1; ++i)
	{
		assert(path_coords[i].index < coords_size);
		if (coords[path_coords[i].index].isHolePoint())
			continue;
		
		MapCoordF to_coord = MapCoordF(coord.getX() - path_coords[i].pos.getX(), coord.getY() - path_coords[i].pos.getY());
		MapCoordF to_next = MapCoordF(path_coords[i+1].pos.getX() - path_coords[i].pos.getX(), path_coords[i+1].pos.getY() - path_coords[i].pos.getY());
		MapCoordF tangent = to_next;
		tangent.normalize();
		
		float dist_along_line = to_coord.dot(tangent);
		if (dist_along_line <= 0)
		{
			if (to_coord.lengthSquared() < out_distance_sq)
			{
				out_distance_sq = to_coord.lengthSquared();
				out_path_coord = path_coords[i];
			}
			continue;
		}
		
		float line_length = path_coords[i+1].clen - path_coords[i].clen;
		if (dist_along_line >= line_length)
		{
			if (coord.lengthToSquared(path_coords[i+1].pos) < out_distance_sq)
			{
				out_distance_sq = coord.lengthToSquared(path_coords[i+1].pos);
				out_path_coord = path_coords[i+1];
			}
			continue;
		}
		
		MapCoordF right = tangent;
		right.perpRight();
		
		float dist_from_line = qAbs(right.dot(to_coord));
		if (dist_from_line*dist_from_line < out_distance_sq)
		{
			float factor = (dist_along_line / line_length);
			out_distance_sq = dist_from_line*dist_from_line;
			out_path_coord.clen = path_coords[i].clen + dist_along_line;
			out_path_coord.index = path_coords[i+1].index;
			if (path_coords[i+1].index == path_coords[i].index)
				out_path_coord.param = path_coords[i].param + (path_coords[i+1].param - path_coords[i].param) * factor;
			else
				out_path_coord.param = path_coords[i+1].param * factor;
			if (coords[out_path_coord.index].isCurveStart())
			{
				MapCoordF o0, o1, o3, o4;
				PathCoord::splitBezierCurve(MapCoordF(coords[out_path_coord.index]), MapCoordF(coords[out_path_coord.index+1]),
											MapCoordF(coords[out_path_coord.index+2]), MapCoordF(coords[out_path_coord.index+3]),
											out_path_coord.param, o0, o1, out_path_coord.pos, o3, o4);
			}
			else
			{
				out_path_coord.pos = MapCoordF(path_coords[i].pos.getX() + (path_coords[i+1].pos.getX() - path_coords[i].pos.getX()) * factor,
											   path_coords[i].pos.getY() + (path_coords[i+1].pos.getY() - path_coords[i].pos.getY()) * factor);
			}
		}
	}
}

void PathObject::calcClosestCoordinate(MapCoordF coord, float& out_distance_sq, int& out_index)
{
	update(false);
	
	int coords_size = (int)coords.size();
	if (coords_size == 0)
	{
		out_distance_sq = -1;
		out_index = -1;
		return;	
	}
	
	// NOTE: do not try to optimize this by starting with index 1, it will overlook curve starts this way
	out_distance_sq = 999999;
	out_index = 0;
	for (int i = 0; i < coords_size; ++i)
	{
		double length_sq = (coord - MapCoordF(coords[i])).lengthSquared();
		if (length_sq < out_distance_sq)
		{
			out_distance_sq = length_sq;
			out_index = i;
		}
		
		if (coords[i].isCurveStart())
			i += 2;
	}
}

int PathObject::subdivide(int index, float param)
{
	update(false);
	
	if (coords[index].isCurveStart())
	{
		MapCoordF o0, o1, o2, o3, o4;
		PathCoord::splitBezierCurve(MapCoordF(coords[index]), MapCoordF(coords[index+1]),
									MapCoordF(coords[index+2]), MapCoordF(coords[index+3]),
									param, o0, o1, o2, o3, o4);
		coords[index + 1] = o0.toMapCoord();
		coords[index + 2] = o4.toMapCoord();
		addCoordinate(index + 2, o3.toMapCoord());
		MapCoord middle_coord = o2.toMapCoord();
		middle_coord.setCurveStart(true);
		addCoordinate(index + 2, middle_coord);
		addCoordinate(index + 2, o1.toMapCoord());
		return index + 3;
	}
	else
	{
		addCoordinate(index + 1, (MapCoordF(coords[index]) + (MapCoordF(coords[index+1]) - MapCoordF(coords[index])) * param).toMapCoord());
		return index + 1;
	}
}

bool PathObject::canBeConnected(PathObject* other, double connect_threshold_sq)
{
	int num_parts = getNumParts();
	for (int i = 0; i < num_parts; ++i)
	{
		if (parts[i].isClosed())
			continue;
		
		int num_other_parts = other->getNumParts();
		for (int k = 0; k < num_other_parts; ++k)
		{
			if (other->parts[k].isClosed())
				continue;
			
			if (coords[parts[i].start_index].lengthSquaredTo(other->coords[other->parts[k].start_index]) <= connect_threshold_sq)
				return true;
			else if (coords[parts[i].start_index].lengthSquaredTo(other->coords[other->parts[k].end_index]) <= connect_threshold_sq)
				return true;
			else if (coords[parts[i].end_index].lengthSquaredTo(other->coords[other->parts[k].start_index]) <= connect_threshold_sq)
				return true;
			else if (coords[parts[i].end_index].lengthSquaredTo(other->coords[other->parts[k].end_index]) <= connect_threshold_sq)
				return true;
		}
	}
	
	return false;
}
bool PathObject::connectIfClose(PathObject* other, double connect_threshold_sq)
{
	bool did_connect_path = false;
	
	int num_parts = getNumParts();
	int num_other_parts = other->getNumParts();
	std::vector<bool> other_parts;	// Which parts have not been connected to this part yet?
	other_parts.assign(num_other_parts, true);
	for (int i = 0; i < num_parts; ++i)
	{
		if (parts[i].isClosed())
			continue;
		
		for (int k = 0; k < num_other_parts; ++k)
		{
			if (!other_parts[k] || other->parts[k].isClosed())
				continue;
	
			if (coords[parts[i].start_index].lengthSquaredTo(other->coords[other->parts[k].start_index]) <= connect_threshold_sq)
			{
				other->reversePart(k);
				connectPathParts(i, other, k, true);
			}
			else if (coords[parts[i].start_index].lengthSquaredTo(other->coords[other->parts[k].end_index]) <= connect_threshold_sq)
				connectPathParts(i, other, k, true);
			else if (coords[parts[i].end_index].lengthSquaredTo(other->coords[other->parts[k].start_index]) <= connect_threshold_sq)
				connectPathParts(i, other, k, false);
			else if (coords[parts[i].end_index].lengthSquaredTo(other->coords[other->parts[k].end_index]) <= connect_threshold_sq)
			{
				other->reversePart(k);
				connectPathParts(i, other, k, false);
			}
			else
				continue;
			
			if (coords[parts[i].start_index].lengthSquaredTo(coords[parts[i].end_index]) <= connect_threshold_sq)
				parts[i].connectEnds();
			
			did_connect_path = true;
			
			other_parts[k] = false;
		}
	}
	
	if (did_connect_path)
	{
		// Copy over all remaining parts of the other object
		getCoordinate(getCoordinateCount() - 1).setHolePoint(true);
		for (int i = 0; i < num_other_parts; ++i)
		{
			if (other_parts[i])
				appendPathPart(other, i);
		}
		
		output_dirty = true;
	}
	
	return did_connect_path;
}
void PathObject::connectPathParts(int part_index, PathObject* other, int other_part_index, bool prepend)
{
	PathPart& part = parts[part_index];
	PathPart& other_part = other->parts[other_part_index];
	assert(!part.isClosed() && !other_part.isClosed());
	
	int other_part_size = other_part.getNumCoords();
	coords.resize(coords.size() + other_part_size - 1);
	
	if (prepend)
	{
		for (int i = (int)coords.size() - 1; i >= part.start_index + (other_part_size - 1); --i)
			coords[i] = coords[i - (other_part_size - 1)];
		
		MapCoord& join_coord = coords[part.start_index + other_part_size - 1];
		join_coord.setRawX((join_coord.rawX() + other->coords[other_part.end_index].rawX()) / 2);
		join_coord.setRawY((join_coord.rawY() + other->coords[other_part.end_index].rawY()) / 2);
		join_coord.setHolePoint(false);
		join_coord.setClosePoint(false);
		
		for (int i = part.start_index; i < part.start_index + other_part_size - 1; ++i)
			coords[i] = other->coords[i - part.start_index + other_part.start_index];
	}
	else
	{
		MapCoord coord = other->coords[other_part.start_index];	// take flags from first coord of path to append
		coord.setRawX((coords[part.end_index].rawX() + coord.rawX()) / 2);
		coord.setRawY((coords[part.end_index].rawY() + coord.rawY()) / 2);
		coords[part.end_index] = coord;
		
		for (int i = (int)coords.size() - 1; i > part.end_index + (other_part_size - 1); --i)
			coords[i] = coords[i - (other_part_size - 1)];
		for (int i = part.end_index + 1; i <= part.end_index + (other_part_size - 1); ++i)
			coords[i] = other->coords[i - part.end_index + other_part.start_index];
	}
	
	partSizeChanged(part_index, other_part_size - 1);
}

void PathObject::splitAt(const PathCoord& split_pos, Object*& out1, Object*& out2)
{
	out1 = NULL;
	out2 = NULL;
	
	int part_index = findPartIndexForIndex(split_pos.index);
	PathPart& part = parts[part_index];
	if (part.isClosed())
	{
		PathObject* path1 = new PathObject(symbol, coords, map);
		out1 = path1;
		
		if (split_pos.clen == path_coords[part.path_coord_start_index].clen || split_pos.clen == path_coords[part.path_coord_end_index].clen)
		{
			(path1->parts[part_index]).setClosed(false);
			return;
		}
		
		path1->changePathBounds(part_index, split_pos.clen, split_pos.clen);
		return;
	}
	
	if (split_pos.clen == path_coords[part.path_coord_start_index].clen)
	{
		if (part.calcNumRegularPoints() > 2)
		{
			PathObject* path1 = new PathObject(symbol, coords, map);
			out1 = path1;
			
			if (coords[part.start_index].isCurveStart())
			{
				path1->deleteCoordinate(part.start_index + 2, false);
				path1->deleteCoordinate(part.start_index + 1, false);
				path1->deleteCoordinate(part.start_index + 0, false);
			}
			else
				path1->deleteCoordinate(part.start_index + 0, false);
		}
	}
	else if (split_pos.clen == path_coords[part.path_coord_end_index].clen)
	{
		if (part.calcNumRegularPoints() > 2)
		{
			PathObject* path1 = new PathObject(symbol, coords, map);
			out1 = path1;
			
			if (part.getNumCoords() >= 4 && coords[part.end_index - 3].isCurveStart())
			{
				path1->deleteCoordinate(path1->parts[part_index].end_index, false);
				path1->deleteCoordinate(path1->parts[part_index].end_index, false);
				path1->deleteCoordinate(path1->parts[part_index].end_index, false);
				coords[path1->parts[part_index].end_index].setCurveStart(false);
			}
			else
				path1->deleteCoordinate(path1->parts[part_index].end_index, false);
		}
	}
	else
	{
		PathObject* path1 = new PathObject(symbol, coords, map);
		out1 = path1;
		PathObject* path2 = new PathObject(symbol, coords, map);
		out2 = path2;
		
		path1->changePathBounds(part_index, path_coords[part.path_coord_start_index].clen, split_pos.clen);
		path2->changePathBounds(part_index, split_pos.clen, path_coords[part.path_coord_end_index].clen);
	}
}
void PathObject::changePathBounds(int part_index, double start_len, double end_len)
{
	//if (start_len == end_len)
	//	return;
	
	update();
	PathPart& part = parts[part_index];
	int part_size = part.end_index - part.start_index + 1;
	
	MapCoordVector* p_coords = &coords;
	MapCoordVector temp_coords;
	if (getNumParts() > 1)
	{
		// TODO: optimize this HACK-ed case
		temp_coords.resize(part_size);
		for (int i = 0; i < part_size; ++i)
			temp_coords[i] = coords[part.start_index + i];
		
		p_coords = &temp_coords;
	}
	
	MapCoordVectorF coordsF;
	coordsF.resize(part_size);
	for (int i = 0; i < part_size; ++i)
		coordsF[i] = MapCoordF(coords[part.start_index + i]);
	
	MapCoordVector out_coords;
	out_coords.reserve(part_size + 2);
	MapCoordVectorF out_coordsF;
	out_coordsF.reserve(part_size + 2);
	
	if (end_len == 0 && part.isClosed())
		end_len = path_coords[part.path_coord_end_index].clen;
	
	int cur_path_coord = part.path_coord_start_index + 1;
	while (cur_path_coord < part.path_coord_end_index && path_coords[cur_path_coord].clen < start_len)
		++cur_path_coord;
	
	// Start position
	int start_bezier_index = -1;		// if the range starts at a bezier curve, this is the curve's index, otherwise -1
	float start_bezier_split_param = 0;	// the parameter value where the split of the curve for the range start was made
	MapCoordF o3, o4;					// temporary bezier control points
	if (p_coords->at(path_coords[cur_path_coord].index).isCurveStart())
	{
		int index = path_coords[cur_path_coord].index;
		float factor = (start_len - path_coords[cur_path_coord-1].clen) / (path_coords[cur_path_coord].clen - path_coords[cur_path_coord-1].clen);
		assert(factor >= -0.01f && factor <= 1.01f);
		if (factor > 1)
			factor = 1;
		else if (factor < 0)
			factor = 0;
		float prev_param = (path_coords[cur_path_coord-1].index == path_coords[cur_path_coord].index) ? path_coords[cur_path_coord-1].param : 0;
		assert(prev_param <= path_coords[cur_path_coord].param);
		float p = prev_param + (path_coords[cur_path_coord].param - prev_param) * factor;
		assert(p >= 0 && p <= 1);
		
		MapCoordF o0, o1;
		out_coordsF.push_back(MapCoordF(0, 0));
		PathCoord::splitBezierCurve(coordsF[index], coordsF[index+1], coordsF[index+2], (index < (int)coordsF.size() - 3) ? coordsF[index+3] : coordsF[0], p, o0, o1, out_coordsF[out_coordsF.size() - 1], o3, o4);
		MapCoord flag = out_coordsF[out_coordsF.size() - 1].toMapCoord();
		flag.setCurveStart(true);
		out_coords.push_back(flag);
		
		start_bezier_split_param = p;
		start_bezier_index = index;
	}
	else
	{
		out_coordsF.push_back(MapCoordF(0, 0));
		PathCoord::calculatePositionAt(*p_coords, coordsF, path_coords, start_len, cur_path_coord, &out_coordsF[out_coordsF.size() - 1], NULL);
		out_coords.push_back(out_coordsF[out_coordsF.size() - 1].toMapCoord());
	}
	
	int current_index = path_coords[cur_path_coord].index;
	if (start_len == path_coords[cur_path_coord].clen && path_coords[cur_path_coord].param == 1)
	{
		if (part.isClosed())
		{
			if (p_coords->at(current_index).isCurveStart())
				current_index += 3;
			else
				++current_index;
			if (current_index >= part.end_index)
				current_index = 0;
			out_coords[out_coords.size() - 1].setFlags(p_coords->at(current_index).getFlags());
			out_coords[out_coords.size() - 1].setHolePoint(false);
			out_coords[out_coords.size() - 1].setClosePoint(false);
			++cur_path_coord;
			if (cur_path_coord > part.path_coord_end_index || p_coords->at(path_coords[cur_path_coord].index - part.start_index).isClosePoint())
				cur_path_coord = part.path_coord_start_index + 1;
		}
	}
	else if (start_len == path_coords[cur_path_coord - 1].clen && path_coords[cur_path_coord - 1].param == 0)
	{
		out_coords[out_coords.size() - 1].setFlags(p_coords->at(current_index).getFlags());
		out_coords[out_coords.size() - 1].setHolePoint(false);
		out_coords[out_coords.size() - 1].setClosePoint(false);
	}
	
	// End position
	bool enforce_wrap = (end_len <= path_coords[cur_path_coord].clen && end_len <= start_len);
	bool advanced_current_index = advanceCoordinateRangeTo(*p_coords, coordsF, path_coords, cur_path_coord, current_index, end_len, enforce_wrap, start_bezier_index, out_coords, out_coordsF, o3, o4);
	if (current_index < part.end_index)
	{
		out_coordsF.push_back(MapCoordF(0, 0));
		if (p_coords->at(current_index).isCurveStart())
		{
			int index = path_coords[cur_path_coord].index - part.start_index;
			float factor = (end_len - path_coords[cur_path_coord-1].clen) / (path_coords[cur_path_coord].clen - path_coords[cur_path_coord-1].clen);
			assert(factor >= -0.01f && factor <= 1.01f);
			if (factor > 1)
				factor = 1;
			else if (factor < 0)
				factor = 0;
			float prev_param = (path_coords[cur_path_coord-1].index == path_coords[cur_path_coord].index) ? path_coords[cur_path_coord-1].param : 0;
			assert(prev_param <= path_coords[cur_path_coord].param);
			float p = prev_param + (path_coords[cur_path_coord].param - prev_param) * factor;
			assert(p >= 0 && p <= 1);
			
			out_coordsF.push_back(MapCoordF(0, 0));
			out_coordsF.push_back(MapCoordF(0, 0));
			MapCoordF unused, unused2;
			
			if (start_bezier_index == current_index && start_len <= end_len && !advanced_current_index)
			{
				// The dash end is in the same curve as the start, need to make a second split with the correct parameter
				p = (p - start_bezier_split_param) / (1 - start_bezier_split_param);
				assert(p >= 0 && p <= 1);
				
				PathCoord::splitBezierCurve(out_coordsF[out_coordsF.size() - 4], o3, o4, (index < (int)coordsF.size() - 3) ? coordsF[index+3] : coordsF[0],
											p, out_coordsF[out_coordsF.size() - 3], out_coordsF[out_coordsF.size() - 2],
											out_coordsF[out_coordsF.size() - 1], unused, unused2);
			}
			else
			{
				PathCoord::splitBezierCurve(coordsF[index], coordsF[index+1], coordsF[index+2], (index < (int)coordsF.size() - 3) ? coordsF[index+3] : coordsF[0],
											p, out_coordsF[out_coordsF.size() - 3], out_coordsF[out_coordsF.size() - 2],
											out_coordsF[out_coordsF.size() - 1], unused, unused2);
			}
			
			out_coords.push_back(out_coordsF[out_coordsF.size() - 3].toMapCoord());
			out_coords.push_back(out_coordsF[out_coordsF.size() - 2].toMapCoord());
			out_coords.push_back(out_coordsF[out_coordsF.size() - 1].toMapCoord());
		}
		else
		{
			PathCoord::calculatePositionAt(*p_coords, coordsF, path_coords, end_len, cur_path_coord, &out_coordsF[out_coordsF.size() - 1], NULL);
			out_coords.push_back(out_coordsF[out_coordsF.size() - 1].toMapCoord());
		}
	}
	
	if (end_len == path_coords[cur_path_coord].clen && path_coords[cur_path_coord].param == 0)
	{
		out_coords[out_coords.size() - 1].setFlags(p_coords->at(current_index).getFlags());
		out_coords[out_coords.size() - 1].setClosePoint(false);
	}
	if (end_len == path_coords[cur_path_coord].clen && path_coords[cur_path_coord].param == 1)
	{
		if (p_coords->at(current_index).isCurveStart())
			current_index += 3;
		else
			++current_index;
		out_coords[out_coords.size() - 1].setFlags(p_coords->at(current_index).getFlags());
		out_coords[out_coords.size() - 1].setClosePoint(false);
	}
	
	out_coords[out_coords.size() - 1].setCurveStart(false);
	out_coords[out_coords.size() - 1].setHolePoint(true);
	
	if ((int)out_coords.size() > part_size)
		coords.insert(coords.begin() + part.start_index, (int)out_coords.size() - part_size, MapCoord());
	else if ((int)out_coords.size() < part_size)
		coords.erase(coords.begin() + part.start_index, coords.begin() + (part.start_index + part_size - (int)out_coords.size()));
	
	for (int i = part.start_index; i < part.start_index + (int)out_coords.size(); ++i)
		coords[i] = out_coords[i - part.start_index];
	//partSizeChanged(part_index, out_coords.size() - part_size);
	recalculateParts();
	setOutputDirty();
}
bool PathObject::advanceCoordinateRangeTo(const MapCoordVector& flags, const MapCoordVectorF& coords, const PathCoordVector& path_coords, int& cur_path_coord, int& current_index, float cur_length,
										  bool enforce_wrap, int start_bezier_index, MapCoordVector& out_flags, MapCoordVectorF& out_coords, const MapCoordF& o3, const MapCoordF& o4)
{
	PathPart& part = findPartForIndex(path_coords[cur_path_coord].index);
	
	bool advanced_current_index = false;
	int path_coords_size = (int)path_coords.size();
	bool have_to_wrap = enforce_wrap;
	while (cur_length > path_coords[cur_path_coord].clen || cur_length < path_coords[cur_path_coord - 1].clen || have_to_wrap)
	{
		++cur_path_coord;
		if (cur_path_coord > part.path_coord_end_index || flags[path_coords[cur_path_coord].index - part.start_index].isClosePoint())
			cur_path_coord = part.path_coord_start_index + 1;
		assert(cur_path_coord < path_coords_size);
		
		int index = path_coords[cur_path_coord].index - part.start_index;
		
		if (index != current_index)
		{
			if (current_index == start_bezier_index && !(enforce_wrap && !have_to_wrap))
			{
				current_index += 3;
				assert(current_index <= part.end_index);
				if (flags[current_index].isClosePoint())
					current_index = 0;
				
				advanced_current_index = true;
				
				out_flags.push_back(o3.toMapCoord());
				out_coords.push_back(o3);
				out_flags.push_back(o4.toMapCoord());
				out_coords.push_back(o4);
				out_flags.push_back(flags[current_index]);
				out_flags[out_flags.size() - 1].setClosePoint(false);
				out_flags[out_flags.size() - 1].setHolePoint(false);
				out_coords.push_back(coords[current_index]);
				
				if (current_index == part.end_index)
				{
					current_index = 0;
					out_flags.push_back(flags[current_index]);
					out_flags[out_flags.size() - 1].setClosePoint(false);
					out_flags[out_flags.size() - 1].setHolePoint(false);
					out_coords.push_back(coords[current_index]);
				}
				assert(current_index == index);
			}
			else
			{
				//assert((!flags[current_index].isCurveStart() && current_index + 1 == line_coords[cur_line_coord].index) ||
				//       (flags[current_index].isCurveStart() && current_index + 3 == line_coords[cur_line_coord].index) ||
				//       (flags[current_index+1].isHolePoint() && current_index + 2 == line_coords[cur_line_coord].index));
				do
				{
					++current_index;
					assert(current_index <= part.end_index);
					if (flags[current_index].isClosePoint() || current_index == part.end_index)
					{
						if (out_coords[current_index] != out_coords[0])
						{
							out_flags.push_back(flags[current_index]);
							out_flags[out_flags.size() - 1].setClosePoint(false);
							out_flags[out_flags.size() - 1].setHolePoint(false);
							out_coords.push_back(coords[current_index]);
						}
						current_index = 0;
					}
					advanced_current_index = true;
					
					out_flags.push_back(flags[current_index]);
					out_flags[out_flags.size() - 1].setClosePoint(false);
					out_flags[out_flags.size() - 1].setHolePoint(false);
					out_coords.push_back(coords[current_index]);
				} while (current_index != index);
			}
			
			have_to_wrap = false;
		}
	}
	return advanced_current_index;
}

void PathObject::calcBezierPointDeletionRetainingShapeFactors(MapCoord p0, MapCoord p1, MapCoord p2, MapCoord q0, MapCoord q1, MapCoord q2, MapCoord q3, double& out_pfactor, double& out_qfactor)
{
	// Heuristic for the split parameter sp (zero to one)
	QBezierCopy p_curve = QBezierCopy::fromPoints(p0.toQPointF(), p1.toQPointF(), p2.toQPointF(), q0.toQPointF());
	double p_length = p_curve.length(PathCoord::bezier_error);
	QBezierCopy q_curve = QBezierCopy::fromPoints(q0.toQPointF(), q1.toQPointF(), q2.toQPointF(), q3.toQPointF());
	double q_length = q_curve.length(PathCoord::bezier_error);
	double sp = p_length / (p_length + q_length);
	
	// Least squares curve fitting with the constraint that handles are on the same line as before.
	// To reproduce the formulas, run the following Matlab script:
	/*
	 
	 clear all; close all; clc;
	 
	 syms P0x P1x P2x P3x;
	 syms Q1x Q2x Q3x;
	 syms P0y P1y P2y P3y;
	 syms Q1y Q2y Q3y;
	 
	 syms u v w;
	 syms pfactor qfactor;
	 
	 Xx = (1-u)^3*P0x+3*(1-u)^2*u*P1x+3*(1-u)*u^2*P2x+u^3*P3x;
	 Yx = (1-v)^3*P3x+3*(1-v)^2*v*Q1x+3*(1-v)*v^2*Q2x+v^3*Q3x;
	 Zx = (1-w)^3*P0x+3*(1-w)^2*w*(P0x+pfactor*(P1x-P0x))+3*(1-w)*w^2*(Q3x+qfactor*(Q2x-Q3x))+w^3*Q3x;
	 
	 Xy = (1-u)^3*P0y+3*(1-u)^2*u*P1y+3*(1-u)*u^2*P2y+u^3*P3y;
	 Yy = (1-v)^3*P3y+3*(1-v)^2*v*Q1y+3*(1-v)*v^2*Q2y+v^3*Q3y;
	 Zy = (1-w)^3*P0y+3*(1-w)^2*w*(P0y+pfactor*(P1y-P0y))+3*(1-w)*w^2*(Q3y+qfactor*(Q2y-Q3y))+w^3*Q3y;
	 
	 syms sp;   %split_param
	 E = int((Zx - subs(Xx, u, w/sp))^2 + (Zy - subs(Xy, u, w/sp))^2, w, 0, sp) + int((Zx - subs(Yx, v, (w - sp) / (1 - sp)))^2 + (Zy - subs(Yy, v, (w - sp) / (1 - sp)))^2, w, sp, 1);
	 
	 Epfactor = diff(E, pfactor);
	 Eqfactor = diff(E, qfactor);
	 S = solve(Epfactor, Eqfactor, pfactor, qfactor);
	 S = [S.pfactor, S.qfactor]
	 
	 */
	
	double P0x = p0.xd(), P1x = p1.xd(), P2x = p2.xd(), P3x = q0.xd();
	double Q1x = q1.xd(), Q2x = q2.xd(), Q3x = q3.xd();
	double P0y = p0.yd(), P1y = p1.yd(), P2y = p2.yd(), P3y = q0.yd();
	double Q1y = q1.yd(), Q2y = q2.yd(), Q3y = q3.yd();
	
	out_pfactor = -(126*P0x*pow(Q2x,3.0)*pow(sp,3.0) - 49*pow(P0x,2.0)*pow(Q3x,2.0) - 88*pow(P0x,2.0)*pow(Q2y,2.0) - 88*pow(P0y,2.0)*pow(Q2x,2.0) - 88*pow(P0x,2.0)*pow(Q3y,2.0) - 88*pow(P0y,2.0)*pow(Q3x,2.0) - 49*pow(P0y,2.0)*pow(Q2y,2.0) -
	49*pow(P0y,2.0)*pow(Q3y,2.0) - 49*pow(P0x,2.0)*pow(Q2x,2.0) + 21*P0x*pow(Q3x,3.0)*pow(sp,2.0) - 84*P0x*pow(Q2x,3.0)*pow(sp,4.0) + 14*P0x*pow(Q3x,3.0)*pow(sp,3.0) - 126*P1x*pow(Q2x,3.0)*pow(sp,3.0) -
	21*P1x*pow(Q3x,3.0)*pow(sp,2.0) - 21*P0x*pow(Q3x,3.0)*pow(sp,4.0) + 84*P1x*pow(Q2x,3.0)*pow(sp,4.0) - 14*P1x*pow(Q3x,3.0)*pow(sp,3.0) + 21*P1x*pow(Q3x,3.0)*pow(sp,4.0) + 126*P0y*pow(Q2y,3.0)*pow(sp,3.0) +
	21*P0y*pow(Q3y,3.0)*pow(sp,2.0) - 84*P0y*pow(Q2y,3.0)*pow(sp,4.0) + 14*P0y*pow(Q3y,3.0)*pow(sp,3.0) - 126*P1y*pow(Q2y,3.0)*pow(sp,3.0) - 21*P1y*pow(Q3y,3.0)*pow(sp,2.0) - 21*P0y*pow(Q3y,3.0)*pow(sp,4.0) +
	84*P1y*pow(Q2y,3.0)*pow(sp,4.0) - 14*P1y*pow(Q3y,3.0)*pow(sp,3.0) + 21*P1y*pow(Q3y,3.0)*pow(sp,4.0) + 84*pow(P0x,2.0)*pow(Q2x,2.0)*pow(sp,2.0) - 77*pow(P0x,2.0)*pow(Q2x,2.0)*pow(sp,3.0) +
	84*pow(P0x,2.0)*pow(Q3x,2.0)*pow(sp,2.0) - 168*pow(P1x,2.0)*pow(Q2x,2.0)*pow(sp,2.0) + 21*pow(P0x,2.0)*pow(Q2x,2.0)*pow(sp,4.0) - 77*pow(P0x,2.0)*pow(Q3x,2.0)*pow(sp,3.0) + 231*pow(P1x,2.0)*pow(Q2x,2.0)*pow(sp,3.0) -
	168*pow(P1x,2.0)*pow(Q3x,2.0)*pow(sp,2.0) + 21*pow(P0x,2.0)*pow(Q3x,2.0)*pow(sp,4.0) - 84*pow(P1x,2.0)*pow(Q2x,2.0)*pow(sp,4.0) + 231*pow(P1x,2.0)*pow(Q3x,2.0)*pow(sp,3.0) - 84*pow(P1x,2.0)*pow(Q3x,2.0)*pow(sp,4.0) +
	84*pow(P0x,2.0)*pow(Q2y,2.0)*pow(sp,2.0) + 84*pow(P0y,2.0)*pow(Q2x,2.0)*pow(sp,2.0) - 56*pow(P0x,2.0)*pow(Q2y,2.0)*pow(sp,3.0) + 84*pow(P0x,2.0)*pow(Q3y,2.0)*pow(sp,2.0) - 168*pow(P1x,2.0)*pow(Q2y,2.0)*pow(sp,2.0) -
	56*pow(P0y,2.0)*pow(Q2x,2.0)*pow(sp,3.0) + 84*pow(P0y,2.0)*pow(Q3x,2.0)*pow(sp,2.0) - 168*pow(P1y,2.0)*pow(Q2x,2.0)*pow(sp,2.0) + 12*pow(P0x,2.0)*pow(Q2y,2.0)*pow(sp,4.0) - 56*pow(P0x,2.0)*pow(Q3y,2.0)*pow(sp,3.0) +
	168*pow(P1x,2.0)*pow(Q2y,2.0)*pow(sp,3.0) - 168*pow(P1x,2.0)*pow(Q3y,2.0)*pow(sp,2.0) + 12*pow(P0y,2.0)*pow(Q2x,2.0)*pow(sp,4.0) - 56*pow(P0y,2.0)*pow(Q3x,2.0)*pow(sp,3.0) + 168*pow(P1y,2.0)*pow(Q2x,2.0)*pow(sp,3.0) -
	168*pow(P1y,2.0)*pow(Q3x,2.0)*pow(sp,2.0) + 12*pow(P0x,2.0)*pow(Q3y,2.0)*pow(sp,4.0) - 48*pow(P1x,2.0)*pow(Q2y,2.0)*pow(sp,4.0) + 168*pow(P1x,2.0)*pow(Q3y,2.0)*pow(sp,3.0) + 12*pow(P0y,2.0)*pow(Q3x,2.0)*pow(sp,4.0) -
	48*pow(P1y,2.0)*pow(Q2x,2.0)*pow(sp,4.0) + 168*pow(P1y,2.0)*pow(Q3x,2.0)*pow(sp,3.0) - 48*pow(P1x,2.0)*pow(Q3y,2.0)*pow(sp,4.0) - 48*pow(P1y,2.0)*pow(Q3x,2.0)*pow(sp,4.0) + 84*pow(P0y,2.0)*pow(Q2y,2.0)*pow(sp,2.0) -
	77*pow(P0y,2.0)*pow(Q2y,2.0)*pow(sp,3.0) + 84*pow(P0y,2.0)*pow(Q3y,2.0)*pow(sp,2.0) - 168*pow(P1y,2.0)*pow(Q2y,2.0)*pow(sp,2.0) + 21*pow(P0y,2.0)*pow(Q2y,2.0)*pow(sp,4.0) - 77*pow(P0y,2.0)*pow(Q3y,2.0)*pow(sp,3.0) +
	231*pow(P1y,2.0)*pow(Q2y,2.0)*pow(sp,3.0) - 168*pow(P1y,2.0)*pow(Q3y,2.0)*pow(sp,2.0) + 21*pow(P0y,2.0)*pow(Q3y,2.0)*pow(sp,4.0) - 84*pow(P1y,2.0)*pow(Q2y,2.0)*pow(sp,4.0) + 231*pow(P1y,2.0)*pow(Q3y,2.0)*pow(sp,3.0) -
	84*pow(P1y,2.0)*pow(Q3y,2.0)*pow(sp,4.0) + 49*P0x*P1x*pow(Q2x,2.0) + 49*P0x*P1x*pow(Q3x,2.0) + 28*P0x*P3x*pow(Q2x,2.0) + 28*P0x*P3x*pow(Q3x,2.0) - 28*P1x*P3x*pow(Q2x,2.0) - 28*P1x*P3x*pow(Q3x,2.0) + 88*P0x*P1x*pow(Q2y,2.0) +
	88*P0x*P1x*pow(Q3y,2.0) + 40*P0x*P3x*pow(Q2y,2.0) + 40*P0x*P3x*pow(Q3y,2.0) - 40*P1x*P3x*pow(Q2y,2.0) - 40*P1x*P3x*pow(Q3y,2.0) + 88*P0y*P1y*pow(Q2x,2.0) + 88*P0y*P1y*pow(Q3x,2.0) + 40*P0y*P3y*pow(Q2x,2.0) +
	40*P0y*P3y*pow(Q3x,2.0) - 40*P1y*P3y*pow(Q2x,2.0) - 40*P1y*P3y*pow(Q3x,2.0) + 49*P0y*P1y*pow(Q2y,2.0) + 49*P0y*P1y*pow(Q3y,2.0) + 28*P0y*P3y*pow(Q2y,2.0) + 28*P0y*P3y*pow(Q3y,2.0) - 28*P1y*P3y*pow(Q2y,2.0) -
	28*P1y*P3y*pow(Q3y,2.0) + 21*P0x*Q1x*pow(Q2x,2.0) + 21*P0x*Q1x*pow(Q3x,2.0) - 21*P1x*Q1x*pow(Q2x,2.0) - 21*P1x*Q1x*pow(Q3x,2.0) + 98*pow(P0x,2.0)*Q2x*Q3x + 48*P0x*Q1x*pow(Q2y,2.0) + 48*P0x*Q1x*pow(Q3y,2.0) -
	48*P1x*Q1x*pow(Q2y,2.0) - 48*P1x*Q1x*pow(Q3y,2.0) + 176*pow(P0y,2.0)*Q2x*Q3x + 48*P0y*pow(Q2x,2.0)*Q1y + 48*P0y*pow(Q3x,2.0)*Q1y - 48*P1y*pow(Q2x,2.0)*Q1y - 48*P1y*pow(Q3x,2.0)*Q1y + 176*pow(P0x,2.0)*Q2y*Q3y +
	21*P0y*Q1y*pow(Q2y,2.0) + 21*P0y*Q1y*pow(Q3y,2.0) - 21*P1y*Q1y*pow(Q2y,2.0) - 21*P1y*Q1y*pow(Q3y,2.0) + 98*pow(P0y,2.0)*Q2y*Q3y - 42*P0x*pow(Q2x,3.0)*sp + 42*P1x*pow(Q2x,3.0)*sp - 42*P0y*pow(Q2y,3.0)*sp +
	42*P1y*pow(Q2y,3.0)*sp + 84*P0x*P3x*pow(Q2x,2.0)*sp + 84*P0x*P3x*pow(Q3x,2.0)*sp - 84*P1x*P3x*pow(Q2x,2.0)*sp - 84*P1x*P3x*pow(Q3x,2.0)*sp + 120*P0x*P3x*pow(Q2y,2.0)*sp + 120*P0x*P3x*pow(Q3y,2.0)*sp -
	120*P1x*P3x*pow(Q2y,2.0)*sp - 120*P1x*P3x*pow(Q3y,2.0)*sp + 120*P0y*P3y*pow(Q2x,2.0)*sp + 120*P0y*P3y*pow(Q3x,2.0)*sp - 120*P1y*P3y*pow(Q2x,2.0)*sp - 120*P1y*P3y*pow(Q3x,2.0)*sp + 84*P0y*P3y*pow(Q2y,2.0)*sp +
	84*P0y*P3y*pow(Q3y,2.0)*sp - 84*P1y*P3y*pow(Q2y,2.0)*sp - 84*P1y*P3y*pow(Q3y,2.0)*sp - 42*P0x*Q1x*pow(Q2x,2.0)*sp - 42*P0x*Q1x*pow(Q3x,2.0)*sp + 42*P1x*Q1x*pow(Q2x,2.0)*sp - 42*P0x*Q2x*pow(Q3x,2.0)*sp +
	84*P0x*pow(Q2x,2.0)*Q3x*sp + 42*P1x*Q1x*pow(Q3x,2.0)*sp + 42*P1x*Q2x*pow(Q3x,2.0)*sp - 84*P1x*pow(Q2x,2.0)*Q3x*sp - 24*P0x*Q1x*pow(Q2y,2.0)*sp - 24*P0x*Q1x*pow(Q3y,2.0)*sp - 42*P0x*Q2x*pow(Q2y,2.0)*sp +
	24*P1x*Q1x*pow(Q2y,2.0)*sp - 96*P0x*Q2x*pow(Q3y,2.0)*sp - 54*P0x*Q3x*pow(Q2y,2.0)*sp + 24*P1x*Q1x*pow(Q3y,2.0)*sp + 42*P1x*Q2x*pow(Q2y,2.0)*sp + 96*P1x*Q2x*pow(Q3y,2.0)*sp + 54*P1x*Q3x*pow(Q2y,2.0)*sp -
	24*P0y*pow(Q2x,2.0)*Q1y*sp - 42*P0y*pow(Q2x,2.0)*Q2y*sp - 24*P0y*pow(Q3x,2.0)*Q1y*sp + 24*P1y*pow(Q2x,2.0)*Q1y*sp - 54*P0y*pow(Q2x,2.0)*Q3y*sp - 96*P0y*pow(Q3x,2.0)*Q2y*sp + 42*P1y*pow(Q2x,2.0)*Q2y*sp +
	24*P1y*pow(Q3x,2.0)*Q1y*sp + 54*P1y*pow(Q2x,2.0)*Q3y*sp + 96*P1y*pow(Q3x,2.0)*Q2y*sp - 42*P0y*Q1y*pow(Q2y,2.0)*sp - 42*P0y*Q1y*pow(Q3y,2.0)*sp + 42*P1y*Q1y*pow(Q2y,2.0)*sp - 42*P0y*Q2y*pow(Q3y,2.0)*sp +
	84*P0y*pow(Q2y,2.0)*Q3y*sp + 42*P1y*Q1y*pow(Q3y,2.0)*sp + 42*P1y*Q2y*pow(Q3y,2.0)*sp - 84*P1y*pow(Q2y,2.0)*Q3y*sp + 84*P0x*P1x*pow(Q2x,2.0)*pow(sp,2.0) - 154*P0x*P1x*pow(Q2x,2.0)*pow(sp,3.0) +
	84*P0x*P1x*pow(Q3x,2.0)*pow(sp,2.0) + 252*P0x*P2x*pow(Q2x,2.0)*pow(sp,2.0) + 63*P0x*P1x*pow(Q2x,2.0)*pow(sp,4.0) - 154*P0x*P1x*pow(Q3x,2.0)*pow(sp,3.0) - 462*P0x*P2x*pow(Q2x,2.0)*pow(sp,3.0) +
	252*P0x*P2x*pow(Q3x,2.0)*pow(sp,2.0) - 336*P0x*P3x*pow(Q2x,2.0)*pow(sp,2.0) - 252*P1x*P2x*pow(Q2x,2.0)*pow(sp,2.0) + 63*P0x*P1x*pow(Q3x,2.0)*pow(sp,4.0) + 210*P0x*P2x*pow(Q2x,2.0)*pow(sp,4.0) -
	462*P0x*P2x*pow(Q3x,2.0)*pow(sp,3.0) + 210*P0x*P3x*pow(Q2x,2.0)*pow(sp,3.0) - 336*P0x*P3x*pow(Q3x,2.0)*pow(sp,2.0) + 462*P1x*P2x*pow(Q2x,2.0)*pow(sp,3.0) - 252*P1x*P2x*pow(Q3x,2.0)*pow(sp,2.0) +
	336*P1x*P3x*pow(Q2x,2.0)*pow(sp,2.0) + 210*P0x*P2x*pow(Q3x,2.0)*pow(sp,4.0) + 210*P0x*P3x*pow(Q3x,2.0)*pow(sp,3.0) - 210*P1x*P2x*pow(Q2x,2.0)*pow(sp,4.0) + 462*P1x*P2x*pow(Q3x,2.0)*pow(sp,3.0) -
	210*P1x*P3x*pow(Q2x,2.0)*pow(sp,3.0) + 336*P1x*P3x*pow(Q3x,2.0)*pow(sp,2.0) - 210*P1x*P2x*pow(Q3x,2.0)*pow(sp,4.0) - 210*P1x*P3x*pow(Q3x,2.0)*pow(sp,3.0) + 84*P0x*P1x*pow(Q2y,2.0)*pow(sp,2.0) -
	112*P0x*P1x*pow(Q2y,2.0)*pow(sp,3.0) + 84*P0x*P1x*pow(Q3y,2.0)*pow(sp,2.0) + 252*P0x*P2x*pow(Q2y,2.0)*pow(sp,2.0) + 36*P0x*P1x*pow(Q2y,2.0)*pow(sp,4.0) - 112*P0x*P1x*pow(Q3y,2.0)*pow(sp,3.0) -
	336*P0x*P2x*pow(Q2y,2.0)*pow(sp,3.0) + 252*P0x*P2x*pow(Q3y,2.0)*pow(sp,2.0) - 264*P0x*P3x*pow(Q2y,2.0)*pow(sp,2.0) - 252*P1x*P2x*pow(Q2y,2.0)*pow(sp,2.0) + 36*P0x*P1x*pow(Q3y,2.0)*pow(sp,4.0) +
	120*P0x*P2x*pow(Q2y,2.0)*pow(sp,4.0) - 336*P0x*P2x*pow(Q3y,2.0)*pow(sp,3.0) + 120*P0x*P3x*pow(Q2y,2.0)*pow(sp,3.0) - 264*P0x*P3x*pow(Q3y,2.0)*pow(sp,2.0) + 336*P1x*P2x*pow(Q2y,2.0)*pow(sp,3.0) -
	252*P1x*P2x*pow(Q3y,2.0)*pow(sp,2.0) + 264*P1x*P3x*pow(Q2y,2.0)*pow(sp,2.0) + 120*P0x*P2x*pow(Q3y,2.0)*pow(sp,4.0) + 120*P0x*P3x*pow(Q3y,2.0)*pow(sp,3.0) - 120*P1x*P2x*pow(Q2y,2.0)*pow(sp,4.0) +
	336*P1x*P2x*pow(Q3y,2.0)*pow(sp,3.0) - 120*P1x*P3x*pow(Q2y,2.0)*pow(sp,3.0) + 264*P1x*P3x*pow(Q3y,2.0)*pow(sp,2.0) - 120*P1x*P2x*pow(Q3y,2.0)*pow(sp,4.0) - 120*P1x*P3x*pow(Q3y,2.0)*pow(sp,3.0) +
	84*P0y*P1y*pow(Q2x,2.0)*pow(sp,2.0) - 112*P0y*P1y*pow(Q2x,2.0)*pow(sp,3.0) + 84*P0y*P1y*pow(Q3x,2.0)*pow(sp,2.0) + 252*P0y*P2y*pow(Q2x,2.0)*pow(sp,2.0) + 36*P0y*P1y*pow(Q2x,2.0)*pow(sp,4.0) -
	112*P0y*P1y*pow(Q3x,2.0)*pow(sp,3.0) - 336*P0y*P2y*pow(Q2x,2.0)*pow(sp,3.0) + 252*P0y*P2y*pow(Q3x,2.0)*pow(sp,2.0) - 264*P0y*P3y*pow(Q2x,2.0)*pow(sp,2.0) - 252*P1y*P2y*pow(Q2x,2.0)*pow(sp,2.0) +
	36*P0y*P1y*pow(Q3x,2.0)*pow(sp,4.0) + 120*P0y*P2y*pow(Q2x,2.0)*pow(sp,4.0) - 336*P0y*P2y*pow(Q3x,2.0)*pow(sp,3.0) + 120*P0y*P3y*pow(Q2x,2.0)*pow(sp,3.0) - 264*P0y*P3y*pow(Q3x,2.0)*pow(sp,2.0) +
	336*P1y*P2y*pow(Q2x,2.0)*pow(sp,3.0) - 252*P1y*P2y*pow(Q3x,2.0)*pow(sp,2.0) + 264*P1y*P3y*pow(Q2x,2.0)*pow(sp,2.0) + 120*P0y*P2y*pow(Q3x,2.0)*pow(sp,4.0) + 120*P0y*P3y*pow(Q3x,2.0)*pow(sp,3.0) -
	120*P1y*P2y*pow(Q2x,2.0)*pow(sp,4.0) + 336*P1y*P2y*pow(Q3x,2.0)*pow(sp,3.0) - 120*P1y*P3y*pow(Q2x,2.0)*pow(sp,3.0) + 264*P1y*P3y*pow(Q3x,2.0)*pow(sp,2.0) - 120*P1y*P2y*pow(Q3x,2.0)*pow(sp,4.0) -
	120*P1y*P3y*pow(Q3x,2.0)*pow(sp,3.0) + 84*P0y*P1y*pow(Q2y,2.0)*pow(sp,2.0) - 154*P0y*P1y*pow(Q2y,2.0)*pow(sp,3.0) + 84*P0y*P1y*pow(Q3y,2.0)*pow(sp,2.0) + 252*P0y*P2y*pow(Q2y,2.0)*pow(sp,2.0) +
	63*P0y*P1y*pow(Q2y,2.0)*pow(sp,4.0) - 154*P0y*P1y*pow(Q3y,2.0)*pow(sp,3.0) - 462*P0y*P2y*pow(Q2y,2.0)*pow(sp,3.0) + 252*P0y*P2y*pow(Q3y,2.0)*pow(sp,2.0) - 336*P0y*P3y*pow(Q2y,2.0)*pow(sp,2.0) -
	252*P1y*P2y*pow(Q2y,2.0)*pow(sp,2.0) + 63*P0y*P1y*pow(Q3y,2.0)*pow(sp,4.0) + 210*P0y*P2y*pow(Q2y,2.0)*pow(sp,4.0) - 462*P0y*P2y*pow(Q3y,2.0)*pow(sp,3.0) + 210*P0y*P3y*pow(Q2y,2.0)*pow(sp,3.0) -
	336*P0y*P3y*pow(Q3y,2.0)*pow(sp,2.0) + 462*P1y*P2y*pow(Q2y,2.0)*pow(sp,3.0) - 252*P1y*P2y*pow(Q3y,2.0)*pow(sp,2.0) + 336*P1y*P3y*pow(Q2y,2.0)*pow(sp,2.0) + 210*P0y*P2y*pow(Q3y,2.0)*pow(sp,4.0) +
	210*P0y*P3y*pow(Q3y,2.0)*pow(sp,3.0) - 210*P1y*P2y*pow(Q2y,2.0)*pow(sp,4.0) + 462*P1y*P2y*pow(Q3y,2.0)*pow(sp,3.0) - 210*P1y*P3y*pow(Q2y,2.0)*pow(sp,3.0) + 336*P1y*P3y*pow(Q3y,2.0)*pow(sp,2.0) -
	210*P1y*P2y*pow(Q3y,2.0)*pow(sp,4.0) - 210*P1y*P3y*pow(Q3y,2.0)*pow(sp,3.0) - 189*P0x*Q1x*pow(Q2x,2.0)*pow(sp,2.0) + 420*P0x*Q1x*pow(Q2x,2.0)*pow(sp,3.0) - 189*P0x*Q1x*pow(Q3x,2.0)*pow(sp,2.0) +
	189*P1x*Q1x*pow(Q2x,2.0)*pow(sp,2.0) - 210*P0x*Q1x*pow(Q2x,2.0)*pow(sp,4.0) + 420*P0x*Q1x*pow(Q3x,2.0)*pow(sp,3.0) - 42*P0x*Q2x*pow(Q3x,2.0)*pow(sp,2.0) + 21*P0x*pow(Q2x,2.0)*Q3x*pow(sp,2.0) -
	420*P1x*Q1x*pow(Q2x,2.0)*pow(sp,3.0) + 189*P1x*Q1x*pow(Q3x,2.0)*pow(sp,2.0) - 168*pow(P0x,2.0)*Q2x*Q3x*pow(sp,2.0) - 210*P0x*Q1x*pow(Q3x,2.0)*pow(sp,4.0) + 98*P0x*Q2x*pow(Q3x,2.0)*pow(sp,3.0) -
	238*P0x*pow(Q2x,2.0)*Q3x*pow(sp,3.0) + 210*P1x*Q1x*pow(Q2x,2.0)*pow(sp,4.0) - 420*P1x*Q1x*pow(Q3x,2.0)*pow(sp,3.0) + 42*P1x*Q2x*pow(Q3x,2.0)*pow(sp,2.0) - 21*P1x*pow(Q2x,2.0)*Q3x*pow(sp,2.0) +
	154*pow(P0x,2.0)*Q2x*Q3x*pow(sp,3.0) + 336*pow(P1x,2.0)*Q2x*Q3x*pow(sp,2.0) - 42*P0x*Q2x*pow(Q3x,2.0)*pow(sp,4.0) + 147*P0x*pow(Q2x,2.0)*Q3x*pow(sp,4.0) + 210*P1x*Q1x*pow(Q3x,2.0)*pow(sp,4.0) -
	98*P1x*Q2x*pow(Q3x,2.0)*pow(sp,3.0) + 238*P1x*pow(Q2x,2.0)*Q3x*pow(sp,3.0) - 42*pow(P0x,2.0)*Q2x*Q3x*pow(sp,4.0) - 462*pow(P1x,2.0)*Q2x*Q3x*pow(sp,3.0) + 42*P1x*Q2x*pow(Q3x,2.0)*pow(sp,4.0) -
	147*P1x*pow(Q2x,2.0)*Q3x*pow(sp,4.0) + 168*pow(P1x,2.0)*Q2x*Q3x*pow(sp,4.0) - 216*P0x*Q1x*pow(Q2y,2.0)*pow(sp,2.0) + 312*P0x*Q1x*pow(Q2y,2.0)*pow(sp,3.0) - 216*P0x*Q1x*pow(Q3y,2.0)*pow(sp,2.0) +
	216*P1x*Q1x*pow(Q2y,2.0)*pow(sp,2.0) - 120*P0x*Q1x*pow(Q2y,2.0)*pow(sp,4.0) + 312*P0x*Q1x*pow(Q3y,2.0)*pow(sp,3.0) + 126*P0x*Q2x*pow(Q2y,2.0)*pow(sp,3.0) - 45*P0x*Q2x*pow(Q3y,2.0)*pow(sp,2.0) -
	24*P0x*Q3x*pow(Q2y,2.0)*pow(sp,2.0) - 312*P1x*Q1x*pow(Q2y,2.0)*pow(sp,3.0) + 216*P1x*Q1x*pow(Q3y,2.0)*pow(sp,2.0) - 168*pow(P0y,2.0)*Q2x*Q3x*pow(sp,2.0) - 120*P0x*Q1x*pow(Q3y,2.0)*pow(sp,4.0) -
	84*P0x*Q2x*pow(Q2y,2.0)*pow(sp,4.0) + 114*P0x*Q2x*pow(Q3y,2.0)*pow(sp,3.0) + 2*P0x*Q3x*pow(Q2y,2.0)*pow(sp,3.0) + 21*P0x*Q3x*pow(Q3y,2.0)*pow(sp,2.0) + 120*P1x*Q1x*pow(Q2y,2.0)*pow(sp,4.0) -
	312*P1x*Q1x*pow(Q3y,2.0)*pow(sp,3.0) - 126*P1x*Q2x*pow(Q2y,2.0)*pow(sp,3.0) + 45*P1x*Q2x*pow(Q3y,2.0)*pow(sp,2.0) + 24*P1x*Q3x*pow(Q2y,2.0)*pow(sp,2.0) + 112*pow(P0y,2.0)*Q2x*Q3x*pow(sp,3.0) +
	336*pow(P1y,2.0)*Q2x*Q3x*pow(sp,2.0) - 39*P0x*Q2x*pow(Q3y,2.0)*pow(sp,4.0) + 24*P0x*Q3x*pow(Q2y,2.0)*pow(sp,4.0) + 14*P0x*Q3x*pow(Q3y,2.0)*pow(sp,3.0) + 120*P1x*Q1x*pow(Q3y,2.0)*pow(sp,4.0) +
	84*P1x*Q2x*pow(Q2y,2.0)*pow(sp,4.0) - 114*P1x*Q2x*pow(Q3y,2.0)*pow(sp,3.0) - 2*P1x*Q3x*pow(Q2y,2.0)*pow(sp,3.0) - 21*P1x*Q3x*pow(Q3y,2.0)*pow(sp,2.0) - 24*pow(P0y,2.0)*Q2x*Q3x*pow(sp,4.0) -
	336*pow(P1y,2.0)*Q2x*Q3x*pow(sp,3.0) - 21*P0x*Q3x*pow(Q3y,2.0)*pow(sp,4.0) + 39*P1x*Q2x*pow(Q3y,2.0)*pow(sp,4.0) - 24*P1x*Q3x*pow(Q2y,2.0)*pow(sp,4.0) - 14*P1x*Q3x*pow(Q3y,2.0)*pow(sp,3.0) +
	96*pow(P1y,2.0)*Q2x*Q3x*pow(sp,4.0) + 21*P1x*Q3x*pow(Q3y,2.0)*pow(sp,4.0) - 216*P0y*pow(Q2x,2.0)*Q1y*pow(sp,2.0) + 312*P0y*pow(Q2x,2.0)*Q1y*pow(sp,3.0) - 216*P0y*pow(Q3x,2.0)*Q1y*pow(sp,2.0) +
	216*P1y*pow(Q2x,2.0)*Q1y*pow(sp,2.0) - 120*P0y*pow(Q2x,2.0)*Q1y*pow(sp,4.0) + 126*P0y*pow(Q2x,2.0)*Q2y*pow(sp,3.0) - 24*P0y*pow(Q2x,2.0)*Q3y*pow(sp,2.0) + 312*P0y*pow(Q3x,2.0)*Q1y*pow(sp,3.0) -
	45*P0y*pow(Q3x,2.0)*Q2y*pow(sp,2.0) - 312*P1y*pow(Q2x,2.0)*Q1y*pow(sp,3.0) + 216*P1y*pow(Q3x,2.0)*Q1y*pow(sp,2.0) - 168*pow(P0x,2.0)*Q2y*Q3y*pow(sp,2.0) - 84*P0y*pow(Q2x,2.0)*Q2y*pow(sp,4.0) +
	2*P0y*pow(Q2x,2.0)*Q3y*pow(sp,3.0) - 120*P0y*pow(Q3x,2.0)*Q1y*pow(sp,4.0) + 114*P0y*pow(Q3x,2.0)*Q2y*pow(sp,3.0) + 21*P0y*pow(Q3x,2.0)*Q3y*pow(sp,2.0) + 120*P1y*pow(Q2x,2.0)*Q1y*pow(sp,4.0) -
	126*P1y*pow(Q2x,2.0)*Q2y*pow(sp,3.0) + 24*P1y*pow(Q2x,2.0)*Q3y*pow(sp,2.0) - 312*P1y*pow(Q3x,2.0)*Q1y*pow(sp,3.0) + 45*P1y*pow(Q3x,2.0)*Q2y*pow(sp,2.0) + 112*pow(P0x,2.0)*Q2y*Q3y*pow(sp,3.0) +
	336*pow(P1x,2.0)*Q2y*Q3y*pow(sp,2.0) + 24*P0y*pow(Q2x,2.0)*Q3y*pow(sp,4.0) - 39*P0y*pow(Q3x,2.0)*Q2y*pow(sp,4.0) + 14*P0y*pow(Q3x,2.0)*Q3y*pow(sp,3.0) + 84*P1y*pow(Q2x,2.0)*Q2y*pow(sp,4.0) -
	2*P1y*pow(Q2x,2.0)*Q3y*pow(sp,3.0) + 120*P1y*pow(Q3x,2.0)*Q1y*pow(sp,4.0) - 114*P1y*pow(Q3x,2.0)*Q2y*pow(sp,3.0) - 21*P1y*pow(Q3x,2.0)*Q3y*pow(sp,2.0) - 24*pow(P0x,2.0)*Q2y*Q3y*pow(sp,4.0) -
	336*pow(P1x,2.0)*Q2y*Q3y*pow(sp,3.0) - 21*P0y*pow(Q3x,2.0)*Q3y*pow(sp,4.0) - 24*P1y*pow(Q2x,2.0)*Q3y*pow(sp,4.0) + 39*P1y*pow(Q3x,2.0)*Q2y*pow(sp,4.0) - 14*P1y*pow(Q3x,2.0)*Q3y*pow(sp,3.0) +
	96*pow(P1x,2.0)*Q2y*Q3y*pow(sp,4.0) + 21*P1y*pow(Q3x,2.0)*Q3y*pow(sp,4.0) - 189*P0y*Q1y*pow(Q2y,2.0)*pow(sp,2.0) + 420*P0y*Q1y*pow(Q2y,2.0)*pow(sp,3.0) - 189*P0y*Q1y*pow(Q3y,2.0)*pow(sp,2.0) +
	189*P1y*Q1y*pow(Q2y,2.0)*pow(sp,2.0) - 210*P0y*Q1y*pow(Q2y,2.0)*pow(sp,4.0) + 420*P0y*Q1y*pow(Q3y,2.0)*pow(sp,3.0) - 42*P0y*Q2y*pow(Q3y,2.0)*pow(sp,2.0) + 21*P0y*pow(Q2y,2.0)*Q3y*pow(sp,2.0) -
	420*P1y*Q1y*pow(Q2y,2.0)*pow(sp,3.0) + 189*P1y*Q1y*pow(Q3y,2.0)*pow(sp,2.0) - 168*pow(P0y,2.0)*Q2y*Q3y*pow(sp,2.0) - 210*P0y*Q1y*pow(Q3y,2.0)*pow(sp,4.0) + 98*P0y*Q2y*pow(Q3y,2.0)*pow(sp,3.0) -
	238*P0y*pow(Q2y,2.0)*Q3y*pow(sp,3.0) + 210*P1y*Q1y*pow(Q2y,2.0)*pow(sp,4.0) - 420*P1y*Q1y*pow(Q3y,2.0)*pow(sp,3.0) + 42*P1y*Q2y*pow(Q3y,2.0)*pow(sp,2.0) - 21*P1y*pow(Q2y,2.0)*Q3y*pow(sp,2.0) +
	154*pow(P0y,2.0)*Q2y*Q3y*pow(sp,3.0) + 336*pow(P1y,2.0)*Q2y*Q3y*pow(sp,2.0) - 42*P0y*Q2y*pow(Q3y,2.0)*pow(sp,4.0) + 147*P0y*pow(Q2y,2.0)*Q3y*pow(sp,4.0) + 210*P1y*Q1y*pow(Q3y,2.0)*pow(sp,4.0) -
	98*P1y*Q2y*pow(Q3y,2.0)*pow(sp,3.0) + 238*P1y*pow(Q2y,2.0)*Q3y*pow(sp,3.0) - 42*pow(P0y,2.0)*Q2y*Q3y*pow(sp,4.0) - 462*pow(P1y,2.0)*Q2y*Q3y*pow(sp,3.0) + 42*P1y*Q2y*pow(Q3y,2.0)*pow(sp,4.0) -
	147*P1y*pow(Q2y,2.0)*Q3y*pow(sp,4.0) + 168*pow(P1y,2.0)*Q2y*Q3y*pow(sp,4.0) - 98*P0x*P1x*Q2x*Q3x - 56*P0x*P3x*Q2x*Q3x + 56*P1x*P3x*Q2x*Q3x + 78*P0x*P0y*Q2x*Q2y - 78*P0x*P0y*Q2x*Q3y - 78*P0x*P0y*Q3x*Q2y -
	39*P0x*P1y*Q2x*Q2y - 39*P1x*P0y*Q2x*Q2y - 176*P0x*P1x*Q2y*Q3y + 78*P0x*P0y*Q3x*Q3y + 39*P0x*P1y*Q2x*Q3y + 39*P0x*P1y*Q3x*Q2y + 39*P1x*P0y*Q2x*Q3y + 39*P1x*P0y*Q3x*Q2y - 176*P0y*P1y*Q2x*Q3x -
	39*P0x*P1y*Q3x*Q3y - 12*P0x*P3y*Q2x*Q2y - 39*P1x*P0y*Q3x*Q3y - 12*P3x*P0y*Q2x*Q2y - 80*P0x*P3x*Q2y*Q3y + 12*P0x*P3y*Q2x*Q3y + 12*P0x*P3y*Q3x*Q2y + 12*P1x*P3y*Q2x*Q2y + 12*P3x*P0y*Q2x*Q3y +
	12*P3x*P0y*Q3x*Q2y + 12*P3x*P1y*Q2x*Q2y - 80*P0y*P3y*Q2x*Q3x - 12*P0x*P3y*Q3x*Q3y + 80*P1x*P3x*Q2y*Q3y - 12*P1x*P3y*Q2x*Q3y - 12*P1x*P3y*Q3x*Q2y - 12*P3x*P0y*Q3x*Q3y - 12*P3x*P1y*Q2x*Q3y -
	12*P3x*P1y*Q3x*Q2y + 80*P1y*P3y*Q2x*Q3x + 12*P1x*P3y*Q3x*Q3y + 12*P3x*P1y*Q3x*Q3y - 98*P0y*P1y*Q2y*Q3y - 56*P0y*P3y*Q2y*Q3y + 56*P1y*P3y*Q2y*Q3y - 42*P0x*Q1x*Q2x*Q3x + 42*P1x*Q1x*Q2x*Q3x - 27*P0x*Q2x*Q1y*Q2y -
	27*P0y*Q1x*Q2x*Q2y - 96*P0x*Q1x*Q2y*Q3y + 27*P0x*Q2x*Q1y*Q3y + 27*P0x*Q3x*Q1y*Q2y + 27*P1x*Q2x*Q1y*Q2y + 27*P0y*Q1x*Q2x*Q3y + 27*P0y*Q1x*Q3x*Q2y - 96*P0y*Q2x*Q3x*Q1y + 27*P1y*Q1x*Q2x*Q2y - 27*P0x*Q3x*Q1y*Q3y +
	96*P1x*Q1x*Q2y*Q3y - 27*P1x*Q2x*Q1y*Q3y - 27*P1x*Q3x*Q1y*Q2y - 27*P0y*Q1x*Q3x*Q3y - 27*P1y*Q1x*Q2x*Q3y - 27*P1y*Q1x*Q3x*Q2y + 96*P1y*Q2x*Q3x*Q1y + 27*P1x*Q3x*Q1y*Q3y + 27*P1y*Q1x*Q3x*Q3y - 42*P0y*Q1y*Q2y*Q3y +
	42*P1y*Q1y*Q2y*Q3y - 168*P0x*P3x*Q2x*Q3x*sp + 168*P1x*P3x*Q2x*Q3x*sp - 36*P0x*P3y*Q2x*Q2y*sp - 36*P3x*P0y*Q2x*Q2y*sp - 240*P0x*P3x*Q2y*Q3y*sp + 36*P0x*P3y*Q2x*Q3y*sp + 36*P0x*P3y*Q3x*Q2y*sp + 36*P1x*P3y*Q2x*Q2y*sp +
	36*P3x*P0y*Q2x*Q3y*sp + 36*P3x*P0y*Q3x*Q2y*sp + 36*P3x*P1y*Q2x*Q2y*sp - 240*P0y*P3y*Q2x*Q3x*sp - 36*P0x*P3y*Q3x*Q3y*sp + 240*P1x*P3x*Q2y*Q3y*sp - 36*P1x*P3y*Q2x*Q3y*sp - 36*P1x*P3y*Q3x*Q2y*sp - 36*P3x*P0y*Q3x*Q3y*sp -
	36*P3x*P1y*Q2x*Q3y*sp - 36*P3x*P1y*Q3x*Q2y*sp + 240*P1y*P3y*Q2x*Q3x*sp + 36*P1x*P3y*Q3x*Q3y*sp + 36*P3x*P1y*Q3x*Q3y*sp - 168*P0y*P3y*Q2y*Q3y*sp + 168*P1y*P3y*Q2y*Q3y*sp + 84*P0x*Q1x*Q2x*Q3x*sp - 84*P1x*Q1x*Q2x*Q3x*sp -
	18*P0x*Q2x*Q1y*Q2y*sp - 18*P0y*Q1x*Q2x*Q2y*sp + 48*P0x*Q1x*Q2y*Q3y*sp + 18*P0x*Q2x*Q1y*Q3y*sp + 18*P0x*Q3x*Q1y*Q2y*sp + 18*P1x*Q2x*Q1y*Q2y*sp + 18*P0y*Q1x*Q2x*Q3y*sp + 18*P0y*Q1x*Q3x*Q2y*sp + 48*P0y*Q2x*Q3x*Q1y*sp +
	18*P1y*Q1x*Q2x*Q2y*sp + 138*P0x*Q2x*Q2y*Q3y*sp - 18*P0x*Q3x*Q1y*Q3y*sp - 48*P1x*Q1x*Q2y*Q3y*sp - 18*P1x*Q2x*Q1y*Q3y*sp - 18*P1x*Q3x*Q1y*Q2y*sp - 18*P0y*Q1x*Q3x*Q3y*sp + 138*P0y*Q2x*Q3x*Q2y*sp - 18*P1y*Q1x*Q2x*Q3y*sp -
	18*P1y*Q1x*Q3x*Q2y*sp - 48*P1y*Q2x*Q3x*Q1y*sp + 54*P0x*Q3x*Q2y*Q3y*sp - 138*P1x*Q2x*Q2y*Q3y*sp + 18*P1x*Q3x*Q1y*Q3y*sp + 54*P0y*Q2x*Q3x*Q3y*sp + 18*P1y*Q1x*Q3x*Q3y*sp - 138*P1y*Q2x*Q3x*Q2y*sp - 54*P1x*Q3x*Q2y*Q3y*sp -
	54*P1y*Q2x*Q3x*Q3y*sp + 84*P0y*Q1y*Q2y*Q3y*sp - 84*P1y*Q1y*Q2y*Q3y*sp - 168*P0x*P1x*Q2x*Q3x*pow(sp,2.0) + 308*P0x*P1x*Q2x*Q3x*pow(sp,3.0) - 504*P0x*P2x*Q2x*Q3x*pow(sp,2.0) - 126*P0x*P1x*Q2x*Q3x*pow(sp,4.0) +
	924*P0x*P2x*Q2x*Q3x*pow(sp,3.0) + 672*P0x*P3x*Q2x*Q3x*pow(sp,2.0) + 504*P1x*P2x*Q2x*Q3x*pow(sp,2.0) - 420*P0x*P2x*Q2x*Q3x*pow(sp,4.0) - 420*P0x*P3x*Q2x*Q3x*pow(sp,3.0) - 924*P1x*P2x*Q2x*Q3x*pow(sp,3.0) -
	672*P1x*P3x*Q2x*Q3x*pow(sp,2.0) + 420*P1x*P2x*Q2x*Q3x*pow(sp,4.0) + 420*P1x*P3x*Q2x*Q3x*pow(sp,3.0) - 42*P0x*P0y*Q2x*Q2y*pow(sp,3.0) - 168*P0x*P1x*Q2y*Q3y*pow(sp,2.0) + 18*P0x*P0y*Q2x*Q2y*pow(sp,4.0) +
	42*P0x*P0y*Q2x*Q3y*pow(sp,3.0) + 42*P0x*P0y*Q3x*Q2y*pow(sp,3.0) - 42*P0x*P1y*Q2x*Q2y*pow(sp,3.0) - 42*P1x*P0y*Q2x*Q2y*pow(sp,3.0) - 168*P0y*P1y*Q2x*Q3x*pow(sp,2.0) + 224*P0x*P1x*Q2y*Q3y*pow(sp,3.0) -
	504*P0x*P2x*Q2y*Q3y*pow(sp,2.0) - 18*P0x*P0y*Q2x*Q3y*pow(sp,4.0) - 18*P0x*P0y*Q3x*Q2y*pow(sp,4.0) - 42*P0x*P0y*Q3x*Q3y*pow(sp,3.0) + 27*P0x*P1y*Q2x*Q2y*pow(sp,4.0) + 42*P0x*P1y*Q2x*Q3y*pow(sp,3.0) +
	42*P0x*P1y*Q3x*Q2y*pow(sp,3.0) - 126*P0x*P2y*Q2x*Q2y*pow(sp,3.0) - 72*P0x*P3y*Q2x*Q2y*pow(sp,2.0) + 27*P1x*P0y*Q2x*Q2y*pow(sp,4.0) + 42*P1x*P0y*Q2x*Q3y*pow(sp,3.0) + 42*P1x*P0y*Q3x*Q2y*pow(sp,3.0) +
	126*P1x*P1y*Q2x*Q2y*pow(sp,3.0) - 126*P2x*P0y*Q2x*Q2y*pow(sp,3.0) - 72*P3x*P0y*Q2x*Q2y*pow(sp,2.0) + 224*P0y*P1y*Q2x*Q3x*pow(sp,3.0) - 504*P0y*P2y*Q2x*Q3x*pow(sp,2.0) - 72*P0x*P1x*Q2y*Q3y*pow(sp,4.0) +
	672*P0x*P2x*Q2y*Q3y*pow(sp,3.0) + 528*P0x*P3x*Q2y*Q3y*pow(sp,2.0) + 18*P0x*P0y*Q3x*Q3y*pow(sp,4.0) - 27*P0x*P1y*Q2x*Q3y*pow(sp,4.0) - 27*P0x*P1y*Q3x*Q2y*pow(sp,4.0) - 42*P0x*P1y*Q3x*Q3y*pow(sp,3.0) +
	90*P0x*P2y*Q2x*Q2y*pow(sp,4.0) + 126*P0x*P2y*Q2x*Q3y*pow(sp,3.0) + 126*P0x*P2y*Q3x*Q2y*pow(sp,3.0) + 90*P0x*P3y*Q2x*Q2y*pow(sp,3.0) + 72*P0x*P3y*Q2x*Q3y*pow(sp,2.0) + 72*P0x*P3y*Q3x*Q2y*pow(sp,2.0) +
	504*P1x*P2x*Q2y*Q3y*pow(sp,2.0) - 27*P1x*P0y*Q2x*Q3y*pow(sp,4.0) - 27*P1x*P0y*Q3x*Q2y*pow(sp,4.0) - 42*P1x*P0y*Q3x*Q3y*pow(sp,3.0) - 72*P1x*P1y*Q2x*Q2y*pow(sp,4.0) - 126*P1x*P1y*Q2x*Q3y*pow(sp,3.0) -
	126*P1x*P1y*Q3x*Q2y*pow(sp,3.0) + 126*P1x*P2y*Q2x*Q2y*pow(sp,3.0) + 72*P1x*P3y*Q2x*Q2y*pow(sp,2.0) + 90*P2x*P0y*Q2x*Q2y*pow(sp,4.0) + 126*P2x*P0y*Q2x*Q3y*pow(sp,3.0) + 126*P2x*P0y*Q3x*Q2y*pow(sp,3.0) +
	126*P2x*P1y*Q2x*Q2y*pow(sp,3.0) + 90*P3x*P0y*Q2x*Q2y*pow(sp,3.0) + 72*P3x*P0y*Q2x*Q3y*pow(sp,2.0) + 72*P3x*P0y*Q3x*Q2y*pow(sp,2.0) + 72*P3x*P1y*Q2x*Q2y*pow(sp,2.0) - 72*P0y*P1y*Q2x*Q3x*pow(sp,4.0) +
	672*P0y*P2y*Q2x*Q3x*pow(sp,3.0) + 528*P0y*P3y*Q2x*Q3x*pow(sp,2.0) + 504*P1y*P2y*Q2x*Q3x*pow(sp,2.0) - 240*P0x*P2x*Q2y*Q3y*pow(sp,4.0) - 240*P0x*P3x*Q2y*Q3y*pow(sp,3.0) + 27*P0x*P1y*Q3x*Q3y*pow(sp,4.0) -
	90*P0x*P2y*Q2x*Q3y*pow(sp,4.0) - 90*P0x*P2y*Q3x*Q2y*pow(sp,4.0) - 126*P0x*P2y*Q3x*Q3y*pow(sp,3.0) - 90*P0x*P3y*Q2x*Q3y*pow(sp,3.0) - 90*P0x*P3y*Q3x*Q2y*pow(sp,3.0) - 72*P0x*P3y*Q3x*Q3y*pow(sp,2.0) -
	672*P1x*P2x*Q2y*Q3y*pow(sp,3.0) - 528*P1x*P3x*Q2y*Q3y*pow(sp,2.0) + 27*P1x*P0y*Q3x*Q3y*pow(sp,4.0) + 72*P1x*P1y*Q2x*Q3y*pow(sp,4.0) + 72*P1x*P1y*Q3x*Q2y*pow(sp,4.0) + 126*P1x*P1y*Q3x*Q3y*pow(sp,3.0) -
	90*P1x*P2y*Q2x*Q2y*pow(sp,4.0) - 126*P1x*P2y*Q2x*Q3y*pow(sp,3.0) - 126*P1x*P2y*Q3x*Q2y*pow(sp,3.0) - 90*P1x*P3y*Q2x*Q2y*pow(sp,3.0) - 72*P1x*P3y*Q2x*Q3y*pow(sp,2.0) - 72*P1x*P3y*Q3x*Q2y*pow(sp,2.0) -
	90*P2x*P0y*Q2x*Q3y*pow(sp,4.0) - 90*P2x*P0y*Q3x*Q2y*pow(sp,4.0) - 126*P2x*P0y*Q3x*Q3y*pow(sp,3.0) - 90*P2x*P1y*Q2x*Q2y*pow(sp,4.0) - 126*P2x*P1y*Q2x*Q3y*pow(sp,3.0) - 126*P2x*P1y*Q3x*Q2y*pow(sp,3.0) -
	90*P3x*P0y*Q2x*Q3y*pow(sp,3.0) - 90*P3x*P0y*Q3x*Q2y*pow(sp,3.0) - 72*P3x*P0y*Q3x*Q3y*pow(sp,2.0) - 90*P3x*P1y*Q2x*Q2y*pow(sp,3.0) - 72*P3x*P1y*Q2x*Q3y*pow(sp,2.0) - 72*P3x*P1y*Q3x*Q2y*pow(sp,2.0) -
	240*P0y*P2y*Q2x*Q3x*pow(sp,4.0) - 240*P0y*P3y*Q2x*Q3x*pow(sp,3.0) - 672*P1y*P2y*Q2x*Q3x*pow(sp,3.0) - 528*P1y*P3y*Q2x*Q3x*pow(sp,2.0) + 90*P0x*P2y*Q3x*Q3y*pow(sp,4.0) + 90*P0x*P3y*Q3x*Q3y*pow(sp,3.0) +
	240*P1x*P2x*Q2y*Q3y*pow(sp,4.0) + 240*P1x*P3x*Q2y*Q3y*pow(sp,3.0) - 72*P1x*P1y*Q3x*Q3y*pow(sp,4.0) + 90*P1x*P2y*Q2x*Q3y*pow(sp,4.0) + 90*P1x*P2y*Q3x*Q2y*pow(sp,4.0) + 126*P1x*P2y*Q3x*Q3y*pow(sp,3.0) +
	90*P1x*P3y*Q2x*Q3y*pow(sp,3.0) + 90*P1x*P3y*Q3x*Q2y*pow(sp,3.0) + 72*P1x*P3y*Q3x*Q3y*pow(sp,2.0) + 90*P2x*P0y*Q3x*Q3y*pow(sp,4.0) + 90*P2x*P1y*Q2x*Q3y*pow(sp,4.0) + 90*P2x*P1y*Q3x*Q2y*pow(sp,4.0) +
	126*P2x*P1y*Q3x*Q3y*pow(sp,3.0) + 90*P3x*P0y*Q3x*Q3y*pow(sp,3.0) + 90*P3x*P1y*Q2x*Q3y*pow(sp,3.0) + 90*P3x*P1y*Q3x*Q2y*pow(sp,3.0) + 72*P3x*P1y*Q3x*Q3y*pow(sp,2.0) + 240*P1y*P2y*Q2x*Q3x*pow(sp,4.0) +
	240*P1y*P3y*Q2x*Q3x*pow(sp,3.0) - 90*P1x*P2y*Q3x*Q3y*pow(sp,4.0) - 90*P1x*P3y*Q3x*Q3y*pow(sp,3.0) - 90*P2x*P1y*Q3x*Q3y*pow(sp,4.0) - 90*P3x*P1y*Q3x*Q3y*pow(sp,3.0) - 168*P0y*P1y*Q2y*Q3y*pow(sp,2.0) +
	308*P0y*P1y*Q2y*Q3y*pow(sp,3.0) - 504*P0y*P2y*Q2y*Q3y*pow(sp,2.0) - 126*P0y*P1y*Q2y*Q3y*pow(sp,4.0) + 924*P0y*P2y*Q2y*Q3y*pow(sp,3.0) + 672*P0y*P3y*Q2y*Q3y*pow(sp,2.0) + 504*P1y*P2y*Q2y*Q3y*pow(sp,2.0) -
	420*P0y*P2y*Q2y*Q3y*pow(sp,4.0) - 420*P0y*P3y*Q2y*Q3y*pow(sp,3.0) - 924*P1y*P2y*Q2y*Q3y*pow(sp,3.0) - 672*P1y*P3y*Q2y*Q3y*pow(sp,2.0) + 420*P1y*P2y*Q2y*Q3y*pow(sp,4.0) + 420*P1y*P3y*Q2y*Q3y*pow(sp,3.0) +
	378*P0x*Q1x*Q2x*Q3x*pow(sp,2.0) - 840*P0x*Q1x*Q2x*Q3x*pow(sp,3.0) - 378*P1x*Q1x*Q2x*Q3x*pow(sp,2.0) + 420*P0x*Q1x*Q2x*Q3x*pow(sp,4.0) + 840*P1x*Q1x*Q2x*Q3x*pow(sp,3.0) - 420*P1x*Q1x*Q2x*Q3x*pow(sp,4.0) +
	27*P0x*Q2x*Q1y*Q2y*pow(sp,2.0) + 27*P0y*Q1x*Q2x*Q2y*pow(sp,2.0) + 432*P0x*Q1x*Q2y*Q3y*pow(sp,2.0) + 108*P0x*Q2x*Q1y*Q2y*pow(sp,3.0) - 27*P0x*Q2x*Q1y*Q3y*pow(sp,2.0) - 27*P0x*Q3x*Q1y*Q2y*pow(sp,2.0) -
	27*P1x*Q2x*Q1y*Q2y*pow(sp,2.0) + 108*P0y*Q1x*Q2x*Q2y*pow(sp,3.0) - 27*P0y*Q1x*Q2x*Q3y*pow(sp,2.0) - 27*P0y*Q1x*Q3x*Q2y*pow(sp,2.0) + 432*P0y*Q2x*Q3x*Q1y*pow(sp,2.0) - 27*P1y*Q1x*Q2x*Q2y*pow(sp,2.0) -
	624*P0x*Q1x*Q2y*Q3y*pow(sp,3.0) - 90*P0x*Q2x*Q1y*Q2y*pow(sp,4.0) - 108*P0x*Q2x*Q1y*Q3y*pow(sp,3.0) + 45*P0x*Q2x*Q2y*Q3y*pow(sp,2.0) - 108*P0x*Q3x*Q1y*Q2y*pow(sp,3.0) + 27*P0x*Q3x*Q1y*Q3y*pow(sp,2.0) -
	432*P1x*Q1x*Q2y*Q3y*pow(sp,2.0) - 108*P1x*Q2x*Q1y*Q2y*pow(sp,3.0) + 27*P1x*Q2x*Q1y*Q3y*pow(sp,2.0) + 27*P1x*Q3x*Q1y*Q2y*pow(sp,2.0) - 90*P0y*Q1x*Q2x*Q2y*pow(sp,4.0) - 108*P0y*Q1x*Q2x*Q3y*pow(sp,3.0) -
	108*P0y*Q1x*Q3x*Q2y*pow(sp,3.0) + 27*P0y*Q1x*Q3x*Q3y*pow(sp,2.0) - 624*P0y*Q2x*Q3x*Q1y*pow(sp,3.0) + 45*P0y*Q2x*Q3x*Q2y*pow(sp,2.0) - 108*P1y*Q1x*Q2x*Q2y*pow(sp,3.0) + 27*P1y*Q1x*Q2x*Q3y*pow(sp,2.0) +
	27*P1y*Q1x*Q3x*Q2y*pow(sp,2.0) - 432*P1y*Q2x*Q3x*Q1y*pow(sp,2.0) + 240*P0x*Q1x*Q2y*Q3y*pow(sp,4.0) + 90*P0x*Q2x*Q1y*Q3y*pow(sp,4.0) - 240*P0x*Q2x*Q2y*Q3y*pow(sp,3.0) + 90*P0x*Q3x*Q1y*Q2y*pow(sp,4.0) +
	108*P0x*Q3x*Q1y*Q3y*pow(sp,3.0) + 3*P0x*Q3x*Q2y*Q3y*pow(sp,2.0) + 624*P1x*Q1x*Q2y*Q3y*pow(sp,3.0) + 90*P1x*Q2x*Q1y*Q2y*pow(sp,4.0) + 108*P1x*Q2x*Q1y*Q3y*pow(sp,3.0) - 45*P1x*Q2x*Q2y*Q3y*pow(sp,2.0) +
	108*P1x*Q3x*Q1y*Q2y*pow(sp,3.0) - 27*P1x*Q3x*Q1y*Q3y*pow(sp,2.0) + 90*P0y*Q1x*Q2x*Q3y*pow(sp,4.0) + 90*P0y*Q1x*Q3x*Q2y*pow(sp,4.0) + 108*P0y*Q1x*Q3x*Q3y*pow(sp,3.0) + 240*P0y*Q2x*Q3x*Q1y*pow(sp,4.0) -
	240*P0y*Q2x*Q3x*Q2y*pow(sp,3.0) + 3*P0y*Q2x*Q3x*Q3y*pow(sp,2.0) + 90*P1y*Q1x*Q2x*Q2y*pow(sp,4.0) + 108*P1y*Q1x*Q2x*Q3y*pow(sp,3.0) + 108*P1y*Q1x*Q3x*Q2y*pow(sp,3.0) - 27*P1y*Q1x*Q3x*Q3y*pow(sp,2.0) +
	624*P1y*Q2x*Q3x*Q1y*pow(sp,3.0) - 45*P1y*Q2x*Q3x*Q2y*pow(sp,2.0) + 123*P0x*Q2x*Q2y*Q3y*pow(sp,4.0) - 90*P0x*Q3x*Q1y*Q3y*pow(sp,4.0) - 16*P0x*Q3x*Q2y*Q3y*pow(sp,3.0) - 240*P1x*Q1x*Q2y*Q3y*pow(sp,4.0) -
	90*P1x*Q2x*Q1y*Q3y*pow(sp,4.0) + 240*P1x*Q2x*Q2y*Q3y*pow(sp,3.0) - 90*P1x*Q3x*Q1y*Q2y*pow(sp,4.0) - 108*P1x*Q3x*Q1y*Q3y*pow(sp,3.0) - 3*P1x*Q3x*Q2y*Q3y*pow(sp,2.0) - 90*P0y*Q1x*Q3x*Q3y*pow(sp,4.0) +
	123*P0y*Q2x*Q3x*Q2y*pow(sp,4.0) - 16*P0y*Q2x*Q3x*Q3y*pow(sp,3.0) - 90*P1y*Q1x*Q2x*Q3y*pow(sp,4.0) - 90*P1y*Q1x*Q3x*Q2y*pow(sp,4.0) - 108*P1y*Q1x*Q3x*Q3y*pow(sp,3.0) - 240*P1y*Q2x*Q3x*Q1y*pow(sp,4.0) +
	240*P1y*Q2x*Q3x*Q2y*pow(sp,3.0) - 3*P1y*Q2x*Q3x*Q3y*pow(sp,2.0) - 3*P0x*Q3x*Q2y*Q3y*pow(sp,4.0) - 123*P1x*Q2x*Q2y*Q3y*pow(sp,4.0) + 90*P1x*Q3x*Q1y*Q3y*pow(sp,4.0) + 16*P1x*Q3x*Q2y*Q3y*pow(sp,3.0) -
	3*P0y*Q2x*Q3x*Q3y*pow(sp,4.0) + 90*P1y*Q1x*Q3x*Q3y*pow(sp,4.0) - 123*P1y*Q2x*Q3x*Q2y*pow(sp,4.0) + 16*P1y*Q2x*Q3x*Q3y*pow(sp,3.0) + 3*P1x*Q3x*Q2y*Q3y*pow(sp,4.0) + 3*P1y*Q2x*Q3x*Q3y*pow(sp,4.0) +
	378*P0y*Q1y*Q2y*Q3y*pow(sp,2.0) - 840*P0y*Q1y*Q2y*Q3y*pow(sp,3.0) - 378*P1y*Q1y*Q2y*Q3y*pow(sp,2.0) + 420*P0y*Q1y*Q2y*Q3y*pow(sp,4.0) + 840*P1y*Q1y*Q2y*Q3y*pow(sp,3.0) -
	420*P1y*Q1y*Q2y*Q3y*pow(sp,4.0))/(3*(7*pow(P0x,2.0)*pow(Q2x,2.0) + 7*pow(P0x,2.0)*pow(Q3x,2.0) + 7*pow(P1x,2.0)*pow(Q2x,2.0) + 7*pow(P1x,2.0)*pow(Q3x,2.0) + 16*pow(P0x,2.0)*pow(Q2y,2.0) +
	16*pow(P0y,2.0)*pow(Q2x,2.0) + 16*pow(P0x,2.0)*pow(Q3y,2.0) + 16*pow(P1x,2.0)*pow(Q2y,2.0) + 16*pow(P0y,2.0)*pow(Q3x,2.0) + 16*pow(P1y,2.0)*pow(Q2x,2.0) + 16*pow(P1x,2.0)*pow(Q3y,2.0) +
	16*pow(P1y,2.0)*pow(Q3x,2.0) + 7*pow(P0y,2.0)*pow(Q2y,2.0) + 7*pow(P0y,2.0)*pow(Q3y,2.0) + 7*pow(P1y,2.0)*pow(Q2y,2.0) + 7*pow(P1y,2.0)*pow(Q3y,2.0) - 14*P0x*P1x*pow(Q2x,2.0) - 14*P0x*P1x*pow(Q3x,2.0) -
	32*P0x*P1x*pow(Q2y,2.0) - 32*P0x*P1x*pow(Q3y,2.0) - 32*P0y*P1y*pow(Q2x,2.0) - 32*P0y*P1y*pow(Q3x,2.0) - 14*P0y*P1y*pow(Q2y,2.0) - 14*P0y*P1y*pow(Q3y,2.0) - 14*pow(P0x,2.0)*Q2x*Q3x - 14*pow(P1x,2.0)*Q2x*Q3x -
	32*pow(P0y,2.0)*Q2x*Q3x - 32*pow(P1y,2.0)*Q2x*Q3x - 32*pow(P0x,2.0)*Q2y*Q3y - 32*pow(P1x,2.0)*Q2y*Q3y - 14*pow(P0y,2.0)*Q2y*Q3y - 14*pow(P1y,2.0)*Q2y*Q3y + 28*P0x*P1x*Q2x*Q3x - 18*P0x*P0y*Q2x*Q2y + 18*P0x*P0y*Q2x*Q3y +
	18*P0x*P0y*Q3x*Q2y + 18*P0x*P1y*Q2x*Q2y + 18*P1x*P0y*Q2x*Q2y + 64*P0x*P1x*Q2y*Q3y - 18*P0x*P0y*Q3x*Q3y - 18*P0x*P1y*Q2x*Q3y - 18*P0x*P1y*Q3x*Q2y - 18*P1x*P0y*Q2x*Q3y - 18*P1x*P0y*Q3x*Q2y - 18*P1x*P1y*Q2x*Q2y +
	64*P0y*P1y*Q2x*Q3x + 18*P0x*P1y*Q3x*Q3y + 18*P1x*P0y*Q3x*Q3y + 18*P1x*P1y*Q2x*Q3y + 18*P1x*P1y*Q3x*Q2y - 18*P1x*P1y*Q3x*Q3y + 28*P0y*P1y*Q2y*Q3y));
	
	out_qfactor = (14*pow(P0x,3.0)*Q2x - 14*pow(P0x,3.0)*Q3x + 14*pow(P0y,3.0)*Q2y - 14*pow(P0y,3.0)*Q3y + 21*pow(P0x,2.0)*pow(Q2x,2.0) + 21*pow(P0x,2.0)*pow(Q3x,2.0) + 21*pow(P1x,2.0)*pow(Q2x,2.0) + 21*pow(P1x,2.0)*pow(Q3x,2.0) +
	48*pow(P0x,2.0)*pow(Q2y,2.0) + 48*pow(P0y,2.0)*pow(Q2x,2.0) + 48*pow(P0x,2.0)*pow(Q3y,2.0) + 48*pow(P1x,2.0)*pow(Q2y,2.0) + 48*pow(P0y,2.0)*pow(Q3x,2.0) + 48*pow(P1y,2.0)*pow(Q2x,2.0) + 48*pow(P1x,2.0)*pow(Q3y,2.0) +
	48*pow(P1y,2.0)*pow(Q3x,2.0) + 21*pow(P0y,2.0)*pow(Q2y,2.0) + 21*pow(P0y,2.0)*pow(Q3y,2.0) + 21*pow(P1y,2.0)*pow(Q2y,2.0) + 21*pow(P1y,2.0)*pow(Q3y,2.0) + 21*pow(P0x,2.0)*pow(Q2x,2.0)*sp + 21*pow(P0x,2.0)*pow(Q3x,2.0)*sp -
	63*pow(P0x,3.0)*Q2x*pow(sp,2.0) + 21*pow(P1x,2.0)*pow(Q2x,2.0)*sp + 70*pow(P0x,3.0)*Q2x*pow(sp,3.0) + 63*pow(P0x,3.0)*Q3x*pow(sp,2.0) + 21*pow(P1x,2.0)*pow(Q3x,2.0)*sp - 126*pow(P1x,3.0)*Q2x*pow(sp,2.0) -
	21*pow(P0x,3.0)*Q2x*pow(sp,4.0) - 70*pow(P0x,3.0)*Q3x*pow(sp,3.0) + 210*pow(P1x,3.0)*Q2x*pow(sp,3.0) + 126*pow(P1x,3.0)*Q3x*pow(sp,2.0) + 21*pow(P0x,3.0)*Q3x*pow(sp,4.0) - 84*pow(P1x,3.0)*Q2x*pow(sp,4.0) -
	210*pow(P1x,3.0)*Q3x*pow(sp,3.0) + 84*pow(P1x,3.0)*Q3x*pow(sp,4.0) - 24*pow(P0x,2.0)*pow(Q2y,2.0)*sp - 24*pow(P0y,2.0)*pow(Q2x,2.0)*sp + 48*pow(P0x,2.0)*pow(Q3y,2.0)*sp - 24*pow(P1x,2.0)*pow(Q2y,2.0)*sp +
	48*pow(P0y,2.0)*pow(Q3x,2.0)*sp - 24*pow(P1y,2.0)*pow(Q2x,2.0)*sp + 48*pow(P1x,2.0)*pow(Q3y,2.0)*sp + 48*pow(P1y,2.0)*pow(Q3x,2.0)*sp + 21*pow(P0y,2.0)*pow(Q2y,2.0)*sp + 21*pow(P0y,2.0)*pow(Q3y,2.0)*sp -
	63*pow(P0y,3.0)*Q2y*pow(sp,2.0) + 21*pow(P1y,2.0)*pow(Q2y,2.0)*sp + 70*pow(P0y,3.0)*Q2y*pow(sp,3.0) + 63*pow(P0y,3.0)*Q3y*pow(sp,2.0) + 21*pow(P1y,2.0)*pow(Q3y,2.0)*sp - 126*pow(P1y,3.0)*Q2y*pow(sp,2.0) -
	21*pow(P0y,3.0)*Q2y*pow(sp,4.0) - 70*pow(P0y,3.0)*Q3y*pow(sp,3.0) + 210*pow(P1y,3.0)*Q2y*pow(sp,3.0) + 126*pow(P1y,3.0)*Q3y*pow(sp,2.0) + 21*pow(P0y,3.0)*Q3y*pow(sp,4.0) - 84*pow(P1y,3.0)*Q2y*pow(sp,4.0) -
	210*pow(P1y,3.0)*Q3y*pow(sp,3.0) + 84*pow(P1y,3.0)*Q3y*pow(sp,4.0) - 21*pow(P0x,2.0)*pow(Q2x,2.0)*pow(sp,2.0) - 105*pow(P0x,2.0)*pow(Q2x,2.0)*pow(sp,3.0) + 21*pow(P0x,2.0)*pow(Q3x,2.0)*pow(sp,2.0) -
	21*pow(P1x,2.0)*pow(Q2x,2.0)*pow(sp,2.0) + 84*pow(P0x,2.0)*pow(Q2x,2.0)*pow(sp,4.0) + 7*pow(P0x,2.0)*pow(Q3x,2.0)*pow(sp,3.0) - 105*pow(P1x,2.0)*pow(Q2x,2.0)*pow(sp,3.0) + 21*pow(P1x,2.0)*pow(Q3x,2.0)*pow(sp,2.0) -
	21*pow(P0x,2.0)*pow(Q3x,2.0)*pow(sp,4.0) + 84*pow(P1x,2.0)*pow(Q2x,2.0)*pow(sp,4.0) + 7*pow(P1x,2.0)*pow(Q3x,2.0)*pow(sp,3.0) - 21*pow(P1x,2.0)*pow(Q3x,2.0)*pow(sp,4.0) - 48*pow(P0x,2.0)*pow(Q2y,2.0)*pow(sp,2.0) -
	48*pow(P0y,2.0)*pow(Q2x,2.0)*pow(sp,2.0) - 24*pow(P0x,2.0)*pow(Q2y,2.0)*pow(sp,3.0) + 12*pow(P0x,2.0)*pow(Q3y,2.0)*pow(sp,2.0) - 48*pow(P1x,2.0)*pow(Q2y,2.0)*pow(sp,2.0) - 24*pow(P0y,2.0)*pow(Q2x,2.0)*pow(sp,3.0) +
	12*pow(P0y,2.0)*pow(Q3x,2.0)*pow(sp,2.0) - 48*pow(P1y,2.0)*pow(Q2x,2.0)*pow(sp,2.0) + 48*pow(P0x,2.0)*pow(Q2y,2.0)*pow(sp,4.0) - 8*pow(P0x,2.0)*pow(Q3y,2.0)*pow(sp,3.0) - 24*pow(P1x,2.0)*pow(Q2y,2.0)*pow(sp,3.0) +
	12*pow(P1x,2.0)*pow(Q3y,2.0)*pow(sp,2.0) + 48*pow(P0y,2.0)*pow(Q2x,2.0)*pow(sp,4.0) - 8*pow(P0y,2.0)*pow(Q3x,2.0)*pow(sp,3.0) - 24*pow(P1y,2.0)*pow(Q2x,2.0)*pow(sp,3.0) + 12*pow(P1y,2.0)*pow(Q3x,2.0)*pow(sp,2.0) -
	12*pow(P0x,2.0)*pow(Q3y,2.0)*pow(sp,4.0) + 48*pow(P1x,2.0)*pow(Q2y,2.0)*pow(sp,4.0) - 8*pow(P1x,2.0)*pow(Q3y,2.0)*pow(sp,3.0) - 12*pow(P0y,2.0)*pow(Q3x,2.0)*pow(sp,4.0) + 48*pow(P1y,2.0)*pow(Q2x,2.0)*pow(sp,4.0) -
	8*pow(P1y,2.0)*pow(Q3x,2.0)*pow(sp,3.0) - 12*pow(P1x,2.0)*pow(Q3y,2.0)*pow(sp,4.0) - 12*pow(P1y,2.0)*pow(Q3x,2.0)*pow(sp,4.0) - 21*pow(P0y,2.0)*pow(Q2y,2.0)*pow(sp,2.0) - 105*pow(P0y,2.0)*pow(Q2y,2.0)*pow(sp,3.0) +
	21*pow(P0y,2.0)*pow(Q3y,2.0)*pow(sp,2.0) - 21*pow(P1y,2.0)*pow(Q2y,2.0)*pow(sp,2.0) + 84*pow(P0y,2.0)*pow(Q2y,2.0)*pow(sp,4.0) + 7*pow(P0y,2.0)*pow(Q3y,2.0)*pow(sp,3.0) - 105*pow(P1y,2.0)*pow(Q2y,2.0)*pow(sp,3.0) +
	21*pow(P1y,2.0)*pow(Q3y,2.0)*pow(sp,2.0) - 21*pow(P0y,2.0)*pow(Q3y,2.0)*pow(sp,4.0) + 84*pow(P1y,2.0)*pow(Q2y,2.0)*pow(sp,4.0) + 7*pow(P1y,2.0)*pow(Q3y,2.0)*pow(sp,3.0) - 21*pow(P1y,2.0)*pow(Q3y,2.0)*pow(sp,4.0) -
	42*P0x*P1x*pow(Q2x,2.0) + 14*P0x*pow(P1x,2.0)*Q2x - 28*pow(P0x,2.0)*P1x*Q2x - 42*P0x*P1x*pow(Q3x,2.0) - 14*P0x*pow(P1x,2.0)*Q3x + 28*pow(P0x,2.0)*P1x*Q3x - 14*pow(P0x,2.0)*P3x*Q2x + 14*pow(P0x,2.0)*P3x*Q3x -
	14*pow(P1x,2.0)*P3x*Q2x + 14*pow(P1x,2.0)*P3x*Q3x + 14*P0x*pow(P0y,2.0)*Q2x - 96*P0x*P1x*pow(Q2y,2.0) - 14*P0x*pow(P0y,2.0)*Q3x - 52*P0x*pow(P1y,2.0)*Q2x - 66*P1x*pow(P0y,2.0)*Q2x - 96*P0x*P1x*pow(Q3y,2.0) +
	52*P0x*pow(P1y,2.0)*Q3x + 66*P1x*pow(P0y,2.0)*Q3x + 16*P3x*pow(P0y,2.0)*Q2x - 16*P3x*pow(P0y,2.0)*Q3x + 16*P3x*pow(P1y,2.0)*Q2x - 16*P3x*pow(P1y,2.0)*Q3x + 14*pow(P0x,2.0)*P0y*Q2y - 96*P0y*P1y*pow(Q2x,2.0) -
	14*pow(P0x,2.0)*P0y*Q3y - 66*pow(P0x,2.0)*P1y*Q2y - 52*pow(P1x,2.0)*P0y*Q2y - 96*P0y*P1y*pow(Q3x,2.0) + 66*pow(P0x,2.0)*P1y*Q3y + 52*pow(P1x,2.0)*P0y*Q3y + 16*pow(P0x,2.0)*P3y*Q2y - 16*pow(P0x,2.0)*P3y*Q3y +
	16*pow(P1x,2.0)*P3y*Q2y - 16*pow(P1x,2.0)*P3y*Q3y - 42*P0y*P1y*pow(Q2y,2.0) + 14*P0y*pow(P1y,2.0)*Q2y - 28*pow(P0y,2.0)*P1y*Q2y - 42*P0y*P1y*pow(Q3y,2.0) - 14*P0y*pow(P1y,2.0)*Q3y + 28*pow(P0y,2.0)*P1y*Q3y -
	14*pow(P0y,2.0)*P3y*Q2y + 14*pow(P0y,2.0)*P3y*Q3y - 14*pow(P1y,2.0)*P3y*Q2y + 14*pow(P1y,2.0)*P3y*Q3y - 42*pow(P0x,2.0)*Q2x*Q3x - 42*pow(P1x,2.0)*Q2x*Q3x + 36*pow(P0y,2.0)*Q1x*Q2x - 36*pow(P0y,2.0)*Q1x*Q3x +
	36*pow(P1y,2.0)*Q1x*Q2x - 96*pow(P0y,2.0)*Q2x*Q3x - 36*pow(P1y,2.0)*Q1x*Q3x - 96*pow(P1y,2.0)*Q2x*Q3x + 36*pow(P0x,2.0)*Q1y*Q2y - 36*pow(P0x,2.0)*Q1y*Q3y + 36*pow(P1x,2.0)*Q1y*Q2y - 96*pow(P0x,2.0)*Q2y*Q3y -
	36*pow(P1x,2.0)*Q1y*Q3y - 96*pow(P1x,2.0)*Q2y*Q3y - 42*pow(P0y,2.0)*Q2y*Q3y - 42*pow(P1y,2.0)*Q2y*Q3y - 42*P0x*P1x*pow(Q2x,2.0)*sp - 42*P0x*P1x*pow(Q3x,2.0)*sp - 42*pow(P0x,2.0)*P3x*Q2x*sp + 42*pow(P0x,2.0)*P3x*Q3x*sp -
	42*pow(P1x,2.0)*P3x*Q2x*sp + 42*pow(P1x,2.0)*P3x*Q3x*sp + 48*P0x*P1x*pow(Q2y,2.0)*sp - 96*P0x*P1x*pow(Q3y,2.0)*sp + 48*P3x*pow(P0y,2.0)*Q2x*sp - 48*P3x*pow(P0y,2.0)*Q3x*sp + 48*P3x*pow(P1y,2.0)*Q2x*sp -
	48*P3x*pow(P1y,2.0)*Q3x*sp + 48*P0y*P1y*pow(Q2x,2.0)*sp - 96*P0y*P1y*pow(Q3x,2.0)*sp + 48*pow(P0x,2.0)*P3y*Q2y*sp - 48*pow(P0x,2.0)*P3y*Q3y*sp + 48*pow(P1x,2.0)*P3y*Q2y*sp - 48*pow(P1x,2.0)*P3y*Q3y*sp -
	42*P0y*P1y*pow(Q2y,2.0)*sp - 42*P0y*P1y*pow(Q3y,2.0)*sp - 42*pow(P0y,2.0)*P3y*Q2y*sp + 42*pow(P0y,2.0)*P3y*Q3y*sp - 42*pow(P1y,2.0)*P3y*Q2y*sp + 42*pow(P1y,2.0)*P3y*Q3y*sp + 42*pow(P0x,2.0)*Q1x*Q2x*sp -
	42*pow(P0x,2.0)*Q1x*Q3x*sp + 42*pow(P1x,2.0)*Q1x*Q2x*sp - 42*pow(P0x,2.0)*Q2x*Q3x*sp - 42*pow(P1x,2.0)*Q1x*Q3x*sp - 42*pow(P1x,2.0)*Q2x*Q3x*sp + 24*pow(P0y,2.0)*Q1x*Q2x*sp - 24*pow(P0y,2.0)*Q1x*Q3x*sp +
	24*pow(P1y,2.0)*Q1x*Q2x*sp - 24*pow(P0y,2.0)*Q2x*Q3x*sp - 24*pow(P1y,2.0)*Q1x*Q3x*sp - 24*pow(P1y,2.0)*Q2x*Q3x*sp + 24*pow(P0x,2.0)*Q1y*Q2y*sp - 24*pow(P0x,2.0)*Q1y*Q3y*sp + 24*pow(P1x,2.0)*Q1y*Q2y*sp -
	24*pow(P0x,2.0)*Q2y*Q3y*sp - 24*pow(P1x,2.0)*Q1y*Q3y*sp - 24*pow(P1x,2.0)*Q2y*Q3y*sp + 42*pow(P0y,2.0)*Q1y*Q2y*sp - 42*pow(P0y,2.0)*Q1y*Q3y*sp + 42*pow(P1y,2.0)*Q1y*Q2y*sp - 42*pow(P0y,2.0)*Q2y*Q3y*sp -
	42*pow(P1y,2.0)*Q1y*Q3y*sp - 42*pow(P1y,2.0)*Q2y*Q3y*sp + 42*P0x*P1x*pow(Q2x,2.0)*pow(sp,2.0) + 189*P0x*pow(P1x,2.0)*Q2x*pow(sp,2.0) + 210*P0x*P1x*pow(Q2x,2.0)*pow(sp,3.0) - 42*P0x*P1x*pow(Q3x,2.0)*pow(sp,2.0) -
	350*P0x*pow(P1x,2.0)*Q2x*pow(sp,3.0) - 189*P0x*pow(P1x,2.0)*Q3x*pow(sp,2.0) + 70*pow(P0x,2.0)*P1x*Q2x*pow(sp,3.0) - 189*pow(P0x,2.0)*P2x*Q2x*pow(sp,2.0) - 168*P0x*P1x*pow(Q2x,2.0)*pow(sp,4.0) -
	14*P0x*P1x*pow(Q3x,2.0)*pow(sp,3.0) + 147*P0x*pow(P1x,2.0)*Q2x*pow(sp,4.0) + 350*P0x*pow(P1x,2.0)*Q3x*pow(sp,3.0) - 42*pow(P0x,2.0)*P1x*Q2x*pow(sp,4.0) - 70*pow(P0x,2.0)*P1x*Q3x*pow(sp,3.0) +
	420*pow(P0x,2.0)*P2x*Q2x*pow(sp,3.0) + 189*pow(P0x,2.0)*P2x*Q3x*pow(sp,2.0) + 294*pow(P0x,2.0)*P3x*Q2x*pow(sp,2.0) - 189*pow(P1x,2.0)*P2x*Q2x*pow(sp,2.0) + 42*P0x*P1x*pow(Q3x,2.0)*pow(sp,4.0) -
	147*P0x*pow(P1x,2.0)*Q3x*pow(sp,4.0) + 42*pow(P0x,2.0)*P1x*Q3x*pow(sp,4.0) - 210*pow(P0x,2.0)*P2x*Q2x*pow(sp,4.0) - 420*pow(P0x,2.0)*P2x*Q3x*pow(sp,3.0) - 210*pow(P0x,2.0)*P3x*Q2x*pow(sp,3.0) -
	294*pow(P0x,2.0)*P3x*Q3x*pow(sp,2.0) + 420*pow(P1x,2.0)*P2x*Q2x*pow(sp,3.0) + 189*pow(P1x,2.0)*P2x*Q3x*pow(sp,2.0) + 294*pow(P1x,2.0)*P3x*Q2x*pow(sp,2.0) + 210*pow(P0x,2.0)*P2x*Q3x*pow(sp,4.0) +
	210*pow(P0x,2.0)*P3x*Q3x*pow(sp,3.0) - 210*pow(P1x,2.0)*P2x*Q2x*pow(sp,4.0) - 420*pow(P1x,2.0)*P2x*Q3x*pow(sp,3.0) - 210*pow(P1x,2.0)*P3x*Q2x*pow(sp,3.0) - 294*pow(P1x,2.0)*P3x*Q3x*pow(sp,2.0) +
	210*pow(P1x,2.0)*P2x*Q3x*pow(sp,4.0) + 210*pow(P1x,2.0)*P3x*Q3x*pow(sp,3.0) - 63*P0x*pow(P0y,2.0)*Q2x*pow(sp,2.0) + 96*P0x*P1x*pow(Q2y,2.0)*pow(sp,2.0) + 70*P0x*pow(P0y,2.0)*Q2x*pow(sp,3.0) +
	63*P0x*pow(P0y,2.0)*Q3x*pow(sp,2.0) + 126*P0x*pow(P1y,2.0)*Q2x*pow(sp,2.0) + 63*P1x*pow(P0y,2.0)*Q2x*pow(sp,2.0) + 48*P0x*P1x*pow(Q2y,2.0)*pow(sp,3.0) - 24*P0x*P1x*pow(Q3y,2.0)*pow(sp,2.0) -
	21*P0x*pow(P0y,2.0)*Q2x*pow(sp,4.0) - 70*P0x*pow(P0y,2.0)*Q3x*pow(sp,3.0) - 98*P0x*pow(P1y,2.0)*Q2x*pow(sp,3.0) - 126*P0x*pow(P1y,2.0)*Q3x*pow(sp,2.0) + 42*P1x*pow(P0y,2.0)*Q2x*pow(sp,3.0) -
	63*P1x*pow(P0y,2.0)*Q3x*pow(sp,2.0) - 126*P1x*pow(P1y,2.0)*Q2x*pow(sp,2.0) - 96*P0x*P1x*pow(Q2y,2.0)*pow(sp,4.0) + 16*P0x*P1x*pow(Q3y,2.0)*pow(sp,3.0) + 21*P0x*pow(P0y,2.0)*Q3x*pow(sp,4.0) +
	24*P0x*pow(P1y,2.0)*Q2x*pow(sp,4.0) + 98*P0x*pow(P1y,2.0)*Q3x*pow(sp,3.0) - 39*P1x*pow(P0y,2.0)*Q2x*pow(sp,4.0) - 42*P1x*pow(P0y,2.0)*Q3x*pow(sp,3.0) + 210*P1x*pow(P1y,2.0)*Q2x*pow(sp,3.0) +
	126*P1x*pow(P1y,2.0)*Q3x*pow(sp,2.0) + 168*P2x*pow(P0y,2.0)*Q2x*pow(sp,3.0) + 96*P3x*pow(P0y,2.0)*Q2x*pow(sp,2.0) + 24*P0x*P1x*pow(Q3y,2.0)*pow(sp,4.0) - 24*P0x*pow(P1y,2.0)*Q3x*pow(sp,4.0) +
	39*P1x*pow(P0y,2.0)*Q3x*pow(sp,4.0) - 84*P1x*pow(P1y,2.0)*Q2x*pow(sp,4.0) - 210*P1x*pow(P1y,2.0)*Q3x*pow(sp,3.0) - 120*P2x*pow(P0y,2.0)*Q2x*pow(sp,4.0) - 168*P2x*pow(P0y,2.0)*Q3x*pow(sp,3.0) +
	168*P2x*pow(P1y,2.0)*Q2x*pow(sp,3.0) - 120*P3x*pow(P0y,2.0)*Q2x*pow(sp,3.0) - 96*P3x*pow(P0y,2.0)*Q3x*pow(sp,2.0) + 96*P3x*pow(P1y,2.0)*Q2x*pow(sp,2.0) + 84*P1x*pow(P1y,2.0)*Q3x*pow(sp,4.0) +
	120*P2x*pow(P0y,2.0)*Q3x*pow(sp,4.0) - 120*P2x*pow(P1y,2.0)*Q2x*pow(sp,4.0) - 168*P2x*pow(P1y,2.0)*Q3x*pow(sp,3.0) + 120*P3x*pow(P0y,2.0)*Q3x*pow(sp,3.0) - 120*P3x*pow(P1y,2.0)*Q2x*pow(sp,3.0) -
	96*P3x*pow(P1y,2.0)*Q3x*pow(sp,2.0) + 120*P2x*pow(P1y,2.0)*Q3x*pow(sp,4.0) + 120*P3x*pow(P1y,2.0)*Q3x*pow(sp,3.0) - 63*pow(P0x,2.0)*P0y*Q2y*pow(sp,2.0) + 96*P0y*P1y*pow(Q2x,2.0)*pow(sp,2.0) +
	70*pow(P0x,2.0)*P0y*Q2y*pow(sp,3.0) + 63*pow(P0x,2.0)*P0y*Q3y*pow(sp,2.0) + 63*pow(P0x,2.0)*P1y*Q2y*pow(sp,2.0) + 126*pow(P1x,2.0)*P0y*Q2y*pow(sp,2.0) + 48*P0y*P1y*pow(Q2x,2.0)*pow(sp,3.0) -
	24*P0y*P1y*pow(Q3x,2.0)*pow(sp,2.0) - 21*pow(P0x,2.0)*P0y*Q2y*pow(sp,4.0) - 70*pow(P0x,2.0)*P0y*Q3y*pow(sp,3.0) + 42*pow(P0x,2.0)*P1y*Q2y*pow(sp,3.0) - 63*pow(P0x,2.0)*P1y*Q3y*pow(sp,2.0) -
	98*pow(P1x,2.0)*P0y*Q2y*pow(sp,3.0) - 126*pow(P1x,2.0)*P0y*Q3y*pow(sp,2.0) - 126*pow(P1x,2.0)*P1y*Q2y*pow(sp,2.0) - 96*P0y*P1y*pow(Q2x,2.0)*pow(sp,4.0) + 16*P0y*P1y*pow(Q3x,2.0)*pow(sp,3.0) +
	21*pow(P0x,2.0)*P0y*Q3y*pow(sp,4.0) - 39*pow(P0x,2.0)*P1y*Q2y*pow(sp,4.0) - 42*pow(P0x,2.0)*P1y*Q3y*pow(sp,3.0) + 168*pow(P0x,2.0)*P2y*Q2y*pow(sp,3.0) + 96*pow(P0x,2.0)*P3y*Q2y*pow(sp,2.0) +
	24*pow(P1x,2.0)*P0y*Q2y*pow(sp,4.0) + 98*pow(P1x,2.0)*P0y*Q3y*pow(sp,3.0) + 210*pow(P1x,2.0)*P1y*Q2y*pow(sp,3.0) + 126*pow(P1x,2.0)*P1y*Q3y*pow(sp,2.0) + 24*P0y*P1y*pow(Q3x,2.0)*pow(sp,4.0) +
	39*pow(P0x,2.0)*P1y*Q3y*pow(sp,4.0) - 120*pow(P0x,2.0)*P2y*Q2y*pow(sp,4.0) - 168*pow(P0x,2.0)*P2y*Q3y*pow(sp,3.0) - 120*pow(P0x,2.0)*P3y*Q2y*pow(sp,3.0) - 96*pow(P0x,2.0)*P3y*Q3y*pow(sp,2.0) -
	24*pow(P1x,2.0)*P0y*Q3y*pow(sp,4.0) - 84*pow(P1x,2.0)*P1y*Q2y*pow(sp,4.0) - 210*pow(P1x,2.0)*P1y*Q3y*pow(sp,3.0) + 168*pow(P1x,2.0)*P2y*Q2y*pow(sp,3.0) + 96*pow(P1x,2.0)*P3y*Q2y*pow(sp,2.0) +
	120*pow(P0x,2.0)*P2y*Q3y*pow(sp,4.0) + 120*pow(P0x,2.0)*P3y*Q3y*pow(sp,3.0) + 84*pow(P1x,2.0)*P1y*Q3y*pow(sp,4.0) - 120*pow(P1x,2.0)*P2y*Q2y*pow(sp,4.0) - 168*pow(P1x,2.0)*P2y*Q3y*pow(sp,3.0) -
	120*pow(P1x,2.0)*P3y*Q2y*pow(sp,3.0) - 96*pow(P1x,2.0)*P3y*Q3y*pow(sp,2.0) + 120*pow(P1x,2.0)*P2y*Q3y*pow(sp,4.0) + 120*pow(P1x,2.0)*P3y*Q3y*pow(sp,3.0) + 42*P0y*P1y*pow(Q2y,2.0)*pow(sp,2.0) +
	189*P0y*pow(P1y,2.0)*Q2y*pow(sp,2.0) + 210*P0y*P1y*pow(Q2y,2.0)*pow(sp,3.0) - 42*P0y*P1y*pow(Q3y,2.0)*pow(sp,2.0) - 350*P0y*pow(P1y,2.0)*Q2y*pow(sp,3.0) - 189*P0y*pow(P1y,2.0)*Q3y*pow(sp,2.0) +
	70*pow(P0y,2.0)*P1y*Q2y*pow(sp,3.0) - 189*pow(P0y,2.0)*P2y*Q2y*pow(sp,2.0) - 168*P0y*P1y*pow(Q2y,2.0)*pow(sp,4.0) - 14*P0y*P1y*pow(Q3y,2.0)*pow(sp,3.0) + 147*P0y*pow(P1y,2.0)*Q2y*pow(sp,4.0) +
	350*P0y*pow(P1y,2.0)*Q3y*pow(sp,3.0) - 42*pow(P0y,2.0)*P1y*Q2y*pow(sp,4.0) - 70*pow(P0y,2.0)*P1y*Q3y*pow(sp,3.0) + 420*pow(P0y,2.0)*P2y*Q2y*pow(sp,3.0) + 189*pow(P0y,2.0)*P2y*Q3y*pow(sp,2.0) +
	294*pow(P0y,2.0)*P3y*Q2y*pow(sp,2.0) - 189*pow(P1y,2.0)*P2y*Q2y*pow(sp,2.0) + 42*P0y*P1y*pow(Q3y,2.0)*pow(sp,4.0) - 147*P0y*pow(P1y,2.0)*Q3y*pow(sp,4.0) + 42*pow(P0y,2.0)*P1y*Q3y*pow(sp,4.0) -
	210*pow(P0y,2.0)*P2y*Q2y*pow(sp,4.0) - 420*pow(P0y,2.0)*P2y*Q3y*pow(sp,3.0) - 210*pow(P0y,2.0)*P3y*Q2y*pow(sp,3.0) - 294*pow(P0y,2.0)*P3y*Q3y*pow(sp,2.0) + 420*pow(P1y,2.0)*P2y*Q2y*pow(sp,3.0) +
	189*pow(P1y,2.0)*P2y*Q3y*pow(sp,2.0) + 294*pow(P1y,2.0)*P3y*Q2y*pow(sp,2.0) + 210*pow(P0y,2.0)*P2y*Q3y*pow(sp,4.0) + 210*pow(P0y,2.0)*P3y*Q3y*pow(sp,3.0) - 210*pow(P1y,2.0)*P2y*Q2y*pow(sp,4.0) -
	420*pow(P1y,2.0)*P2y*Q3y*pow(sp,3.0) - 210*pow(P1y,2.0)*P3y*Q2y*pow(sp,3.0) - 294*pow(P1y,2.0)*P3y*Q3y*pow(sp,2.0) + 210*pow(P1y,2.0)*P2y*Q3y*pow(sp,4.0) + 210*pow(P1y,2.0)*P3y*Q3y*pow(sp,3.0) +
	126*pow(P0x,2.0)*Q1x*Q2x*pow(sp,2.0) - 378*pow(P0x,2.0)*Q1x*Q2x*pow(sp,3.0) - 126*pow(P0x,2.0)*Q1x*Q3x*pow(sp,2.0) + 126*pow(P1x,2.0)*Q1x*Q2x*pow(sp,2.0) + 210*pow(P0x,2.0)*Q1x*Q2x*pow(sp,4.0) +
	378*pow(P0x,2.0)*Q1x*Q3x*pow(sp,3.0) - 378*pow(P1x,2.0)*Q1x*Q2x*pow(sp,3.0) - 126*pow(P1x,2.0)*Q1x*Q3x*pow(sp,2.0) - 210*pow(P0x,2.0)*Q1x*Q3x*pow(sp,4.0) + 98*pow(P0x,2.0)*Q2x*Q3x*pow(sp,3.0) +
	210*pow(P1x,2.0)*Q1x*Q2x*pow(sp,4.0) + 378*pow(P1x,2.0)*Q1x*Q3x*pow(sp,3.0) - 63*pow(P0x,2.0)*Q2x*Q3x*pow(sp,4.0) - 210*pow(P1x,2.0)*Q1x*Q3x*pow(sp,4.0) + 98*pow(P1x,2.0)*Q2x*Q3x*pow(sp,3.0) -
	63*pow(P1x,2.0)*Q2x*Q3x*pow(sp,4.0) - 36*pow(P0y,2.0)*Q1x*Q2x*pow(sp,2.0) - 144*pow(P0y,2.0)*Q1x*Q2x*pow(sp,3.0) + 36*pow(P0y,2.0)*Q1x*Q3x*pow(sp,2.0) - 36*pow(P1y,2.0)*Q1x*Q2x*pow(sp,2.0) +
	120*pow(P0y,2.0)*Q1x*Q2x*pow(sp,4.0) + 144*pow(P0y,2.0)*Q1x*Q3x*pow(sp,3.0) + 36*pow(P0y,2.0)*Q2x*Q3x*pow(sp,2.0) - 144*pow(P1y,2.0)*Q1x*Q2x*pow(sp,3.0) + 36*pow(P1y,2.0)*Q1x*Q3x*pow(sp,2.0) -
	120*pow(P0y,2.0)*Q1x*Q3x*pow(sp,4.0) + 32*pow(P0y,2.0)*Q2x*Q3x*pow(sp,3.0) + 120*pow(P1y,2.0)*Q1x*Q2x*pow(sp,4.0) + 144*pow(P1y,2.0)*Q1x*Q3x*pow(sp,3.0) + 36*pow(P1y,2.0)*Q2x*Q3x*pow(sp,2.0) -
	36*pow(P0y,2.0)*Q2x*Q3x*pow(sp,4.0) - 120*pow(P1y,2.0)*Q1x*Q3x*pow(sp,4.0) + 32*pow(P1y,2.0)*Q2x*Q3x*pow(sp,3.0) - 36*pow(P1y,2.0)*Q2x*Q3x*pow(sp,4.0) - 36*pow(P0x,2.0)*Q1y*Q2y*pow(sp,2.0) -
	144*pow(P0x,2.0)*Q1y*Q2y*pow(sp,3.0) + 36*pow(P0x,2.0)*Q1y*Q3y*pow(sp,2.0) - 36*pow(P1x,2.0)*Q1y*Q2y*pow(sp,2.0) + 120*pow(P0x,2.0)*Q1y*Q2y*pow(sp,4.0) + 144*pow(P0x,2.0)*Q1y*Q3y*pow(sp,3.0) +
	36*pow(P0x,2.0)*Q2y*Q3y*pow(sp,2.0) - 144*pow(P1x,2.0)*Q1y*Q2y*pow(sp,3.0) + 36*pow(P1x,2.0)*Q1y*Q3y*pow(sp,2.0) - 120*pow(P0x,2.0)*Q1y*Q3y*pow(sp,4.0) + 32*pow(P0x,2.0)*Q2y*Q3y*pow(sp,3.0) +
	120*pow(P1x,2.0)*Q1y*Q2y*pow(sp,4.0) + 144*pow(P1x,2.0)*Q1y*Q3y*pow(sp,3.0) + 36*pow(P1x,2.0)*Q2y*Q3y*pow(sp,2.0) - 36*pow(P0x,2.0)*Q2y*Q3y*pow(sp,4.0) - 120*pow(P1x,2.0)*Q1y*Q3y*pow(sp,4.0) +
	32*pow(P1x,2.0)*Q2y*Q3y*pow(sp,3.0) - 36*pow(P1x,2.0)*Q2y*Q3y*pow(sp,4.0) + 126*pow(P0y,2.0)*Q1y*Q2y*pow(sp,2.0) - 378*pow(P0y,2.0)*Q1y*Q2y*pow(sp,3.0) - 126*pow(P0y,2.0)*Q1y*Q3y*pow(sp,2.0) +
	126*pow(P1y,2.0)*Q1y*Q2y*pow(sp,2.0) + 210*pow(P0y,2.0)*Q1y*Q2y*pow(sp,4.0) + 378*pow(P0y,2.0)*Q1y*Q3y*pow(sp,3.0) - 378*pow(P1y,2.0)*Q1y*Q2y*pow(sp,3.0) - 126*pow(P1y,2.0)*Q1y*Q3y*pow(sp,2.0) -
	210*pow(P0y,2.0)*Q1y*Q3y*pow(sp,4.0) + 98*pow(P0y,2.0)*Q2y*Q3y*pow(sp,3.0) + 210*pow(P1y,2.0)*Q1y*Q2y*pow(sp,4.0) + 378*pow(P1y,2.0)*Q1y*Q3y*pow(sp,3.0) - 63*pow(P0y,2.0)*Q2y*Q3y*pow(sp,4.0) -
	210*pow(P1y,2.0)*Q1y*Q3y*pow(sp,4.0) + 98*pow(P1y,2.0)*Q2y*Q3y*pow(sp,3.0) - 63*pow(P1y,2.0)*Q2y*Q3y*pow(sp,4.0) + 28*P0x*P1x*P3x*Q2x - 28*P0x*P1x*P3x*Q3x + 38*P0x*P1x*P0y*Q2y + 38*P0x*P0y*P1y*Q2x -
	38*P0x*P1x*P0y*Q3y + 66*P0x*P1x*P1y*Q2y - 38*P0x*P0y*P1y*Q3x + 66*P1x*P0y*P1y*Q2x - 66*P0x*P1x*P1y*Q3y - 30*P0x*P3x*P0y*Q2y - 30*P0x*P0y*P3y*Q2x - 66*P1x*P0y*P1y*Q3x - 32*P0x*P1x*P3y*Q2y + 30*P0x*P3x*P0y*Q3y +
	30*P0x*P3x*P1y*Q2y + 30*P0x*P0y*P3y*Q3x + 30*P0x*P1y*P3y*Q2x + 30*P1x*P3x*P0y*Q2y + 30*P1x*P0y*P3y*Q2x - 32*P3x*P0y*P1y*Q2x + 32*P0x*P1x*P3y*Q3y - 30*P0x*P3x*P1y*Q3y - 30*P0x*P1y*P3y*Q3x - 30*P1x*P3x*P0y*Q3y -
	30*P1x*P3x*P1y*Q2y - 30*P1x*P0y*P3y*Q3x - 30*P1x*P1y*P3y*Q2x + 32*P3x*P0y*P1y*Q3x + 30*P1x*P3x*P1y*Q3y + 30*P1x*P1y*P3y*Q3x + 28*P0y*P1y*P3y*Q2y - 28*P0y*P1y*P3y*Q3y + 84*P0x*P1x*Q2x*Q3x - 36*P0x*P0y*Q1x*Q2y -
	36*P0x*P0y*Q2x*Q1y - 72*P0x*P1x*Q1y*Q2y + 36*P0x*P0y*Q1x*Q3y - 54*P0x*P0y*Q2x*Q2y + 36*P0x*P0y*Q3x*Q1y + 36*P0x*P1y*Q1x*Q2y + 36*P0x*P1y*Q2x*Q1y + 36*P1x*P0y*Q1x*Q2y + 36*P1x*P0y*Q2x*Q1y - 72*P0y*P1y*Q1x*Q2x +
	72*P0x*P1x*Q1y*Q3y + 54*P0x*P0y*Q2x*Q3y + 54*P0x*P0y*Q3x*Q2y - 36*P0x*P1y*Q1x*Q3y + 54*P0x*P1y*Q2x*Q2y - 36*P0x*P1y*Q3x*Q1y - 36*P1x*P0y*Q1x*Q3y + 54*P1x*P0y*Q2x*Q2y - 36*P1x*P0y*Q3x*Q1y - 36*P1x*P1y*Q1x*Q2y -
	36*P1x*P1y*Q2x*Q1y + 72*P0y*P1y*Q1x*Q3x + 192*P0x*P1x*Q2y*Q3y - 54*P0x*P0y*Q3x*Q3y - 54*P0x*P1y*Q2x*Q3y - 54*P0x*P1y*Q3x*Q2y - 54*P1x*P0y*Q2x*Q3y - 54*P1x*P0y*Q3x*Q2y + 36*P1x*P1y*Q1x*Q3y - 54*P1x*P1y*Q2x*Q2y +
	36*P1x*P1y*Q3x*Q1y + 192*P0y*P1y*Q2x*Q3x + 54*P0x*P1y*Q3x*Q3y + 54*P1x*P0y*Q3x*Q3y + 54*P1x*P1y*Q2x*Q3y + 54*P1x*P1y*Q3x*Q2y - 54*P1x*P1y*Q3x*Q3y + 84*P0y*P1y*Q2y*Q3y + 84*P0x*P1x*P3x*Q2x*sp - 84*P0x*P1x*P3x*Q3x*sp -
	90*P0x*P3x*P0y*Q2y*sp - 90*P0x*P0y*P3y*Q2x*sp - 96*P0x*P1x*P3y*Q2y*sp + 90*P0x*P3x*P0y*Q3y*sp + 90*P0x*P3x*P1y*Q2y*sp + 90*P0x*P0y*P3y*Q3x*sp + 90*P0x*P1y*P3y*Q2x*sp + 90*P1x*P3x*P0y*Q2y*sp + 90*P1x*P0y*P3y*Q2x*sp -
	96*P3x*P0y*P1y*Q2x*sp + 96*P0x*P1x*P3y*Q3y*sp - 90*P0x*P3x*P1y*Q3y*sp - 90*P0x*P1y*P3y*Q3x*sp - 90*P1x*P3x*P0y*Q3y*sp - 90*P1x*P3x*P1y*Q2y*sp - 90*P1x*P0y*P3y*Q3x*sp - 90*P1x*P1y*P3y*Q2x*sp + 96*P3x*P0y*P1y*Q3x*sp +
	90*P1x*P3x*P1y*Q3y*sp + 90*P1x*P1y*P3y*Q3x*sp + 84*P0y*P1y*P3y*Q2y*sp - 84*P0y*P1y*P3y*Q3y*sp - 84*P0x*P1x*Q1x*Q2x*sp + 84*P0x*P1x*Q1x*Q3x*sp + 84*P0x*P1x*Q2x*Q3x*sp + 18*P0x*P0y*Q1x*Q2y*sp + 18*P0x*P0y*Q2x*Q1y*sp -
	48*P0x*P1x*Q1y*Q2y*sp - 18*P0x*P0y*Q1x*Q3y*sp + 90*P0x*P0y*Q2x*Q2y*sp - 18*P0x*P0y*Q3x*Q1y*sp - 18*P0x*P1y*Q1x*Q2y*sp - 18*P0x*P1y*Q2x*Q1y*sp - 18*P1x*P0y*Q1x*Q2y*sp - 18*P1x*P0y*Q2x*Q1y*sp - 48*P0y*P1y*Q1x*Q2x*sp +
	48*P0x*P1x*Q1y*Q3y*sp - 18*P0x*P0y*Q2x*Q3y*sp - 18*P0x*P0y*Q3x*Q2y*sp + 18*P0x*P1y*Q1x*Q3y*sp - 90*P0x*P1y*Q2x*Q2y*sp + 18*P0x*P1y*Q3x*Q1y*sp + 18*P1x*P0y*Q1x*Q3y*sp - 90*P1x*P0y*Q2x*Q2y*sp + 18*P1x*P0y*Q3x*Q1y*sp +
	18*P1x*P1y*Q1x*Q2y*sp + 18*P1x*P1y*Q2x*Q1y*sp + 48*P0y*P1y*Q1x*Q3x*sp + 48*P0x*P1x*Q2y*Q3y*sp - 54*P0x*P0y*Q3x*Q3y*sp + 18*P0x*P1y*Q2x*Q3y*sp + 18*P0x*P1y*Q3x*Q2y*sp + 18*P1x*P0y*Q2x*Q3y*sp + 18*P1x*P0y*Q3x*Q2y*sp -
	18*P1x*P1y*Q1x*Q3y*sp + 90*P1x*P1y*Q2x*Q2y*sp - 18*P1x*P1y*Q3x*Q1y*sp + 48*P0y*P1y*Q2x*Q3x*sp + 54*P0x*P1y*Q3x*Q3y*sp + 54*P1x*P0y*Q3x*Q3y*sp - 18*P1x*P1y*Q2x*Q3y*sp - 18*P1x*P1y*Q3x*Q2y*sp - 54*P1x*P1y*Q3x*Q3y*sp -
	84*P0y*P1y*Q1y*Q2y*sp + 84*P0y*P1y*Q1y*Q3y*sp + 84*P0y*P1y*Q2y*Q3y*sp + 378*P0x*P1x*P2x*Q2x*pow(sp,2.0) - 840*P0x*P1x*P2x*Q2x*pow(sp,3.0) - 378*P0x*P1x*P2x*Q3x*pow(sp,2.0) - 588*P0x*P1x*P3x*Q2x*pow(sp,2.0) +
	420*P0x*P1x*P2x*Q2x*pow(sp,4.0) + 840*P0x*P1x*P2x*Q3x*pow(sp,3.0) + 420*P0x*P1x*P3x*Q2x*pow(sp,3.0) + 588*P0x*P1x*P3x*Q3x*pow(sp,2.0) - 420*P0x*P1x*P2x*Q3x*pow(sp,4.0) - 420*P0x*P1x*P3x*Q3x*pow(sp,3.0) -
	63*P0x*P1x*P0y*Q2y*pow(sp,2.0) - 63*P0x*P0y*P1y*Q2x*pow(sp,2.0) + 28*P0x*P1x*P0y*Q2y*pow(sp,3.0) + 63*P0x*P1x*P0y*Q3y*pow(sp,2.0) + 63*P0x*P1x*P1y*Q2y*pow(sp,2.0) - 189*P0x*P2x*P0y*Q2y*pow(sp,2.0) +
	28*P0x*P0y*P1y*Q2x*pow(sp,3.0) + 63*P0x*P0y*P1y*Q3x*pow(sp,2.0) - 189*P0x*P0y*P2y*Q2x*pow(sp,2.0) + 63*P1x*P0y*P1y*Q2x*pow(sp,2.0) - 3*P0x*P1x*P0y*Q2y*pow(sp,4.0) - 28*P0x*P1x*P0y*Q3y*pow(sp,3.0) -
	252*P0x*P1x*P1y*Q2y*pow(sp,3.0) - 63*P0x*P1x*P1y*Q3y*pow(sp,2.0) + 252*P0x*P2x*P0y*Q2y*pow(sp,3.0) + 189*P0x*P2x*P0y*Q3y*pow(sp,2.0) + 189*P0x*P2x*P1y*Q2y*pow(sp,2.0) + 198*P0x*P3x*P0y*Q2y*pow(sp,2.0) -
	3*P0x*P0y*P1y*Q2x*pow(sp,4.0) - 28*P0x*P0y*P1y*Q3x*pow(sp,3.0) + 252*P0x*P0y*P2y*Q2x*pow(sp,3.0) + 189*P0x*P0y*P2y*Q3x*pow(sp,2.0) + 198*P0x*P0y*P3y*Q2x*pow(sp,2.0) + 189*P0x*P1y*P2y*Q2x*pow(sp,2.0) +
	189*P1x*P2x*P0y*Q2y*pow(sp,2.0) - 252*P1x*P0y*P1y*Q2x*pow(sp,3.0) - 63*P1x*P0y*P1y*Q3x*pow(sp,2.0) + 189*P1x*P0y*P2y*Q2x*pow(sp,2.0) + 3*P0x*P1x*P0y*Q3y*pow(sp,4.0) + 123*P0x*P1x*P1y*Q2y*pow(sp,4.0) +
	252*P0x*P1x*P1y*Q3y*pow(sp,3.0) - 336*P0x*P1x*P2y*Q2y*pow(sp,3.0) - 192*P0x*P1x*P3y*Q2y*pow(sp,2.0) - 90*P0x*P2x*P0y*Q2y*pow(sp,4.0) - 252*P0x*P2x*P0y*Q3y*pow(sp,3.0) - 252*P0x*P2x*P1y*Q2y*pow(sp,3.0) -
	189*P0x*P2x*P1y*Q3y*pow(sp,2.0) - 90*P0x*P3x*P0y*Q2y*pow(sp,3.0) - 198*P0x*P3x*P0y*Q3y*pow(sp,2.0) - 198*P0x*P3x*P1y*Q2y*pow(sp,2.0) + 3*P0x*P0y*P1y*Q3x*pow(sp,4.0) - 90*P0x*P0y*P2y*Q2x*pow(sp,4.0) -
	252*P0x*P0y*P2y*Q3x*pow(sp,3.0) - 90*P0x*P0y*P3y*Q2x*pow(sp,3.0) - 198*P0x*P0y*P3y*Q3x*pow(sp,2.0) - 252*P0x*P1y*P2y*Q2x*pow(sp,3.0) - 189*P0x*P1y*P2y*Q3x*pow(sp,2.0) - 198*P0x*P1y*P3y*Q2x*pow(sp,2.0) -
	252*P1x*P2x*P0y*Q2y*pow(sp,3.0) - 189*P1x*P2x*P0y*Q3y*pow(sp,2.0) - 189*P1x*P2x*P1y*Q2y*pow(sp,2.0) - 198*P1x*P3x*P0y*Q2y*pow(sp,2.0) + 123*P1x*P0y*P1y*Q2x*pow(sp,4.0) + 252*P1x*P0y*P1y*Q3x*pow(sp,3.0) -
	252*P1x*P0y*P2y*Q2x*pow(sp,3.0) - 189*P1x*P0y*P2y*Q3x*pow(sp,2.0) - 198*P1x*P0y*P3y*Q2x*pow(sp,2.0) - 189*P1x*P1y*P2y*Q2x*pow(sp,2.0) - 336*P2x*P0y*P1y*Q2x*pow(sp,3.0) - 192*P3x*P0y*P1y*Q2x*pow(sp,2.0) -
	123*P0x*P1x*P1y*Q3y*pow(sp,4.0) + 240*P0x*P1x*P2y*Q2y*pow(sp,4.0) + 336*P0x*P1x*P2y*Q3y*pow(sp,3.0) + 240*P0x*P1x*P3y*Q2y*pow(sp,3.0) + 192*P0x*P1x*P3y*Q3y*pow(sp,2.0) + 90*P0x*P2x*P0y*Q3y*pow(sp,4.0) +
	90*P0x*P2x*P1y*Q2y*pow(sp,4.0) + 252*P0x*P2x*P1y*Q3y*pow(sp,3.0) + 90*P0x*P3x*P0y*Q3y*pow(sp,3.0) + 90*P0x*P3x*P1y*Q2y*pow(sp,3.0) + 198*P0x*P3x*P1y*Q3y*pow(sp,2.0) + 90*P0x*P0y*P2y*Q3x*pow(sp,4.0) +
	90*P0x*P0y*P3y*Q3x*pow(sp,3.0) + 90*P0x*P1y*P2y*Q2x*pow(sp,4.0) + 252*P0x*P1y*P2y*Q3x*pow(sp,3.0) + 90*P0x*P1y*P3y*Q2x*pow(sp,3.0) + 198*P0x*P1y*P3y*Q3x*pow(sp,2.0) + 90*P1x*P2x*P0y*Q2y*pow(sp,4.0) +
	252*P1x*P2x*P0y*Q3y*pow(sp,3.0) + 252*P1x*P2x*P1y*Q2y*pow(sp,3.0) + 189*P1x*P2x*P1y*Q3y*pow(sp,2.0) + 90*P1x*P3x*P0y*Q2y*pow(sp,3.0) + 198*P1x*P3x*P0y*Q3y*pow(sp,2.0) + 198*P1x*P3x*P1y*Q2y*pow(sp,2.0) -
	123*P1x*P0y*P1y*Q3x*pow(sp,4.0) + 90*P1x*P0y*P2y*Q2x*pow(sp,4.0) + 252*P1x*P0y*P2y*Q3x*pow(sp,3.0) + 90*P1x*P0y*P3y*Q2x*pow(sp,3.0) + 198*P1x*P0y*P3y*Q3x*pow(sp,2.0) + 252*P1x*P1y*P2y*Q2x*pow(sp,3.0) +
	189*P1x*P1y*P2y*Q3x*pow(sp,2.0) + 198*P1x*P1y*P3y*Q2x*pow(sp,2.0) + 240*P2x*P0y*P1y*Q2x*pow(sp,4.0) + 336*P2x*P0y*P1y*Q3x*pow(sp,3.0) + 240*P3x*P0y*P1y*Q2x*pow(sp,3.0) + 192*P3x*P0y*P1y*Q3x*pow(sp,2.0) -
	240*P0x*P1x*P2y*Q3y*pow(sp,4.0) - 240*P0x*P1x*P3y*Q3y*pow(sp,3.0) - 90*P0x*P2x*P1y*Q3y*pow(sp,4.0) - 90*P0x*P3x*P1y*Q3y*pow(sp,3.0) - 90*P0x*P1y*P2y*Q3x*pow(sp,4.0) - 90*P0x*P1y*P3y*Q3x*pow(sp,3.0) -
	90*P1x*P2x*P0y*Q3y*pow(sp,4.0) - 90*P1x*P2x*P1y*Q2y*pow(sp,4.0) - 252*P1x*P2x*P1y*Q3y*pow(sp,3.0) - 90*P1x*P3x*P0y*Q3y*pow(sp,3.0) - 90*P1x*P3x*P1y*Q2y*pow(sp,3.0) - 198*P1x*P3x*P1y*Q3y*pow(sp,2.0) -
	90*P1x*P0y*P2y*Q3x*pow(sp,4.0) - 90*P1x*P0y*P3y*Q3x*pow(sp,3.0) - 90*P1x*P1y*P2y*Q2x*pow(sp,4.0) - 252*P1x*P1y*P2y*Q3x*pow(sp,3.0) - 90*P1x*P1y*P3y*Q2x*pow(sp,3.0) - 198*P1x*P1y*P3y*Q3x*pow(sp,2.0) -
	240*P2x*P0y*P1y*Q3x*pow(sp,4.0) - 240*P3x*P0y*P1y*Q3x*pow(sp,3.0) + 90*P1x*P2x*P1y*Q3y*pow(sp,4.0) + 90*P1x*P3x*P1y*Q3y*pow(sp,3.0) + 90*P1x*P1y*P2y*Q3x*pow(sp,4.0) + 90*P1x*P1y*P3y*Q3x*pow(sp,3.0) +
	378*P0y*P1y*P2y*Q2y*pow(sp,2.0) - 840*P0y*P1y*P2y*Q2y*pow(sp,3.0) - 378*P0y*P1y*P2y*Q3y*pow(sp,2.0) - 588*P0y*P1y*P3y*Q2y*pow(sp,2.0) + 420*P0y*P1y*P2y*Q2y*pow(sp,4.0) + 840*P0y*P1y*P2y*Q3y*pow(sp,3.0) +
	420*P0y*P1y*P3y*Q2y*pow(sp,3.0) + 588*P0y*P1y*P3y*Q3y*pow(sp,2.0) - 420*P0y*P1y*P2y*Q3y*pow(sp,4.0) - 420*P0y*P1y*P3y*Q3y*pow(sp,3.0) - 252*P0x*P1x*Q1x*Q2x*pow(sp,2.0) + 756*P0x*P1x*Q1x*Q2x*pow(sp,3.0) +
	252*P0x*P1x*Q1x*Q3x*pow(sp,2.0) - 420*P0x*P1x*Q1x*Q2x*pow(sp,4.0) - 756*P0x*P1x*Q1x*Q3x*pow(sp,3.0) + 420*P0x*P1x*Q1x*Q3x*pow(sp,4.0) - 196*P0x*P1x*Q2x*Q3x*pow(sp,3.0) + 126*P0x*P1x*Q2x*Q3x*pow(sp,4.0) +
	162*P0x*P0y*Q1x*Q2y*pow(sp,2.0) + 162*P0x*P0y*Q2x*Q1y*pow(sp,2.0) + 72*P0x*P1x*Q1y*Q2y*pow(sp,2.0) - 234*P0x*P0y*Q1x*Q2y*pow(sp,3.0) - 162*P0x*P0y*Q1x*Q3y*pow(sp,2.0) - 234*P0x*P0y*Q2x*Q1y*pow(sp,3.0) +
	54*P0x*P0y*Q2x*Q2y*pow(sp,2.0) - 162*P0x*P0y*Q3x*Q1y*pow(sp,2.0) - 162*P0x*P1y*Q1x*Q2y*pow(sp,2.0) - 162*P0x*P1y*Q2x*Q1y*pow(sp,2.0) - 162*P1x*P0y*Q1x*Q2y*pow(sp,2.0) - 162*P1x*P0y*Q2x*Q1y*pow(sp,2.0) +
	72*P0y*P1y*Q1x*Q2x*pow(sp,2.0) + 288*P0x*P1x*Q1y*Q2y*pow(sp,3.0) - 72*P0x*P1x*Q1y*Q3y*pow(sp,2.0) + 90*P0x*P0y*Q1x*Q2y*pow(sp,4.0) + 234*P0x*P0y*Q1x*Q3y*pow(sp,3.0) + 90*P0x*P0y*Q2x*Q1y*pow(sp,4.0) -
	162*P0x*P0y*Q2x*Q2y*pow(sp,3.0) - 36*P0x*P0y*Q2x*Q3y*pow(sp,2.0) + 234*P0x*P0y*Q3x*Q1y*pow(sp,3.0) - 36*P0x*P0y*Q3x*Q2y*pow(sp,2.0) + 234*P0x*P1y*Q1x*Q2y*pow(sp,3.0) + 162*P0x*P1y*Q1x*Q3y*pow(sp,2.0) +
	234*P0x*P1y*Q2x*Q1y*pow(sp,3.0) - 54*P0x*P1y*Q2x*Q2y*pow(sp,2.0) + 162*P0x*P1y*Q3x*Q1y*pow(sp,2.0) + 234*P1x*P0y*Q1x*Q2y*pow(sp,3.0) + 162*P1x*P0y*Q1x*Q3y*pow(sp,2.0) + 234*P1x*P0y*Q2x*Q1y*pow(sp,3.0) -
	54*P1x*P0y*Q2x*Q2y*pow(sp,2.0) + 162*P1x*P0y*Q3x*Q1y*pow(sp,2.0) + 162*P1x*P1y*Q1x*Q2y*pow(sp,2.0) + 162*P1x*P1y*Q2x*Q1y*pow(sp,2.0) + 288*P0y*P1y*Q1x*Q2x*pow(sp,3.0) - 72*P0y*P1y*Q1x*Q3x*pow(sp,2.0) -
	240*P0x*P1x*Q1y*Q2y*pow(sp,4.0) - 288*P0x*P1x*Q1y*Q3y*pow(sp,3.0) - 72*P0x*P1x*Q2y*Q3y*pow(sp,2.0) - 90*P0x*P0y*Q1x*Q3y*pow(sp,4.0) + 72*P0x*P0y*Q2x*Q2y*pow(sp,4.0) + 66*P0x*P0y*Q2x*Q3y*pow(sp,3.0) -
	90*P0x*P0y*Q3x*Q1y*pow(sp,4.0) + 66*P0x*P0y*Q3x*Q2y*pow(sp,3.0) + 18*P0x*P0y*Q3x*Q3y*pow(sp,2.0) - 90*P0x*P1y*Q1x*Q2y*pow(sp,4.0) - 234*P0x*P1y*Q1x*Q3y*pow(sp,3.0) - 90*P0x*P1y*Q2x*Q1y*pow(sp,4.0) +
	162*P0x*P1y*Q2x*Q2y*pow(sp,3.0) + 36*P0x*P1y*Q2x*Q3y*pow(sp,2.0) - 234*P0x*P1y*Q3x*Q1y*pow(sp,3.0) + 36*P0x*P1y*Q3x*Q2y*pow(sp,2.0) - 90*P1x*P0y*Q1x*Q2y*pow(sp,4.0) - 234*P1x*P0y*Q1x*Q3y*pow(sp,3.0) -
	90*P1x*P0y*Q2x*Q1y*pow(sp,4.0) + 162*P1x*P0y*Q2x*Q2y*pow(sp,3.0) + 36*P1x*P0y*Q2x*Q3y*pow(sp,2.0) - 234*P1x*P0y*Q3x*Q1y*pow(sp,3.0) + 36*P1x*P0y*Q3x*Q2y*pow(sp,2.0) - 234*P1x*P1y*Q1x*Q2y*pow(sp,3.0) -
	162*P1x*P1y*Q1x*Q3y*pow(sp,2.0) - 234*P1x*P1y*Q2x*Q1y*pow(sp,3.0) + 54*P1x*P1y*Q2x*Q2y*pow(sp,2.0) - 162*P1x*P1y*Q3x*Q1y*pow(sp,2.0) - 240*P0y*P1y*Q1x*Q2x*pow(sp,4.0) - 288*P0y*P1y*Q1x*Q3x*pow(sp,3.0) -
	72*P0y*P1y*Q2x*Q3x*pow(sp,2.0) + 240*P0x*P1x*Q1y*Q3y*pow(sp,4.0) - 64*P0x*P1x*Q2y*Q3y*pow(sp,3.0) - 27*P0x*P0y*Q2x*Q3y*pow(sp,4.0) - 27*P0x*P0y*Q3x*Q2y*pow(sp,4.0) + 30*P0x*P0y*Q3x*Q3y*pow(sp,3.0) +
	90*P0x*P1y*Q1x*Q3y*pow(sp,4.0) - 72*P0x*P1y*Q2x*Q2y*pow(sp,4.0) - 66*P0x*P1y*Q2x*Q3y*pow(sp,3.0) + 90*P0x*P1y*Q3x*Q1y*pow(sp,4.0) - 66*P0x*P1y*Q3x*Q2y*pow(sp,3.0) - 18*P0x*P1y*Q3x*Q3y*pow(sp,2.0) +
	90*P1x*P0y*Q1x*Q3y*pow(sp,4.0) - 72*P1x*P0y*Q2x*Q2y*pow(sp,4.0) - 66*P1x*P0y*Q2x*Q3y*pow(sp,3.0) + 90*P1x*P0y*Q3x*Q1y*pow(sp,4.0) - 66*P1x*P0y*Q3x*Q2y*pow(sp,3.0) - 18*P1x*P0y*Q3x*Q3y*pow(sp,2.0) +
	90*P1x*P1y*Q1x*Q2y*pow(sp,4.0) + 234*P1x*P1y*Q1x*Q3y*pow(sp,3.0) + 90*P1x*P1y*Q2x*Q1y*pow(sp,4.0) - 162*P1x*P1y*Q2x*Q2y*pow(sp,3.0) - 36*P1x*P1y*Q2x*Q3y*pow(sp,2.0) + 234*P1x*P1y*Q3x*Q1y*pow(sp,3.0) -
	36*P1x*P1y*Q3x*Q2y*pow(sp,2.0) + 240*P0y*P1y*Q1x*Q3x*pow(sp,4.0) - 64*P0y*P1y*Q2x*Q3x*pow(sp,3.0) + 72*P0x*P1x*Q2y*Q3y*pow(sp,4.0) - 18*P0x*P0y*Q3x*Q3y*pow(sp,4.0) + 27*P0x*P1y*Q2x*Q3y*pow(sp,4.0) +
	27*P0x*P1y*Q3x*Q2y*pow(sp,4.0) - 30*P0x*P1y*Q3x*Q3y*pow(sp,3.0) + 27*P1x*P0y*Q2x*Q3y*pow(sp,4.0) + 27*P1x*P0y*Q3x*Q2y*pow(sp,4.0) - 30*P1x*P0y*Q3x*Q3y*pow(sp,3.0) - 90*P1x*P1y*Q1x*Q3y*pow(sp,4.0) +
	72*P1x*P1y*Q2x*Q2y*pow(sp,4.0) + 66*P1x*P1y*Q2x*Q3y*pow(sp,3.0) - 90*P1x*P1y*Q3x*Q1y*pow(sp,4.0) + 66*P1x*P1y*Q3x*Q2y*pow(sp,3.0) + 18*P1x*P1y*Q3x*Q3y*pow(sp,2.0) + 72*P0y*P1y*Q2x*Q3x*pow(sp,4.0) +
	18*P0x*P1y*Q3x*Q3y*pow(sp,4.0) + 18*P1x*P0y*Q3x*Q3y*pow(sp,4.0) - 27*P1x*P1y*Q2x*Q3y*pow(sp,4.0) - 27*P1x*P1y*Q3x*Q2y*pow(sp,4.0) + 30*P1x*P1y*Q3x*Q3y*pow(sp,3.0) - 18*P1x*P1y*Q3x*Q3y*pow(sp,4.0) -
	252*P0y*P1y*Q1y*Q2y*pow(sp,2.0) + 756*P0y*P1y*Q1y*Q2y*pow(sp,3.0) + 252*P0y*P1y*Q1y*Q3y*pow(sp,2.0) - 420*P0y*P1y*Q1y*Q2y*pow(sp,4.0) - 756*P0y*P1y*Q1y*Q3y*pow(sp,3.0) + 420*P0y*P1y*Q1y*Q3y*pow(sp,4.0) -
	196*P0y*P1y*Q2y*Q3y*pow(sp,3.0) + 126*P0y*P1y*Q2y*Q3y*pow(sp,4.0))/(3*(7*pow(P0x,2.0)*pow(Q2x,2.0) + 7*pow(P0x,2.0)*pow(Q3x,2.0) + 7*pow(P1x,2.0)*pow(Q2x,2.0) + 7*pow(P1x,2.0)*pow(Q3x,2.0) +
	16*pow(P0x,2.0)*pow(Q2y,2.0) + 16*pow(P0y,2.0)*pow(Q2x,2.0) + 16*pow(P0x,2.0)*pow(Q3y,2.0) + 16*pow(P1x,2.0)*pow(Q2y,2.0) + 16*pow(P0y,2.0)*pow(Q3x,2.0) + 16*pow(P1y,2.0)*pow(Q2x,2.0) + 16*pow(P1x,2.0)*pow(Q3y,2.0) +
	16*pow(P1y,2.0)*pow(Q3x,2.0) + 7*pow(P0y,2.0)*pow(Q2y,2.0) + 7*pow(P0y,2.0)*pow(Q3y,2.0) + 7*pow(P1y,2.0)*pow(Q2y,2.0) + 7*pow(P1y,2.0)*pow(Q3y,2.0) - 14*P0x*P1x*pow(Q2x,2.0) - 14*P0x*P1x*pow(Q3x,2.0) -
	32*P0x*P1x*pow(Q2y,2.0) - 32*P0x*P1x*pow(Q3y,2.0) - 32*P0y*P1y*pow(Q2x,2.0) - 32*P0y*P1y*pow(Q3x,2.0) - 14*P0y*P1y*pow(Q2y,2.0) - 14*P0y*P1y*pow(Q3y,2.0) - 14*pow(P0x,2.0)*Q2x*Q3x - 14*pow(P1x,2.0)*Q2x*Q3x -
	32*pow(P0y,2.0)*Q2x*Q3x - 32*pow(P1y,2.0)*Q2x*Q3x - 32*pow(P0x,2.0)*Q2y*Q3y - 32*pow(P1x,2.0)*Q2y*Q3y - 14*pow(P0y,2.0)*Q2y*Q3y - 14*pow(P1y,2.0)*Q2y*Q3y + 28*P0x*P1x*Q2x*Q3x - 18*P0x*P0y*Q2x*Q2y + 18*P0x*P0y*Q2x*Q3y +
	18*P0x*P0y*Q3x*Q2y + 18*P0x*P1y*Q2x*Q2y + 18*P1x*P0y*Q2x*Q2y + 64*P0x*P1x*Q2y*Q3y - 18*P0x*P0y*Q3x*Q3y - 18*P0x*P1y*Q2x*Q3y - 18*P0x*P1y*Q3x*Q2y - 18*P1x*P0y*Q2x*Q3y - 18*P1x*P0y*Q3x*Q2y - 18*P1x*P1y*Q2x*Q2y +
	64*P0y*P1y*Q2x*Q3x + 18*P0x*P1y*Q3x*Q3y + 18*P1x*P0y*Q3x*Q3y + 18*P1x*P1y*Q2x*Q3y + 18*P1x*P1y*Q3x*Q2y - 18*P1x*P1y*Q3x*Q3y + 28*P0y*P1y*Q2y*Q3y));
	
	// Just an arbitrary correction heuristic for negative factors ...
	if (out_pfactor < 0)
		out_pfactor = -0.1 * out_pfactor;
	if (out_qfactor < 0)
		out_qfactor = -0.1 * out_qfactor;
}

float PathObject::calcBezierPointDeletionRetainingShapeCost(MapCoord p0, MapCoordF p1, MapCoordF p2, MapCoord p3, PathObject* reference)
{
	const int num_test_points = 20;
	QBezierCopy curve = QBezierCopy::fromPoints(p0.toQPointF(), p1.toQPointF(), p2.toQPointF(), p3.toQPointF());
	
	float cost = 0;
	for (int i = 0; i < num_test_points; ++i)
	{
		QPointF point = curve.pointAt((i + 1) / (float)(num_test_points + 1));
		float distance_sq;
		PathCoord path_coord;
		reference->calcClosestPointOnPath(MapCoordF(point), distance_sq, path_coord);
		cost += distance_sq;
	}
	// Just some random scaling to pretend that we have 50 sample points
	return cost * (50 / 20.0f);
}
void PathObject::calcBezierPointDeletionRetainingShapeOptimization(MapCoord p0, MapCoord p1, MapCoord p2, MapCoord q0, MapCoord q1, MapCoord q2, MapCoord q3, double& out_pfactor, double& out_qfactor)
{
	const float gradient_abort_threshold = 0.05f;	// if the gradient magnitude is lower than this over num_abort_steps step, the optimization is aborted
	const float decrease_abort_threshold = 0.004f;	// if the cost descrease if lower than this over num_abort_steps step, the optimization is aborted
	const int num_abort_steps = 2;
	const double derivative_delta = 0.05;
	const int num_tested_step_sizes = 5;
	const int max_num_line_search_iterations = 5;
	float step_size_stepping_base = 0.001f;
	
	PathObject old_curve;
	old_curve.setSymbol(map->getCoveringRedLine(), true);	// HACK; needed to generate path coords
	p0.setCurveStart(true);
	old_curve.addCoordinate(p0);
	old_curve.addCoordinate(p1);
	old_curve.addCoordinate(p2);
	q0.setCurveStart(true);
	old_curve.addCoordinate(q0);
	old_curve.addCoordinate(q1);
	old_curve.addCoordinate(q2);
	q3.setCurveStart(false);
	q3.setClosePoint(false);
	q3.setHolePoint(false);
	old_curve.addCoordinate(q3);
	old_curve.update();
	
	float cur_cost;
	int num_no_improvement_iterations = 0;
	float old_gradient[2];
	float old_step_dir[2];
	for (int i = 0; i < 30; ++i)
	{
		MapCoordF p_direction = MapCoordF(p1) - MapCoordF(p0);
		MapCoordF r1 = MapCoordF(p0) + out_pfactor * p_direction;
		MapCoordF q_direction = MapCoordF(q2) - MapCoordF(q3);
		MapCoordF r2 = MapCoordF(q3) + out_qfactor * q_direction;
		
		// Calculate gradient and cost (if first iteration)
		if (i == 0)
			cur_cost = calcBezierPointDeletionRetainingShapeCost(p0, r1, r2, q3, &old_curve);
		//if (i == 0)
		//	qDebug() << "\nStart cost: " << cur_cost;
		
		float gradient[2];
		gradient[0] = (calcBezierPointDeletionRetainingShapeCost(p0, r1 + derivative_delta * p_direction, r2, q3, &old_curve) -
		               calcBezierPointDeletionRetainingShapeCost(p0, r1 - derivative_delta * p_direction, r2, q3, &old_curve)) / (2 * derivative_delta);
		gradient[1] = (calcBezierPointDeletionRetainingShapeCost(p0, r1, r2 + derivative_delta * q_direction, q3, &old_curve) -
		               calcBezierPointDeletionRetainingShapeCost(p0, r1, r2 - derivative_delta * q_direction, q3, &old_curve)) / (2 * derivative_delta);
		
		// Calculate step direction
		float step_dir_p;
		float step_dir_q;
		float conjugate_gradient_factor = 0;
		if (i == 0)
		{
			// Steepest descent
			step_dir_p = -gradient[0];
			step_dir_q = -gradient[1];
		}
		else
		{
			// Conjugate gradient
			// Fletcher - Reeves:
			//conjugate_gradient_factor = (pow(gradient[0], 2.0) + pow(gradient[1], 2.0)) / (pow(old_gradient[0], 2.0) + pow(old_gradient[1], 2.0));
			// Polak – Ribiere:
			conjugate_gradient_factor = qMax(0.0, ( gradient[0] * (gradient[0] - old_gradient[0]) + gradient[1] * (gradient[1] - old_gradient[1]) ) /
			                                      ( pow(old_gradient[0], 2.0) + pow(old_gradient[1], 2.0) ));
			//qDebug() << "Factor: " << conjugate_gradient_factor;
			step_dir_p = -gradient[0] + conjugate_gradient_factor * old_step_dir[0];
			step_dir_q = -gradient[1] + conjugate_gradient_factor * old_step_dir[1];
		}
		
		// Line search in step direction for lowest cost
		float best_step_size = 0;
		float best_step_size_cost = cur_cost;
		int best_step_factor = 0;
		float adjusted_step_size_stepping_base = step_size_stepping_base;
		
		for (int iteration = 0; iteration < max_num_line_search_iterations; ++iteration)
		{
			const float step_size_stepping = adjusted_step_size_stepping_base * (out_pfactor + out_qfactor) / 2; // * qSqrt(step_dir_p*step_dir_p + step_dir_q*step_dir_q); // qSqrt(cur_cost);
			for (int step_test = 1; step_test <= num_tested_step_sizes; ++step_test)
			{
				float step_size = step_test * step_size_stepping;
				float test_p_step = step_size * step_dir_p;
				float test_q_step = step_size * step_dir_q;
				
				MapCoordF test_r1 = r1 + test_p_step * p_direction;
				MapCoordF test_r2 = r2 + test_q_step * q_direction;
				float test_cost = calcBezierPointDeletionRetainingShapeCost(p0, test_r1, test_r2, q3, &old_curve);
				if (test_cost < best_step_size_cost)
				{
					best_step_size_cost = test_cost;
					best_step_size = step_size;
					best_step_factor = step_test;
				}
			}
			//qDebug() << best_step_factor;
			if (best_step_factor == num_tested_step_sizes)
				adjusted_step_size_stepping_base *= num_tested_step_sizes;
			else if (best_step_factor == 0)
				adjusted_step_size_stepping_base *= (1 / (float)num_tested_step_sizes);
			else
				break;
			if (iteration < 3)
				step_size_stepping_base = adjusted_step_size_stepping_base;
		}
		if (best_step_factor == 0 && conjugate_gradient_factor == 0)
			return;
		
		// Update optimized parameters and constrain them to non-negative values
		out_pfactor += best_step_size * step_dir_p;
		if (out_pfactor < 0)
			out_pfactor = 0;
		out_qfactor += best_step_size * step_dir_q;
		if (out_qfactor < 0)
			out_qfactor = 0;
		
		// Abort if gradient is really low for a number of steps
		float gradient_magnitude = qSqrt(gradient[0]*gradient[0] + gradient[1]*gradient[1]);
		//qDebug() << "Gradient magnitude: " << gradient_magnitude;
		if (gradient_magnitude < gradient_abort_threshold || cur_cost - best_step_size_cost < decrease_abort_threshold)
		{
			++num_no_improvement_iterations;
			if (num_no_improvement_iterations == num_abort_steps)
				return;
		}
		else
			num_no_improvement_iterations = 0;
		
		cur_cost = best_step_size_cost;
		old_gradient[0] = gradient[0];
		old_gradient[1] = gradient[1];
		old_step_dir[0] = step_dir_p;
		old_step_dir[1] = step_dir_q;
		//qDebug() << "Cost: " << cur_cost;
	}
}

void PathObject::appendPath(PathObject* other)
{
	int coords_size = coords.size();
	int other_coords_size = (int)other->coords.size();
	
	coords.resize(coords_size + other_coords_size);
	
	for (int i = 0; i < other_coords_size; ++i)
		coords[coords_size + i] = other->coords[i];
	
	recalculateParts();
	setOutputDirty();
}
void PathObject::appendPathPart(PathObject* other, int part_index)
{
	int coords_size = coords.size();
	PathPart& part = other->getPart(part_index);
	
	coords.resize(coords_size + part.getNumCoords());
	
	for (int i = 0; i < part.getNumCoords(); ++i)
		coords[coords_size + i] = other->coords[part.start_index + i];
	
	recalculateParts();
	setOutputDirty();
}

void PathObject::reverse()
{
	int parts_size = parts.size();
	for (int i = 0; i < parts_size; ++i)
		reversePart(i);
}

void PathObject::reversePart(int part_index)
{
	PathPart& part = parts[part_index];
	
	bool set_last_hole_point = false;
	int half = (part.end_index + part.start_index + 1) / 2;
	for (int c = part.start_index; c <= part.end_index; ++c)
	{
		MapCoord coord = coords[c];
		if (c < half)
		{
			int mirror_index = part.end_index - (c - part.start_index);
			coords[c] = coords[mirror_index];
			coords[mirror_index] = coord;
		}
		
		if (!(c == part.start_index && part.isClosed()) && coords[c].isCurveStart())
		{
			assert((c - part.start_index) >= 3);
			coords[c - 3].setCurveStart(true);
			if (!(c == part.end_index && part.isClosed()))
				coords[c].setCurveStart(false);
		}
		else if (coords[c].isHolePoint())
		{
			coords[c].setHolePoint(false);
			if (c >= part.start_index + 1)
				coords[c - 1].setHolePoint(true);
			else
				set_last_hole_point = true;
		}
	}
	
	if (coords[part.start_index].isClosePoint())
	{
		coords[part.start_index].setClosePoint(false);
		coords[part.end_index].setClosePoint(true);
	}
	if (set_last_hole_point)
		coords[part.end_index].setHolePoint(true);
	
	setOutputDirty();
}

void PathObject::closeAllParts()
{
	for (int path_part = 0; path_part < getNumParts(); ++path_part)
	{
		if (!getPart(path_part).isClosed())
			getPart(path_part).setClosed(true, true);
	}
}

int PathObject::isPointOnPath(MapCoordF coord, float tolerance, bool treat_areas_as_paths)
{
	Symbol::Type contained_types = symbol->getContainedTypes();
	int coords_size = (int)coords.size();
	
	if ((contained_types & Symbol::Line || treat_areas_as_paths) && tolerance > 0)
	{
		update(false);
		int size = (int)path_coords.size();
		for (int i = 0; i < size - 1; ++i)
		{
			assert(path_coords[i].index < coords_size);
			if (coords[path_coords[i].index].isHolePoint())
				continue;
			
			MapCoordF to_coord = MapCoordF(coord.getX() - path_coords[i].pos.getX(), coord.getY() - path_coords[i].pos.getY());
			MapCoordF to_next = MapCoordF(path_coords[i+1].pos.getX() - path_coords[i].pos.getX(), path_coords[i+1].pos.getY() - path_coords[i].pos.getY());
			MapCoordF tangent = to_next;
			tangent.normalize();
			
			float dist_along_line = to_coord.dot(tangent);
			if (dist_along_line < -tolerance)
				continue;
			else if (dist_along_line < 0 && to_coord.lengthSquared() <= tolerance*tolerance)
				return Symbol::Line;
			
			float line_length = path_coords[i+1].clen - path_coords[i].clen;
			if (line_length < 1e-7)
				continue;
			if (dist_along_line > line_length + tolerance)
				continue;
			else if (dist_along_line > line_length && coord.lengthToSquared(path_coords[i+1].pos) <= tolerance*tolerance)
				return Symbol::Line;
			
			MapCoordF right = tangent;
			right.perpRight();
			
			float dist_from_line = qAbs(right.dot(to_coord));
			if (dist_from_line <= tolerance)
				return Symbol::Line;
		}
	}
	
	// Check for area selection
	if ((contained_types & Symbol::Area) && !treat_areas_as_paths)
	{
		update(false);
		bool inside = false;
		for (int part = 0, end = (int)parts.size(); part < end; ++part)
		{
			int size = parts[part].path_coord_end_index + 1;
			int i, j;
			for (i = parts[part].path_coord_start_index, j = size - 1; i < size; j = i++)
			{
				if ( ((path_coords[i].pos.getY() > coord.getY()) != (path_coords[j].pos.getY() > coord.getY())) &&
					(coord.getX() < (path_coords[j].pos.getX() - path_coords[i].pos.getX()) *
					(coord.getY() - path_coords[i].pos.getY()) / (path_coords[j].pos.getY() - path_coords[i].pos.getY()) + path_coords[i].pos.getX()) )
					inside = !inside;
			}
		}
		if (inside)
			return Symbol::Area;
	}
	
	return Symbol::NoSymbol;
}

void PathObject::setCoordinate(int pos, MapCoord c)
{
	assert(pos >= 0 && pos < getCoordinateCount());
	
	PathPart& part = findPartForIndex(pos);
	if (part.isClosed() && pos == part.end_index)
		pos = part.start_index;
	coords[pos] = c;
	if (part.isClosed() && pos == part.start_index)
		setClosingPoint(part.end_index, c);
	
	setOutputDirty();
}
void PathObject::addCoordinate(int pos, MapCoord c)
{
	assert(pos >= 0 && pos <= getCoordinateCount());
	int part_index = coords.empty() ? -1 : findPartIndexForIndex(qMin(pos, (int)coords.size() - 1));
	coords.insert(coords.begin() + pos, c);
	
	if (part_index >= 0)
	{
		++parts[part_index].end_index;
		for (int i = part_index + 1; i < (int)parts.size(); ++i)
		{
			++parts[i].start_index;
			++parts[i].end_index;
		}
		
		if (parts[part_index].isClosed() && parts[part_index].start_index == pos)
			setClosingPoint(parts[part_index].end_index, c);
	}
	else
	{
		PathPart part;
		part.start_index = 0;
		part.end_index = 0;
		part.path = this;
		parts.push_back(part);
	}
	
	setOutputDirty();
}
void PathObject::addCoordinate(MapCoord c, bool start_new_part)
{
	if (!start_new_part && !parts.empty() && parts[parts.size() - 1].isClosed())
	{
		coords.insert(coords.begin() + (coords.size() - 1), c);
		++parts[parts.size() - 1].end_index;
	}
	else
	{
		coords.push_back(c);
		if (start_new_part || parts.empty())
		{
			if (!parts.empty())
				assert(parts[parts.size() - 1].isClosed());
			PathPart part;
			part.start_index = (int)coords.size() - 1;
			part.end_index = (int)coords.size() - 1;
			part.path = this;
			parts.push_back(part);
		}
		else
			++parts[parts.size() - 1].end_index;
	}
	
	setOutputDirty();
}
void PathObject::deleteCoordinate(int pos, bool adjust_other_coords, int delete_bezier_point_action)
{
	assert(pos >= 0 && pos < getCoordinateCount());
	MapCoord old_coord = coords[pos];
	coords.erase(coords.begin() + pos);
	
	int part_index = findPartIndexForIndex(pos);
	PathPart& part = parts[part_index];
	partSizeChanged(part_index, -1);
	
	if (parts[part_index].isClosed())
	{
		if (parts[part_index].end_index - parts[part_index].start_index == 0)
		{
			deletePart(part_index);
			return;
		}
		else if (pos == parts[part_index].start_index)
			setClosingPoint(parts[part_index].end_index, coords[parts[part_index].start_index]);
	}
	else if (parts[part_index].end_index - parts[part_index].start_index == -1)
	{
		deletePart(part_index);
		return;
	}
	
	if (adjust_other_coords && part.getNumCoords() >= 2)
	{
		// Deleting points at line ends
		if (!part.isClosed())
		{
			if (pos == part.start_index && old_coord.isCurveStart())
			{
				deleteCoordinate(part.start_index + 1, false);
				deleteCoordinate(part.start_index, false);
				return;
			}
			else if (pos == part.end_index + 1 && pos >= part.start_index + 3 && coords[pos - 3].isCurveStart())
			{
				deleteCoordinate(pos - 1, false);
				deleteCoordinate(pos - 2, false);
				coords[pos - 3].setCurveStart(false);
				return;
			}
		}
		
		// Too few points for a closed path? Open the path
		if (part.isClosed() && part.getNumCoords() < 4)
		{
			part.setClosed(false);
			deleteCoordinate(part.end_index, false);
			getCoordinate(part.end_index).setHolePoint(true);
		}
		
		if (old_coord.isHolePoint())
			shiftedCoord(pos, -1, part).setHolePoint(true);
		if (old_coord.isCurveStart())
		{
			if ((pos - part.start_index >= 3 && coords[pos - 3].isCurveStart()) || (part.isClosed() && part.getNumCoords() >= 4 && shiftedCoord(pos, -3, part).isCurveStart()))
			{
				// Bezier curves on both sides. Merge curves to one and adjust handle positions to preserve the original shape somewhat (of course, in general it is impossible to do this)
				MapCoord& p0 = shiftedCoord(pos, -3, part);
				MapCoord& p1 = shiftedCoord(pos, -2, part);
				MapCoord& p2 = shiftedCoord(pos, -1, part);
				MapCoord& q0 = old_coord;
				MapCoord& q1 = coords[pos];
				MapCoord& q2 = coords[pos + 1];
				MapCoord& q3 = coords[pos + 2];
				
				double pfactor, qfactor;
				if (delete_bezier_point_action == Settings::DeleteBezierPoint_KeepHandles)
				{
					pfactor = 1;
					qfactor = 1;
				}
				else if (delete_bezier_point_action == Settings::DeleteBezierPoint_ResetHandles)
				{
					double target_length = 0.4 * p0.lengthTo(q3);
					pfactor = target_length / qMax(p0.lengthTo(p1), 0.01);
					qfactor = target_length / qMax(q3.lengthTo(q2), 0.01);
				}
				else if (delete_bezier_point_action == Settings::DeleteBezierPoint_RetainExistingShape)
				{
					calcBezierPointDeletionRetainingShapeFactors(p0, p1, p2, q0, q1, q2, q3, pfactor, qfactor);
					calcBezierPointDeletionRetainingShapeOptimization(p0, p1, p2, q0, q1, q2, q3, pfactor, qfactor);
					
					double minimum_length = 0.01 * p0.lengthTo(q3);
					pfactor = qMax(minimum_length / qMax(p0.lengthTo(p1), 0.01), pfactor);
					qfactor = qMax(minimum_length / qMax(q3.lengthTo(q2), 0.01), qfactor);
				}
				else
					assert(false);
				
				MapCoordF p0p1 = MapCoordF(p1) - MapCoordF(p0);
				MapCoord r1 = MapCoord(p0.xd() + pfactor * p0p1.getX(), p0.yd() + pfactor * p0p1.getY());
				
				MapCoordF q3q2 = MapCoordF(q2) - MapCoordF(q3);
				MapCoord r2 = MapCoord(q3.xd() + qfactor * q3q2.getX(), q3.yd() + qfactor * q3q2.getY());
				
				deleteCoordinate(pos + 1, false);
				deleteCoordinate(pos, false);
				shiftedCoord(pos, -2, part) = r1;
				shiftedCoord(pos, -1, part) = r2;
			}
			else
			{
				shiftedCoord(pos, -1, part).setCurveStart(true);
				if (part.isClosed() && pos == part.start_index)
				{
					// Zero is at the wrong position - can't start on a bezier curve handle! Switch the handles over to the end of the path
					coords[part.end_index] = coords[part.start_index];
					coords.insert(coords.begin() + (part.end_index + 1), coords[part.start_index + 1]);
					coords.erase(coords.begin() + part.start_index, coords.begin() + (part.start_index + 2));
					coords.insert(coords.begin() + part.end_index, coords[part.start_index]);
					setClosingPoint(part.end_index, coords[part.start_index]);
				}
			}
		}
	}
	
	setOutputDirty();
}
void PathObject::clearCoordinates()
{
	coords.clear();
	parts.clear();
	setOutputDirty();
}

void PathObject::updatePathCoords(MapCoordVectorF& float_coords)
{
	path_coords.clear();
	
	int part_index = 0;
	int part_start = 0;
	int part_end = 0;
	int path_coord_start = 0;
	while (PathCoord::getNextPathPart(coords, float_coords, part_start, part_end, &path_coords, false, true))
	{
		assert(parts[part_index].start_index == part_start);
		assert(parts[part_index].end_index == part_end);
		parts[part_index].path_coord_start_index = path_coord_start;
		parts[part_index].path_coord_end_index = path_coords.size() - 1;
		path_coord_start = parts[part_index].path_coord_end_index + 1;
		
		++part_index;
	}
}

void PathObject::recalculateParts()
{
	parts.clear();
	
	int start = 0;
	int coords_size = coords.size();
	for (int i = 0; i < coords_size - 1; ++i)
	{
		if (coords[i].isCurveStart())
			i += 2;
		else if (coords[i].isHolePoint())
		{
			PathPart part;
			part.start_index = start;
			part.end_index = i;
			part.path = this;
			parts.push_back(part);
			start = i + 1;
			++i;	// assume that there are never two hole points in a row
		}
	}
	
	PathPart part;
	part.start_index = start;
	part.end_index = coords_size - 1;
	part.path = this;
	parts.push_back(part);
}
void PathObject::setClosingPoint(int index, MapCoord coord)
{
	coord.setCurveStart(false);
	coord.setHolePoint(true);
	coord.setClosePoint(true);
	coords[index] = coord;
}

// ### PointObject ###

PointObject::PointObject(Symbol* symbol) : Object(Object::Point, symbol)
{
	assert(!symbol || (symbol->getType() == Symbol::Point));
	rotation = 0;
	coords.push_back(MapCoord(0, 0));
}
Object* PointObject::duplicate()
{
	PointObject* new_point = new PointObject(symbol);
	new_point->coords = coords;
	new_point->rotation = rotation;
	return new_point;
}

void PointObject::setPosition(qint64 x, qint64 y)
{
	coords[0].setRawX(x);
	coords[0].setRawY(y);
	setOutputDirty();
}
void PointObject::setPosition(MapCoordF coord)
{
	coords[0].setX(coord.getX());
	coords[0].setY(coord.getY());
	setOutputDirty();
}
void PointObject::getPosition(qint64& x, qint64& y) const
{
	x = coords[0].rawX();
	y = coords[0].rawY();
}
MapCoordF PointObject::getCoordF() const
{
	return MapCoordF(coords[0]);
}
MapCoord PointObject::getCoord() const
{
	return coords[0];
}
void PointObject::setRotation(float new_rotation)
{
	PointSymbol* point = reinterpret_cast<PointSymbol*>(symbol);
	assert(point && point->isRotatable());
	
	rotation = new_rotation;
	setOutputDirty();
}
