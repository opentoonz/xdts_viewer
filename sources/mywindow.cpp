#include "mywindow.h"

#include "xdtsio.h"
#include "xsheetpreviewarea.h"
#include "pathutils.h"
#include "tool.h"
#include "commandmanager.h"
#include "dialogs.h"

#include <iostream>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QIntValidator>
#include <QMenuBar>
#include <QFileDialog>
#include <QCoreApplication>
#include <QMessageBox>
#include <QToolBar>
#include <QActionGroup>
#include <QStandardPaths>
#include <QSettings>
#include <QApplication>
#include <QCloseEvent>
#include <QToolButton>
#include <QScrollBar>
#include <QClipboard>
#include <QMimeData>
#include <QProxyStyle>
#include <QDesktopServices>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QImageReader>

//-----------------------------

namespace {
QIcon color2Icon(QColor color) {
  QPixmap pm(20, 20);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setBrush(color);
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPoint(10, 10), 8, 8);
  p.end();
  return QIcon(pm);
}
}  // namespace

class MyToolButtonProxyStyle : public QProxyStyle {
public:
  int styleHint(StyleHint hint, const QStyleOption* option = nullptr,
                const QWidget* widget        = nullptr,
                QStyleHintReturn* returnData = nullptr) const override {
    if (hint == QStyle::SH_ToolButton_PopupDelay) return 100;
    return QProxyStyle::styleHint(hint, option, widget, returnData);
  }
};

//-----------------------------

