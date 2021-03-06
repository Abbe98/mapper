/*
 *    Copyright 2012, 2013 Jan Dalheimer
 *    Copyright 2013-2015 Kai Pastor
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

#ifndef _OPENORIENTEERING_SETTINGS_DIALOG_PRIVATE_H_
#define _OPENORIENTEERING_SETTINGS_DIALOG_PRIVATE_H_

#include <QVariant>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QDoubleSpinBox;
class QSpinBox;

class MainWindow;


class SettingsPage : public QWidget
{
Q_OBJECT
public:
	explicit SettingsPage(QWidget* parent = nullptr);
	virtual QString title() const = 0;
	virtual void cancel();
	virtual void apply();
	virtual void ok();

protected:
	// The changes to be done when accepted
	QHash<QString, QVariant> changes;
};



class EditorPage : public SettingsPage
{
Q_OBJECT
public:
	explicit EditorPage(QWidget* parent = nullptr);
	QString title() const override;

private slots:
	void antialiasingClicked(bool checked);
	void textAntialiasingClicked(bool checked);
	void toleranceChanged(int value);
	void snapDistanceChanged(int value);
	void fixedAngleSteppingChanged(int value);
	void selectSymbolOfObjectsClicked(bool checked);
	void zoomOutAwayFromCursorClicked(bool checked);
	void drawLastPointOnRightClickClicked(bool checked);
	
	void editToolDeleteBezierPointActionChanged(int index);
	void editToolDeleteBezierPointActionAlternativeChanged(int index);
	
	void rectangleHelperCrossRadiusChanged(int value);
	void rectanglePreviewLineWidthChanged(bool checked);
	
	void keepSettingsOfClosedTemplatesClicked(bool checked);
	
private:
	void updateWidgets();
	
	QCheckBox* antialiasing;
	QCheckBox* text_antialiasing;
	QComboBox* edit_tool_delete_bezier_point_action;
	QComboBox* edit_tool_delete_bezier_point_action_alternative;
};



class GeneralPage : public SettingsPage
{
Q_OBJECT
public:
	explicit GeneralPage(QWidget* parent = nullptr);
	QString title() const override;
	void apply() override;
	
protected:
	/** This event filter stops LanguageChange events. */
	bool eventFilter(QObject* watched, QEvent* event) override;
	
private slots:
	void languageChanged(int index);
	
	void openMRUFileClicked(bool state);
	
	void tipsVisibleClicked(bool state);
	
	void openTranslationFileDialog();
	
	void ppiChanged(double ppi);
	
	void openPPICalculationDialog();
	
	void encodingChanged(const QString& name);
	
	void ocdImporterClicked(bool state);
	
	void autosaveChanged(bool state);
	
	void autosaveIntervalChanged(int value);
	
	void retainCompatibilityChanged(bool state);
	
private:
	/** Adds the available languages to the language combo box,
	 *  and sets the current element.
	 */
	void updateLanguageBox();
	
	QComboBox* language_box;
	const static int TranslationFromFile;
	
	QDoubleSpinBox* ppi_edit;
	
	QComboBox* encoding_box;
	
	QLabel*    autosave_interval_label;
	QSpinBox*  autosave_interval_edit;
};

#endif
