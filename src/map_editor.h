/*
 *    Copyright 2012, 2013, 2014 Thomas Schöps
 *    Copyright 2013, 2014 Kai Pastor
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


#ifndef _OPENORIENTEERING_MAP_EDITOR_H_
#define _OPENORIENTEERING_MAP_EDITOR_H_

#include <vector>

#include <QClipboard>
#include <QPointer>
#include <QScopedPointer>
#include <QTimer>

#include "gui/main_window_controller.h"
#include "map.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QDockWidget;
class QFrame;
class QLabel;
class QMenu;
class QSignalMapper;
class QSizeGrip;
class QToolBar;
class QToolButton;
QT_END_NAMESPACE

class ActionGridBar;
class CompassDisplay;
class EditorDockWidget;
class GeoreferencingDialog;
class Map;
class MapView;
class MapWidget;
class MapEditorActivity;
class MapEditorTool;
class GPSDisplay;
class GPSTemporaryMarkers;
class GPSTrackRecorder;
class PrintWidget;
class ReopenTemplateDialog;
class SymbolWidget;
class Template;
class TemplateListWidget;
class TemplatePositionDockWidget;


/**
 * MainWindowController for editing a map.
 * 
 * Creates menus and toolbars, manages editing tools,
 * dock widgets, and much more.
 */
class MapEditorController : public MainWindowController
{
friend class Map;
Q_OBJECT
public:
	/** See MapEditorController constructor. */
	enum OperatingMode
	{
		MapEditor = 0,
		SymbolEditor = 1
	};
	
	/**
	 * Constructs a new MapEditorController for a map.
	 * 
	 * @param mode Normally, MapEditor should be used. However, as a HACK the
	 *     MapEditorController is also used in the symbol editor for the preview.
	 *     In this case, SymbolEditor is passed to disable showing the menus,
	 *     toolbars, etc.
	 * @param map The map for which the controller is created. If passing NULL,
	 *     it has to be set later using setMap() (private!)
	 */
	MapEditorController(OperatingMode mode, Map* map = NULL);
	
	/** Destroys the MapEditorController. */
	~MapEditorController();
	
	/** Returns if the editor is in mobile mode. */
	bool isInMobileMode() const;
	
	/**
	 * Changes to new_tool as the new active tool.
	 * If there is a current tool before, calls deleteLater() on it.
	 * new_tool may be NULL, but it is unusual to have no active tool, so
	 * consider setEditTool() instead.
	 */
	void setTool(MapEditorTool* new_tool);
	
	/**
	 * Shortcut to change to the point edit tool as new active tool.
	 * See setTool().
	 */
	void setEditTool();
	
	/**
	 * Sets new_override_tool as the new active override tool.
	 * This takes precedence over all tools set via setTool().
	 * new_override_tool may be NULL, which disables using an override tool
	 * and re-enables the normal tool set via setTool().
	 */
	void setOverrideTool(MapEditorTool* new_override_tool);
	
	/** Returns the current tool. */
	inline MapEditorTool* getTool() const {return current_tool;}
	
	/** Returns the default drawing tool for a given symbol. */
	MapEditorTool* getDefaultDrawToolForSymbol(const Symbol* symbol);
	
	
	/**
	 * @brief Returns the active symbol, or NULL.
	 * 
	 * The active symbol is the single symbol which is to be used by drawing
	 * tools and actions.
	 * 
	 * It there is no active symbol, this function returns NULL.
	 */
	Symbol* activeSymbol() const;
	
	
	/**
	 * If this is set to true (usually by the current tool),
	 * undo/redo and saving the map is deactivated.
	 * 
	 * This is important if the map is in an "unstable" state temporarily.
	 */
	void setEditingInProgress(bool value);
	
	/**
	 * Returns true when editing is in progress.
	 * @see setEditingInProgress
	 */
	virtual bool isEditingInProgress() const;
	
	/**
	 * Adds a a floating dock widget to the main window.
	 * Adjusts some geometric properties.
	 */
	void addFloatingDockWidget(QDockWidget* dock_widget);
	