MyWindow::MyWindow()
    : m_data(nullptr), m_settingsDialog(nullptr), m_preferencesDialog(nullptr) {
  setAcceptDrops(true);

  m_previewPane     = new XsheetPdfPreviewPane(this);
  m_previewArea     = new XsheetPdfPreviewArea(this);
  m_currentPageEdit = new QLineEdit(this);
  m_totalPageCount  = 0;
  m_prev            = new QPushButton(tr("< Prev"), this);
  m_next            = new QPushButton(tr("Next >"), this);

  m_settingsDialog    = new SettingsDialog(this);
  m_preferencesDialog = new PreferencesDialog(this);

  m_previewArea->setPane(m_previewPane);
  m_previewArea->setAlignment(Qt::AlignCenter);
  m_previewArea->setBackgroundRole(QPalette::Dark);
  // m_previewArea->setStyleSheet("background-color:black;");
  m_previewPane->setArea(m_previewArea);

  // m_currentPageEdit->setValidator(new QIntValidator(1, m_totalPageCount,
  // this));
  m_currentPageEdit->setText("1");
  m_currentPageEdit->setObjectName("LargeSizedText");
  m_currentPageEdit->setFixedWidth(100);
  m_prev->setDisabled(true);
  m_next->setDisabled(true);

  // Toolbar
  QToolBar* toolBar = new QToolBar(this);
  toolBar->setFloatable(false);
  toolBar->setMovable(false);
  toolBar->setIconSize(QSize(24, 24));
  toolBar->setStyleSheet("QToolButton{padding: 1px;}");

  QAction* loadAct =
      toolBar->addAction(QIcon(":Resources/load.svg"), tr("Load (Ctrl+O)"));
  QAction* saveAct =
      toolBar->addAction(QIcon(":Resources/save.svg"), tr("Save (Ctrl+S)"));
#ifdef WIN32
  QAction* exportAct = toolBar->addAction(QIcon(":Resources/export_img.svg"),
                                          tr("Export Image (Ctrl+E)"));
#else
  QAction* exportAct = toolBar->addAction(QIcon(":Resources/export.svg"),
                                          tr("Export Image (Ctrl+E)"));
#endif
  toolBar->addSeparator();
  QAction* undoAct =
      toolBar->addAction(QIcon(":Resources/undo.svg"), tr("Undo (Ctrl+Z)"));
  QAction* redoAct =
      toolBar->addAction(QIcon(":Resources/redo.svg"), tr("Redo (Ctrl+Y)"));
  toolBar->addSeparator();
  QAction* zoomInAct  = toolBar->addAction(QIcon(":Resources/zoomin.svg"),
                                           tr("Zoom In (Ctrl+Plus)"));
  QAction* zoomOutAct = toolBar->addAction(QIcon(":Resources/zoomout.svg"),
                                           tr("Zoom Out (Ctrl+Minus)"));
  toolBar->addSeparator();

  // Spacer
  QWidget* spacer = new QWidget();
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  toolBar->addWidget(spacer);

  QWidget* container      = new QWidget(this);
  QHBoxLayout* prevBtnLay = new QHBoxLayout();
  prevBtnLay->setMargin(0);
  prevBtnLay->setSpacing(0);
  {
    prevBtnLay->addWidget(m_prev, 0);
    prevBtnLay->addWidget(m_currentPageEdit, 0);
    prevBtnLay->addWidget(m_next, 0);
  }
  container->setLayout(prevBtnLay);
  toolBar->addWidget(container);

  spacer = new QWidget();
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  toolBar->addWidget(spacer);

  // color selector
  QList<QColor> colors = {QColor(Qt::black),     QColor(Qt::red),
                          QColor(Qt::blue),      QColor(Qt::yellow),
                          QColor(Qt::darkGreen), QColor(Qt::white)};
  QColor currentColor  = MyParams::instance()->currentColor();
  m_currentColorAction =
      toolBar->addAction(color2Icon(currentColor), tr("Current Color"));
  // 色選択のメニュー
  QToolButton* currentColorButton = qobject_cast<QToolButton*>(
      toolBar->widgetForAction(m_currentColorAction));
  QMenu* currentColorMenu         = new QMenu(this);
  QActionGroup* currentColorGroup = new QActionGroup(this);
  for (auto color : colors) {
    QAction* colorAction = currentColorMenu->addAction(color2Icon(color), "");
    colorAction->setCheckable(true);
    if (color == currentColor) colorAction->setChecked(true);
    colorAction->setData(color);
    currentColorGroup->addAction(colorAction);
  }
  currentColorGroup->setExclusive(true);
  currentColorButton->setMenu(currentColorMenu);
  currentColorButton->setStyle(new MyToolButtonProxyStyle);

  toolBar->addSeparator();
  m_toolActionGroup    = new QActionGroup(this);
  ToolId currentToolId = MyParams::instance()->currentToolId();

  QMap<BrushTool::BrushSize, QIcon> brushIcons = {
      {BrushTool::Small, QIcon(":Resources/brush_small.svg")},
      {BrushTool::Large, QIcon(":Resources/brush_large.svg")},
  };
  BrushTool::BrushSize currentBrushSize =
      dynamic_cast<BrushTool*>(MyParams::instance()->getTool(Tool_Brush))
          ->brushSizeId();
  QAction* brushToolAct =
      toolBar->addAction(brushIcons.value(currentBrushSize), tr("Brush (B)"));
  brushToolAct->setCheckable(true);
  if (currentToolId == Tool_Brush) brushToolAct->setChecked(true);
  brushToolAct->setData(Tool_Brush);
  m_toolActionGroup->addAction(brushToolAct);

  // ペンツールのメニュー
  QToolButton* brushButton =
      qobject_cast<QToolButton*>(toolBar->widgetForAction(brushToolAct));
  QMenu* brushMenu             = new QMenu(this);
  QActionGroup* brushSizeGroup = new QActionGroup(this);
  QAction* largeSize =
      brushMenu->addAction(brushIcons.value(BrushTool::Large), tr("Large"));
  largeSize->setCheckable(true);
  largeSize->setData(BrushTool::Large);
  if (currentBrushSize == BrushTool::Large) largeSize->setChecked(true);
  brushSizeGroup->addAction(largeSize);
  QAction* smallSize =
      brushMenu->addAction(brushIcons.value(BrushTool::Small), tr("Small"));
  smallSize->setCheckable(true);
  smallSize->setData(BrushTool::Small);
  if (currentBrushSize == BrushTool::Small) smallSize->setChecked(true);
  brushSizeGroup->addAction(smallSize);
  brushSizeGroup->setExclusive(true);
  brushButton->setMenu(brushMenu);
  brushButton->setStyle(new MyToolButtonProxyStyle);

  QAction* eraserToolAct =
      toolBar->addAction(QIcon(":Resources/eraser.svg"), tr("Eraser (E)"));
  eraserToolAct->setCheckable(true);
  if (currentToolId == Tool_Eraser) eraserToolAct->setChecked(true);
  eraserToolAct->setData(Tool_Eraser);
  m_toolActionGroup->addAction(eraserToolAct);

  QMap<SelectionTool::SelectionMode, QIcon> selectionIcons = {
      {SelectionTool::Rect, QIcon(":Resources/selection_rectangular.svg")},
      {SelectionTool::FreeHand, QIcon(":Resources/selection_freehand.svg")},
  };
  SelectionTool::SelectionMode currentSelectionMode =
      dynamic_cast<SelectionTool*>(
          MyParams::instance()->getTool(Tool_Selection))
          ->mode();
  QAction* selectionToolAct = toolBar->addAction(
      selectionIcons.value(currentSelectionMode), tr("Selection (M)"));
  selectionToolAct->setCheckable(true);
  if (currentToolId == Tool_Selection) selectionToolAct->setChecked(true);
  selectionToolAct->setData(Tool_Selection);
  m_toolActionGroup->addAction(selectionToolAct);

  // 選択ツールのメニュー
  QToolButton* selectionToolButton =
      qobject_cast<QToolButton*>(toolBar->widgetForAction(selectionToolAct));
  QMenu* selectionMenu             = new QMenu(this);
  QActionGroup* selectionModeGroup = new QActionGroup(this);
  QAction* rectMode                = selectionMenu->addAction(
      selectionIcons.value(SelectionTool::Rect), tr("Rectangle"));
  rectMode->setCheckable(true);
  if (currentSelectionMode == SelectionTool::Rect) rectMode->setChecked(true);
  rectMode->setData(SelectionTool::Rect);
  selectionModeGroup->addAction(rectMode);
  QAction* freehandMode = selectionMenu->addAction(
      selectionIcons.value(SelectionTool::FreeHand), tr("Freehand"));
  freehandMode->setCheckable(true);
  if (currentSelectionMode == SelectionTool::FreeHand)
    freehandMode->setChecked(true);
  freehandMode->setData(SelectionTool::FreeHand);
  selectionModeGroup->addAction(freehandMode);
  selectionModeGroup->setExclusive(true);
  selectionToolButton->setMenu(selectionMenu);
  selectionToolButton->setStyle(new MyToolButtonProxyStyle);

  //---- Line Tool
  QStringList iconPaths = {
      ":Resources/line_thin.svg",       ":Resources/line_thick.svg",
      ":Resources/line_thin_arrow.svg", ":Resources/line_thick_arrow.svg",
      ":Resources/line_double.svg",     ":Resources/line_dot.svg"};
  QStringList lineTypeNames = {tr("Thin Line"),   tr("Thick Line"),
                               tr("Thin Arrow"),  tr("Thick Arrow"),
                               tr("Double Line"), tr("Dotted Line")};
  LineId currentLineId =
      dynamic_cast<LineTool*>(MyParams::instance()->getTool(Tool_Line))->type();
  QAction* lineToolAct = toolBar->addAction(
      QIcon(iconPaths.at((int)currentLineId)), tr("Line (U)"));
  lineToolAct->setCheckable(true);
  if (currentToolId == Tool_Line) lineToolAct->setChecked(true);
  lineToolAct->setData(Tool_Line);
  m_toolActionGroup->addAction(lineToolAct);

  // ラインツールのメニュー
  QToolButton* lineToolButton =
      qobject_cast<QToolButton*>(toolBar->widgetForAction(lineToolAct));
  QMenu* lineMenu             = new QMenu(this);
  QActionGroup* lineTypeGroup = new QActionGroup(this);
  for (int id = (int)ThinLine; id < (int)TypeCount; id++) {
    QAction* typeAction =
        lineMenu->addAction(QIcon(iconPaths[id]), lineTypeNames[id]);
    typeAction->setCheckable(true);
    if (id == (int)currentLineId) typeAction->setChecked(true);
    typeAction->setData(id);
    lineTypeGroup->addAction(typeAction);
  }
  lineTypeGroup->setExclusive(true);
  lineToolButton->setMenu(lineMenu);
  lineToolButton->setStyle(new MyToolButtonProxyStyle);

  QAction* stampToolAct =
      toolBar->addAction(QIcon(":Resources/stamp.svg"), tr("Stamp (S)"));
  stampToolAct->setCheckable(true);
  if (currentToolId == Tool_Stamp) stampToolAct->setChecked(true);
  stampToolAct->setData(Tool_Stamp);
  m_toolActionGroup->addAction(stampToolAct);

  // スタンプツールのメニュー
  QToolButton* stampToolButton =
      qobject_cast<QToolButton*>(toolBar->widgetForAction(stampToolAct));
  QMenu* stampMenu             = new QMenu(this);
  QActionGroup* stampTypeGroup = new QActionGroup(this);
  StampTool* stampTool =
      dynamic_cast<StampTool*>(MyParams::instance()->getTool(Tool_Stamp));
  int currentStampId = stampTool->stampId();
  for (int stampId = 0; stampId < stampTool->stampCount(); stampId++) {
    QPair<QString, QPixmap> sInfo = stampTool->getStampInfo(stampId);
    QAction* sTypeAction =
        stampMenu->addAction(QIcon(sInfo.second), sInfo.first);
    sTypeAction->setCheckable(true);
    if (stampId == currentStampId) sTypeAction->setChecked(true);
    sTypeAction->setData(stampId);
    stampTypeGroup->addAction(sTypeAction);
  }
  stampTypeGroup->setExclusive(true);
  stampToolButton->setMenu(stampMenu);
  stampToolButton->setStyle(new MyToolButtonProxyStyle);

  // 動画番号着色ツール
  QAction* dyeToolAct =
      toolBar->addAction(QIcon(":Resources/dye.svg"), tr("Dye Numbers (D)"));
  dyeToolAct->setCheckable(true);
  if (currentToolId == Tool_Dye) dyeToolAct->setChecked(true);
  dyeToolAct->setData(Tool_Dye);
  m_toolActionGroup->addAction(dyeToolAct);

  // AEタイムリマップ書き出しツール
  QAction* aeToolAct =
      toolBar->addAction(QIcon(":Resources/ae.svg"),
                         tr("Copy After Effects Keyframe Data To Clipboard"));
  aeToolAct->setCheckable(true);
  if (currentToolId == Tool_AE) aeToolAct->setChecked(true);
  aeToolAct->setData(Tool_AE);
  m_toolActionGroup->addAction(aeToolAct);

  m_toolActionGroup->setExclusive(true);

  toolBar->addSeparator();
  QAction* settingsAct =
      toolBar->addAction(QIcon(":Resources/gear.svg"), tr("Settings"));

  QAction* menuAct =
      toolBar->addAction(QIcon(":Resources/menu.svg"), tr("Menu"));
  QMenu* menu       = new QMenu(this);
  QAction* prefAct  = menu->addAction(tr("Preferences"));
  QAction* aboutAct = menu->addAction(tr("About"));
  QToolButton* menuBtn =
      qobject_cast<QToolButton*>(toolBar->widgetForAction(menuAct));
  menuBtn->setMenu(menu);
  menuBtn->setPopupMode(QToolButton::InstantPopup);

  // addToolBar(Qt::TopToolBarArea, toolBar);

  QWidget* central    = new QWidget(this);
  QVBoxLayout* layout = new QVBoxLayout();
  layout->setMargin(0);
  layout->setSpacing(0);
  {
    layout->addWidget(toolBar, 0);
    layout->addWidget(m_previewArea, 1);
  }
  central->setLayout(layout);
  setCentralWidget(central);

  // コマンドの登録
  QAction* cutCmd      = new QAction(tr("Cut"));
  QAction* copyCmd     = new QAction(tr("Copy"));
  QAction* pasteCmd    = new QAction(tr("Paste"));
  QAction* deleteCmd   = new QAction(tr("Delete"));
  QAction* prevPageCmd = new QAction(tr("Previous Page"));
  QAction* nextPageCmd = new QAction(tr("Next Page"));
  CommandManager::instance()->registerAction(Cmd_Cut, cutCmd);
  CommandManager::instance()->registerAction(Cmd_Copy, copyCmd);
  CommandManager::instance()->registerAction(Cmd_Paste, pasteCmd);
  CommandManager::instance()->registerAction(Cmd_Delete, deleteCmd);
  CommandManager::instance()->registerAction(Cmd_PrevPage, prevPageCmd);
  CommandManager::instance()->registerAction(Cmd_NextPage, nextPageCmd);
  cutCmd->setShortcut(QKeySequence(QKeySequence::Cut));
  copyCmd->setShortcut(QKeySequence(QKeySequence::Copy));
  pasteCmd->setShortcut(QKeySequence(QKeySequence::Paste));
  deleteCmd->setShortcut(QKeySequence(QKeySequence::Delete));
  prevPageCmd->setShortcut(QKeySequence(QKeySequence::MoveToPreviousPage));
  nextPageCmd->setShortcut(QKeySequence(QKeySequence::MoveToNextPage));
  addAction(cutCmd);
  addAction(copyCmd);
  addAction(pasteCmd);
  addAction(deleteCmd);
  addAction(prevPageCmd);
  addAction(nextPageCmd);

  // signal-slot connections
  connect(m_currentPageEdit, SIGNAL(editingFinished()), this,
          SLOT(updatePreview()));
  connect(m_prev, SIGNAL(clicked(bool)), this, SLOT(onPrev()));
  connect(m_next, SIGNAL(clicked(bool)), this, SLOT(onNext()));
  connect(prevPageCmd, SIGNAL(triggered()), this, SLOT(onPrev()));
  connect(nextPageCmd, SIGNAL(triggered()), this, SLOT(onNext()));

  connect(MyParams::instance(), SIGNAL(templateSwitched()), this,
          SLOT(initTemplate()));
  connect(MyParams::instance(), SIGNAL(somethingChanged()), this,
          SLOT(updatePreview()));

  connect(m_previewPane, SIGNAL(imageEdited()), this, SLOT(onImageEdited()));

  connect(loadAct, SIGNAL(triggered()), this, SLOT(onLoad()));
  connect(saveAct, SIGNAL(triggered()), this, SLOT(onSave()));
  connect(exportAct, SIGNAL(triggered()), this, SLOT(onExport()));

  connect(undoAct, SIGNAL(triggered()), this, SLOT(onUndo()));
  connect(redoAct, SIGNAL(triggered()), this, SLOT(onRedo()));

  connect(zoomInAct, SIGNAL(triggered()), m_previewArea, SLOT(zoomIn()));
  connect(zoomOutAct, SIGNAL(triggered()), m_previewArea, SLOT(zoomOut()));

  connect(settingsAct, SIGNAL(triggered()), m_settingsDialog, SLOT(show()));
  connect(prefAct, SIGNAL(triggered()), m_preferencesDialog, SLOT(show()));
  connect(aboutAct, SIGNAL(triggered()), this, SLOT(onAbout()));

  connect(currentColorGroup, SIGNAL(triggered(QAction*)), this,
          SLOT(onCurrentColorActionTriggered(QAction*)));

  connect(m_toolActionGroup, SIGNAL(triggered(QAction*)), this,
          SLOT(onToolActionTriggered(QAction*)));
  connect(brushSizeGroup, SIGNAL(triggered(QAction*)), this,
          SLOT(onBrushSizeActionTriggered(QAction*)));
  connect(selectionModeGroup, SIGNAL(triggered(QAction*)), this,
          SLOT(onSelectionModeActionTriggered(QAction*)));
  connect(lineTypeGroup, SIGNAL(triggered(QAction*)), this,
          SLOT(onLineTypeActionTriggered(QAction*)));
  connect(stampTypeGroup, SIGNAL(triggered(QAction*)), this,
          SLOT(onStampTypeActionTriggered(QAction*)));

  connect(MyParams::instance(), SIGNAL(dirtyStateChanged()), this,
          SLOT(updateTitleBar()));
  connect(MyParams::instance(), SIGNAL(toolSwitched()), this,
          SLOT(onToolSwitched()));
  connect(MyParams::instance(), SIGNAL(backsideImgPathChanged()), this,
          SLOT(onBacksideImgPathChanged()));

  connect(m_previewArea->horizontalScrollBar(), SIGNAL(sliderReleased()),
          m_previewPane, SLOT(updateCanvas()));
  connect(m_previewArea->verticalScrollBar(), SIGNAL(sliderReleased()),
          m_previewPane, SLOT(updateCanvas()));

  connect(cutCmd, SIGNAL(triggered()), this, SLOT(onCut()));
  connect(copyCmd, SIGNAL(triggered()), this, SLOT(onCopy()));
  connect(pasteCmd, SIGNAL(triggered()), this, SLOT(onPaste()));
  connect(deleteCmd, SIGNAL(triggered()), this, SLOT(onDelete()));

  resize(820, 1200);

  // shortcut keys
  loadAct->setShortcut(QKeySequence(QKeySequence::Open));
  saveAct->setShortcut(QKeySequence(QKeySequence::Save));
  exportAct->setShortcut(QKeySequence("Ctrl+E"));
  undoAct->setShortcut(QKeySequence(QKeySequence::Undo));
  redoAct->setShortcut(QKeySequence(QKeySequence::Redo));
  zoomInAct->setShortcut(QKeySequence(QKeySequence::ZoomIn));
  zoomOutAct->setShortcut(QKeySequence(QKeySequence::ZoomOut));
  brushToolAct->setShortcut(QKeySequence("b"));
  eraserToolAct->setShortcut(QKeySequence("e"));
  selectionToolAct->setShortcut(QKeySequence("m"));
  lineToolAct->setShortcut(QKeySequence("u"));
  stampToolAct->setShortcut(QKeySequence("s"));
  dyeToolAct->setShortcut(QKeySequence("d"));

  // m_previewPane->fitScaleTo(m_previewArea->size());
}

