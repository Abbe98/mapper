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


#ifndef _OPENORIENTEERING_SYMBOL_LINE_H_
#define _OPENORIENTEERING_SYMBOL_LINE_H_

#include "symbol.h"
#include "path_coord.h"
#include "symbol_properties_widget.h"

QT_BEGIN_NAMESPACE
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QScrollArea;
class QSpinBox;
class QGridLayout;
class QXmlStreamReader;
class QXmlStreamWriter;
QT_END_NAMESPACE

class ColorDropDown;
class SymbolSettingDialog;
class PointSymbolEditorWidget;
class PointSymbol;

struct LineSymbolBorder
{
	MapColor* color;
	int width;
	int shift;
	bool dashed;
	int dash_length;
	int break_length;
	
	void reset();
	void save(QIODevice* file, Map* map);
	bool load(QIODevice* file, int version, Map* map);
	void save(QXmlStreamWriter& xml, const Map& map) const;
	bool load(QXmlStreamReader& xml, Map& map);
	bool equals(const LineSymbolBorder* other) const;
	void assign(const LineSymbolBorder& other, const QHash<MapColor*, MapColor*>* color_map);
	
	bool isVisible() const;
	void createSymbol(LineSymbol& out);
	void scale(double factor);
};


class LineSymbol : public Symbol
{
friend class LineSymbolSettings;
friend class PointSymbolEditorWidget;
friend class OCAD8FileImport;
public:
	enum CapStyle
	{
		FlatCap = 0,
		RoundCap = 1,
		SquareCap = 2,
		PointedCap = 3
	};
	
	enum JoinStyle
	{
		BevelJoin = 0,
		MiterJoin = 1,
		RoundJoin = 2
	};
	
	/// Constructs an empty line symbol
	LineSymbol();
	virtual ~LineSymbol();
	virtual Symbol* duplicate(const QHash<MapColor*, MapColor*>* color_map = NULL) const;
	
	virtual void createRenderables(Object* object, const MapCoordVector& flags, const MapCoordVectorF& coords, ObjectRenderables& output);
	void createRenderables(Object* object, bool path_closed, const MapCoordVector& flags, const MapCoordVectorF& coords, PathCoordVector* path_coords, ObjectRenderables& output);
	virtual void colorDeleted(MapColor* color);
    virtual bool containsColor(MapColor* color);
    virtual MapColor* getDominantColorGuess();
    virtual void scale(double factor);
	
	/// Creates empty point symbols for contained NULL symbols with the given names
    void ensurePointSymbols(const QString& start_name, const QString& mid_name, const QString& end_name, const QString& dash_name);
	/// Deletes unused point symbols and sets them to NULL
	void cleanupPointSymbols();
	
	/// Returns the limit for miter joins in units of the line width.
	/// See the Qt docs for QPainter::setMiterJoin().
	/// TODO: Should that better be a line property?
	static const float miterLimit() {return 1;}
	
	// Getters / Setters
	inline int getLineWidth() const {return line_width;}
	inline void setLineWidth(double width) {line_width = qRound(1000 * width);}
	inline MapColor* getColor() const {return color;}
	inline void setColor(MapColor* color) {this->color = color;}
	inline int getMinimumLength() const {return minimum_length;}
	inline void setMinimumLength(int length) {this->minimum_length = length;}
	inline CapStyle getCapStyle() const {return cap_style;}
	inline void setCapStyle(CapStyle style) {cap_style = style;}
	inline JoinStyle getJoinStyle() const {return join_style;}
	inline void setJoinStyle(JoinStyle style) {join_style = style;}
	inline int getPointedCapLength() const {return pointed_cap_length;}
	inline void setPointedCapLength(int value) {pointed_cap_length = value;}
	inline bool isDashed() const {return dashed;}
	inline void setDashed(bool value) {dashed = value;}
	
