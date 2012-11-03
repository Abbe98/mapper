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


#include "map.h"

#include <cassert>
#include <algorithm>

#include <QDebug>
#include <QFile>
#include <qmath.h>
#include <QDir>
#include <QMessageBox>
#include <QPainter>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "map_color.h"
#include "map_editor.h"
#include "map_grid.h"
#include "map_widget.h"
#include "map_undo.h"
#include "util.h"
#include "template.h"
#include "gps_coordinates.h"
#include "object.h"
#include "object_operations.h"
#include "renderable.h"
#include "symbol.h"
#include "symbol_point.h"
#include "symbol_line.h"
#include "symbol_combined.h"
#include "file_format_ocad8.h"
#include "file_format_xml.h"
#include "georeferencing.h"

MapPart::MapPart(const QString& name, Map* map) : name(name), map(map)
{
}
MapPart::~MapPart()
{
	int size = (int)objects.size();
	for (int i = 0; i < size; ++i)
		delete objects[i];
}
void MapPart::save(QIODevice* file, Map* map)
{
	saveString(file, name);
	
	int size = (int)objects.size();
	file->write((const char*)&size, sizeof(int));
	
	for (int i = 0; i < size; ++i)
	{
		int save_type = static_cast<int>(objects[i]->getType());
		file->write((const char*)&save_type, sizeof(int));
		objects[i]->save(file);
	}
}

bool MapPart::load(QIODevice* file, int version, Map* map)
{
	loadString(file, name);
	
	int size;
	file->read((char*)&size, sizeof(int));
	objects.resize(size);
	
	for (int i = 0; i < size; ++i)
	{
		int save_type;
		file->read((char*)&save_type, sizeof(int));
		objects[i] = Object::getObjectForType(static_cast<Object::Type>(save_type), NULL);
		if (!objects[i])
			return false;
		objects[i]->load(file, version, map);
	}
	return true;
}

void MapPart::save(QXmlStreamWriter& xml, const Map& map) const
{
	xml.writeStartElement("part");
	xml.writeAttribute("name", name);
	
	xml.writeStartElement("objects");
	int size = (int)objects.size();
	xml.writeAttribute("count", QString::number(size));
	for (int i = 0; i < size; ++i)
	{
		objects[i]->save(xml);
	}
	xml.writeEndElement(/*objects*/);
	
	xml.writeEndElement(/*part*/);
}

MapPart* MapPart::load(QXmlStreamReader& xml, Map& map, SymbolDictionary& symbol_dict)
{
	Q_ASSERT(xml.name() == "part");
	
	MapPart* part = new MapPart(xml.attributes().value("name").toString(), &map);
	
	while (xml.readNextStartElement())
	{
		if (xml.name() == "objects")
		{
			int num_objects = xml.attributes().value("count").toString().toInt();
			part->objects.reserve(qMin(num_objects, 50000)); // 50000 is not a limit
			while (xml.readNextStartElement())
			{
				if (xml.name() == "object")
					part->objects.push_back(Object::load(xml, &map, symbol_dict));
				else
					xml.skipCurrentElement(); // unknown
			}
		}
		else
			xml.skipCurrentElement(); // unknown
	}
	
	return part;
}

int MapPart::findObjectIndex(Object* object)
{
	int size = objects.size();
	for (int i = size - 1; i >= 0; --i)
	{
		if (objects[i] == object)
			return i;
	}
	assert(false);
	return -1;
}
void MapPart::setObject(Object* object, int pos, bool delete_old)
{
	map->removeRenderablesOfObject(objects[pos], true);
	if (delete_old)
		delete objects[pos];
	
	objects[pos] = object;
	bool delete_old_renderables = object->getMap() == map;
	object->setMap(map);
	object->update(true, delete_old_renderables);
	map->setObjectsDirty();
}
void MapPart::addObject(Object* object, int pos)
{
	objects.insert(objects.begin() + pos, object);
	object->setMap(map);
	object->update(true, true);
	map->setObjectsDirty();
	
	if (map->getNumObjects() == 1)
		map->updateAllMapWidgets();
}
void MapPart::deleteObject(int pos, bool remove_only)
{
	map->removeRenderablesOfObject(objects[pos], true);
	if (remove_only)
		objects[pos]->setMap(NULL);
	else
		delete objects[pos];
	objects.erase(objects.begin() + pos);
	map->setObjectsDirty();
	
	if (map->getNumObjects() == 0)
		map->updateAllMapWidgets();
}
bool MapPart::deleteObject(Object* object, bool remove_only)
{
	int size = objects.size();
	for (int i = size - 1; i >= 0; --i)
	{
		if (objects[i] == object)
		{
			deleteObject(i, remove_only);
			return true;
		}
	}
	return false;
}