MyWindow::~MyWindow() {
  if (m_data) delete m_data;
  if (m_settingsDialog) delete m_settingsDialog;
  if (m_preferencesDialog) delete m_preferencesDialog;
}

void MyWindow::initialize() { onLoad(MyParams::instance()->currentXdtsPath()); }

void MyWindow::initTemplate() {
  XSheetPDFTemplate* tmpl;
  QString tmplPath = MyParams::instance()->templatePath();
  if (tmplPath.isEmpty()) {
    tmpl = new XSheetPDFTemplate_B4_6sec(m_columns, duration());
  } else {
    XSheetPDFTemplate_Custom* ret =
        new XSheetPDFTemplate_Custom(tmplPath, m_columns, duration());

    if (ret->isValid())
      tmpl = ret;
    else {
      QMessageBox::critical(
          this, tr("Error"),
          tr("The preset file %1 is not valid.").arg(tmplPath));
      delete ret;
      tmpl = new XSheetPDFTemplate_B4_6sec(m_columns, duration());
    }
  }

  MyParams::instance()->setCurrentTemplate(tmpl);
  // if (!m_soundColumns.isEmpty())
  // m_currentTmpl->setSoundColumns(m_soundColumns);

  updatePreview();
}

void MyWindow::setInfo(bool forExportImage) {
  MyParams* p = MyParams::instance();

  XSheetPDFFormatInfo info;

  info.lineColor = (forExportImage) ? p->exportLineColor() : p->lineColor();
  // info.dateTimeText =
  //   (m_addDateTimeCB->isChecked())
  //   ? QLocale::system().toString(QDateTime::currentDateTime())
  //   : "";
  if (forExportImage)
    info.dateTimeText =
        QLocale::system().toString(QDateTime::currentDateTime());
  else
    info.dateTimeText = "";
  // ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  // info.scenePathText =
  //   (m_addScenePathCB->isEnabled() && m_addScenePathCB->isChecked())
  //   ? scene->getScenePath().getQString()
  //   : "";
  info.scenePathText = "";
  // info.sceneNameText =
  //   (m_addSceneNameCB->isChecked()) ? m_sceneNameEdit->text() : "";
  info.sceneNameText = "";

  if (p->exposeAreas().count() == 1)
    info.exportAreas.append(p->exportArea());
  else if (!p->exposeAreas().isEmpty()) {
    info.exportAreas.append(Area_Actions);
    info.exportAreas.append(Area_Cells);
  }

  info.continuousLineMode = p->continuousLineMode();
  info.templateFontFamily =
      (forExportImage) ? p->exportTemplateFont() : p->templateFont();
  info.contentsFontFamily =
      (forExportImage) ? p->exportContentsFont() : p->contentsFont();

  info.memoText = "";
  // info.memoText = m_memoEdit->toPlainText();

  info.logoText  = "";
  info.drawSound = false;
  // info.logoText = (m_logoTxtRB->isChecked()) ? m_logoTextEdit->text() : "";
  // info.drawSound = m_drawSoundCB->isEnabled() && m_drawSoundCB->isChecked();

  info.serialFrameNumber     = p->isSerialFrameNumber();
  info.drawLevelNameOnBottom = p->isLevelNameOnBottom();

  info.tick1MarkId   = -1;
  info.tick2MarkId   = -1;
  info.keyMarkId     = -1;
  info.tick1MarkType = TickMark_Dot;
  info.tick2MarkType = TickMark_Filled;
  // info.tick1MarkId = m_tick1IdCombo->currentData().toInt();
  // info.tick2MarkId = m_tick2IdCombo->currentData().toInt();
  // info.keyMarkId = m_keyIdCombo->currentData().toInt();
  // info.tick1MarkType =
  // (TickMarkType)(m_tick1MarkCombo->currentData().toInt()); info.tick2MarkType
  // = (TickMarkType)(m_tick2MarkCombo->currentData().toInt());
  info.expandColumns = p->isExpandColumns();

  info.startOverlapFrameLength = p->startOverlapFrameLength();
  info.endOverlapFrameLength   = p->endOverlapFrameLength();

  XSheetPDFTemplate* tmpl = MyParams::instance()->currentTemplate();
  tmpl->setInfo(info);

  // if (m_drawDialogueCB->isChecked() && m_drawDialogueCB->isEnabled())
  //   m_currentTmpl->setNoteColumn(m_noteColumns.value(
  //     m_dialogueColCombo->currentData().toInt(), nullptr));
  // else
  //   m_currentTmpl->setNoteColumn(nullptr);

  if (p->logoPath().isEmpty()) return;

  // prepare logo image
  QSize logoPixSize = tmpl->logoPixelSize();
  if (logoPixSize.isEmpty()) return;

  QImage img(p->logoPath());
  img = img.scaled(logoPixSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  QImage alphaImg = img;
  alphaImg.invertPixels();
  img.setAlphaChannel(alphaImg);
  QImage logoImg(img.size(), QImage::Format_ARGB32_Premultiplied);
  logoImg.fill(info.lineColor);
  QPainter painter(&logoImg);
  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  painter.drawImage(QPoint(0, 0), img);
  painter.end();

  tmpl->setLogoPixmap(QPixmap::fromImage(logoImg));
}
void MyWindow::updatePreview() {
  // if (MyParams::instance()->currentXdtsPath().isEmpty()) return;
  setInfo();

  XSheetPDFTemplate* tmpl = MyParams::instance()->currentTemplate();

  int framePageCount =
      std::max(tmpl->framePageCount(),
               MyParams::instance()->getScannedSheetPageAmount());
  int parallelPageCount = tmpl->parallelPageCount();
  int totalPage         = framePageCount * parallelPageCount;

  int currentPage = m_currentPageEdit->text().toInt();
  // if the page count is changed
  if (m_totalPageCount != totalPage) {
    int firstPage = (MyParams::instance()->backsideImgPath().isEmpty()) ? 1 : 0;
    m_currentPageEdit->setValidator(
        new QIntValidator(firstPage, totalPage, this));
    if (currentPage > totalPage) {
      m_currentPageEdit->setText(QString::number(totalPage));
      currentPage = totalPage;
    }
    m_totalPageCount = totalPage;

    m_prev->setDisabled(currentPage == firstPage);
    m_next->setDisabled(currentPage == totalPage);
  }
  // m_serialFrameNumberCB->setDisabled(framePageCount == 1);
  // m_levelNameOnBottomCB->setEnabled(m_duration > 36);

  // メモ画像の格納
  if (!m_previewPane->scribbleImage().isNull())
    MyParams::instance()->setScribbleImage(MyParams::instance()->currentPage(),
                                           m_previewPane->scribbleImage());

  if (MyParams::instance()->currentPage() != currentPage) {
    MyParams::instance()->setCurrentPage(currentPage);
  }

  // backside page
  if (currentPage == 0) {
    m_previewPane->setPixmap(MyParams::instance()->backsidePixmap());
  }
  // normal xsheet pages
  else {
    int fPage = (currentPage - 1) / parallelPageCount;
    int pPage = (currentPage - 1) % parallelPageCount;

    QPixmap pm = tmpl->initializePreview();
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);
    tmpl->drawXsheetTemplate(painter, fPage, true);
    if (duration() > 0) tmpl->drawXsheetContents(painter, fPage, pPage, true);
    m_previewPane->setPixmap(pm);
  }
  // ここでメモ画像を格納。表示時に読み込みを行う。
  m_previewPane->setScribbleImage(
      MyParams::instance()->scribbleImage(currentPage));

  // QColor infoColor;
  // if (parallelPageCount == 1) {
  //   m_pageInfoLbl->setText(tr("%n page(s)", "", framePageCount));
  //   infoColor = QColor(Qt::green);
  // }
  // else {
  //   m_pageInfoLbl->setText(
  //     tr("%1 x %2 pages").arg(framePageCount).arg(parallelPageCount));
  //   infoColor = QColor(Qt::yellow);
  // }
  // m_pageInfoLbl->setStyleSheet(QString("QL abel{color: %1;}\
  //                                        QLabel QWidget{ color: black;}")
  //   .arg(infoColor.name()));
}