	inline PointSymbol* getStartSymbol() const {return start_symbol;}
	void setStartSymbol(PointSymbol* symbol);
	inline PointSymbol* getMidSymbol() const {return mid_symbol;}
	void setMidSymbol(PointSymbol* symbol);
	inline PointSymbol* getEndSymbol() const {return end_symbol;}
	void setEndSymbol(PointSymbol* symbol);
	inline PointSymbol* getDashSymbol() const {return dash_symbol;}
	void setDashSymbol(PointSymbol* symbol);
	
	inline int getMidSymbolsPerSpot() const {return mid_symbols_per_spot;}
	inline void setMidSymbolsPerSpot(int value) {mid_symbols_per_spot = value;}
	inline int getMidSymbolDistance() const {return mid_symbol_distance;}
	inline void setMidSymbolDistance(int value) {mid_symbol_distance = value;}
	
	inline int getSegmentLength() const {return segment_length;}
	inline void setSegmentLength(int value) {segment_length = value;}
	inline int getEndLength() const {return end_length;}
	inline void setEndLength(int value) {end_length = value;}
	inline bool getShowAtLeastOneSymbol() const {return show_at_least_one_symbol;}
	inline void setShowAtLeastOneSymbol(bool value) {show_at_least_one_symbol = value;}
	inline int getMinimumMidSymbolCount() const {return minimum_mid_symbol_count;}
	inline void setMinimumMidSymbolCount(int value) {minimum_mid_symbol_count = value;}
	inline int getMinimumMidSymbolCountWhenClosed() const {return minimum_mid_symbol_count_when_closed;}
	inline void setMinimumMidSymbolCountWhenClosed(int value) {minimum_mid_symbol_count_when_closed = value;}
	
	inline int getDashLength() const {return dash_length;}
	inline void setDashLength(int value) {dash_length = value;}
	inline int getBreakLength() const {return break_length;}
	inline void setBreakLength(int value) {break_length = value;}
	inline int getDashesInGroup() const {return dashes_in_group;}
	inline void setDashesInGroup(int value) {dashes_in_group = value;}
	inline int getInGroupBreakLength() const {return in_group_break_length;}
	inline void setInGroupBreakLength(int value) {in_group_break_length = value;}
	inline bool getHalfOuterDashes() const {return half_outer_dashes;}
	inline void setHalfOuterDashes(bool value) {half_outer_dashes = value;}
	
	inline bool hasBorder() const {return have_border_lines;}
	inline void setHasBorder(bool value) {have_border_lines = value;}
	inline bool areBordersDifferent() const {return !border.equals(&right_border);}
	
	inline LineSymbolBorder& getBorder() {return border;}
	inline const LineSymbolBorder& getBorder() const {return border;}
	inline LineSymbolBorder& getRightBorder() {return right_border;}
	inline const LineSymbolBorder& getRightBorder() const {return right_border;}
	
	virtual SymbolPropertiesWidget* createPropertiesWidget(SymbolSettingDialog* dialog);
	
protected:
	virtual void saveImpl(QIODevice* file, Map* map);
	virtual bool loadImpl(QIODevice* file, int version, Map* map);
	virtual void saveImpl(QXmlStreamWriter& xml, const Map& map) const;
	virtual bool loadImpl(QXmlStreamReader& xml, Map& map, SymbolDictionary& symbol_dict);
	PointSymbol* loadPointSymbol(QXmlStreamReader& xml, Map& map, SymbolDictionary& symbol_dict);
	virtual bool equalsImpl(Symbol* other, Qt::CaseSensitivity case_sensitivity);
	