	/**
	 * Sets the current editor activity.
	 * new_activity may be NULL to disable the current editor activity.
	 */
	void setEditorActivity(MapEditorActivity* new_activity);
	
	/** Returns the current editor activity. */
	inline MapEditorActivity* getEditorActivity() const {return editor_activity;}
	
	/** Returns the map on which this controller operates. */
	inline Map* getMap() const {return map;}
	/** Returns the main map widget (which is currently the only map widget). */
	inline MapWidget* getMainWidget() const {return map_widget;}
	/** Returns this controller's symbol widget, where the symbol selection happens. */
	inline SymbolWidget* getSymbolWidget() const {return symbol_widget;}
	
	/** Returns if a template position dock widget exists for a template. */
	inline bool existsTemplatePositionDockWidget(Template* temp) const {return template_position_widgets.contains(temp);}
	/** Returns the template position dock widget for a template. */
	inline TemplatePositionDockWidget* getTemplatePositionDockWidget(Template* temp) const {return template_position_widgets.value(temp);}
	/** Adds a template position dock widget for the given template. */
	void addTemplatePositionDockWidget(Template* temp);
	/**
	 * Removes the template position dock widget for the template.
	 * 
	 * Should be called by the dock widget if it is closed or the
	 * template deleted; deletes the dock widget.
	 */
	void removeTemplatePositionDockWidget(Template* temp);
	
	
	/**
	 * Shows the given widget in a popup window with specified title.
	 * 
	 * In the desktop version, the widget is shown inside a dock widget.
	 * In the mobile version, the widget is shown as a popup over the map,
	 * ignoring the title.
	 * 
	 * Make sure that the child widget has a reasonable size hint.
	 */
	void showPopupWidget(QWidget* child_widget, const QString& title);
	
	/**
	 * Deletes the given popup widget, which was previously shown with
	 * showPopupWidget().
	 */
	void deletePopupWidget(QWidget* child_widget);
	
	
	/**
	 * Returns the action identified by id if it exists, or NULL.
	 * This allows the reuse of the controller's actions in dock widgets etc.
	 */
	QAction* getAction(const char* id);
	
	/** Override from MainWindowController */
	virtual bool save(const QString& path);
	/** Override from MainWindowController */
	virtual bool exportTo(const QString& path, const FileFormat* format = NULL);
	/** Override from MainWindowController */
	virtual bool load(const QString& path, QWidget* dialog_parent = NULL);
	
	/** Override from MainWindowController */
	virtual void attach(MainWindow* window);
	/** Override from MainWindowController */
	virtual void detach();
	
	/**
	 * @copybrief MainWindowController::keyPressEventFilter
	 * This implementation passes the event to MapWidget::keyPressEventFilter.
	 */
	virtual bool keyPressEventFilter(QKeyEvent* event);
	
	/** 
	 * @copybrief MainWindowController::keyReleaseEventFilter
	 * This implementation passes the event to MapWidget::keyReleaseEventFilter.
	 */
	virtual bool keyReleaseEventFilter(QKeyEvent* event);
	
public slots:
	/**
	 * Makes the print/export dock widget visible, and configures it for 
	 * the given task (which is of type PrintWidget::TaskFlags).
	 */
	void printClicked(int task);
	
	/** Undoes the last object edit step. */
	void undo();
	/** Redoes the last object edit step */
	void redo();
	/** Cuts the selected object(s). */
	void cut();
	/** Copies the selecte object(s). */
	void copy();
	/** Pastes the object(s) from the clipboard. */
	void paste();
	/** Empties the undo / redo history to save space. */
	void clearUndoRedoHistory();
	
	/** Toggles visivbility of the map grid. */
	void showGrid();
	/** Shows the map grid configuration dialog. */
	void configureGrid();
	
	/** Activates the pan tool. */
	void pan();
	/** Zooms in in the current map widget. */
	void zoomIn();
	/** Zooms out in the current map widget. */
	void zoomOut();
	/** Shows the dialog to set a custom zoom factor in the current map widget. */
	void setCustomZoomFactorClicked();
	