void MyWindow::setPage(int page) {
  int current = m_currentPageEdit->text().toInt();
  if (current == page) return;
  m_currentPageEdit->setText(QString::number(page));
  int firstPage = (MyParams::instance()->backsideImgPath().isEmpty()) ? 1 : 0;
  m_prev->setDisabled(page == firstPage);
  m_next->setDisabled(page == m_totalPageCount);
  updatePreview();
}

int MyWindow::duration() {
  if (m_durations.isEmpty()) return 0;
  // 動画シートがある場合はそちらを優先
  if (m_durations.contains(Area_Cells))
    return m_durations.value(Area_Cells);
  else
    return m_durations.first();
}

class ChangePagesUndo : public QUndoCommand {
  int m_before, m_after;
  MyWindow* m_viewer;

public:
  ChangePagesUndo(MyWindow* viewer, int before, int after)
      : m_viewer(viewer), m_before(before), m_after(after) {}
  void undo() { m_viewer->setPage(m_before); }
  void redo() { m_viewer->setPage(m_after); }
};

void MyWindow::onPrev() {
  if (!m_prev->isEnabled()) return;
  int current = m_currentPageEdit->text().toInt();
  assert(current > 0);

  MyParams::instance()->undoStack()->push(
      new ChangePagesUndo(this, current, current - 1));
}