void MapPart::importPart(MapPart* other, QHash<Symbol*, Symbol*>& symbol_map, bool select_new_objects)
{
	if (other->getNumObjects() == 0)
		return;
	
	bool first_objects = map->getNumObjects() == 0;
	DeleteObjectsUndoStep* undo_step = new DeleteObjectsUndoStep(map);
	if (select_new_objects)
		map->clearObjectSelection(false);
	
	objects.reserve(objects.size() + other->objects.size());
	for (size_t i = 0, end = other->objects.size(); i < end; ++i)
	{
		Object* new_object = other->objects[i]->duplicate();
		if (symbol_map.contains(new_object->getSymbol()))
			new_object->setSymbol(symbol_map.value(new_object->getSymbol()), true);
		
		objects.push_back(new_object);
		new_object->setMap(map);
		new_object->update(true, true);
		
		undo_step->addObject((int)objects.size() - 1);
		if (select_new_objects)
			map->addObjectToSelection(new_object, false);
	}
	
	map->objectUndoManager().addNewUndoStep(undo_step);
	map->setObjectsDirty();
	if (select_new_objects)
	{
		map->emitSelectionChanged();
		map->emitSelectionEdited();		// TODO: is this necessary here?
	}
	if (first_objects)
		map->updateAllMapWidgets();
}

void MapPart::findObjectsAt(MapCoordF coord, float tolerance, bool treat_areas_as_paths, bool extended_selection, bool include_hidden_objects, bool include_protected_objects, SelectionInfoVector& out)
{
	int size = objects.size();
	for (int i = 0; i < size; ++i)
	{
		if (!include_hidden_objects && objects[i]->getSymbol()->isHidden())
			continue;
		if (!include_protected_objects && objects[i]->getSymbol()->isProtected())
			continue;
		
		objects[i]->update();
		int selected_type = objects[i]->isPointOnObject(coord, tolerance, treat_areas_as_paths, extended_selection);
		if (selected_type != (int)Symbol::NoSymbol)
			out.push_back(std::pair<int, Object*>(selected_type, objects[i]));
	}
}
void MapPart::findObjectsAtBox(MapCoordF corner1, MapCoordF corner2, bool include_hidden_objects, bool include_protected_objects, std::vector< Object* >& out)
{
	QRectF rect = QRectF(corner1.toQPointF(), corner2.toQPointF());
	
	int size = objects.size();
	for (int i = 0; i < size; ++i)
	{
		if (!include_hidden_objects && objects[i]->getSymbol()->isHidden())
			continue;
		if (!include_protected_objects && objects[i]->getSymbol()->isProtected())
			continue;
		
		objects[i]->update();
		if (rect.intersects(objects[i]->getExtent()) && objects[i]->intersectsBox(rect))
			out.push_back(objects[i]);
	}
}

int MapPart::countObjectsInRect(QRectF map_coord_rect, bool include_hidden_objects)
{
	int count = 0;
	int size = objects.size();
	for (int i = 0; i < size; ++i)
	{
		if (objects[i]->getSymbol()->isHidden() && !include_hidden_objects)
			continue;
		objects[i]->update();
		if (objects[i]->getExtent().intersects(map_coord_rect))
			++count;
	}
	return count;
}

QRectF MapPart::calculateExtent(bool include_helper_symbols)
{
	QRectF rect;
	
	int i = 0;
	int size = objects.size();
	while (size > i && !rect.isValid())
	{
		if ((include_helper_symbols || !objects[i]->getSymbol()->isHelperSymbol()) && !objects[i]->getSymbol()->isHidden())
		{
			objects[i]->update();
			rect = objects[i]->getExtent();
		}
		++i;
	}
	
	for (; i < size; ++i)
	{
		if ((include_helper_symbols || !objects[i]->getSymbol()->isHelperSymbol()) && !objects[i]->getSymbol()->isHidden())
		{
			objects[i]->update();
			rectInclude(rect, objects[i]->getExtent());
		}
	}
	
	return rect;
}
void MapPart::scaleAllObjects(double factor)
{
	operationOnAllObjects(ObjectOp::Scale(factor));
}
void MapPart::rotateAllObjects(double rotation)
{
	operationOnAllObjects(ObjectOp::Rotate(rotation));
}
void MapPart::updateAllObjects()
{
	operationOnAllObjects(ObjectOp::Update(true));
}
void MapPart::updateAllObjectsWithSymbol(Symbol* symbol)
{
	operationOnAllObjects(ObjectOp::Update(true), ObjectOp::HasSymbol(symbol));
}
void MapPart::changeSymbolForAllObjects(Symbol* old_symbol, Symbol* new_symbol)
{
	operationOnAllObjects(ObjectOp::ChangeSymbol(new_symbol), ObjectOp::HasSymbol(old_symbol));
}
bool MapPart::deleteAllObjectsWithSymbol(Symbol* symbol)
{
	return operationOnAllObjects(ObjectOp::Delete(), ObjectOp::HasSymbol(symbol)) & ObjectOperationResult::Success;
}
bool MapPart::doObjectsExistWithSymbol(Symbol* symbol)
{
	return operationOnAllObjects(ObjectOp::NoOp(), ObjectOp::HasSymbol(symbol)) & ObjectOperationResult::Success;
}
