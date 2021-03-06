#include "../config.h"
#include "../dbmanager.h"
#include "../iconmanager.h"
#include "../mainwindow.h"
#include "../tools/logger.h"

#include "queryeditorwidget.h"

#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QSqlQuery>
#include <QSqlRecord>

QueryEditorWidget::QueryEditorWidget(QWidget *parent)
  : AbstractTabWidget(parent) {
  setupUi(this);
  setupWidgets();

  dataProvider = new QueryDataProvider(this);
  tableView->setDataProvider(dataProvider);

  setupConnections();

  // setAutoDelete(false);

  // watcher     = new QFileSystemWatcher(this);
}

AbstractTabWidget::Actions QueryEditorWidget::availableActions() {
  Actions ret = baseActions;
  if (!isSaved()) {
    ret |= Save;
  }

  if (editor->document()->isUndoAvailable()) {
    ret |= Undo;
  }

  if (editor->document()->isRedoAvailable()) {
    ret |= Redo;
  }

  return ret;
}

void QueryEditorWidget::checkDbOpen() {
  DbManager::instance->lastUsedDbIndex = dbChooser->currentIndex();

  QSqlDatabase *db = currentDb();
  if (db == NULL) {
    runButton->setEnabled(false);
    return;
  }

  updateTransactionButtons(db);

  runButton->setEnabled(db->isOpen());

  reloadContext(db);
}

void QueryEditorWidget::closeEvent(QCloseEvent *event) {
  if (isSaved()) {
    event->accept();
  } else {
    if (confirmClose()) {
      event->accept();
    } else {
      event->ignore();
    }
  }
}

void QueryEditorWidget::commit() {
  if (currentDb()->commit()) {
    commitButton->hide();
    rollbackButton->hide();
    transactionButton->show();
    dbChooser->setEnabled(true);
  }
}

bool QueryEditorWidget::confirmClose() {
  // check if it's saved or not
  if (!isSaved()) {
    int ret = QMessageBox::warning(
        this,
        tr( "DbMaster" ),
        tr( "Warning ! All your modifications will be lost.\nDo you want to save ?" ),
        QMessageBox::Cancel | QMessageBox::Save | QMessageBox::Discard,
        QMessageBox::Cancel);

    switch (ret) {
    case QMessageBox::Cancel:
      return false;

    case QMessageBox::Save:
      return save();
    }
  }

  return true;
}

int QueryEditorWidget::confirmCloseAll() {
  if (isSaved()) {
    return QMessageBox::Yes;
  }

  QString msg(tr("Warning ! All your modifications will be lost.\nDo you want to save ?"));
  if (filePath.isNull()) {
    msg = QString(tr("Warning ! %1 hasn't been saved\nDo you want to save ?"))
                  .arg(filePath);
  }

  int ret = QMessageBox::warning(this, tr("DbMaster"), msg,
                                 QMessageBox::Save    | QMessageBox::SaveAll  |
                                 QMessageBox::No      | QMessageBox::NoAll    |
                                 QMessageBox::Cancel,
                                 QMessageBox::Cancel);

  return ret;
}

void QueryEditorWidget::copy() {
  editor->copy();
}

QSqlDatabase* QueryEditorWidget::currentDb() {
  return DbManager::instance->connections()[dbChooser->currentIndex()]->db();
}

void QueryEditorWidget::cut() {
  editor->cut();
}

QString QueryEditorWidget::file() {
  return filePath;
}

void QueryEditorWidget::onFileChanged(QString path) {
  if (QFile::exists(path)) {
    // Le fichier a été modifié
  } else {
    // Le fichier a été supprimé
  }
}

QIcon QueryEditorWidget::icon() {
  return IconManager::get("accessories-text-editor");
}

QString QueryEditorWidget::id() {
  QString ret = "q";
  if (!filePath.isEmpty()) {
    ret.append(" %1").arg( filePath );
  }
  return ret;
}

bool QueryEditorWidget::isSaved() {
  return !editor->document()->isModified();
}

void QueryEditorWidget::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    tableContainer->hide();
    resultButton->setChecked(false);
  } else {
    QWidget::keyPressEvent(event);
  }
}

void QueryEditorWidget::lowerCase() {
  QTextCursor tc = editor->textCursor();
  if (tc.selectedText().size() > 0) {
    QString txt = tc.selectedText();
    tc.removeSelectedText();
    tc.insertText(txt.toLower());
    tc.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, txt.size());
    editor->setTextCursor(tc);
  }
}

void QueryEditorWidget::open(QString path) {
  if (!confirmClose())
    return;

  setFilePath(path);
  reloadFile();
}

void QueryEditorWidget::paste() {
  editor->paste();
}