void MyWindow::onNext() {
  if (!m_next->isEnabled()) return;
  int current = m_currentPageEdit->text().toInt();
  assert(current < m_totalPageCount);

  MyParams::instance()->undoStack()->push(
      new ChangePagesUndo(this, current, current + 1));
}

void MyWindow::onImageEdited() {
  MyParams::instance()->setImageDirty(MyParams::instance()->currentPage());
}

QAction* MyWindow::getToolAction(ToolId id) {
  for (auto action : m_toolActionGroup->actions()) {
    if (id == (ToolId)action->data().toInt()) return action;
  }
  return nullptr;
}

void MyWindow::onToolActionTriggered(QAction* act) {
  ToolId toolId = (ToolId)act->data().toInt();
  MyParams::instance()->setCurrentTool(toolId);
}

void MyWindow::onToolSwitched() {
  ToolId toolId = MyParams::instance()->currentToolId();
  getToolAction(toolId)->setChecked(true);
}

void MyWindow::onCurrentColorActionTriggered(QAction* act) {
  QColor color = act->data().value<QColor>();

  m_currentColorAction->setIcon(color2Icon(color));
  MyParams::instance()->setCurrentColor(color);
}

void MyWindow::onBrushSizeActionTriggered(QAction* act) {
  BrushTool::BrushSize sizeId = (BrushTool::BrushSize)act->data().toInt();

  BrushTool* brushTool =
      dynamic_cast<BrushTool*>(MyParams::instance()->getTool(Tool_Brush));
  brushTool->setSize(sizeId);

  QIcon icon = (sizeId == BrushTool::Large)
                   ? QIcon(":Resources/brush_large.svg")
                   : QIcon(":Resources/brush_small.svg");
  getToolAction(Tool_Brush)->setIcon(icon);

  MyParams::instance()->setCurrentTool(Tool_Brush);
}

