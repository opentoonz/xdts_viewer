#include "aetool.h"
#include "xsheetpreviewarea.h"
#include "myparams.h"

#include <QMessageBox>
#include <QGuiApplication>
#include <QClipboard>
#include <QPropertyAnimation>

AETool::AETool() : Tool(Tool_AE), m_currentColumnId(-1) {
  setProperty("HighlightAlpha", 0);
  m_animation = new QPropertyAnimation(this, "HighlightAlpha", this);
  m_animation->setDuration(250);
  m_animation->setStartValue(255);
  m_animation->setEndValue(0);
}

void AETool::setView(XsheetPdfPreviewPane* view) {
  m_view = view;
  connect(m_animation, SIGNAL(valueChanged(const QVariant&)), m_view,
          SLOT(update()));
}

void AETool::onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) {
  if (m_currentColumnId == -1) return;

  XSheetPDFTemplate* tmpl = MyParams::instance()->currentTemplate();
  int currentPage         = MyParams::instance()->currentPage();
  int parallelPageCount   = tmpl->parallelPageCount();
  int pPage               = (currentPage - 1) % parallelPageCount;

  // 表示可能列が小さい方に合わせる
  QMap<ExportArea, ColumnsData> columns = tmpl->columns();
  int colsInPage                        = 1000;
  for (auto area : columns.keys())
    colsInPage = std::min(colsInPage, tmpl->columnsInPage(area));

  int startColId = colsInPage * pPage;
  int colId      = m_currentColumnId + startColId;

  ExportArea area =
      columns.keys().contains(Area_Cells) ? Area_Cells : Area_Unspecified;
  if (area == Area_Unspecified &&
      MyParams::instance()->exportArea() == Area_Actions)
    return;

  if (colId >= columns.value(area).size()) return;

  ColumnData columnData = columns.value(area).at(colId);

  // std::cout << "Level name = " << columnData.name.toStdString() << std::endl;

  int emptyFrame = MyParams::instance()->emptyFrameForAE();

  QStringList errorFrames;
  bool hasTheSameFrameAsEmpty =
      false;  // 空コマの予約フレームと同じ番号が入っていた場合に警告を出す

  QString kfDataTxt;

  kfDataTxt +=
      "Adobe After Effects 9.0 Keyframe Data\n"
      "\n"
      "\tUnits Per Second\t24\n"
      "\tSource Width\t640\n"
      "\tSource Height\t480\n"
      "\tSource Pixel Aspect Ratio\t1\n"
      "\tComp Pixel Aspect Ratio\t1\n"
      "\n"
      "Layer\n"
      "Time Remap\n"
      "\tFrame\tseconds\n";

  for (int f = 0; f < columnData.cells.size(); f++) {
    QString valTxt = columnData.cells.at(f).frame;
    if (valTxt.isEmpty()) continue;
    int val;
    if (valTxt == XdtsFrameDataItem::SYMBOL_NULL_CELL)
      val = emptyFrame;
    else {
      bool ok;
      val = valTxt.toInt(&ok);
      // 数値に変換できない文字列が入っていた場合
      if (!ok) {
        errorFrames.append(QString::number(f + 1));
        continue;
      }
      if (val == emptyFrame) hasTheSameFrameAsEmpty = true;
    }
    kfDataTxt += QString("\t%1\t%2\n").arg(f).arg((double)(val - 1) / 24.);
  }
  kfDataTxt += "\nEnd of Keyframe Data\n";

  QClipboard* clipboard = QGuiApplication::clipboard();
  clipboard->setText(kfDataTxt);

  if (!errorFrames.isEmpty() || hasTheSameFrameAsEmpty) {
    QString errorMsg;
    if (!errorFrames.isEmpty()) {
      errorMsg = tr("The following frames were not copied because they "
                    "contain non-numeric strings.\n - %1")
                     .arg(errorFrames.join(", "));
      if (hasTheSameFrameAsEmpty) errorMsg += "\n\n";
    }
    if (hasTheSameFrameAsEmpty)
      errorMsg +=
          tr("There is a cell with the same number as the one reserved for "
             "empty frames.");

    QMessageBox::warning(m_view,
                         tr("Copy After Effects Keyframe Data To Clipboard"),
                         errorMsg, QMessageBox::Ok);
  }

  setProperty("HighlightColumn", m_currentColumnId);
  m_animation->start();
}