void QueryEditorWidget::print() {
  QPainter painter;
  painter.begin(&m_printer);
  editor->document()->drawContents(&painter);
  painter.end();
}

QPrinter *QueryEditorWidget::printer() {
  return &m_printer;
}

void QueryEditorWidget::redo() {
  editor->redo();
}

void QueryEditorWidget::refresh() {
  checkDbOpen();
  updateCursorPosition();
}

void QueryEditorWidget::reload() {
  dataProvider->start();
  // tableView->updateView();
}

void QueryEditorWidget::reloadContext(QSqlDatabase *db) {
  if (!db->isOpen() || db->driverName().startsWith("QOCI")) {
    return;
  }

  QMultiMap<QString, QString> fields;
  QSqlRecord r;
  QStringList tables = db->tables();
  foreach (QString t, tables) {
    r = db->record(t);
    for (int i=0; i<r.count(); i++) {
      fields.insert(t, r.fieldName(i));
    }
  }

  editor->reloadContext(tables, fields);
}

void QueryEditorWidget::reloadFile() {
  if (!confirmClose()) {
    return;
  }

  editor->clear();

  // loading the file
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    // OMG ! an error occured
    QMessageBox::critical(this, tr("DbMaster"),
                          tr("Unable to open the file !"),
                          QMessageBox::Ok );
    return;
  }

  if (Config::editorEncoding == "latin1") {
    editor->append(QString::fromLatin1(file.readAll().data()));
  }

  if (Config::editorEncoding == "utf8") {
    editor->append(QString::fromUtf8(file.readAll().data()));
  }

  file.close();

  editor->document()->setModified(false);

  emit fileChanged(filePath);
}

void QueryEditorWidget::queryError() {
  statusBar->showMessage(tr("Unable to run query"));
  runButton->setEnabled(true);
}

void QueryEditorWidget::querySuccess() {
  tableContainer->setVisible(true);
  resultButton->setEnabled(true);
  resultButton->setChecked(true);

  QString logMsg = tr("Query executed with success (%1 lines returned)")
      .arg(dataProvider->model()->rowCount());
  statusBar->showMessage(logMsg);

  runButton->setEnabled(true);
  emit success();
}

QString QueryEditorWidget::queryText() {
  QTextCursor tc = editor->textCursor();
  QString qtext = tc.selectedText();
  if (!qtext.isEmpty()) {
    return qtext;
  }

  qtext = editor->toPlainText();
  tc.movePosition(QTextCursor::StartOfLine);

  if (qtext.contains(";")) {
    int start = qtext.lastIndexOf(";", tc.position());
    int end = qtext.indexOf(";", tc.position());
    start = start < -1 ? 0 : start+1;
    end = end < -1 ? qtext.length()-1 : end;
    return qtext.mid(start, end-start);
  } else {
    return qtext;
  }
}

void QueryEditorWidget::rollback() {
  if (currentDb()->rollback()) {
    commitButton->hide();
    rollbackButton->hide();
    transactionButton->show();
    dbChooser->setEnabled(true);
  }
}

/*
void QueryEditorWidget::run() {
  QString qtext = editor->textCursor().selectedText();
  if (qtext.isEmpty()) {
    qtext = editor->toPlainText();
  }

  QSqlDatabase* db = DbManager::instance->getDatabase(dbChooser->currentIndex());
  query = QSqlQuery(*db);
  query.exec(qtext);
  model->setQuery(query);

  emit queryFinished();
}
*/

/**
 * @returns false in case of error
 */
bool QueryEditorWidget::save() {
  if (isSaved()) {
    return true;
  }

  if (filePath.isNull()) {
    setFilePath(QFileDialog::getSaveFileName(
        this, tr("DbMaster"), lastDir.path(),
        tr("Query file (*.sql);;All files (*.*)")));

    if(filePath.isNull())
      return false;
  }

  QFile file(filePath);
  lastDir = filePath;

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::critical(this, tr("DbMaster"), tr("Unable to save the file !"),
                          QMessageBox::Ok);
    return false;
  }

  editor->setEnabled(false);
  setCursor(Qt::BusyCursor);

  QTextStream out( &file );
  QStringList list( editor->toPlainText().split( '\n' ) );

  int count = list.count();
  for (int i = 0; i < count; i++) {
    out << list[i];
    if( i != count )
    out << '\n';
  }

  file.close();

  editor->document()->setModified(false);

  editor->setEnabled(true);
  setCursor(Qt::ArrowCursor);

  emit modificationChanged(editor->document()->isModified());
  return true;
}

void QueryEditorWidget::saveAs(QString name) {
  filePath = name;
  save();
}

void QueryEditorWidget::selectAll() {
  editor->selectAll();
}