void MyWindow::onSelectionModeActionTriggered(QAction* act) {
  SelectionTool::SelectionMode modeId =
      (SelectionTool::SelectionMode)act->data().toInt();

  SelectionTool* selectionTool = dynamic_cast<SelectionTool*>(
      MyParams::instance()->getTool(Tool_Selection));
  selectionTool->setMode(modeId);

  QIcon icon = (modeId == SelectionTool::Rect)
                   ? QIcon(":Resources/selection_rectangular.svg")
                   : QIcon(":Resources/selection_freehand.svg");
  getToolAction(Tool_Selection)->setIcon(icon);

  MyParams::instance()->setCurrentTool(Tool_Selection);
}

void MyWindow::onLineTypeActionTriggered(QAction* act) {
  LineId lineId = (LineId)act->data().toInt();

  LineTool* lineTool =
      dynamic_cast<LineTool*>(MyParams::instance()->getTool(Tool_Line));
  lineTool->setType(lineId);
  static QStringList iconPaths = {
      ":Resources/line_thin.svg",       ":Resources/line_thick.svg",
      ":Resources/line_thin_arrow.svg", ":Resources/line_thick_arrow.svg",
      ":Resources/line_double.svg",     ":Resources/line_dot.svg"};
  QIcon icon(iconPaths[lineId]);
  getToolAction(Tool_Line)->setIcon(icon);

  MyParams::instance()->setCurrentTool(Tool_Line);
}

void MyWindow::onStampTypeActionTriggered(QAction* act) {
  int stampId = act->data().toInt();

  StampTool* stampTool =
      dynamic_cast<StampTool*>(MyParams::instance()->getTool(Tool_Stamp));
  stampTool->setStampId(stampId);

  MyParams::instance()->setCurrentTool(Tool_Stamp);
}
//---------------------------------------------------
// Undo, Redo
//---------------------------------------------------
void MyWindow::onUndo() {
  if (MyParams::instance()->isBusy()) return;
  // これ以上Undoできなければ警告を出してreturn
  if (!MyParams::instance()->undoStack()->canUndo()) {
    QMessageBox::warning(this, tr("Undo"),
                         tr("No more Undo operations available."),
                         QMessageBox::Ok);
    return;
  }

  // Undo
  MyParams::instance()->undoStack()->undo();
}

void MyWindow::onRedo() {
  if (MyParams::instance()->isBusy()) return;
  // これ以上Redoできなければ警告を出してreturn
  if (!MyParams::instance()->undoStack()->canRedo()) {
    QMessageBox::warning(this, tr("Redo"),
                         tr("No more Redo operations available."),
                         QMessageBox::Ok);
    return;
  }

  // Undo
  MyParams::instance()->undoStack()->redo();
}