	/** Sets the hatch areas view option. */
	void hatchAreas(bool checked);
	/** Sets the baseline view option. */
	void baselineView(bool checked);
	/** Sets the "hide all templates" view option. */
	void hideAllTemplates(bool checked);
	/** Sets the overprinting simulation view option. */
	void overprintingSimulation(bool checked);
	
	/** Adjusts the coordinates display of the map widget to the selected option. */
	void coordsDisplayChanged();
	
	/** Shows or hides the symbol pane. */
	void showSymbolWindow(bool show);
	/** Shows or hides the color dock widget. */
	void showColorWindow(bool show);
	/** Shows the "load symbols from" dialog. */
	void loadSymbolsFromClicked();
	/** TODO: not implemented yet. */
	void loadColorsFromClicked();
	/** Shows the "scale all symbols" dialog. */
	void scaleAllSymbolsClicked();
	
	/** Shows the ScaleMapDialog. */
	void scaleMapClicked();
	/** Shows the RotateMapDialog. */
	void rotateMapClicked();
	/** Shows the dialog to enter map notes. */
	void mapNotesClicked();
	
	/** Shows or hides the template setup dock widget. */
	void showTemplateWindow(bool show);
	/** Shows a file selector to open a template. */
	void openTemplateClicked();
	/** Shows the ReopenTemplateDialog. */
	void reopenTemplateClicked();
	/** Adjusts action availability based on the presence of templates */
	void templateAvailabilityChanged();
	/** Adjusts action availability based on the presence of closed templates */
	void closedTemplateAvailabilityChanged();
	
	/** Shows or hides the tags editor dock widget. */
	void showTagsWindow(bool show);
	
	/** Shows the GeoreferencingDialog. */
	void editGeoreferencing();
	
	/**
	 * Makes the editor aware of a change of the selected symbols.
	 */
	void selectedSymbolsChanged();
	
	/**
	 * Makes the editor aware of a change of the selected object.
	 */
	void objectSelectionChanged();
	
	/** Adjusts the enabled state of the undo / redo actions. */
	void undoStepAvailabilityChanged();
	/** Adjusts the enabled state of the paste action (specific signature required). */
	void clipboardChanged(QClipboard::Mode mode);
	/** Adjusts the enabled state of the paste action. */
	void updatePasteAvailability();
	
	/**
	 * Checks the presence of spot colors,
	 * and to disables overprinting simulation if there are no spot colors.
	 */
	void spotColorPresenceChanged(bool has_spot_colors);
	
	/** Adjusts the view in the current map widget to show the whole map. */
	void showWholeMap();
	
	/** Activates the point edit tool. */
	void editToolClicked();
	/** Activates the line edit tool. */
	void editLineToolClicked();
	/** Activates the draw point tool. */
	void drawPointClicked();
	/** Activates the draw path tool. */
	void drawPathClicked();
	/** Activates the draw circle tool. */
	void drawCircleClicked();
	/** Activates the draw rectangle tool. */
	void drawRectangleClicked();
	/** Activates the draw freehand tool. */
	void drawFreehandClicked();
	/** Activates the draw fill tool. */
	void drawFillClicked();
	/** Activates the draw text tool. */
	void drawTextClicked();
	
