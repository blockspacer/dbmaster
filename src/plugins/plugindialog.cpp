/*
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 */


#include "plugindialog.h"
#include "pluginmanager.h"
#include "ui_plugindialog.h"

PluginDialog::PluginDialog(QWidget *parent)
  : QDialog(parent) {
  setupUi(this);

  pluginTree->setModel(PluginManager::model());

  pluginTree->header()->setResizeMode(0, QHeaderView::Stretch);
  pluginTree->header()->setResizeMode(1, QHeaderView::ResizeToContents);
  pluginTree->header()->setResizeMode(2, QHeaderView::ResizeToContents);
  pluginTree->header()->setResizeMode(3, QHeaderView::ResizeToContents);
}

void PluginDialog::on_configButton_clicked() {
  if (pluginTree->selectionModel()->selectedRows().size() == 0) {
    return;
  }

  QString plid = pluginTree->selectionModel()->selectedIndexes()[0]
      .data(Qt::UserRole).toString();
  Plugin *p = PluginManager::plugin(plid);
  if (!p || !p->configDialog()) {
    return;
  }
//  p->configDialog()->setParent(parentWidget());
  p->configDialog()->exec();
}