void MyWindow::onLoad(const QString& xdtsPath) {
  // 前のファイルの保存確認
  if (m_data) {
    bool ok = askAndSaveChanges();
    if (!ok) return;
  }

  // ここでブラウザを開いてXDTSを指定させる
  QString currentXdtsPath = xdtsPath;
  if (currentXdtsPath.isEmpty()) {
    QString dirPath =
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString previousXdtsPath = MyParams::instance()->currentXdtsPath();
    if (previousXdtsPath.isEmpty()) {
      if (QFileInfo(PathUtils::getUserSettingsPath()).exists()) {
        QSettings settings(PathUtils::getUserSettingsPath(),
                           QSettings::IniFormat);
        previousXdtsPath =
            settings.value("UserSettings/LastOpenedXdtsPath", previousXdtsPath)
                .toString();
      }
    }
    if (!previousXdtsPath.isEmpty())
      dirPath = QFileInfo(previousXdtsPath).dir().absolutePath();

    currentXdtsPath = QFileDialog::getOpenFileName(
        this, tr("Open XDTS File"), dirPath, tr("XDTS files (*.xdts)"));
    // if (currentXdtsPath.isEmpty()) {
    //   return;
    // }
  }
  if (!currentXdtsPath.isEmpty())
    MyParams::instance()->setCurrentXdtsPath(currentXdtsPath);

  // 手描き画像のリセット
  m_previewPane->setScribbleImage(QImage());

  m_columns.clear();
  m_durations.clear();

  QList<ExportArea> areaIds = MyParams::instance()->exposeAreas();
  for (auto areaId : areaIds) {
    if (m_data) delete m_data;
    m_data = new XdtsData();

    QString path = MyParams::instance()->currentXdtsPath(areaId);

    bool ok = loadXdtsScene(m_data, path);

    if (!ok) {
      QMessageBox::critical(this, tr("Error"),
                            tr("%1 is not a valid XDTS file.").arg(path));
      // close();
      // qApp->quit();
      continue;
    }

    // obtain level names
    // QStringList levelNames = m_data->getLevelNames();
    // in case multiple columns have the same name
    // levelNames.removeDuplicates();

    if (m_data->isEmpty() || m_data->timeTable().isEmpty()) continue;

    XdtsTimeTableFieldItem cellField   = m_data->timeTable().getCellField();
    XdtsTimeTableHeaderItem cellHeader = m_data->timeTable().getCellHeader();
    int tmpDuration                    = m_data->timeTable().getDuration();
    m_durations.insert(areaId, tmpDuration);
    QStringList layerNames = cellHeader.getLayerNames();
    QList<int> columns     = cellField.getOccupiedColumns();

    ColumnsData data;
    for (int column : columns) {
      QString levelName = layerNames.at(column);
      QList<int> tick1, tick2;
      bool isHoldFrame =
          false;  // 1フレーム目の値と空セルのみ格納された列はtrueを返す
      QVector<FrameData> track =
          cellField.getColumnTrack(column, tick1, tick2, isHoldFrame);
      // std::cout << "level:" << levelName.toStdString() << "   trackSize:" <<
      // track.size() << std::endl;
      //  空セルで埋める
      //  std::maxを用いているのは、原画シートと動画シートの尺が違う場合への対応。
      //  動画シートがある場合、そちらの尺を優先する
      for (int i = track.size(); i < std::max(duration(), tmpDuration); i++) {
        track.append({QString(), FrameOption_None});
        isHoldFrame =
            false;  // このセルは最後まで止メセルではない（途中で空セルになる）のでfalseに戻す
      }
      data.append({track, levelName, isHoldFrame});
    }

    m_columns.insert(areaId, data);
  }

  // カット尺を入れる（原画／動画両方ある場合は動画を優先）
  m_settingsDialog->setDuration(duration());

  // 空でも閉じない
  // if (m_columns.isEmpty()) {
  //  QMessageBox::critical(this, tr("Error"), tr("Failed to open any files."));
  //  //close();
  //  //qApp->quit();
  //  return;
  //}

  // プロダクション初期値を読み込む。無ければ決め打ちの初期値にする
  MyParams::instance()->resetValues();

  // もし設定があれば読み込む
  bool settingExists = MyParams::instance()->loadFormatSettingsIfExists();

  initTemplate();

  MyParams::instance()->convertCompatibleParameters();

  m_settingsDialog->syncUIs();
  m_preferencesDialog->syncUIs();
  if (!settingExists) {
    m_settingsDialog->show();
  }

  updateTitleBar();
}

// タイトルバーに「名称未設定」と入れる
void MyWindow::updateTitleBar() {
  QString str;
  QString path = MyParams::instance()->currentXdtsPath();
  if (path.isEmpty())
    str = tr("Untitled");
  else
    str = QFileInfo(MyParams::instance()->currentXdtsPath()).fileName();
  if (MyParams::instance()->somethingIsDirty()) str += " *";

  str += " - " + qApp->applicationName();
  setWindowTitle(str);
}

bool MyWindow::askAndSaveChanges() {
  if (MyParams::instance()->currentXdtsPath().isEmpty()) return true;
  if (!MyParams::instance()->somethingIsDirty()) return true;
  int ret = QMessageBox::question(
      this, tr("Question"),
      tr("The document has been modified.\nDo you want to save your changes?"),
      QMessageBox::StandardButtons(QMessageBox::Save | QMessageBox::Discard |
                                   QMessageBox::Cancel),
      QMessageBox::Save);
  if (ret == QMessageBox::Cancel)
    return false;
  else if (ret == QMessageBox::Discard)
    return true;
  // save data

  MyParams::instance()->setScribbleImage(MyParams::instance()->currentPage(),
                                         m_previewPane->scribbleImage());
  MyParams::instance()->saveChanges();

  return true;
}

void MyWindow::closeEvent(QCloseEvent* event) {
  if (askAndSaveChanges()) {
    QMainWindow::closeEvent(event);
    MyParams::instance()->saveUserSettings();
    MyParams::instance()->saveWindowGeometry(geometry());
  } else
    event->ignore();
}

void MyWindow::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
}

void MyWindow::dropEvent(QDropEvent* event) {
  if (MyParams::instance()->isBusy()) return;
  foreach (const QUrl& url, event->mimeData()->urls()) {
    QString fileName = url.toLocalFile();
    std::cout << "Dropped file:" << fileName.toStdString() << std::endl;
    // open xdts file
    if (fileName.endsWith(".xdts")) {
      onLoad(fileName);
    } else if (!QImageReader::imageFormat(fileName).isEmpty()) {
      QImage image(fileName);

      if (!image.isNull()) {
        // 120dpi以外の場合、MyParams::PDF_Resolutionとの比で拡大縮小して配置する
        QRect rect;
        // dpi取れてるっぽいね。。dpi無しの場合は"4724"(120dpi)となるようだ
        int img_dpmx = image.dotsPerMeterX();
        if (img_dpmx != 4724) {
          double scaleRatio = (double)MyParams::PDF_Resolution /
                              ((double)img_dpmx * 2.54 / 100.);
          rect = QRect(QPoint(), image.size() * scaleRatio);
        }

        MyParams::instance()->setCurrentTool(Tool_Selection);
        SelectionTool* selectionTool =
            dynamic_cast<SelectionTool*>(MyParams::instance()->currentTool());
        selectionTool->pasteImage(image, rect);
      }
    }
    // accept only the first dropped file
    break;
  }
}

void MyWindow::resizeEvent(QResizeEvent* event) {
  m_previewPane->updateCanvas();
}