	/** Deletes the selected object(s) */
	void deleteClicked();
	/** Duplicates the selected object(s) */
	void duplicateClicked();
	/** Switches the symbol of the selected object(s) to the selected symbol. */
	void switchSymbolClicked();
	/** Creates duplicates of the selected object(s) and assigns them the selected symbol. */
	void fillBorderClicked();
	/** Selects all objects with the selected symbol(s) */
	void selectObjectsClicked(bool select_exclusively);
	/** Deselects all objects with the selected symbol(s) */
	void deselectObjectsClicked();
	/**
	 * Reverses the selected object(s) direcction(s),
	 * thus switching dash directions for lines.
	 */
	void switchDashesClicked();
	/** Connects close endpoints of selected lines */
	void connectPathsClicked();
	/** Activates the cut tool */
	void cutClicked();
	/** Activates the cut hole tool */
	void cutHoleClicked();
	/** Activates the cut circular hole tool */
	void cutHoleCircleClicked();
	/** Activates the cut rectangular hole tool */
	void cutHoleRectangleClicked();
	/** Activates the rotate tool */
	void rotateClicked();
	/** Activates the rotate pattern tool */
	void rotatePatternClicked();
	/** Activates the scale tool */
	void scaleClicked();
	/** Shows or hides the MeasureWidget */
	void measureClicked(bool checked);
	/** Calculates the union of selected same-symbol area objects */
	void booleanUnionClicked();
	/** Calculates the intersection of selected same-symbol area objects */
	void booleanIntersectionClicked();
	/** Calculates the difference of selected area objects from the first selected area object */
	void booleanDifferenceClicked();
	/** Calculates the boolean XOr of selected same-symbol area objects */
	void booleanXOrClicked();
	/** Merges holes of the (single) selected area object */
	void booleanMergeHolesClicked();
	/** Converts selected polygonal paths to curves */
	void convertToCurvesClicked();
	/** Tries to remove points of selected paths while retaining their shape */
	void simplifyPathClicked();
	/** Activates the physical cutout tool */
	void cutoutPhysicalClicked();
	/** Activates the physical cutout tool (inversed) */
	void cutawayPhysicalClicked();
	/** Executes the "distribute points along path" action.
	 *  The prerequisites for using the tool must be given. */
	void distributePointsClicked();
	
	/** Shows or hides the paint-on-template widget */
	void paintOnTemplateClicked(bool checked);
	/** Shows the template selection dialog for for the paint-on-template functionality */
	void paintOnTemplateSelectClicked();
	
	/** Enables or disables GPS display. */
	void enableGPSDisplay(bool enable);
	/** Enables or disables showing distance rings when GPS display is active. */
	void enableGPSDistanceRings(bool enable);
	/** Updates availability of the GPS point drawing tool. */
	void updateDrawPointGPSAvailability();
	/** Switches to the GPS point drawing tool. */
	void drawPointGPSClicked();
	/** Sets a temporary marker at the GPS position. */
	void gpsTemporaryPointClicked();
	/** Draws a temporary path at the GPS position. */
	void gpsTemporaryPathClicked(bool enable);
	/** Clears temporary GPS markers. */
	void gpsTemporaryClearClicked();
	
	/** Enables or disables digital compass display. */
	void enableCompassDisplay(bool enable);
	/** Enables or disables map auto-rotation according to compass. */
	void alignMapWithNorth(bool enable);
	/** Called regularly after enabled with alignMapWithNorth() to update the map rotation. */
	void alignMapWithNorthUpdate();
	
	/** For mobile UI: hides the top action bar. */
	void hideTopActionBar();
	/** For mobile UI: shows the top action bar again after hiding it. */
	void showTopActionBar();
	/** For mobile UI: shows the symbol selection screen. */
	void mobileSymbolSelectorClicked();
	/** Counterpart to mobileSymbolSelectorClicked(). */
	void mobileSymbolSelectorFinished();

	/** Creates and adds a new map part */
	void addMapPart();
	/** Removes the current map part */
	void removeMapPart();
	/** Renames the current map part */
	void renameMapPart();
	/** Moves all selected objects to a different map part */
	void reassignObjectsToMapPart(int target);
	/** Merges the current map part with another one */
	void mergeCurrentMapPartTo(int target);
	/** Merges all map parts into the current one. */
	void mergeAllMapParts();
	
	/** Updates action enabled states after a template has been added */
	void templateAdded(int pos, const Template* temp);
	/** Updates action enabled states after a template has been deleted */
	void templateDeleted(int pos, const Template* temp);
	
	/** Imports a track file (GPX, DXF, OSM, ...) into the map */
	void importGeoFile(const QString& filename);
	/** Imports a map file into the loaded map */
	bool importMapFile(const QString& filename);
	/** Shows the import file selector and imports the selected file, if any. */
	void importClicked();
	
	/** Sets the enabled state of actions which change how the map is rendered,
	 *  such as with grid, with templates, with overprinting simulation. */
	void setViewOptionsEnabled(bool enabled = true);
	
	/** Save the current toolbar and dock widget positions */
	void saveWindowState();
	