void AETool::onMove(const QPointF& pos) {
  m_currentColumnId = -1;

  if (MyParams::instance()->currentPage() == 0) return;

  // 現在Hoverしているカラムを得る
  XSheetPDFTemplate* tmpl = MyParams::instance()->currentTemplate();

  QList<QList<QRect>> colLabelRects = tmpl->colLabelRects(Area_Cells);
  QList<QList<QRect>> cellRects     = tmpl->cellRects(Area_Cells);

  QPoint _pos = (pos / m_view->scaleFactor()).toPoint();

  for (int colId = 0; colId < colLabelRects.size(); colId++) {
    for (auto labelRect : colLabelRects[colId]) {
      if (labelRect.contains(_pos)) {
        m_currentColumnId = colId;
        return;
      }
    }
    for (auto cellRect : cellRects[colId]) {
      if (cellRect.contains(_pos)) {
        m_currentColumnId = colId;
        return;
      }
    }
  }
}

void AETool::draw(QPainter& painter, QPointF pos, double scaleFactor) {
  int columnId = m_currentColumnId;

  int highlightAlpha = property("HighlightAlpha").toInt();
  if (highlightAlpha > 0) columnId = property("HighlightColumn").toInt();

  if (columnId < 0) return;

  XSheetPDFTemplate* tmpl           = MyParams::instance()->currentTemplate();
  QList<QList<QRect>> colLabelRects = tmpl->colLabelRects(Area_Cells);
  QList<QList<QRect>> cellRects     = tmpl->cellRects(Area_Cells);
  if (cellRects.isEmpty()) return;
  painter.save();

  painter.scale(scaleFactor, scaleFactor);
  painter.setCompositionMode(QPainter::CompositionMode_Multiply);

  painter.setBrush(QColor(255, 255, 168));
  painter.setPen(Qt::NoPen);

  for (auto labelRect : colLabelRects[columnId]) {
    painter.drawRect(labelRect);
  }
  for (auto cellRect : cellRects[columnId]) {
    painter.drawRect(cellRect);
  }

  if (highlightAlpha) {
    painter.setBrush(QColor(120, 255, 0, highlightAlpha));
    for (auto labelRect : colLabelRects[columnId]) {
      painter.drawRect(labelRect);
    }
    for (auto cellRect : cellRects[columnId]) {
      painter.drawRect(cellRect);
    }
  }
}

bool AETool::onActivate() {
  // 動画欄にデータが無ければ警告を出してツールを元に戻す
  bool hasDougaSheet;
  if (MyParams::instance()->isAreaSpecified())
    hasDougaSheet = MyParams::instance()->exposeAreas().contains(Area_Cells);
  else
    hasDougaSheet = MyParams::instance()->exportArea() == Area_Cells;

  if (!hasDougaSheet) {
    QMessageBox::warning(
        m_view, tr("Copy After Effects Keyframe Data To Clipboard"),
        tr("This tool requires XDTS to be displayed in the Cell area."),
        QMessageBox::Ok);
    return false;
  }

  // テレコ表示がONで、テレコが実際にあったら警告を出してツールを元に戻す
  XSheetPDFTemplate* tmpl = MyParams::instance()->currentTemplate();
  if (tmpl && tmpl->hasTerekoColumns(Area_Cells)) {
    QMessageBox::warning(
        m_view, tr("Copy After Effects Keyframe Data To Clipboard"),
        tr("There are mix-up columns in the Cell area.\nPlease turn off the "
           "\"Mix-up Columns\" checkbox in the Settings in order to use this "
           "tool."),
        QMessageBox::Ok);
    return false;
  }
  return true;
}

AETool _aeTool;