	void createBorderLines(Object* object, const MapCoordVector& flags, const MapCoordVectorF& coords, bool path_closed, ObjectRenderables& output);
	void createBorderLine(Object* object, const MapCoordVector& flags, const MapCoordVectorF& coords, bool path_closed, ObjectRenderables& output, LineSymbolBorder& border, float shift);
	void shiftCoordinates(const MapCoordVector& flags, const MapCoordVectorF& coords, bool path_closed, float shift, MapCoordVector& out_flags, MapCoordVectorF& out_coords);
	void processContinuousLine(Object* object, bool path_closed, const MapCoordVector& flags, const MapCoordVectorF& coords, const PathCoordVector& line_coords,
							   float start, float end, bool has_start, bool has_end, int& cur_line_coord,
							   MapCoordVector& processed_flags, MapCoordVectorF& processed_coords, bool include_first_point, bool set_mid_symbols, ObjectRenderables& output);
	void createPointedLineCap(Object* object, const MapCoordVector& flags, const MapCoordVectorF& coords, const PathCoordVector& line_coords,
							  float start, float end, int& cur_line_coord, bool is_end, ObjectRenderables& output);
	void processDashedLine(Object* object, bool path_closed, const MapCoordVector& flags, const MapCoordVectorF& coords, MapCoordVector& out_flags, MapCoordVectorF& out_coords, ObjectRenderables& output);
	void createDashSymbolRenderables(Object* object, bool path_closed, const MapCoordVector& flags, const MapCoordVectorF& coords, ObjectRenderables& output);
    void createDottedRenderables(Object* object, bool path_closed, const MapCoordVector& flags, const MapCoordVectorF& coords, ObjectRenderables& output);
	
	void calculateCoordinatesForRange(const MapCoordVector& flags, const MapCoordVectorF& coords, const PathCoordVector& line_coords,
									  float start, float end, int& cur_line_coord, bool include_start_coord, MapCoordVector& out_flags, MapCoordVectorF& out_coords,
									  std::vector<float>* out_lengths, bool set_mid_symbols, ObjectRenderables& output);
	void advanceCoordinateRangeTo(const MapCoordVector& flags, const MapCoordVectorF& coords, const PathCoordVector& line_coords, int& cur_line_coord, int& current_index, float cur_length,
								  int start_bezier_index, MapCoordVector& out_flags, MapCoordVectorF& out_coords, std::vector<float>* out_lengths, const MapCoordF& o3, const MapCoordF& o4);
	void replaceSymbol(PointSymbol*& old_symbol, PointSymbol* replace_with, const QString& name);
	
	// Base line
	int line_width;		// in 1/1000 mm
	MapColor* color;
	int minimum_length;
	CapStyle cap_style;
	JoinStyle join_style;
	int pointed_cap_length;
	
	bool dashed;
	
	// Point symbols
	PointSymbol* start_symbol;
	PointSymbol* mid_symbol;
	PointSymbol* end_symbol;
    PointSymbol* dash_symbol;

	int mid_symbols_per_spot;
	int mid_symbol_distance;
	
	// Not dashed
	int segment_length;
	int end_length;
	bool show_at_least_one_symbol;
	int minimum_mid_symbol_count;
	int minimum_mid_symbol_count_when_closed;
	
	// Dashed
	int dash_length;
	int break_length;
	int dashes_in_group;
	int in_group_break_length;
	bool half_outer_dashes;
	
	// Border lines
	bool have_border_lines;
	LineSymbolBorder border;
	LineSymbolBorder right_border;
};



class LineSymbolSettings : public SymbolPropertiesWidget
{
Q_OBJECT
public:
	LineSymbolSettings(LineSymbol* symbol, SymbolSettingDialog* dialog);
	
	virtual ~LineSymbolSettings();
	
	virtual void reset(Symbol* symbol);
	
	/**
	 * Updates the content and state of all input fields.
	 */
	void updateContents();
	
protected:
	struct BorderWidgets
	{
		QList<QWidget *> widget_list;
		QDoubleSpinBox* width_edit;
		ColorDropDown* color_edit;
		QDoubleSpinBox* shift_edit;
		QCheckBox* dashed_check;
		
		QList<QWidget *> dash_widget_list;
		QDoubleSpinBox* dash_length_edit;
		QDoubleSpinBox* break_length_edit;
	};
	