	/** Restore previously saved toolbar and dock widget positions */
	void restoreWindowState();
	
signals:
	/**
	 * @brief Indicates a change of the active symbol.
	 * @param symbol The new active symbol, or NULL.
	 */
	void activeSymbolChanged(const Symbol* symbol);
	
	void templatePositionDockWidgetClosed(Template* temp);

protected:
	/**
	 * Adjusts the enabled state of various actions
	 * after the selected symbol(s) have changed.
	 * 
	 * In addition, it disables actions as long as some editing is in progress.
	 * 
	 * The caller shall also call updateSymbolAndObjectDependentActions().
	 */
	void updateSymbolDependentActions();
	
	/**
	 * Adjusts the enabled state of various actions
	 * after the selected object(s) have changed.
	 * 
	 * In addition, it disables actions as long as some editing is in progress.
	 * 
	 * The caller shall also call updateSymbolAndObjectDependentActions().
	 */
	void updateObjectDependentActions();
	
	/**
	 * Adjusts the enabled state of various actions
	 * after the selected symbol(s) or object(s) have changed.
	 * 
	 * In addition, it disables actions as long as some editing is in progress.
	 */
	void updateSymbolAndObjectDependentActions();
	
protected slots:
	void projectionChanged();
	void georeferencingDialogFinished();
	
	/**
	 * Sets the map's current part.
	 */
	void changeMapPart(int index);
	
	/**
	 * Updates all UI components related to map parts.
	 */
	void updateMapPartsUI();
	
private:
	void setMap(Map* map, bool create_new_map_view);
	
	/// Updates enabled state of all widgets
	void updateWidgets();
	
	void createSymbolWidget(QWidget* parent = NULL);
	
	void createColorWindow();
	
	void createTemplateWindow();
	
	void createTagEditor();
	
	QAction* newAction(const char* id, const QString& tr_text, QObject* receiver, const char* slot, const char* icon = NULL, const QString& tr_tip = QString::null, const QString& whatsThisLink = QString::null);
	QAction* newCheckAction(const char* id, const QString& tr_text, QObject* receiver, const char* slot, const char* icon = NULL, const QString& tr_tip = QString::null, const QString& whatsThisLink = QString::null);
	QAction* newToolAction(const char* id, const QString& tr_text, QObject* receiver, const char* slot, const char* icon = NULL, const QString& tr_tip = QString::null, const QString& whatsThisLink = QString::null);
	QAction* findAction(const char* id);
	void assignKeyboardShortcuts();
	void createActions();
	void createMenuAndToolbars();
	void createMobileGUI();
	
	void paintOnTemplate(Template* temp);
	void updatePaintOnTemplateAction();
	
	void doUndo(bool redo);
	
	Map* map;
	MapView* main_view;
	MapWidget* map_widget;
	
	OperatingMode mode;
	bool mobile_mode;
	
	MapEditorTool* current_tool;
	MapEditorTool* override_tool;
	MapEditorActivity* editor_activity;
	
	Symbol* active_symbol;
	
	bool editing_in_progress;
	
	// Action handling
	QHash<QString, QAction *> actionsById;
	
	EditorDockWidget* print_dock_widget;
	PrintWidget* print_widget;
	
	QAction* print_act;
	QAction* export_image_act;
	QAction* export_pdf_act;
	
	QAction* undo_act;
	QAction* redo_act;
	QAction* cut_act;
	QAction* copy_act;
	QAction* paste_act;
	QAction* clear_undo_redo_history_act;
	
	QAction* pan_act;
	QAction* zoom_in_act;
	QAction* zoom_out_act;
	QAction* show_all_act;
	QAction* fullscreen_act;
	QAction* custom_zoom_act;
	QAction* show_grid_act;
	QAction* configure_grid_act;
	QAction* hatch_areas_view_act;
	QAction* baseline_view_act;
	QAction* hide_all_templates_act;
	QAction* overprinting_simulation_act;
	
	QAction* map_coordinates_act;
	QAction* projected_coordinates_act;
	QAction* geographic_coordinates_act;
	QAction* geographic_coordinates_dms_act;
	
