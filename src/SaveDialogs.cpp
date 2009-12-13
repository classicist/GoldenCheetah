/*
 * Copyright (c) 2009 Mark Liversedge (liversedge@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "MainWindow.h"
#include "GcRideFile.h"
#include "RideItem.h"
#include "RideFile.h"
#include "Settings.h"
#include "SaveDialogs.h"

//----------------------------------------------------------------------
// Utility functions to get and set WARN on CONVERT application setting
//----------------------------------------------------------------------
static bool
warnOnConvert()
{
    bool setting;

    boost::shared_ptr<QSettings> settings = GetApplicationSettings();
    QVariant warnsetting = settings->value(GC_WARNCONVERT);
    if (warnsetting.isNull()) setting = true;
    else setting = warnsetting.toBool();
    return setting;
}

void
setWarnOnConvert(bool setting)
{
    boost::shared_ptr<QSettings> settings = GetApplicationSettings();
    settings->setValue(GC_WARNCONVERT, setting);
}

static bool
warnExit()
{
    bool setting;

    boost::shared_ptr<QSettings> settings = GetApplicationSettings();
    QVariant warnsetting = settings->value(GC_WARNEXIT);
    if (warnsetting.isNull()) setting = true;
    else setting = warnsetting.toBool();
    return setting;
}

void
setWarnExit(bool setting)
{
    boost::shared_ptr<QSettings> settings = GetApplicationSettings();
    settings->setValue(GC_WARNEXIT, setting);
}

//----------------------------------------------------------------------
// User selected Save... menu option, prompt if conversion is needed
//----------------------------------------------------------------------
bool
MainWindow::saveRideSingleDialog(RideItem *rideItem)
{
    if (rideItem->isDirty() == false) return false; // nothing to save you must be a ^S addict.

    // get file type
    QFile   currentFile(rideItem->path + QDir::separator() + rideItem->fileName);
    QFileInfo currentFI(currentFile);
    QString currentType =  currentFI.completeSuffix().toUpper();

    // either prompt etc, or just save that file away!
    if (currentType != "GC" && warnOnConvert() == true) {
        SaveSingleDialogWidget dialog(this, rideItem);
        dialog.exec();
        return true;
    } else {
        // go for it, the user doesn't want warnings!
        saveSilent(rideItem);
        return true;
    }
}

//----------------------------------------------------------------------
// Check if data needs saving on exit and prompt user for action
//----------------------------------------------------------------------
bool
MainWindow::saveRideExitDialog()
{
    QList<RideItem*> dirtyList;

    // have we been told to not warn on exit?
    if (warnExit() == false) return true; // just close regardless!

    for (int i=0; i<allRides->childCount(); i++) {
        RideItem *curr = (RideItem *)allRides->child(i);
        if (curr->isDirty() == true) dirtyList.append(curr);
    }

    // we have some files to save...
    if (dirtyList.count() > 0) {
        SaveOnExitDialogWidget dialog(this, dirtyList);
        int result = dialog.exec();
        if (result == QDialog::Rejected) return false; // cancel that closeEvent!
    }

    // You can exit and close now
    return true;
}

//----------------------------------------------------------------------
// Silently save ride and convert to GC format without warning user
//----------------------------------------------------------------------
void
MainWindow::saveSilent(RideItem *rideItem)
{
    QFile   currentFile(rideItem->path + QDir::separator() + rideItem->fileName);
    QFileInfo currentFI(currentFile);
    QString currentType =  currentFI.completeSuffix().toUpper();
    QFile   savedFile;
    bool    convert;

    // Do we need to convert the file type?
    if (currentType != "GC") convert = true;
    else convert = false;

    // set target filename
    if (convert) {
        // rename the source
        savedFile.setFileName(currentFI.path() + QDir::separator() + currentFI.baseName() + ".gc");
    } else {
        savedFile.setFileName(currentFile.fileName());
    }

    // save in GC format
    GcFileReader reader;
    reader.writeRideFile(rideItem->ride, savedFile);

    // rename the file and update the rideItem list to reflect the change
    if (convert) {

        // rename on disk
        currentFile.rename(currentFile.fileName(), currentFile.fileName() + ".sav");

        // rename in memory
        rideItem->setFileName(QFileInfo(savedFile).path(), QFileInfo(savedFile).fileName());
    }

    // mark clean as we have now saved the data
    rideItem->setDirty(false);
}

//----------------------------------------------------------------------
// Save Single File Dialog Widget
//----------------------------------------------------------------------
SaveSingleDialogWidget::SaveSingleDialogWidget(MainWindow *mainWindow, RideItem *rideItem) :
    QDialog(mainWindow, Qt::Dialog), mainWindow(mainWindow), rideItem(rideItem)
{
    setWindowTitle("Save and Conversion");
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Warning text
    warnText = new QLabel(tr("WARNING\n\nYou have made changes to ") + rideItem->fileName + tr(" If you want to save\nthem, we need to convert the ride to GoldenCheetah\'s\nnative format. Should we do so?\n"));
    mainLayout->addWidget(warnText);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    saveButton = new QPushButton(tr("&Save and Convert"), this);
    buttonLayout->addWidget(saveButton);
    abandonButton = new QPushButton(tr("&Discard Changes"), this);
    buttonLayout->addWidget(abandonButton);
    cancelButton = new QPushButton(tr("&Cancel Save"), this);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    // Don't warn me!
    warnCheckBox = new QCheckBox(tr("Always warn me about file conversions"), this);
    warnCheckBox->setChecked(true);
    mainLayout->addWidget(warnCheckBox);

    // connect up slots
    connect(saveButton, SIGNAL(clicked()), this, SLOT(saveClicked()));
    connect(abandonButton, SIGNAL(clicked()), this, SLOT(abandonClicked()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    connect(warnCheckBox, SIGNAL(clicked()), this, SLOT(warnSettingClicked()));
}

void
SaveSingleDialogWidget::saveClicked()
{
    mainWindow->saveSilent(rideItem);
    accept();
}

void
SaveSingleDialogWidget::abandonClicked()
{
    rideItem->setDirty(false); // lose changes
    reject();
}

void
SaveSingleDialogWidget::cancelClicked()
{
    reject();
}

void
SaveSingleDialogWidget::warnSettingClicked()
{
    setWarnOnConvert(warnCheckBox->isChecked());
}

//----------------------------------------------------------------------
// Save on Exit File Dialog Widget
//----------------------------------------------------------------------

SaveOnExitDialogWidget::SaveOnExitDialogWidget(MainWindow *mainWindow, QList<RideItem *>dirtyList) :
    QDialog(mainWindow, Qt::Dialog), mainWindow(mainWindow), dirtyList(dirtyList)
{
    setWindowTitle("Save Changes");
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Warning text
    warnText = new QLabel(tr("WARNING\n\nYou have made changes to some rides which\nhave not been saved. They are listed below."));
    mainLayout->addWidget(warnText);

    // File List
    dirtyFiles = new QTableWidget(dirtyList.count(), 0, this);
    dirtyFiles->setColumnCount(2);
    dirtyFiles->horizontalHeader()->hide();
    dirtyFiles->verticalHeader()->hide();

    // Populate with dirty List
    for (int i=0; i<dirtyList.count(); i++) {
        // checkbox
        QCheckBox *c = new QCheckBox;
        c->setCheckState(Qt::Checked);
        dirtyFiles->setCellWidget(i,0,c);

        // filename
        QTableWidgetItem *t = new QTableWidgetItem;
        t->setText(dirtyList.at(i)->fileName);
        t->setFlags(t->flags() & (~Qt::ItemIsEditable));
        dirtyFiles->setItem(i,1,t);
    }

    // prettify the list
    dirtyFiles->setShowGrid(false);
    dirtyFiles->resizeColumnToContents(0);
    dirtyFiles->resizeColumnToContents(1);
    mainLayout->addWidget(dirtyFiles);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    saveButton = new QPushButton(tr("&Save and Exit"), this);
    buttonLayout->addWidget(saveButton);
    abandonButton = new QPushButton(tr("&Discard and Exit"), this);
    buttonLayout->addWidget(abandonButton);
    cancelButton = new QPushButton(tr("&Cancel Exit"), this);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    // Don't warn me!
    exitWarnCheckBox = new QCheckBox(tr("Always check for unsaved chages on exit"), this);
    exitWarnCheckBox->setChecked(true);
    mainLayout->addWidget(exitWarnCheckBox);

    // connect up slots
    connect(saveButton, SIGNAL(clicked()), this, SLOT(saveClicked()));
    connect(abandonButton, SIGNAL(clicked()), this, SLOT(abandonClicked()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    connect(exitWarnCheckBox, SIGNAL(clicked()), this, SLOT(warnSettingClicked()));
}

void
SaveOnExitDialogWidget::saveClicked()
{
    // whizz through the list and save one by one using
    // singleSave to ensure warnings are given if neccessary
    for (int i=0; i<dirtyList.count(); i++) {
        QCheckBox *c = (QCheckBox *)dirtyFiles->cellWidget(i,0);
        if (c->isChecked()) {
            mainWindow->saveRideSingleDialog(dirtyList.at(i));
        }
    }
    accept();
}

void
SaveOnExitDialogWidget::abandonClicked()
{
    accept();
}

void
SaveOnExitDialogWidget::cancelClicked()
{
    reject();
}

void
SaveOnExitDialogWidget::warnSettingClicked()
{
    setWarnExit(exitWarnCheckBox->isChecked());
}