void QueryEditorWidget::setFilePath(QString path) {
  // watcher->removePath(this->filePath);
  this->filePath = path;

  if(path.size() > 30)
    path = QFileInfo(path).fileName();

  pathLabel->setText(path);
  if (path.length() > 0) {
    // watcher->addPath(path);
  }
}

void QueryEditorWidget::setupConnections() {
  connect(dbChooser, SIGNAL(currentIndexChanged(int)),
          this, SLOT(checkDbOpen()));
  connect(pagination, SIGNAL(reload()), this, SLOT(reload()));

  connect(runButton, SIGNAL(clicked()), this, SLOT(start()));

  connect(editor->document(), SIGNAL(modificationChanged(bool)),
          this, SIGNAL(modificationChanged(bool)));
  connect(editor, SIGNAL(cursorPositionChanged()),
          this, SLOT(updateCursorPosition()));

  connect(commitButton, SIGNAL(clicked()), this, SLOT(commit()));
  connect(rollbackButton, SIGNAL(clicked()), this, SLOT(rollback()));
  connect(transactionButton, SIGNAL(clicked()), this, SLOT(startTransaction()));

  connect(dataProvider, SIGNAL(error()),
          this, SLOT(queryError()));
  connect(dataProvider, SIGNAL(success()),
          this, SLOT(querySuccess()));

  // connect(watcher, SIGNAL(fileChanged(QString)),
  //         this, SLOT(onFileChanged(QString)));
}

void QueryEditorWidget::setupWidgets() {
  editor->setFont(Config::editorFont);

  tableContainer->hide();
  tableView->setPagination(pagination);

  statusBar = new QStatusBar(this);
  statusBar->setSizeGripEnabled(false);
  gridLayout->addWidget(statusBar, gridLayout->rowCount(), 0, 1, -1);

  resultButton = new QToolButton(this);
  resultButton->setText(tr("Display result"));
  resultButton->setCheckable(true);
  resultButton->setEnabled(false);
  connect(resultButton, SIGNAL(clicked(bool)),
          tableContainer, SLOT(setVisible(bool)));

  baseActions = CaseLower | CaseUpper | Copy | Cut | Paste | Print | SaveAs
              | Search | SelectAll;

  dbChooser->setModel(DbManager::instance->model());
  dbChooser->setCurrentIndex(DbManager::instance->lastUsedDbIndex);

  runButton->setIcon(IconManager::get("player_play"));

  cursorPositionLabel = new QLabel(this);


  statusBar->addPermanentWidget(cursorPositionLabel);
  statusBar->addPermanentWidget(resultButton);

  refresh();
}

void QueryEditorWidget::showEvent(QShowEvent *event) {
  editor->setFocus();
}

void QueryEditorWidget::start() {
  resultButton->setChecked(false);
  tableContainer->setVisible(false);
  resultButton->setEnabled(false);
  runButton->setEnabled(false);

  statusBar->showMessage(tr("Running..."));

  dataProvider->setQuery(queryText(), *currentDb());
  dataProvider->start();
  // tabView->reload();
}

void QueryEditorWidget::startTransaction() {
  if (currentDb()->transaction()) {
    commitButton->show();
    rollbackButton->show();
    transactionButton->hide();
    dbChooser->setEnabled(false);
  }
}

QString QueryEditorWidget::title() {
  QString t;
  if(!filePath.isEmpty())
    t = QFileInfo(filePath).fileName();
  else
    t = tr("Query editor");

  return t;
}

QTextEdit* QueryEditorWidget::textEdit() {
  return editor;
}

void QueryEditorWidget::undo() {
  editor->undo();
}

void QueryEditorWidget::updateTransactionButtons(QSqlDatabase *db) {
  commitButton->setIcon(IconManager::get("transaction-commit"));
  commitButton->hide();

  rollbackButton->setIcon(IconManager::get("transaction-rollback"));
  rollbackButton->hide();

  transactionButton->setIcon(IconManager::get("transaction-start"));

  if (!db->driver()->hasFeature(QSqlDriver::Transactions)) {
    transactionButton->hide();
  }

  transactionButton->setEnabled(db->isOpen());
}

void QueryEditorWidget::upperCase() {
  QTextCursor tc = editor->textCursor();
  if (tc.selectedText().size() > 0) {
    QString txt = tc.selectedText();
    tc.removeSelectedText();
    tc.insertText(txt.toUpper());
    tc.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, txt.size());
    editor->setTextCursor(tc);
  }
}

void QueryEditorWidget::updateCursorPosition() {
  cursorPositionLabel->setText(tr("Line: %1, Col: %2").arg(editor->textCursor().blockNumber()+1).arg(editor->textCursor().columnNumber()+1));
}