	QAction* scale_all_symbols_act;
	QAction* georeferencing_act;
	QAction* scale_map_act;
	QAction* rotate_map_act;
	QAction* map_notes_act;
	
	QAction* color_window_act;
	QPointer<EditorDockWidget> color_dock_widget;
	QAction* load_symbols_from_act;
	
	QAction* symbol_window_act;
	EditorDockWidget* symbol_dock_widget;
	SymbolWidget* symbol_widget;
	
	QAction* template_window_act;
	QPointer<QWidget> template_dock_widget;
	TemplateListWidget* template_list_widget;
	QAction* open_template_act;
	QAction* reopen_template_act;
	
	QAction* tags_window_act;
	QPointer<EditorDockWidget> tags_dock_widget;
	
	QAction* edit_tool_act;
	QAction* edit_line_tool_act;
	QAction* draw_point_act;
	QAction* draw_path_act;
	QAction* draw_circle_act;
	QAction* draw_rectangle_act;
	QAction* draw_freehand_act;
	QAction* draw_fill_act;
	QAction* draw_text_act;
	
	QAction* delete_act;
	QAction* duplicate_act;
	QAction* switch_symbol_act;
	QAction* fill_border_act;
	QAction* switch_dashes_act;
	QAction* connect_paths_act;
	QAction* cut_tool_act;
	QMenu* cut_hole_menu;
	QAction* cut_hole_act;
	QAction* cut_hole_circle_act;
	QAction* cut_hole_rectangle_act;
	QAction* rotate_act;
	QAction* rotate_pattern_act;
	QAction* scale_act;
	QAction* measure_act;
	EditorDockWidget* measure_dock_widget;
	QAction* boolean_union_act;
	QAction* boolean_intersection_act;
	QAction* boolean_difference_act;
	QAction* boolean_xor_act;
	QAction* boolean_merge_holes_act;
	QAction* convert_to_curves_act;
	QAction* simplify_path_act;
	QAction* cutout_physical_act;
	QAction* cutaway_physical_act;
	QAction* distribute_points_act;
	
	QAction* paint_on_template_act;
	QAction* paint_on_template_settings_act;
	Template* last_painted_on_template;
	
	QAction* touch_cursor_action;
	QAction* gps_display_action;
	QAction* gps_distance_rings_action;
	QAction* draw_point_gps_act;
	QAction* gps_temporary_point_act;
	QAction* gps_temporary_path_act;
	QAction* gps_temporary_clear_act;
	GPSTemporaryMarkers* gps_marker_display;
	GPSDisplay* gps_display;
	GPSTrackRecorder* gps_track_recorder;
	QAction* compass_action;
	CompassDisplay* compass_display;
	QAction* align_map_with_north_act;
	QTimer align_map_with_north_timer;
	
	QAction* mappart_add_act;
	QAction* mappart_rename_act;
	QAction* mappart_remove_act;
	QAction* mappart_merge_act;
	QMenu* mappart_merge_menu;
	QMenu* mappart_move_menu;
	
	QAction* import_act;
	
	QFrame* statusbar_zoom_frame;
	QLabel* statusbar_cursorpos_label;
	QLabel* statusbar_objecttag_label;
	
	QToolBar* toolbar_view;
	QToolBar* toolbar_drawing;
	QToolBar* toolbar_editing;
	QToolBar* toolbar_advanced_editing;
	QToolBar* toolbar_mapparts;
	
	// For mobile UI
	ActionGridBar* bottom_action_bar;
	ActionGridBar* top_action_bar;
	QToolButton* show_top_bar_button;
	QAction* mobile_symbol_selector_action;
	
	QComboBox* mappart_selector_box;
	
	QScopedPointer<GeoreferencingDialog> georeferencing_dialog;
	QScopedPointer<ReopenTemplateDialog> reopen_template_dialog;
	
	QHash<Template*, TemplatePositionDockWidget*> template_position_widgets;

	QSignalMapper* mappart_merge_mapper;
	QSignalMapper* mappart_move_mapper;
};



//### MapEditorController inline code ###

inline
Symbol* MapEditorController::activeSymbol() const
{
	return active_symbol;
}

#endif