void MyWindow::onSave() {
  MyParams::instance()->setScribbleImage(MyParams::instance()->currentPage(),
                                         m_previewPane->scribbleImage());

  if (MyParams::instance()->currentXdtsPath().isEmpty()) {
    if (!MyParams::instance()->saveUntitled()) return;
  }

  MyParams::instance()->saveChanges();
}

void MyWindow::onExport() {
  MyParams::instance()->setScribbleImage(MyParams::instance()->currentPage(),
                                         m_previewPane->scribbleImage());
  // ファイルパス取得
  QString defaultPath = MyParams::instance()->getImageFolderPath() + ".png";
#ifdef WIN32
  QString fileName =
      QFileDialog::getSaveFileName(this, tr("Export Images"), defaultPath,
                                   tr("PNG (*.png);;PSD files (*.psd)"));
#else
  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Export Images"), defaultPath, tr("PNG (*.png)"));
#endif

  if (fileName.isEmpty()) return;

#ifdef WIN32
  if (fileName.endsWith(".psd")) {
    onExportPSD(fileName);
    return;
  }
#endif

  if (fileName.endsWith(".png")) fileName.chop(4);

  // 上書き確認
  XSheetPDFTemplate* tmpl = MyParams::instance()->currentTemplate();
  QStringList existingNames;
  bool hasBackside = !MyParams::instance()->backsideImgPath().isEmpty();
  int framePageCount =
      std::max(tmpl->framePageCount(),
               MyParams::instance()->getScannedSheetPageAmount());
  int parallelPageCount = tmpl->parallelPageCount();
  for (int fpage = (hasBackside) ? 0 : 1; fpage <= framePageCount; fpage++) {
    for (int ppage = 0; ppage < parallelPageCount; ppage++) {
      QString pageStr = QString::number(fpage);
      if (parallelPageCount > 1) pageStr += QChar('A' + ppage);
      QString tmpFile = fileName + "_" + pageStr + ".png";
      // 存在する場合はファイル名を格納
      if (QFileInfo(tmpFile).exists())
        existingNames.append(QFileInfo(tmpFile).fileName());
      if (fpage == 0) break;
    }
  }
  if (!existingNames.isEmpty()) {
    QString msgStr = tr(
        "Following file(s) are already exist. Do you want to overwrite them?");
    for (auto fn : existingNames) msgStr += "\n - " + fn;
    int ret = QMessageBox::question(
        this, tr("Question"), msgStr,
        QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel),
        QMessageBox::Ok);
    if (ret != QMessageBox::Ok) return;
  }

  // ここから書き出し
  MyWindow::setInfo(true);
  int currentPage = (hasBackside) ? 0 : 1;
  for (int fpage = (hasBackside) ? -1 : 0; fpage < framePageCount; fpage++) {
    for (int ppage = 0; ppage < parallelPageCount; ppage++, currentPage++) {
      QString pageStr = QString::number(fpage + 1);
      if (parallelPageCount > 1 && fpage >= 0) pageStr += QChar('A' + ppage);
      QString tmpFile = fileName + "_" + pageStr + ".png";

      QPixmap pm;
      if (fpage == -1)
        pm = MyParams::instance()->backsidePixmap(true);
      else {
        pm = tmpl->initializePreview();
        QPainter painter(&pm);
        tmpl->drawXsheetTemplate(painter, fpage, true);
        tmpl->drawXsheetContents(painter, fpage, ppage, true);
      }
      QPainter painter(&pm);
      painter.setCompositionMode(QPainter::CompositionMode_Multiply);
      painter.drawImage(0, 0, MyParams::instance()->scribbleImage(currentPage));
      painter.end();

      pm.save(tmpFile);

      if (fpage == -1) {
        currentPage++;
        break;
      }
    }
  }

  std::vector<QString> buttons = {tr("OK"), tr("Open containing folder")};
  int ret = QMessageBox::information(this, tr("Export Images"),
                                     tr("Xsheet images are exported properly."),
                                     buttons[0], buttons[1]);

  if (ret == 1) {
    QString dirPath = QFileInfo(fileName).absoluteDir().path();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
  }
  // info戻し
  MyWindow::setInfo();
}

//---------------------------------------------------

void MyWindow::onCut() {
  if (MyParams::instance()->isBusy()) return;
  // SelectionToolのときのみ
  SelectionTool* selectionTool =
      dynamic_cast<SelectionTool*>(MyParams::instance()->currentTool());
  if (selectionTool) selectionTool->onCut();
}

void MyWindow::onCopy() {
  if (MyParams::instance()->isBusy()) return;
  // SelectionToolのときのみ
  SelectionTool* selectionTool =
      dynamic_cast<SelectionTool*>(MyParams::instance()->currentTool());
  if (selectionTool) selectionTool->onCopy();
}

void MyWindow::onPaste() {
  if (MyParams::instance()->isBusy()) return;
  // 画像のmimedataがある場合、selectionToolに切り替えてマウス位置に貼り付け
  const QClipboard* clipboard = QApplication::clipboard();
  const QMimeData* mimeData   = clipboard->mimeData();
  if (mimeData && mimeData->hasImage()) {
    MyParams::instance()->setCurrentTool(Tool_Selection);
    SelectionTool* selectionTool =
        dynamic_cast<SelectionTool*>(MyParams::instance()->currentTool());
    selectionTool->onPaste();
  }
}

void MyWindow::onDelete() {
  if (MyParams::instance()->isBusy()) return;
  // SelectionToolのときのみ
  SelectionTool* selectionTool =
      dynamic_cast<SelectionTool*>(MyParams::instance()->currentTool());
  if (selectionTool) selectionTool->onDelete();
}

void MyWindow::onBacksideImgPathChanged() {
  int currentPage = m_currentPageEdit->text().toInt();
  int firstPage   = (MyParams::instance()->backsideImgPath().isEmpty()) ? 1 : 0;
  m_currentPageEdit->setValidator(
      new QIntValidator(firstPage, m_totalPageCount, this));

  if (currentPage == 0 && firstPage == 1) {
    m_currentPageEdit->setText("1");
    currentPage = 1;
  }
  m_prev->setDisabled(currentPage == firstPage);
  m_next->setDisabled(currentPage == m_totalPageCount);
}

void MyWindow::onAbout() {
  static AboutDialog aboutDialog(this);
  aboutDialog.exec();
}