	/**
	 * Creates the widgets for one border.
	 */
	void createBorderWidgets(LineSymbolBorder& border, Map* map, int& row, int col, QGridLayout* layout, BorderWidgets& widgets);
	
	/**
	 * Updates the border settings from the values in the widgets.
	 */
	void updateBorder(LineSymbolBorder& border, BorderWidgets& widgets);
	
	void updateBorderContents(LineSymbolBorder& border, BorderWidgets& widgets);
	
	/** 
	 * Ensures that a particular widget is visible in the scoll area. 
	 */
	void ensureWidgetVisible(QWidget* widget);
	
	/** 
	 * Adjusts the visibility and enabled state of all UI parts.
	 * There is a large number of dependencies between different elements
	 * of the line settings. This method handles them all.
	 */
	void updateStates();
	
protected slots:
	/** 
	 * Notifies this settings widget that one of the symbols has been modified.
	 */
	void pointSymbolEdited();
	
	void widthChanged(double value);
	void colorChanged();
	void minimumDimensionsEdited();
	void lineCapChanged(int index);
	void lineJoinChanged(int index);
	void pointedLineCapLengthChanged(double value);
	void dashedChanged(bool checked);
	void segmentLengthChanged(double value);
	void endLengthChanged(double value);
	void showAtLeastOneSymbolChanged(bool checked);
	void dashLengthChanged(double value);
	void breakLengthChanged(double value);
	void dashGroupsChanged(int index);
	void inGroupBreakLengthChanged(double value);
	void halfOuterDashesChanged(bool checked);
	void midSymbolsPerDashChanged(int value);
	void midSymbolDistanceChanged(double value);
	void borderCheckClicked(bool checked);
	void differentBordersClicked(bool checked);
	void borderChanged();
	
private slots:
	/** Ensure that a predetermined widget is visible in the scoll area.
	 *  The widget is set in advance by ensureWidgetVisible(QWidget* widget).
	 */
	void ensureWidgetVisible();
	
private:
	LineSymbol* symbol;
	SymbolSettingDialog* dialog;
	
	QDoubleSpinBox* width_edit;
	ColorDropDown* color_edit;
	QDoubleSpinBox* minimum_length_edit;
	
	// enabled if line_width > 0 && color != NULL
	QList<QWidget *> line_settings_list;
	QComboBox* line_cap_combo;
	QComboBox* line_join_combo;
	QLabel* pointed_cap_length_label;
	QDoubleSpinBox* pointed_cap_length_edit;
	QCheckBox* dashed_check;
	
	// dashed == false && mid_symbol
	QList<QWidget *> undashed_widget_list;
	QDoubleSpinBox* segment_length_edit;
	QDoubleSpinBox* end_length_edit;
	QCheckBox* show_at_least_one_symbol_check;
	QSpinBox* minimum_mid_symbol_count_edit;
	QSpinBox* minimum_mid_symbol_count_when_closed_edit;
	
	// dashed == true
	QList<QWidget *> dashed_widget_list;
	QDoubleSpinBox* dash_length_edit;
	QDoubleSpinBox* break_length_edit;
	QComboBox* dash_group_combo;
	QLabel* in_group_break_length_label;
	QDoubleSpinBox* in_group_break_length_edit;
	QCheckBox* half_outer_dashes_check;
	
	// mid_symbol
	QList<QWidget *> mid_symbol_widget_list;
	QSpinBox* mid_symbol_per_spot_edit;
	QLabel* mid_symbol_distance_label;
	QDoubleSpinBox* mid_symbol_distance_edit;
	
	// enabled if line_width > 0
	QList<QWidget *> border_widget_list;
	QCheckBox* border_check;
	QCheckBox* different_borders_check;
	QList<QWidget *> different_borders_widget_list;
	BorderWidgets border_widgets;
	BorderWidgets right_border_widgets;
	
	QScrollArea* scroll_area;
	QWidget* widget_to_ensure_visible;
};

#endif
