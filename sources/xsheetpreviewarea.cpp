
#include "xsheetpreviewarea.h"
#include "xdtsio.h"
#include "tool.h"

#include <QPainterPath>
#include <QSettings>
#include <QMouseEvent>
#include <QScrollBar>
#include <QMenu>
#include <QList>
#include <QApplication>
#include <QMessageBox>

using namespace XSheetPDFTemplateParamIDs;
int MyParams::PDF_Resolution = 250;

namespace {
// for performance reason
// const int PDF_Resolution = 250;
// const int PDF_Resolution = 400;
int mm2px(double mm) {
  return (int)std::round(mm * (double)MyParams::PDF_Resolution / 25.4);
}

//-----------------------------------------------------------------------------
/*! convert the last one digit of the frame number to alphabet
        Ex.  12 -> 1B    21 -> 2A   30 -> 3
 */
QString getFrameNumberWithLetters(int frame) {
  int letterNum = frame % 10;
  QChar letter;

  switch (letterNum) {
  case 0:
    letter = QChar();
    break;
  case 1:
    letter = 'A';
    break;
  case 2:
    letter = 'B';
    break;
  case 3:
    letter = 'C';
    break;
  case 4:
    letter = 'D';
    break;
  case 5:
    letter = 'E';
    break;
  case 6:
    letter = 'F';
    break;
  case 7:
    letter = 'G';
    break;
  case 8:
    letter = 'H';
    break;
  case 9:
    letter = 'I';
    break;
  default:
    letter = QChar();
    break;
  }

  QString number;
  if (frame >= 10) {
    number = QString::number(frame);
    number.chop(1);
  } else
    number = "0";

  return (QChar(letter).isNull()) ? number : number.append(letter);
}

void decoSceneInfo(QPainter& painter, QRect rect,
                   QMap<XSheetPDFDataType, QRect>& dataRects, bool) {
  dataRects[Data_SceneName] = painter.transform().mapRect(rect);
}

void decoTimeInfo(QPainter& painter, QRect rect,
                  QMap<XSheetPDFDataType, QRect>& dataRects, bool doTranslate) {
  QString plusStr, secStr, frmStr;
  if (doTranslate) {
    plusStr = QObject::tr("+", "XSheetPDF");
    secStr  = QObject::tr("'", "XSheetPDF:second");
    frmStr  = QObject::tr("\"", "XSheetPDF:frame");
  } else {
    plusStr = "+";
    secStr  = "'";
    frmStr  = "\"";
  }

  painter.save();
  {
    QFont font = painter.font();
    font.setPixelSize(rect.height() / 2 - mm2px(1));
    font.setLetterSpacing(QFont::PercentageSpacing, 100);
    while (font.pixelSize() > mm2px(2)) {
      if (QFontMetrics(font).boundingRect(plusStr).width() < rect.width() / 12)
        break;
      font.setPixelSize(font.pixelSize() - mm2px(0.2));
    }
    painter.setFont(font);

    QRect labelRect(0, 0, rect.width() / 2, rect.height() / 2);
    QRect dataRect(0, 0, rect.width() / 2, rect.height());
    painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, secStr);
    dataRects[Data_Second] = painter.transform().mapRect(dataRect);
    painter.translate(labelRect.width(), 0);
    painter.drawText(labelRect.adjusted(0, 0, 0, -mm2px(0.5)),
                     Qt::AlignRight | Qt::AlignVCenter, frmStr);
    dataRects[Data_Frame] = painter.transform().mapRect(dataRect);
    painter.translate(0, labelRect.height());
    painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, plusStr);
  }
  painter.restore();
}

// Display the total pages over the current page.
// This format is odd in terms of fraction, but can be seen in many Japanese
// studios.

void doDecoSheetInfo(QPainter& painter, QRect rect,
                     QMap<XSheetPDFDataType, QRect>& dataRects,
                     bool doTranslate, bool inv) {
  QString totStr, thStr;
  if (doTranslate) {
    totStr = QObject::tr("TOT", "XSheetPDF");
    thStr  = QObject::tr("th", "XSheetPDF");
  } else {
    totStr = "TOT";
    thStr  = "th";
  }
  QString upperStr             = (inv) ? totStr : thStr;
  QString bottomStr            = (inv) ? thStr : totStr;
  XSheetPDFDataType upperType  = (inv) ? Data_TotalPages : Data_CurrentPage;
  XSheetPDFDataType bottomType = (inv) ? Data_CurrentPage : Data_TotalPages;

  painter.save();
  {
    QFont font = painter.font();
    font.setPixelSize(rect.height() / 2 - mm2px(1));
    font.setLetterSpacing(QFont::PercentageSpacing, 100);
    while (font.pixelSize() > mm2px(2)) {
      if (QFontMetrics(font).boundingRect(totStr).width() < rect.width() / 6)
        break;
      font.setPixelSize(font.pixelSize() - mm2px(0.2));
    }
    painter.setFont(font);

    painter.drawLine(
        QPoint((rect.right() + rect.center().x()) / 2, rect.top()),
        QPoint((rect.left() + rect.center().x()) / 2, rect.bottom()));
    // painter.drawLine(rect.topRight(), rect.bottomLeft());
    QRect labelRect(0, 0, rect.width() / 2, rect.height() / 2);
    painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, upperStr);
    dataRects[upperType] = painter.transform().mapRect(
        labelRect.adjusted(0, 0, -labelRect.width() / 4, mm2px(1)));
    painter.translate(labelRect.width(), labelRect.height());
    painter.drawText(labelRect.adjusted(0, 0, -mm2px(0.5), 0),
                     Qt::AlignRight | Qt::AlignVCenter, bottomStr);
    dataRects[bottomType] = painter.transform().mapRect(
        labelRect.adjusted(0, -mm2px(1), -labelRect.width() / 4, 0));
  }
  painter.restore();
}

void decoSheetInfoInv(QPainter& painter, QRect rect,
                      QMap<XSheetPDFDataType, QRect>& dataRects,
                      bool doTranslate) {
  doDecoSheetInfo(painter, rect, dataRects, doTranslate, true);
}

void decoSheetInfo(QPainter& painter, QRect rect,
                   QMap<XSheetPDFDataType, QRect>& dataRects,
                   bool doTranslate) {
  doDecoSheetInfo(painter, rect, dataRects, doTranslate, false);
}

QPageSize::PageSizeId str2PageSizeId(const QString& str) {
  const QMap<QString, QPageSize::PageSizeId> map = {
      {"JisB4", QPageSize::JisB4},
      {"B4", QPageSize::JisB4},  // return Jis B4 instead of ISO B4
      {"A3", QPageSize::A3},
      {"A4", QPageSize::A4}};

  return map.value(str, QPageSize::JisB4);
}

DecoFunc infoType2DecoFunc(const QString& str) {
  const QMap<QString, DecoFunc> map = {{"Scene", decoSceneInfo},
                                       {"Time", decoTimeInfo},
                                       {"Sheet", decoSheetInfo},
                                       {"SheetInv", decoSheetInfoInv}};

  return map.value(str, nullptr);
}

XSheetPDFDataType dataStr2Type(const QString& str) {
  const QMap<QString, XSheetPDFDataType> map = {
      {"Memo", Data_Memo},
      {"Second", Data_Second},
      {"Frame", Data_Frame},
      {"OverlapLength", Data_OverlapLength},
      {"TotalPages", Data_TotalPages},
      {"CurrentPage", Data_CurrentPage},
      {"DateTimeAndScenePath", Data_DateTimeAndScenePath},
      {"SceneName", Data_SceneName},
      {"Logo", Data_Logo}};

  return map.value(str, Data_Invalid);
}

QPixmap tickMarkPm(TickMarkType type, int size, const QColor color,
                   bool withBG = false) {
  QPixmap pm(size, size);
  QPointF center(double(size) * 0.5, double(size) * 0.5);

  pm.fill((withBG) ? Qt::white : Qt::transparent);
  QPainter p(&pm);
  QPen pen(color);
  pen.setWidthF(double(size) / 15.0);
  p.setPen(pen);
  switch (type) {
  case TickMark_Dot: {
    p.setBrush(color);
    double dotR = double(size) * 0.1;
    p.drawEllipse(center, dotR, dotR);
    break;
  }
  case TickMark_Circle: {
    double circleR = double(size) * 0.4;
    p.drawEllipse(center, circleR, circleR);
    break;
  }
  case TickMark_Filled: {
    p.setBrush(color);
    double circleR = double(size) * 0.4;
    p.drawEllipse(center, circleR, circleR);
    break;
  }
  case TickMark_Asterisk: {
    QFont font = p.font();
    font.setPixelSize(size);
    p.setFont(font);
    p.drawText(0, 0, size, size, Qt::AlignCenter, "*");
    break;
  }
  }
  return pm;
}
/*
QComboBox* createTickMarkCombo(QWidget* parent) {
  QComboBox* combo = new QComboBox(parent);
  combo->addItem(QIcon(tickMarkPm(TickMark_Dot, 15, true)),
    QObject::tr("Dot", "XSheetPDF CellMark"), TickMark_Dot);
  combo->addItem(QIcon(tickMarkPm(TickMark_Circle, 15, true)),
    QObject::tr("Circle", "XSheetPDF CellMark"), TickMark_Circle);
  combo->addItem(QIcon(tickMarkPm(TickMark_Filled, 15, true)),
    QObject::tr("Filled circle", "XSheetPDF CellMark"),
    TickMark_Filled);
  combo->addItem(QIcon(tickMarkPm(TickMark_Asterisk, 15, true)),
    QObject::tr("Asterisk", "XSheetPDF CellMark"),
    TickMark_Asterisk);
  return combo;
}
*/
void doDrawText(QPainter& p, const QString& str, const QRect rect) {
  QFontMetrics fm(p.font());
  int spaceWidth = fm.boundingRect('A').width();
  // check if spaces can be iserted between letters
  int textWidth = fm.boundingRect(str).width() + spaceWidth * (str.count() - 1);
  if (rect.width() - spaceWidth * 2 <= textWidth) {
    p.drawText(rect, Qt::AlignCenter, str);
    return;
  }
  // check if spaces can be doubled
  int textWidth_s2 =
      fm.boundingRect(str).width() + spaceWidth * 2 * (str.count() - 1);
  if (rect.width() - spaceWidth * 4 > textWidth_s2) textWidth = textWidth_s2;
  QRect textRect(rect.center().x() - textWidth / 2, rect.y(), textWidth,
                 rect.height());
  p.drawText(textRect,
             Qt::TextJustificationForced | Qt::AlignJustify | Qt::AlignVCenter,
             str);
}

void setFontFittingRectWidth(QPainter& p, const QString& str, const QRect rect,
                             double vmargin = 0.5, double hmargin = 1.0) {
  QFont font    = p.font();
  int pixelSize = rect.height() - mm2px(vmargin);
  while (1) {
    font.setPixelSize(pixelSize);
    if (pixelSize <= mm2px(2) || QFontMetrics(font).boundingRect(str).width() <=
                                     rect.width() - mm2px(hmargin))
      break;
    pixelSize -= mm2px(0.1);
  }
  p.setFont(font);
}

//-------

inline bool areAlmostEqual(double a, double b, double err = 1e-8) {
  return std::abs(a - b) < err;
}
#define ZOOMLEVELS 9
#define NOZOOMINDEX 2

double ZoomFactors[ZOOMLEVELS] = {0.125, 0.17678, 0.25, 0.35355, 0.5,
                                  0.7,   1,       1.41, 2};

// double ZoomFactors[ZOOMLEVELS] = {
//     0.125, 0.25, 0.5, 1, 2 };

double getQuantizedZoomFactor(double zf, bool forward) {
  if (forward && (zf > ZoomFactors[ZOOMLEVELS - 1] ||
                  areAlmostEqual(zf, ZoomFactors[ZOOMLEVELS - 1], 1e-5)))
    return zf;
  else if (!forward &&
           (zf < ZoomFactors[0] || areAlmostEqual(zf, ZoomFactors[0], 1e-5)))
    return zf;

  assert((!forward && zf > ZoomFactors[0]) ||
         (forward && zf < ZoomFactors[ZOOMLEVELS - 1]));
  int i = 0;
  for (i = 0; i <= ZOOMLEVELS - 1; i++)
    if (areAlmostEqual(zf, ZoomFactors[i], 1e-5)) zf = ZoomFactors[i];

  if (forward && zf < ZoomFactors[0])
    return ZoomFactors[0];
  else if (!forward && zf > ZoomFactors[ZOOMLEVELS - 1])
    return ZoomFactors[ZOOMLEVELS - 1];

  for (i = 0; i < ZOOMLEVELS - 1; i++)
    if (ZoomFactors[i + 1] - zf >= 0 && zf - ZoomFactors[i] >= 0) {
      if (forward && ZoomFactors[i + 1] == zf)
        return ZoomFactors[i + 2];
      else if (!forward && ZoomFactors[i] == zf)
        return ZoomFactors[i - 1];
      else
        return forward ? ZoomFactors[i + 1] : ZoomFactors[i];
    }
  return ZoomFactors[NOZOOMINDEX];
}

void frame2SecKoma(int frame, int& sec, int& koma) {
  sec  = frame / 24;
  koma = frame % 24;
}
int secKoma2Frame(int sec, int koma) { return sec * 24 + koma; }

// 空でなく、中割記号でもない場合にtrueを返す
inline bool isNotEmptyOrSymbol(const QString& frame) {
  return !frame.isEmpty() && frame != XdtsFrameDataItem::SYMBOL_TICK_1 &&
         frame != XdtsFrameDataItem::SYMBOL_TICK_2 &&
         frame != XdtsFrameDataItem::SYMBOL_HYPHEN;
}

}  // namespace
//---------------------------------------------------------

void XSheetPDFTemplate::drawGrid(QPainter& painter, int colAmount, int colWidth,
                                 int blockWidth) {
  // horiontal lines
  painter.save();
  {
    // loop 3 seconds
    for (int sec = 0; sec < 3; sec++) {
      painter.save();
      {
        for (int f = 1; f <= 24; f++) {
          if (f % 6 == 0)
            painter.setPen(thickPen);
          else
            painter.setPen(thinPen);
          painter.translate(0, param(RowHeight));
          painter.drawLine(0, 0, blockWidth, 0);
        }
      }
      painter.restore();

      painter.translate(0, param(OneSecHeight));
      painter.setPen(thickPen);
      painter.drawLine(0, 0, blockWidth, 0);
    }
  }
  painter.restore();
  // vertical lines
  painter.save();
  {
    painter.setPen(thinPen);
    for (int kc = 0; kc < colAmount; kc++) {
      painter.translate(colWidth, 0);
      painter.drawLine(0, 0, 0, param(OneSecHeight) * 3);
    }
  }
  painter.restore();
}

void XSheetPDFTemplate::drawHeaderGrid(QPainter& painter, int colAmount,
                                       int colWidth, int blockWidth) {
  // Cells Block header
  painter.save();
  {
    // horizontal lines
    painter.setPen(thinPen);
    painter.drawLine(0, param(HeaderHeight) / 2, blockWidth,
                     param(HeaderHeight) / 2);
    painter.setPen(thickPen);
    painter.drawLine(0, param(HeaderHeight), blockWidth, param(HeaderHeight));
    // vertical lines
    painter.setPen(thinPen);
    painter.save();
    {
      for (int col = 1; col < colAmount; col++) {
        painter.translate(colWidth, 0);
        painter.drawLine(0, param(HeaderHeight) / 2, 0, param(HeaderHeight));
      }
    }
    painter.restore();
  }
  painter.restore();
}

void XSheetPDFTemplate::registerColLabelRects(QPainter& painter, int colAmount,
                                              int colWidth, int bodyId,
                                              ExportArea area) {
  painter.save();
  {
    for (int kc = 0; kc < colAmount; kc++) {
      QRect labelRect(0, 0, colWidth, param(HeaderHeight) / 2);
      labelRect = painter.transform().mapRect(labelRect);

      if (bodyId == 0) {
        QList<QRect> labelRects = {labelRect};
        m_colLabelRects[area].append(labelRects);
      } else
        m_colLabelRects[area][kc].append(labelRect);

      painter.translate(colWidth, 0);
    }
  }
  painter.restore();

  if (!m_info.drawLevelNameOnBottom) return;

  // register bottom rects
  painter.save();
  {
    painter.translate(0, param(BodyHeight) - param(HeaderHeight) / 2);

    for (int kc = 0; kc < colAmount; kc++) {
      QRect labelRect(0, 0, colWidth, param(HeaderHeight) / 2);
      labelRect = painter.transform().mapRect(labelRect);

      if (bodyId == 0) {
        QList<QRect> labelRects = {labelRect};
        m_colLabelRects_bottom[area].append(labelRects);
      } else
        m_colLabelRects_bottom[area][kc].append(labelRect);

      painter.translate(colWidth, 0);
    }
  }
  painter.restore();
}

void XSheetPDFTemplate::registerCellRects(QPainter& painter, int colAmount,
                                          int colWidth, int bodyId,
                                          ExportArea area) {
  int heightAdj = (param(OneSecHeight) - param(RowHeight) * 24) / 2;
  painter.save();
  {
    for (int kc = 0; kc < colAmount; kc++) {
      QList<QRect> colCellRects;
      QRect colBBox;

      painter.save();
      {
        for (int sec = 0; sec < 3; sec++) {
          painter.save();
          {
            for (int f = 1; f <= 24; f++) {
              QRect cellRect(0, 0, colWidth, param(RowHeight));
              // fill gap between the doubled lines between seconds
              if (sec != 0 && f == 1)
                cellRect.adjust(0, -heightAdj, 0, 0);
              else if (sec != 2 && f == 24)
                cellRect.adjust(0, 0, 0, heightAdj);

              colCellRects.append(painter.transform().mapRect(cellRect));
              painter.translate(0, param(RowHeight));

              // BBoxの更新
              colBBox = colBBox.united(colCellRects.last());
            }
          }
          painter.restore();
          painter.translate(0, param(OneSecHeight));
        }
      }
      painter.restore();

      if (bodyId == 0)
        m_cellRects[area].append(colCellRects);
      else
        m_cellRects[area][kc].append(colCellRects);

      // BBoxの更新
      while (m_bodyBBoxes[area].count() <= bodyId)
        m_bodyBBoxes[area].append(QRect());
      m_bodyBBoxes[area][bodyId] = m_bodyBBoxes[area][bodyId].united(colBBox);

      painter.translate(colWidth, 0);
    }
  }
  painter.restore();
}

void XSheetPDFTemplate::registerSoundRects(QPainter& painter, int colWidth,
                                           int bodyId) {
  int heightAdj = (param(OneSecHeight) - param(RowHeight) * 24) / 2;
  painter.save();
  {
    for (int sec = 0; sec < 3; sec++) {
      painter.save();
      {
        for (int f = 1; f <= 24; f++) {
          QRect cellRect(0, 0, colWidth, param(RowHeight));
          // fill gap between the doubled lines between seconds
          if (sec != 0 && f == 1)
            cellRect.adjust(0, -heightAdj, 0, 0);
          else if (sec != 2 && f == 24)
            cellRect.adjust(0, 0, 0, heightAdj);

          m_soundCellRects.append(painter.transform().mapRect(cellRect));
          painter.translate(0, param(RowHeight));
        }
      }
      painter.restore();
      painter.translate(0, param(OneSecHeight));
    }
  }
  painter.restore();
}

void XSheetPDFTemplate::registerCameraRects(QPainter& painter, int colAmount,
                                            int colWidth) {
  int width = (m_useExtraColumns) ? colWidth : colAmount * colWidth;
  int xPos  = (m_useExtraColumns) ? colAmount * colWidth : 0;

  int heightAdj = (param(OneSecHeight) - param(RowHeight) * 24) / 2;
  painter.save();
  painter.translate(xPos, 0);
  {
    for (int sec = 0; sec < 3; sec++) {
      painter.save();
      {
        for (int f = 1; f <= 24; f++) {
          QRect cellRect(0, 0, width, param(RowHeight));
          // fill gap between the doubled lines between seconds
          if (sec != 0 && f == 1)
            cellRect.adjust(0, -heightAdj, 0, 0);
          else if (sec != 2 && f == 24)
            cellRect.adjust(0, 0, 0, heightAdj);

          m_cameraCellRects.append(painter.transform().mapRect(cellRect));
          painter.translate(0, param(RowHeight));
        }
      }
      painter.restore();
      painter.translate(0, param(OneSecHeight));
    }
  }
  painter.restore();
}

// Key Block
void XSheetPDFTemplate::drawKeyBlock(QPainter& painter, int framePage,
                                     const int bodyId) {
  QFont font = painter.font();
  font.setPixelSize(m_p.bodylabelTextSize_Small);
  painter.setFont(font);

  painter.save();
  {
    drawHeaderGrid(painter, param(KeyColAmount), param(KeyColWidth),
                   m_p.keyBlockWidth);

    if (m_info.exportAreas.contains(Area_Actions)) {
      painter.save();
      {
        painter.translate(0, param(HeaderHeight) / 2);
        // register actions rects.
        registerColLabelRects(painter, columnsInPage(Area_Actions),
                              param(KeyColWidth), bodyId, Area_Actions);
      }
      painter.restore();
    }

    if (param(DrawKeysHeaderLabel, 1) == 1) {
      QRect labelRect(0, 0, m_p.keyBlockWidth, param(HeaderHeight) / 2);
      QString actionLabel = (param(TranslateBodyLabel, 1) == 1)
                                ? QObject::tr("ACTION", "XSheetPDF")
                                : "ACTION";
      doDrawText(painter, actionLabel, labelRect);
    }

    painter.save();
    {
      painter.translate(0, param(HeaderHeight));

      // Key Animation Block
      drawGrid(painter, param(KeyColAmount), param(KeyColWidth),
               m_p.keyBlockWidth);

      if (m_info.exportAreas.contains(Area_Actions))
        // register cell rects.
        registerCellRects(painter, columnsInPage(Area_Actions),
                          param(KeyColWidth), bodyId, Area_Actions);

      // frame numbers
      painter.save();
      {
        font.setPixelSize(param(RowHeight) - mm2px(2));
        font.setLetterSpacing(QFont::PercentageSpacing, 100);
        painter.setFont(font);

        if (param(LastKeyColWidth) > 0) {
          painter.translate(param(KeyColAmount) * param(KeyColWidth), 0);
          for (int sec = 0; sec < 3; sec++) {
            painter.save();
            {
              for (int f = 1; f <= 24; f++) {
                if (f % 2 == 0) {
                  int frame = bodyId * 72 + sec * 24 + f;
                  if (m_info.serialFrameNumber)
                    frame += param(FrameLength) * framePage;
                  QRect frameLabelRect(0, 0,
                                       param(LastKeyColWidth) - mm2px(0.5),
                                       param(RowHeight));
                  painter.drawText(frameLabelRect,
                                   Qt::AlignRight | Qt::AlignVCenter,
                                   QString::number(frame));
                }
                painter.translate(0, param(RowHeight));
              }
            }
            painter.restore();
            painter.translate(0, param(OneSecHeight));
          }
        }
        // draw frame numbers on the left side of the block
        else {
          painter.translate(-param(KeyColWidth) * 2, 0);
          for (int sec = 0; sec < 3; sec++) {
            painter.save();
            {
              for (int f = 1; f <= 24; f++) {
                if (f % 2 == 0) {
                  int frame = bodyId * 72 + sec * 24 + f;
                  if (m_info.serialFrameNumber)
                    frame += param(FrameLength) * framePage;
                  QRect frameLabelRect(0, 0,
                                       param(KeyColWidth) * 2 - mm2px(0.5),
                                       param(RowHeight));
                  painter.drawText(frameLabelRect,
                                   Qt::AlignRight | Qt::AlignVCenter,
                                   QString::number(frame));
                }
                painter.translate(0, param(RowHeight));
              }
            }
            painter.restore();
            painter.translate(0, param(OneSecHeight));
          }
        }
      }
      painter.restore();
    }
    painter.restore();

    painter.translate(m_p.keyBlockWidth, 0);
    painter.setPen(blockBorderPen);
    painter.drawLine(0, 0, 0, param(BodyHeight));
  }
  painter.restore();
}

void XSheetPDFTemplate::drawDialogBlock(QPainter& painter, const int framePage,
                                        const int bodyId) {
  QFont font = painter.font();
  font.setPixelSize(m_p.bodylabelTextSize_Large);
  font.setLetterSpacing(QFont::PercentageSpacing, 100);

  QRect labelRect(0, 0, param(DialogColWidth), param(HeaderHeight));
  QString serifLabel =
      (param(TranslateBodyLabel, 1) == 1) ? QObject::tr("S", "XSheetPDF") : "S";
  while (font.pixelSize() > mm2px(1)) {
    if (QFontMetrics(font).boundingRect(serifLabel).width() <
        labelRect.width() - mm2px(2))
      break;
    font.setPixelSize(font.pixelSize() - mm2px(0.5));
  }
  painter.setFont(font);
  painter.drawText(labelRect, Qt::AlignCenter, serifLabel);

  // triangle shapes at every half seconds
  static const QPointF points[3] = {
      QPointF(mm2px(-0.8), 0.0),
      QPointF(mm2px(-2.8), mm2px(1.25)),
      QPointF(mm2px(-2.8), mm2px(-1.25)),
  };
  QRect secLabelRect(0, 0, param(DialogColWidth) - mm2px(1.0),
                     param(RowHeight));

  painter.save();
  {
    font.setPixelSize(param(RowHeight) - mm2px(1.5));
    painter.setFont(font);

    painter.translate(0, param(HeaderHeight));
    if (param(DrawDialogHeaderLine, 0) == 1) {
      painter.drawLine(0, 0, param(DialogColWidth), 0);
    }
    painter.save();
    {
      for (int sec = 1; sec <= 3; sec++) {
        painter.save();
        {
          painter.setBrush(painter.pen().color());
          painter.setPen(Qt::NoPen);
          painter.translate(param(DialogColWidth), param(RowHeight) * 12);
          painter.drawPolygon(points, 3);
        }
        painter.restore();
        painter.save();
        {
          int second = bodyId * 3 + sec;
          if (m_info.serialFrameNumber)
            second += framePage * param(FrameLength) / 24;
          painter.translate(0, param(RowHeight) * ((sec == 3) ? 23 : 23.5));
          painter.drawText(secLabelRect, Qt::AlignRight | Qt::AlignVCenter,
                           QString::number(second));
        }
        painter.restore();
        painter.translate(0, param(OneSecHeight));
      }
    }
    painter.restore();

    // register sound cells
    if (m_info.drawSound)
      //  if (m_info.drawSound || m_noteColumn)
      registerSoundRects(painter, param(DialogColWidth), bodyId);
  }
  painter.restore();

  // 縦線を描く部分はrawCellsBlockに移動する
  // painter.save();
  //{
  //  painter.setPen(blockBorderPen);
  //  painter.translate(param(DialogColWidth), 0);
  //  painter.drawLine(0, 0, 0, param(BodyHeight));
  //}
  // painter.restore();
}

void XSheetPDFTemplate::drawCellsBlock(QPainter& painter, int bodyId) {
  QFont font = painter.font();
  font.setPixelSize(m_p.bodylabelTextSize_Small);
  painter.setFont(font);
  // 縦線を描く
  painter.save();
  {
    painter.setPen(blockBorderPen);
    painter.drawLine(0, 0, 0, param(BodyHeight));
  }
  painter.restore();

  painter.save();
  {
    drawHeaderGrid(painter, param(CellsColAmount), param(CellsColWidth),
                   m_p.cellsBlockWidth);

    if (m_info.exportAreas.contains(Area_Cells)) {
      painter.save();
      {
        painter.translate(0, param(HeaderHeight) / 2);
        // register cell rects
        registerColLabelRects(painter, columnsInPage(Area_Cells),
                              param(CellsColWidth), bodyId, Area_Cells);
      }
      painter.restore();
    }

    if (param(DrawCellsHeaderLabel, 1) == 1) {
      QRect labelRect(0, 0, m_p.cellsBlockWidth, param(HeaderHeight) / 2);
      QString cellsLabel = (param(TranslateBodyLabel, 1) == 1)
                               ? QObject::tr("CELL", "XSheetPDF")
                               : "CELL";
      doDrawText(painter, cellsLabel, labelRect);
    }

    painter.save();
    {
      painter.translate(0, param(HeaderHeight));
      // Cells Block
      drawGrid(painter, param(CellsColAmount) - 1, param(CellsColWidth),
               m_p.cellsBlockWidth);

      if (m_info.exportAreas.contains(Area_Cells))
        // register cell rects.
        registerCellRects(painter, columnsInPage(Area_Cells),
                          param(CellsColWidth), bodyId, Area_Cells);
    }
    painter.restore();

    painter.setPen(blockBorderPen);
    painter.translate(m_p.cellsBlockWidth, 0);
    painter.drawLine(0, 0, 0, param(BodyHeight));
  }
  painter.restore();
}

void XSheetPDFTemplate::drawCameraBlock(QPainter& painter, const int bodyId) {
  QFont font = painter.font();
  font.setPixelSize(m_p.bodylabelTextSize_Large);
  painter.setFont(font);

  int totalCameraColmnAount =
      param(CameraColAmount) + MyParams::instance()->getCameraColumnAddition();
  int totalCameraBlockWidth =
      m_p.cameraBlockWidth + m_p.cameraColumnAdditionWidth;

  painter.save();
  {
    // Camera Block header
    painter.save();
    {
      QString cameraLabel = (param(TranslateBodyLabel, 1) == 1)
                                ? QObject::tr("CAMERA", "XSheetPDF")
                                : "CAMERA";
      if (param(DrawCameraHeaderGrid, 0) == 1) {
        drawHeaderGrid(painter, totalCameraColmnAount, param(CameraColWidth),
                       totalCameraBlockWidth);
        if (param(DrawCameraHeaderLabel, 1) == 1) {
          font.setPixelSize(m_p.bodylabelTextSize_Small);
          painter.setFont(font);
          QRect labelRect(0, 0, totalCameraBlockWidth, param(HeaderHeight) / 2);
          doDrawText(painter, cameraLabel, labelRect);
        }
      } else {
        // horizontal lines
        painter.setPen(thickPen);
        painter.drawLine(0, param(HeaderHeight), totalCameraBlockWidth,
                         param(HeaderHeight));

        if (param(DrawCameraHeaderLabel, 1) == 1) {
          font.setPixelSize(m_p.bodylabelTextSize_Large);
          painter.setFont(font);
          QRect labelRect(0, 0, totalCameraBlockWidth, param(HeaderHeight));
          doDrawText(painter, cameraLabel, labelRect);
        }
      }
    }
    painter.restore();

    if (param(DrawCameraGrid, 1) != 0 || m_useExtraColumns) {
      painter.save();
      {
        painter.translate(0, param(HeaderHeight));
        // Cells Block
        drawGrid(painter, totalCameraColmnAount - 1, param(CameraColWidth),
                 totalCameraBlockWidth);
      }
      painter.restore();
    }

    // register camera cells
    if (m_info.startOverlapFrameLength > 0 ||
        m_info.endOverlapFrameLength > 0) {
      painter.save();
      painter.translate(0, param(HeaderHeight));
      registerCameraRects(painter, totalCameraColmnAount,
                          param(CameraColWidth));
      painter.restore();
    }
  }
  painter.restore();
}

void XSheetPDFTemplate::drawXsheetBody(QPainter& painter, int framePage,
                                       int bodyId) {
  // Body
  painter.save();
  {
    painter.setPen(bodyOutlinePen);
    painter.drawRect(QRect(0, 0, param(BodyWidth), param(BodyHeight)));
    if (MyParams::instance()->isScannedGengaSheet()) {
      setPenVisible(false);
      painter.setPen(bodyOutlinePen);
    }
    drawKeyBlock(painter, framePage, bodyId);
    painter.translate(m_p.keyBlockWidth, 0);
    drawDialogBlock(painter, framePage, bodyId);
    painter.translate(param(DialogColWidth), 0);
    if (MyParams::instance()->isScannedGengaSheet()) {
      setPenVisible(true);
      painter.translate(m_p.cellColumnOffset, 0);
    }
    drawCellsBlock(painter, bodyId);
    painter.translate(m_p.cellsBlockWidth, 0);
    if (MyParams::instance()->isScannedGengaSheet()) {
      setPenVisible(false);
    }
    drawCameraBlock(painter, bodyId);
    if (MyParams::instance()->isScannedGengaSheet()) {
      setPenVisible(true);
    }
  }
  painter.restore();
}

void XSheetPDFTemplate::setPenVisible(bool show) {
  QList<QPen*> pens = {&thinPen, &thickPen, &blockBorderPen, &bodyOutlinePen};
  for (auto pen : pens) {
    QColor col = pen->color();
    col.setAlpha((show) ? 255 : 0);
    pen->setColor(col);
  }
}

// drawXsheetContentsから呼ばれる。テレコ情報を踏まえて
// 各表示列の各フレームでの重ね順を記録する
QList<QList<int>> XSheetPDFTemplate::getOccupiedColumns(ExportArea area,
                                                        int startColId,
                                                        int colsInPage,
                                                        int startFrame) {
  // テレコ情報を考慮
  QList<TerekoInfo> terekoInfo =
      m_terekoColumns.value(area, QList<TerekoInfo>{});
  ColumnsData columns = m_columns.value(area);
  // 元の列がどの位置に表示されるか。テレコが無ければ0…,1…,2…と入るはず
  QList<QList<int>> occupiedColumns;

  int c = 0;
  for (int colId = startColId; c < colsInPage; c++, colId++) {
    if (colId == columns.size()) break;
    occupiedColumns.append(QList<int>{});
  }

  // XDTSのデータに含まれるテレコ列を検出する場合
  if (MyParams::instance()->mixUpColumnsType() == Mixup_Auto) {
    int r = 0;
    for (int f = startFrame; r < param(FrameLength); r++, f++) {
      c             = 0;
      int lastColId = 0;
      for (int colId = startColId; c < colsInPage; c++, colId++) {
        if (colId == columns.size()) break;
        // テレコの確認
        bool isTereko = false;
        for (auto ti : terekoInfo) {
          if (ti.colIdSet.contains(colId) && ti.occupiedColIds.size() > f) {
            if (ti.occupiedColIds[f] != colId)  // スキップされる列の場合
              occupiedColumns[c].append(-1);
            else  // テレコで現在中身のある列の場合
              occupiedColumns[c].append(lastColId++);
            isTereko = true;
            break;
          }
        }
        // テレコと無関係の列の場合、詰めて入れる
        if (!isTereko) occupiedColumns[c].append(lastColId++);
      }
    }
  }
  // テレコをXDTS Viewer内で指定する場合
  else {
    QMap<int, QList<int>> mixUpColumnsKeyframes =
        MyParams::instance()->mixUpColumnsKeyframes(area);

    QMap<int, QList<int>>::iterator itr =
        mixUpColumnsKeyframes.lowerBound(startFrame);
    QList<int> colOrder;
    // テレコなしの並び順
    if (mixUpColumnsKeyframes.isEmpty() ||
        itr == mixUpColumnsKeyframes.begin()) {
      for (int col = 0; col < columns.size(); col++) colOrder.append(col);
    }
    // テレコキーフレームの値
    else {
      itr--;
      colOrder = itr.value();
    }

    int r = 0;
    for (int f = startFrame; r < param(FrameLength); r++, f++) {
      if (mixUpColumnsKeyframes.contains(f)) {
        colOrder = mixUpColumnsKeyframes.value(f);
      }
      c = 0;
      for (int colId = startColId; c < colsInPage; c++, colId++) {
        if (colId == columns.size()) break;
        int oc = colOrder.indexOf(colId) - startColId;
        // 欄外にはみ出している場合 TODO：ページをまたいだ時の表示の解決
        if (oc >= colsInPage) oc = -1;
        occupiedColumns[c].append(oc);
      }
    }
  }

  return occupiedColumns;
}

void XSheetPDFTemplate::drawInfoHeader(QPainter& painter) {
  painter.save();
  {
    painter.translate(param(InfoOriginLeft), param(InfoOriginTop));
    QFont font = painter.font();
    font.setPixelSize(param(InfoTitleHeight) - mm2px(1));
    painter.setFont(font);
    bool isLeftMost = true;
    // draw each info
    for (auto info : m_p.array_Infos) {
      painter.setPen((isLeftMost) ? bodyOutlinePen : thinPen);
      isLeftMost = false;
      // vertical line
      painter.drawLine(0, 0, 0, m_p.infoHeaderHeight);
      // 3 horizontal lines
      painter.setPen(thinPen);
      painter.drawLine(0, param(InfoTitleHeight), info.width,
                       param(InfoTitleHeight));
      painter.setPen(bodyOutlinePen);
      painter.drawLine(0, 0, info.width, 0);
      painter.drawLine(0, m_p.infoHeaderHeight, info.width,
                       m_p.infoHeaderHeight);

      painter.setPen(thinPen);
      // label
      QRect labelRect(0, 0, info.width, param(InfoTitleHeight));
      doDrawText(painter, info.label, labelRect);

      if (info.decoFunc) {
        painter.save();
        {
          painter.translate(0, param(InfoTitleHeight));
          (*info.decoFunc)(painter,
                           QRect(0, 0, info.width, param(InfoBodyHeight)),
                           m_dataRects, param(TranslateInfoLabel, 1));
        }
        painter.restore();
      }

      // translate
      painter.translate(info.width, 0);
    }
    // vertical line at the rightmost edge
    painter.setPen(bodyOutlinePen);
    painter.drawLine(0, 0, 0, m_p.infoHeaderHeight);
  }
  painter.restore();
}

void XSheetPDFTemplate::addInfo(int w, QString lbl, DecoFunc f) {
  XSheetPDF_InfoFormat info;
  info.width    = w;
  info.label    = lbl;
  info.decoFunc = f;
  m_p.array_Infos.append(info);
};

void XSheetPDFTemplate::drawContinuousLine(QPainter& painter, QRect rect,
                                           bool isEmpty) {
  if (isEmpty) {
    int offset = rect.height() / 4;
    QPoint p0(rect.center().x(), rect.top());
    QPoint p3(rect.center().x(), rect.bottom());
    QPoint p1 = p0 + QPoint(-offset, offset);
    QPoint p2 = p3 + QPoint(offset, -offset);
    QPainterPath path(p0);
    path.cubicTo(p1, p2, p3);
    painter.drawPath(path);
  } else
    painter.drawLine(rect.center().x(), rect.top(), rect.center().x(),
                     rect.bottom());
}

void XSheetPDFTemplate::drawRepeatLine(QPainter& painter, QRect rect) {
  painter.save();
  QPen pen(painter.pen());
  pen.setColor(Qt::blue);
  pen.setStyle(Qt::DashLine);
  painter.setPen(pen);
  painter.drawLine(rect.center().x(), rect.top(), rect.center().x(),
                   rect.bottom());
  painter.restore();
}

void XSheetPDFTemplate::drawCellNumber(QPainter& painter, QRect rect,
                                       FrameData& cellFdata,
                                       QString terekoColName) {
  QFont font = painter.font();
  font.setPixelSize(param(RowHeight) - mm2px(1));
  font.setLetterSpacing(QFont::PercentageSpacing, 100);
  painter.setFont(font);

  if (cellFdata.frame == XdtsFrameDataItem::SYMBOL_NULL_CELL) {
    painter.drawLine(rect.topLeft(), rect.bottomRight());
    painter.drawLine(rect.topRight(), rect.bottomLeft());
  } else {
    QString str = cellFdata.frame;
    QRect txtRect =
        (terekoColName.isEmpty())
            ? rect
            : rect.adjusted(param(RowHeight) / 5, param(RowHeight) / 5, 0, 0);
    if (QFontMetrics(font).boundingRect(str).width() <= txtRect.width())
      painter.drawText(txtRect, Qt::AlignCenter, str);
    else {  // 文字を圧縮して表示
      QFont tmpFont = font;
      tmpFont.setLetterSpacing(QFont::PercentageSpacing, 90);
      double shrinkRatio = std::min(
          1., (double)(txtRect.width() - 1) /
                  (double)QFontMetrics(tmpFont).boundingRect(str).width());
      painter.save();
      painter.setFont(tmpFont);
      painter.translate(txtRect.center().x(), txtRect.center().y());
      painter.scale(shrinkRatio, 1.);
      QRect tmpRect = txtRect;
      tmpRect.setWidth(txtRect.width() * 2);
      tmpRect.moveCenter(QPoint(0, 0));
      painter.drawText(tmpRect, Qt::AlignCenter, str);
      painter.restore();
    }

    if (cellFdata.option != FrameOption_None) {
      painter.save();
      painter.setPen(QPen(painter.pen().color(), 2.));
      painter.translate(txtRect.center().x(), txtRect.center().y());
      if (cellFdata.option == FrameOption_KeyFrame) {
        painter.drawEllipse(QPoint(), 20, 20);
      } else if (cellFdata.option == FrameOption_ReferenceFrame) {
        static const QPoint points[6] = {QPoint(0, -23),   QPoint(-13, -2.5),
                                         QPoint(-23, 20),  QPoint(23, 20),
                                         QPoint(13, -2.5), QPoint(0, -23)};
        painter.drawPolyline(points, 6);
      }
      painter.restore();
    }

    if (!terekoColName.isEmpty())
      drawTerekoColumnLabel(painter, rect, terekoColName);

    /*if (isKey) {
      QPen keep(painter.pen());
      QPen circlePen(keep);
      circlePen.setWidth(mm2px(0.3));
      painter.setPen(circlePen);
      QFontMetrics fm(font);
      int keyR_width =
        std::max(param(RowHeight), fm.horizontalAdvance(str) + mm2px(1));
      QRect keyR(0, 0, keyR_width, param(RowHeight));
      keyR.moveCenter(rect.center());
      painter.drawEllipse(keyR);
      painter.setPen(keep);
    }*/
  }
}

void XSheetPDFTemplate::drawTickMark(QPainter& painter, QRect rect,
                                     TickMarkType type, QString terekoColName) {
  QRect tickR(0, 0, rect.height(), rect.height());
  tickR.moveCenter(rect.center());
  painter.drawPixmap(tickR,
                     tickMarkPm(type, rect.height(), painter.pen().color()));

  if (!terekoColName.isEmpty())
    drawTerekoColumnLabel(painter, rect, terekoColName);
}

void XSheetPDFTemplate::drawTerekoColumnLabel(QPainter& painter,
                                              const QRect rect,
                                              const QString terekoColName) {
  painter.save();
  QFont font = painter.font();
  font.setPixelSize(param(RowHeight) / 3);
  QRect terekoRect = QFontMetrics(font).boundingRect(terekoColName);
  painter.setFont(font);
  painter.setPen(Qt::blue);
  painter.drawText(rect.topLeft() - terekoRect.topLeft(), terekoColName);
  painter.restore();
}

void XSheetPDFTemplate::drawEndMark(QPainter& painter, QRect upperRect) {
  QRect rect = upperRect.translated(0, upperRect.height());

  painter.drawLine(rect.topLeft(), rect.topRight());

  painter.save();
  painter.setPen(QPen(QColor(0, 0, 0, 128), mm2px(0.2), Qt::SolidLine,
                      Qt::FlatCap, Qt::MiterJoin));
  painter.drawLine(rect.center().x(), rect.top(), rect.left(),
                   rect.center().y());
  painter.drawLine(rect.topRight(), rect.bottomLeft());
  painter.drawLine(rect.right(), rect.center().y(), rect.center().x(),
                   rect.bottom());
  painter.restore();
}

void XSheetPDFTemplate::drawLevelName(QPainter& painter, QRect rect,
                                      QString name, bool isBottom) {
  QFont font = painter.font();
  font.setPixelSize(rect.height());
  font.setLetterSpacing(QFont::PercentageSpacing, 100);
  painter.setFont(font);

  // if it can fit in the rect, then just draw it
  if (QFontMetrics(font).boundingRect(name).width() < rect.width()) {
    painter.drawText(rect, Qt::AlignCenter, name);
    return;
  }

  // if it can fit with 90% sized font
  QFont altFont(font);
  altFont.setPixelSize(font.pixelSize() * 0.9);
  if (QFontMetrics(altFont).boundingRect(name).width() < rect.width()) {
    painter.setFont(altFont);
    painter.drawText(rect, Qt::AlignCenter, name);
    return;
  }

  // 文字を圧縮して表示
  QFont smallFont(altFont);
  smallFont.setLetterSpacing(QFont::PercentageSpacing, 90);
  double shrinkRatio = std::min(
      1., (double)(rect.width() - 1) /
              (double)QFontMetrics(smallFont).boundingRect(name).width());
  if (shrinkRatio >= 0.5) {
    painter.save();
    painter.setFont(smallFont);
    painter.translate(rect.center().x(), rect.center().y());
    painter.scale(shrinkRatio, 1.);
    QRect tmpRect = rect;
    tmpRect.setWidth(rect.width() * 2);
    tmpRect.moveCenter(QPoint(0, 0));
    painter.drawText(tmpRect, Qt::AlignCenter, name);
    painter.restore();
    return;
  }

  // or, draw level name vertically
  // insert line breaks to every characters
  for (int i = 1; i < name.size(); i += 2) name.insert(i, '\n');
  // QFontMetrics::boundingRect(const QString &text) does NOT process newline
  // characters as linebreaks.
  QRect boundingRect = QFontMetrics(font).boundingRect(
      QRect(0, 0, mm2px(100), mm2px(100)), Qt::AlignTop | Qt::AlignLeft, name);
  if (!isBottom) {
    boundingRect.moveCenter(
        QPoint(rect.center().x(), rect.bottom() - boundingRect.height() / 2));
  } else {
    boundingRect.moveCenter(
        QPoint(rect.center().x(), rect.top() + boundingRect.height() / 2));
  }

  // fill background of the label
  painter.fillRect(boundingRect, Qt::white);
  painter.drawText(boundingRect, Qt::AlignCenter, name);
}

void XSheetPDFTemplate::drawTerekoArrow(QPainter& painter, QRect prevRect,
                                        QRect rect) {
  painter.save();
  QPoint from(prevRect.center().x(), prevRect.bottom() - mm2px(1));
  QPoint to(rect.center().x(), rect.top() + mm2px(1));
  painter.setPen(QPen(Qt::blue, 2));
  painter.drawLine(from, to);
  painter.save();
  painter.translate(to);
  QPoint vec = to - from;
  double deg = 180. * std::atan2(vec.y(), vec.x()) / M_PI;
  painter.rotate(deg);
  painter.rotate(30);
  painter.drawLine(QPoint(), QPoint(-mm2px(0.5), 0));
  painter.rotate(-60);
  painter.drawLine(QPoint(), QPoint(-mm2px(0.5), 0));
  painter.restore();
  painter.restore();
}

void XSheetPDFTemplate::drawOLMark(QPainter& painter, QRect rect,
                                   double topRatio, double bottomRatio) {
  painter.save();

  painter.setCompositionMode(QPainter::CompositionMode_Multiply);
  painter.translate(rect.topLeft());
  QPointF points[4] = {
      QPointF((double)rect.width() * topRatio, 0.),
      QPointF((double)rect.width() * (1. - topRatio), 0.),
      QPointF((double)rect.width() * (1. - bottomRatio), (double)rect.height()),
      QPointF((double)rect.width() * bottomRatio, (double)rect.height())};

  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::gray);
  painter.drawPolygon(points, 4);

  painter.restore();
}

void XSheetPDFTemplate::drawLogo(QPainter& painter) {
  if (!m_dataRects.contains(Data_Logo)) return;
  if (m_info.logoText.isEmpty() && m_info.logoPixmap.isNull()) return;
  QRect logoRect = m_dataRects.value(Data_Logo);

  if (!m_info.logoText.isEmpty()) {
    QFont font = painter.font();
    font.setPixelSize(logoRect.height() - mm2px(2));
    while (1) {
      if (QFontMetrics(font).boundingRect(m_info.logoText).width() <
              logoRect.width() &&
          QFontMetrics(font).boundingRect(m_info.logoText).height() <
              logoRect.height())
        break;
      if (font.pixelSize() <= mm2px(1)) break;
      font.setPixelSize(font.pixelSize() - mm2px(1));
    }
    painter.setFont(font);
    painter.drawText(logoRect, Qt::AlignTop | Qt::AlignLeft, m_info.logoText);
  }

  else if (!m_info.logoPixmap.isNull()) {
    painter.drawPixmap(logoRect.topLeft(), m_info.logoPixmap);
  }
}

int XSheetPDFTemplate::param(const std::string& id, int defaultValue) {
  if (!m_params.contains(id)) std::cout << id << std::endl;

  if (m_info.expandColumns) {
    // 列数が多い方
    int colsInScene = 0;
    for (auto area : m_columns.keys())
      colsInScene = std::max(colsInScene, m_columns[area].size());

    // 表示エリアを拡張する。原画動画ペアの場合は、一方しかデータが無くても両方拡張する。
    if (id == KeyColAmount && (m_info.exportAreas.contains(Area_Actions) ||
                               MyParams::instance()->isAreaSpecified())) {
      return std::max(colsInScene, m_params.value(id, defaultValue));
    } else if (id == CellsColAmount) {
      // 紙原画のスキャンを持ち込んでいるとき、Cell欄がAction欄と同じ列数になるように拡張する
      if (MyParams::instance()->isScannedGengaSheet()) {
        int gengaLevelsCount =
            MyParams::instance()
                ->gengaLevelsCount_Param();  // Autoの場合、-1が返るので考慮されない
        return std::max(
            colsInScene,
            std::max(gengaLevelsCount, m_params.value(id, defaultValue)));
      } else if (m_info.exportAreas.contains(Area_Cells) ||
                 MyParams::instance()->isAreaSpecified()) {
        colsInScene -= param(ExtraCellsColAmount, 0);
        return std::max(colsInScene, m_params.value(id, defaultValue));
      }
    } else if (id == BodyWidth) {
      int width = m_params.value(BodyWidth, defaultValue);
      // Actions area
      if (m_info.exportAreas.contains(Area_Actions) ||
          MyParams::instance()->isAreaSpecified()) {
        int exColAmount = colsInScene - m_params.value(KeyColAmount);
        if (exColAmount > 0) width += exColAmount * param(KeyColWidth);
      }
      // Cells area
      // 紙原画のスキャンを持ち込んでいるとき、Cell欄がAction欄に合わせて増えているかもしれない
      if (MyParams::instance()->isScannedGengaSheet()) {
        int exColAmount =
            param(CellsColAmount) - m_params.value(CellsColAmount);
        if (exColAmount > 0) width += exColAmount * param(CellsColWidth);
      } else if (m_info.exportAreas.contains(Area_Cells) ||
                 MyParams::instance()->isAreaSpecified()) {
        int exColAmount = colsInScene - param(ExtraCellsColAmount, 0) -
                          m_params.value(CellsColAmount);
        if (exColAmount > 0) width += exColAmount * param(CellsColWidth);
      }
      width += m_p.cellColumnOffset + m_p.cameraColumnAdditionWidth;
      return width;
    }
    // 列を増やす場合はカメラ列を喰わないようにする
    else if (id == ExtraCellsColAmount)
      return 0;
  }

  return m_params.value(id, defaultValue);
}

XSheetPDFTemplate::XSheetPDFTemplate(
    const QMap<ExportArea, ColumnsData>& columns, const int duration)
    : m_columns(), m_duration(duration), m_useExtraColumns(false) {
  // ここでスキップするレベル名の条件に照らし合わせて登録する
  QList<QRegExp> conditions;
  QStringList condStrList =
      MyParams::instance()->skippedLevelNames().split(";", Qt::SkipEmptyParts);
  for (auto condStr : condStrList) {
    QRegExp regExp(condStr);
    regExp.setPatternSyntax(QRegExp::Wildcard);
    conditions.append(regExp);
  }
  m_longestDuration = duration;
  // 全ての条件を突破したものだけ登録
  for (auto area : columns.keys()) {
    ColumnsData originalData = columns.value(area);
    ColumnsData displayData;
    for (auto column : originalData) {
      bool skipThis = false;
      for (auto condition : conditions) {
        if (condition.exactMatch(column.name)) {
          skipThis = true;
          break;
        }
      }
      if (!skipThis) {
        if (area == Area_Actions &&
            MyParams::instance()->isCapitalizeFirstLetter()) {
          QChar firstLetter = column.name[0];
          if (firstLetter.isLower())
            column.name = column.name.replace(0, 1, column.name[0].toUpper());
        }

        // カット尺の最後に×が付いている場合は無視する
        if (column.cells.size() == duration + 1 &&
            column.cells[duration].frame == XdtsFrameDataItem::SYMBOL_NULL_CELL)
          column.cells.removeLast();

        displayData.append(column);

        m_longestDuration = std::max(m_longestDuration, column.cells.size());
      }
    }
    m_columns.insert(area, displayData);
  }

  // 自動テレコが有効な場合、テレコ列の有無の確認
  if (MyParams::instance()->mixUpColumnsType() == Mixup_Auto)
    checkTerekoColumns();
  // 手動指定の場合、現在のテレコキーフレームの列数がm_columnsと合っているかを確認
  else
    checkMixupColumnsKeyframes();

  // 動画欠番を確認
  if (MyParams::instance()
          ->showSkippedDrawingsInfo())  // TODO: ONOFFできるように
    checkSkippedDrawings();

  m_colLabelRects.insert(Area_Actions, QList<QList<QRect>>());
  m_colLabelRects.insert(Area_Cells, QList<QList<QRect>>());
  m_colLabelRects_bottom.insert(Area_Actions, QList<QList<QRect>>());
  m_colLabelRects_bottom.insert(Area_Cells, QList<QList<QRect>>());
  m_cellRects.insert(Area_Actions, QList<QList<QRect>>());
  m_cellRects.insert(Area_Cells, QList<QList<QRect>>());
  m_bodyBBoxes.insert(Area_Actions, QList<QRect>());
  m_bodyBBoxes.insert(Area_Cells, QList<QRect>());
}

void XSheetPDFTemplate::setInfo(const XSheetPDFFormatInfo& info) {
  m_info = info;

  thinPen        = QPen(info.lineColor, param(ThinLineWidth, mm2px(0.25)),
                        Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
  thickPen       = QPen(info.lineColor, param(ThickLineWidth, mm2px(0.5)),
                        Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
  bodyOutlinePen = QPen(info.lineColor, param(BodyOutlineWidth, mm2px(0.5)),
                        Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
  blockBorderPen = (param(IsBlockBorderThick, 0) > 0) ? thickPen : thinPen;
  // check if it should use extra columns
  if ((info.exportAreas.contains(Area_Cells) ||
       MyParams::instance()->isAreaSpecified()) &&
      param(ExtraCellsColAmount, 0) > 0) {
    // 列数が多い方
    int colsInScene = 0;
    for (auto area : m_columns.keys()) {
      colsInScene = std::max(colsInScene, m_columns[area].size());
    }

    int colsInPage    = param(CellsColAmount);
    int colsInPage_Ex = param(CellsColAmount) + param(ExtraCellsColAmount);
    auto getPageNum   = [&](int cip) {
      int ret = colsInScene / cip;
      if (colsInScene % cip != 0 || colsInScene == 0) ret += 1;
      return ret;
    };
    if (getPageNum(colsInPage) > getPageNum(colsInPage_Ex))
      m_useExtraColumns = true;
  }

  m_p.keyBlockWidth =
      param(KeyColWidth) * param(KeyColAmount) + param(LastKeyColWidth);
  m_p.cellsBlockWidth         = param(CellsColWidth) * param(CellsColAmount);
  m_p.cameraBlockWidth        = param(CameraColWidth) * param(CameraColAmount);
  m_p.infoHeaderHeight        = param(InfoTitleHeight) + param(InfoBodyHeight);
  m_p.bodylabelTextSize_Large = param(HeaderHeight) - mm2px(4);
  m_p.bodylabelTextSize_Small = param(HeaderHeight) / 2 - mm2px(1);

  int gengaLevelsCount = MyParams::instance()->getGengaLevelsCount();
  m_p.cellColumnOffset =
      (gengaLevelsCount > param(KeyColAmount))
          ? param(CellsColWidth) * (gengaLevelsCount - param(KeyColAmount))
          : 0;
  // not using CameraColWidth, because the camera column overflows to the cell
  // area
  m_p.cameraColumnAdditionWidth =
      param(CellsColWidth) * MyParams::instance()->getCameraColumnAddition();
}

void XSheetPDFTemplate::drawXsheetTemplate(QPainter& painter, int framePage,
                                           bool isPreview) {
  // clear rects
  m_colLabelRects[Area_Actions].clear();
  m_colLabelRects[Area_Cells].clear();
  m_colLabelRects_bottom[Area_Actions].clear();
  m_colLabelRects_bottom[Area_Cells].clear();
  m_cellRects[Area_Actions].clear();
  m_cellRects[Area_Cells].clear();
  m_bodyBBoxes[Area_Actions].clear();
  m_bodyBBoxes[Area_Cells].clear();
  m_soundCellRects.clear();

  // painter.setFont(QFont("Times New Roman"));
  painter.setFont(m_info.templateFontFamily);
  painter.setPen(thinPen);

  painter.save();
  {
    if (isPreview)
      painter.translate(mm2px(m_p.documentMargin.left()),
                        mm2px(m_p.documentMargin.top()));

    drawLogo(painter);

    // draw Info header
    drawInfoHeader(painter);

    painter.translate(0, param(BodyTop));
    for (int bId = 0; bId < param(BodyAmount); bId++) {
      drawXsheetBody(painter, framePage, bId);
      painter.translate(param(BodyWidth) + param(BodyHMargin), 0);
    }
  }
  painter.restore();
}

void XSheetPDFTemplate::drawXsheetContents(QPainter& painter, int framePage,
                                           int parallelPage, bool isPreview) {
  // 継続線の頭のフレームを返す
  auto checkContinuous = [&](const QVector<FrameData> cells, int f, int r) {
    if (m_info.continuousLineMode == Line_Always)
      return f;
    else if (m_info.continuousLineMode == Line_None)
      return -1;
    QString cell = cells.at(f).frame;
    // check subsequent cells and see if more than 3 cells continue.
    int tmp_r = r + 1;
    for (int tmp_f = f + 1; tmp_f <= f + 3; tmp_f++, tmp_r++) {
      if (tmp_f == cells.size()) return -1;
      // if (tmp_f == m_duration) return false;
      // if (tmp_r % 72 == 0) return false;  // step over to the next body //
      // 次の段落にいっても線は継続する
      if (cells.size() <= tmp_f || !cells.at(tmp_f).frame.isEmpty()) return -1;
      // if (column.size() <= tmp_f || column.at(tmp_f) != cell) return false;
      //  tickmark breaks continuous line
      // QString tmpCell = column.at(tmp_f);
      // if (column.at(tmp_f) == XdtsFrameDataItem::SYMBOL_TICK_1
      //   || column.at(tmp_f) == XdtsFrameDataItem::SYMBOL_TICK_2)
      //   return false;
    }
    return f;
  };

  // 2ページ目以降の先頭コマが空セルの場合、フレームをさかのぼって継続線の頭のフレームを返す
  auto checkPrevContinuous = [&](const QVector<FrameData> cells, int f) {
    if (f == 0) return -1;
    // if (m_info.continuousLineMode == Line_Always)
    //   return  f;
    if (m_info.continuousLineMode == Line_None) return -1;
    QString cell = cells.at(f).frame;
    if (!cell.isEmpty()) return -1;

    // check previous cells and see if more than 3 cells continue.
    for (int tmp_f = f - 1; tmp_f >= 0; tmp_f--) {
      if (!cells.at(tmp_f).frame.isEmpty()) return tmp_f;
    }
    return -1;
  };

  const int maximumClLength = 9;  // 原画欄の継続線の最長長さ

  // draw soundtrack
  // drawSound(painter, framePage);

  painter.setPen(
      QPen(Qt::black, mm2px(0.5), Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
  painter.setFont(m_info.contentsFontFamily);

  // draw dialogue
  // drawDialogue(painter, framePage);

  // リピート列の確認
  checkRepeatColumns();

  // 表示可能列が小さい方に合わせる
  int colsInPage = 1000;
  for (auto area : m_columns.keys())
    colsInPage = std::min(colsInPage, columnsInPage(area));

  int startFrame = param(FrameLength) * framePage;
  int endFrame   = param(FrameLength) * (framePage + 1);

  // 列データの数だけ描画する
  for (auto area : m_columns.keys()) {
    ColumnsData columns = m_columns.value(area);
    ExportArea dispArea =
        (area == Area_Unspecified) ? m_info.exportAreas[0] : area;

    int startColId = colsInPage * parallelPage;

    // テレコ情報を考慮
    QList<TerekoInfo> terekoInfo =
        m_terekoColumns.value(area, QList<TerekoInfo>{});
    // 元の列がどの位置に表示されるか。テレコが無ければ0…,1…,2…と入るはず
    QList<QList<int>> occupiedColumns =
        getOccupiedColumns(area, startColId, colsInPage, startFrame);
    // ↑このoccupiedColumnsの情報を使ってセルに書き込む！！

    QMap<int, QList<RepeatInfo>> repeatColumnInfos;
    // リピート表示は原画欄のみ
    if (dispArea == Area_Actions)
      repeatColumnInfos =
          m_repeatColumns.value(area, QMap<int, QList<RepeatInfo>>());

    // コマの動画番号に色を付ける
    DyedCellsData dyedCells = MyParams::instance()->dyedCells(dispArea);

    int c = 0, r;
    for (int colId = startColId; c < colsInPage; c++, colId++) {
      if (colId == columns.size()) break;

      QVector<FrameData> cells = columns.at(colId).cells;
      if (cells.size() <= startFrame) continue;

      QString columnName = columns.at(colId).name;

      QList<RepeatInfo> repeatInfos =
          repeatColumnInfos.value(colId, QList<RepeatInfo>());

      FrameData prevCell;

      //  1ページ目であれば-1を返す。2ぺージ目以降の場合、前のフレームにさかのぼって継続線の開始フレームを探す
      int continuousLineStartFrame = checkPrevContinuous(cells, startFrame);
      if (continuousLineStartFrame != -1)
        prevCell = cells[continuousLineStartFrame];

      r                  = 0;
      int topColumnIndex = c, prevColumnIndex = occupiedColumns[c][0];

      // 描画フレーム f 毎に
      for (int f = startFrame; r < param(FrameLength); r++, f++) {
        if (m_duration == 0) break;
        if (f >= cells.size()) break;

        // 描画する列index
        int oc = occupiedColumns[c][r];

        if (r % 72 == 0) topColumnIndex = oc;

        // テレコ列で非表示の場合 continue
        if (oc < 0) {
          prevColumnIndex = oc;
          continue;
        }

        // draw level name
        if (r % 72 == 0) {
          drawLevelName(painter, m_colLabelRects[dispArea][oc][r / 72],
                        columnName);
        }
        // draw level name on bottom
        if (m_info.drawLevelNameOnBottom && r % 72 == 37 &&
            startFrame + 72 != m_duration)
          drawLevelName(painter, m_colLabelRects_bottom[dispArea][oc][r / 72],
                        columnName, true);

        FrameData cell = cells.at(f);

        // 動画番号に着色する
        if (dyedCells.contains({oc + startColId, f})) {
          QColor color = dyedCells.value({oc + startColId, f});
          if (color == QColor(Qt::white)) color = QColor(Qt::transparent);
          painter.setPen(QPen(color, painter.pen().widthF()));
        } else
          painter.setPen(QPen(Qt::black, painter.pen().widthF()));

        // repeat line
        // 現在のフレームがリピート線/止メの範囲かどうか？
        enum RepeatType { None, Repeat, Hold } currentRepeatType = None;
        // bool isInRepeat = false;
        int currentRepeatStartFrame = -1;
        for (auto repeatInfo : repeatInfos) {
          if (f >= repeatInfo.repeatEndFrame) continue;
          // 繰り返しの1コマ目は動画番号を書く。線が描かれるのは次のコマから
          int repeatStartFrame =
              repeatInfo.repeatChunkStartFrame + repeatInfo.repeatChunkLength;
          if (f < repeatStartFrame) continue;

          currentRepeatType =
              (repeatInfo.repeatChunkLength == 1) ? Hold : Repeat;
          currentRepeatStartFrame = repeatStartFrame;
          break;
        }
        // 繰り返しの1コマ目は動画番号を書く。線が描かれるのは次のコマから
        if (currentRepeatType != None && f != currentRepeatStartFrame) {
          // リピート線がmaximumClLengthより先には何も描かない
          // 止メの線は描かない
          if (currentRepeatType == Repeat &&
              f - currentRepeatStartFrame < maximumClLength)
            drawRepeatLine(painter, m_cellRects[dispArea][oc][r]);

          // テレコの先頭フレームの場合は列名を表示する
          if (prevColumnIndex != oc)
            drawTerekoColumnLabel(painter, m_cellRects[dispArea][oc][r],
                                  columnName);
        }
        // cotinuous line
        else if (cell.frame.isEmpty()) {
          if (continuousLineStartFrame >= 0 &&
              (dispArea == Area_Cells ||
               f - continuousLineStartFrame < maximumClLength))
            drawContinuousLine(painter, m_cellRects[dispArea][oc][r],
                               cells.at(continuousLineStartFrame).frame ==
                                   XdtsFrameDataItem::SYMBOL_NULL_CELL);
          else {
          }  // 原画欄で継続線がmaximumClLengthより先には何も描かない

          // テレコの先頭フレームの場合は列名を表示する
          if (prevColumnIndex != oc)
            drawTerekoColumnLabel(painter, m_cellRects[dispArea][oc][r],
                                  columnName);
        }
        // draw tick mark
        else if (cell.frame == XdtsFrameDataItem::SYMBOL_TICK_1 ||
                 cell.frame == XdtsFrameDataItem::SYMBOL_TICK_2) {
          if (cell.frame == XdtsFrameDataItem::SYMBOL_TICK_1)
            drawTickMark(painter, m_cellRects[dispArea][oc][r],
                         m_info.tick1MarkType,
                         (topColumnIndex != oc) ? columnName : "");
          else
            drawTickMark(painter, m_cellRects[dispArea][oc][r],
                         m_info.tick2MarkType,
                         (topColumnIndex != oc) ? columnName : "");
          continuousLineStartFrame = checkContinuous(cells, f, r);
          prevCell                 = cell;
        }
        // draw cell
        else {
          // リピートに入る1コマ目の文字を青くする
          if (currentRepeatStartFrame >= 2 && f == currentRepeatStartFrame) {
            painter.save();
            painter.setPen(Qt::blue);
          }

          drawCellNumber(painter, m_cellRects[dispArea][oc][r], cell,
                         (topColumnIndex != oc) ? columnName : "");

          if (currentRepeatStartFrame >= 2 && f == currentRepeatStartFrame)
            painter.restore();

          continuousLineStartFrame = checkContinuous(cells, f, r);
          prevCell                 = cell;
        }

        if (f == m_duration - 1) {
          drawEndMark(painter, m_cellRects[dispArea][oc][r]);
        }

        // テレコの矢印
        if (prevColumnIndex != oc && r % 72 != 0) {
          // 他のテレコ列からジャンプしてくる場合、ジャンプ元を調べる
          if (prevColumnIndex == -1) {
            for (auto ti : terekoInfo) {
              if (ti.colIdSet.contains(colId)) {
                int prev_colId  = ti.occupiedColIds[f - 1];
                prevColumnIndex = occupiedColumns[prev_colId][r - 1];
                break;
              }
            }
          }
          // それ以外はテレコ列のためにずれちゃった場合。
          drawTerekoArrow(painter,
                          m_cellRects[dispArea][prevColumnIndex][r - 1],
                          m_cellRects[dispArea][oc][r]);
        }

        prevColumnIndex = oc;
      }  // 次の 描画フレーム f へ

      // 「止メ」「リピート」表示。
      QFont font = painter.font();
      font.setPixelSize(param(RowHeight) - mm2px(1));
      painter.setFont(font);
      for (auto repeatInfo : repeatInfos) {
        int repeatStartFrame =
            repeatInfo.repeatChunkStartFrame + repeatInfo.repeatChunkLength;

        // スタートフレームが現在のページに含まれている場合に表示
        if (repeatStartFrame < startFrame ||
            startFrame + param(FrameLength) <= repeatStartFrame)
          continue;

        bool isRepeat = repeatInfo.repeatChunkLength > 1;

        QString holdTxt =
            (isRepeat) ? QObject::tr("REPEAT") : QObject::tr("HOLD");
        // insert line breaks to every characters
        for (int i = 1; i < holdTxt.size(); i += 2) holdTxt.insert(i, '\n');
        QRect boundingRect =
            QFontMetrics(painter.font())
                .boundingRect(QRect(0, 0, mm2px(100), mm2px(100)),
                              Qt::AlignTop | Qt::AlignLeft, holdTxt);
        int drawTxtFrame = (isRepeat) ? repeatStartFrame + 1 : repeatStartFrame;
        int oc           = occupiedColumns[c][repeatStartFrame - startFrame];
        if (oc < 0) continue;
        QRect rect = m_cellRects[dispArea][oc][drawTxtFrame - startFrame];
        boundingRect.moveCenter(
            QPoint(rect.center().x(), rect.top() + boundingRect.height() / 2));

        // fill background of the label
        painter.fillRect(boundingRect, Qt::white);
        painter.drawText(boundingRect, Qt::AlignCenter, holdTxt);
      }
    }
  }

  // OLの記号
  // 前OL
  if (m_info.startOverlapFrameLength > 0 &&
      startFrame < m_info.startOverlapFrameLength) {
    int r = 0;
    for (int f = startFrame; f < m_info.startOverlapFrameLength; f++, r++) {
      if (f == endFrame || f == m_duration) break;

      double topRatio = (double)f / (double)m_info.startOverlapFrameLength;
      double bottomRatio =
          (double)(f + 1) / (double)m_info.startOverlapFrameLength;

      drawOLMark(painter, m_cameraCellRects[r], topRatio, bottomRatio);
    }
  }
  // 後OL
  if (m_info.endOverlapFrameLength > 0 &&
      m_duration - m_info.endOverlapFrameLength < endFrame) {
    int start = std::max(startFrame, m_duration - m_info.endOverlapFrameLength);
    int r     = start % param(FrameLength);
    for (int f = start; f < endFrame; f++, r++) {
      if (f == m_duration) break;

      double topRatio =
          (double)(f - m_duration + m_info.endOverlapFrameLength) /
          (double)m_info.endOverlapFrameLength;
      double bottomRatio =
          (double)(f + 1 - m_duration + m_info.endOverlapFrameLength) /
          (double)m_info.endOverlapFrameLength;

      drawOLMark(painter, m_cameraCellRects[r], topRatio, bottomRatio);
    }
  }

  painter.setFont(m_info.templateFontFamily);
  QFont font = painter.font();
  font.setLetterSpacing(QFont::PercentageSpacing, 100);

  // draw data
  painter.save();
  {
    if (isPreview)
      painter.translate(mm2px(m_p.documentMargin.left()),
                        mm2px(m_p.documentMargin.top()));

    if (m_dataRects.contains(Data_Memo) && !m_info.memoText.isEmpty() &&
        framePage == 0) {
      // define the preferable font size
      int lines =
          std::max(m_info.memoText.count("\n") + 1, param(MemoLinesAmount));
      int lineSpacing = m_dataRects.value(Data_Memo).height() / lines;
      int pixelSize   = lineSpacing;
      while (1) {
        font.setPixelSize(pixelSize);
        if (pixelSize <= mm2px(2) ||
            QFontMetrics(font).lineSpacing() <= lineSpacing)
          break;
        pixelSize -= mm2px(0.2);
      }
      painter.setFont(font);
      painter.drawText(m_dataRects.value(Data_Memo),
                       Qt::AlignLeft | Qt::AlignTop, m_info.memoText);
    }

    QString dateTime_scenePath = m_info.dateTimeText;
    if (!m_info.scenePathText.isEmpty()) {
      if (!dateTime_scenePath.isEmpty()) dateTime_scenePath += "\n";
      dateTime_scenePath += m_info.scenePathText;
    }
    // 動画欠番  ※ 1ページ目のみ
    if (!m_skippedDrawingText.isEmpty() && framePage == 0) {
      // 動画欄に表示があるとき
      if (m_columns.keys().contains(Area_Cells) ||
          (m_columns.keys()[0] == Area_Unspecified &&
           m_info.exportAreas[0] == Area_Cells)) {
        if (!dateTime_scenePath.isEmpty()) dateTime_scenePath += "\n";
        dateTime_scenePath += m_skippedDrawingText;
      }
    }
    if (m_dataRects.contains(Data_DateTimeAndScenePath) &&
        !dateTime_scenePath.isEmpty()) {
      painter.save();
      QFont font(QString::fromLocal8Bit("BIZ UDPゴシック"));
      font.setPixelSize(m_p.bodylabelTextSize_Small);
      painter.setFont(font);
      painter.drawText(m_dataRects.value(Data_DateTimeAndScenePath),
                       Qt::AlignRight | Qt::AlignTop, dateTime_scenePath);
      painter.restore();
    }

    // OL長を書き込む
    if (m_dataRects.contains(Data_OverlapLength) &&
        (m_info.startOverlapFrameLength > 0 ||
         m_info.endOverlapFrameLength > 0)) {
      painter.save();
      QFont font(QString::fromLocal8Bit("BIZ UDPゴシック"));
      font.setPixelSize(m_dataRects.value(Data_OverlapLength).height() -
                        mm2px(0.5));
      painter.setFont(font);

      QString str;
      if (m_info.startOverlapFrameLength > 0) {
        int sec, koma;
        frame2SecKoma(m_info.startOverlapFrameLength, sec, koma);
        str += QObject::tr("(Start OL %1+%2) ").arg(sec).arg(koma);
      }
      if (m_info.endOverlapFrameLength > 0) {
        int sec, koma;
        frame2SecKoma(m_info.endOverlapFrameLength, sec, koma);
        str += QObject::tr(" (End OL %1+%2)").arg(sec).arg(koma);
      }
      painter.drawText(m_dataRects.value(Data_OverlapLength), Qt::AlignCenter,
                       str);
      painter.restore();
    }
  }
  painter.restore();

  // OL尺が奇数フレームの場合、前カットの尺を1コマ増やすことにする
  int sceneLength = m_duration - m_info.startOverlapFrameLength / 2 -
                    (m_info.endOverlapFrameLength + 1) / 2;

  if (m_dataRects.contains(Data_Second) && sceneLength >= 24) {
    QString str = QString::number(sceneLength / 24);
    setFontFittingRectWidth(painter, str, m_dataRects.value(Data_Second));
    painter.drawText(m_dataRects.value(Data_Second), Qt::AlignCenter, str);
  }
  if (m_dataRects.contains(Data_Frame) && sceneLength > 0) {
    QString str = QString::number(sceneLength % 24);
    setFontFittingRectWidth(painter, str, m_dataRects.value(Data_Frame));
    painter.drawText(m_dataRects.value(Data_Frame), Qt::AlignCenter, str);
  }

  if (m_dataRects.contains(Data_TotalPages)) {
    QString totStr = QString::number(framePageCount());
    if (parallelPageCount() > 1)
      totStr += "x" + QString::number(parallelPageCount());

    setFontFittingRectWidth(painter, totStr,
                            m_dataRects.value(Data_TotalPages));
    painter.drawText(m_dataRects.value(Data_TotalPages), Qt::AlignCenter,
                     totStr);
  }
  if (m_dataRects.contains(Data_CurrentPage)) {
    QString curStr = QString::number(framePage + 1);
    if (parallelPageCount() > 1) curStr += QChar('A' + parallelPage);

    setFontFittingRectWidth(painter, curStr,
                            m_dataRects.value(Data_CurrentPage));
    painter.drawText(m_dataRects.value(Data_CurrentPage),
                     Qt::AlignLeft | Qt::AlignVCenter, curStr);
  }
  if (m_dataRects.contains(Data_SceneName) && !m_info.sceneNameText.isEmpty()) {
    QRect rect = m_dataRects.value(Data_SceneName);
    setFontFittingRectWidth(painter, m_info.sceneNameText, rect);
    painter.drawText(rect, Qt::AlignCenter, m_info.sceneNameText);
  }
}

void XSheetPDFTemplate::initializePage(QPdfWriter& writer) {
  QPageLayout pageLayout;
  pageLayout.setUnits(QPageLayout::Millimeter);
  pageLayout.setPageSize(
      QPageSize(m_p.documentPageSize));  // 普通のB4はISO B4(250x353mm)
                                         // 日本のB4はJIS B4(257x364mm)
  pageLayout.setOrientation(QPageLayout::Portrait);
  pageLayout.setMargins(m_p.documentMargin);
  writer.setPageLayout(pageLayout);
  writer.setResolution(MyParams::PDF_Resolution);
}

QPixmap XSheetPDFTemplate::initializePreview() {
  QSize pxSize = getPixelSize();
  if (pxSize.isEmpty()) return QPixmap();
  QPixmap retPm(pxSize);
  retPm.fill(Qt::white);

  return retPm;
}

QSize XSheetPDFTemplate::getPixelSize() {
  QSizeF size = QPageSize::definitionSize(m_p.documentPageSize);
  QSize pxSize;
  // convert to px
  switch (QPageSize::definitionUnits(m_p.documentPageSize)) {
  case QPageSize::Millimeter:
    pxSize = QSize(mm2px(size.width()), mm2px(size.height()));
    break;
  case QPageSize::Inch:
    pxSize = QSize(size.width() * MyParams::PDF_Resolution,
                   size.height() * MyParams::PDF_Resolution);
    break;
  default:
    std::cout << "unsupported unit" << std::endl;
    return QSize();
  }

  pxSize += QSize(getStretchedColumnWidth(), 0);

  return pxSize;
}

int XSheetPDFTemplate::getStretchedColumnWidth() {
  if (!m_info.expandColumns) return 0;
  return (param(BodyWidth) - m_params.value(BodyWidth)) * param(BodyAmount);
}

int XSheetPDFTemplate::framePageCount() {
  int ret = m_longestDuration / param(FrameLength, 1);
  if (m_longestDuration % param(FrameLength, 1) != 0 || m_longestDuration == 0)
    ret += 1;
  return ret;
}

int XSheetPDFTemplate::parallelPageCount() {
  // 表示可能列が小さい方に合わせる
  int colsInPage = 1000;
  // 列数が多い方
  int colsInScene = 0;
  for (auto area : m_columns.keys()) {
    colsInPage  = std::min(colsInPage, columnsInPage(area));
    colsInScene = std::max(colsInScene, m_columns[area].size());
  }

  int ret = colsInScene / colsInPage;
  if (colsInScene % colsInPage != 0 || colsInScene == 0) ret += 1;
  return ret;
}

int XSheetPDFTemplate::columnsInPage(ExportArea area) {
  if (area == Area_Unspecified) {
    assert(!m_info.exportAreas.isEmpty());
    area = m_info.exportAreas[0];
  }

  if (area == Area_Actions) {
    int colsInPage = param(KeyColAmount);
    if (param(LastKeyColWidth) > 0) colsInPage++;
    return colsInPage;
  }

  // CELLS
  if (m_useExtraColumns)
    return param(CellsColAmount) + param(ExtraCellsColAmount, 0);
  return param(CellsColAmount);
}

QSize XSheetPDFTemplate::logoPixelSize() {
  if (!m_dataRects.contains(Data_Logo)) return QSize();
  return m_dataRects.value(Data_Logo).size();
}

void XSheetPDFTemplate::setLogoPixmap(QPixmap pm) { m_info.logoPixmap = pm; }

// テレコ列の有無の確認
// ① 同じセル名の列が2つ以上あり、かつ
// ② それらの列で全てのフレームについて重複がない
void XSheetPDFTemplate::checkTerekoColumns() {
  // 各表示エリアについて
  for (auto area : m_columns.keys()) {
    ColumnsData data = m_columns[area];
    QMap<QString, QList<int>> levelNameColIds;
    bool hasDupName = false;
    for (int cId = 0; cId < data.size(); cId++) {
      QString levelName = data[cId].name;
      if (levelNameColIds.contains(levelName)) {
        levelNameColIds[levelName].append(cId);
        hasDupName = true;
      } else
        levelNameColIds.insert(levelName, {cId});
    }

    if (!hasDupName) continue;

    QList<TerekoInfo> occupiedColumnsList;
    // テレコ列の分析
    // 最初に素材が入っているフレームが主、それ以外は従となる
    for (auto levelName : levelNameColIds.keys()) {
      QList<int> cIds = levelNameColIds.value(levelName);
      // 複数列にまたがっているものだけ
      if (cIds.size() <= 1) continue;

      // とりあえず各列のコマの内容をダンプする（Holdコマを展開する）
      QMap<int, QList<QString>> dumpedCells;
      for (auto cId : cIds) {
        QList<QString> dumpCells;
        for (auto cell : data[cId].cells) {
          if (cell.frame.isEmpty())
            if (dumpCells.isEmpty())  // 保険
              dumpCells.append(XdtsFrameDataItem::SYMBOL_NULL_CELL);
            else
              dumpCells.append(dumpCells.last());
          else
            dumpCells.append(cell.frame);
        }
        dumpedCells.insert(cId, dumpCells);
      }

      bool isTereko = true;
      QSet<int> colIdSet;
      QList<int> occupiedColumns;
      int firstOccupiedColumn = -1;
      // 毎フレーム、０または１つの列にだけコマが入っているかどうかを調べる
      for (int f = 0; f < m_longestDuration; f++) {
        int occupiedColumn = -1;
        for (auto cId : cIds) {
          QString cell;
          if (dumpedCells[cId].size() > f)  // 列に何かがある場合
            cell = dumpedCells[cId][f];
          else  // この列のデータ範囲をはみ出したフレームの場合、空コマを入れておく
            cell = XdtsFrameDataItem::SYMBOL_NULL_CELL;

          // この列は中身がある
          if (cell != XdtsFrameDataItem::SYMBOL_NULL_CELL) {
            // すでに他の列に中身がある場合、テレコ成立せず！
            if (occupiedColumn != -1) {
              isTereko = false;
              break;
            }
            occupiedColumn = cId;
            // 最初に中身の入っている列
            if (firstOccupiedColumn == -1) firstOccupiedColumn = cId;
          }
          if (!isTereko) break;
        }
        if (!isTereko) break;
        occupiedColumns.append(occupiedColumn);
        if (occupiedColumn != -1) colIdSet.insert(occupiedColumn);
      }

      if (!isTereko) continue;

      // ここで、occupiedColumnsが-1のフレームを全て埋める。
      // すなわち、全ての列で空コマの場合にどこかの列に所属させる。
      int currentCol = firstOccupiedColumn;
      for (int i = 0; i < occupiedColumns.size(); i++) {
        if (occupiedColumns[i] == -1)
          occupiedColumns.replace(i, currentCol);
        else
          currentCol = occupiedColumns[i];
      }

      std::cout << "level " << levelName.toStdString() << " is TEREKO!"
                << std::endl;
      occupiedColumnsList.append({levelName, colIdSet, occupiedColumns});
    }

    if (!occupiedColumnsList.isEmpty())
      m_terekoColumns.insert(area, occupiedColumnsList);
  }
}

//---------------------------------------------------------
namespace {

// rename duplicated columns name in the tempolary column list
void renameDuplicatedColumnName(QStringList& list) {
  int count = 1;
  for (int i = 1; i < list.size(); i++) {
    QString name = list[i];
    for (int j = 0; j < i - 1; j++) {
      if (name == list[j]) {
        for (int c = 0; c < count; c++) name += "_dup";
        list.replace(i, name);
        count++;
      }
    }
  }
}

void modifyTerekoKeyFrames(const ExportArea area, QStringList currentColNames,
                           QStringList savedColNames = QStringList()) {
  QMap<int, QList<int>> keyframes =
      MyParams::instance()->mixUpColumnsKeyframes(area);

  if (savedColNames.isEmpty()) {
    int savedSize = keyframes.begin().value().size();
    for (int i = 0; i < savedSize; i++) {
      if (i < currentColNames.size())
        savedColNames.append(currentColNames[i]);
      else
        savedColNames.append(QString());
    }
  }
  renameDuplicatedColumnName(currentColNames);
  renameDuplicatedColumnName(savedColNames);

  // columnIndexの変換対応表を作る
  QList<QList<int>> convertTable;
  for (auto savedColName : savedColNames) {
    int indexInCurrent = currentColNames.indexOf(savedColName);
    if (indexInCurrent == -1) {
      convertTable.push_back({-1});
    } else {
      QList<int> indexList = {indexInCurrent};
      for (int k = indexInCurrent + 1; k < currentColNames.size(); k++) {
        QString tmpColName = currentColNames.at(k);
        if (!savedColNames.contains(tmpColName))
          indexList.append(k);
        else
          break;
      }
      convertTable.append(indexList);
    }
  }

  // 各キーフレームを編集する
  for (int frame : keyframes.keys()) {
    QList<int> oldKeyFrame = keyframes[frame];
    QList<int> newKeyFrame;

    for (auto oldColIndex : oldKeyFrame) {
      if (convertTable[oldColIndex][0] == -1) continue;
      newKeyFrame.append(convertTable[oldColIndex]);
    }

    MyParams::instance()->setMixupColumnsKeyframe(area, frame, newKeyFrame);
  }
}

}  // namespace

void XSheetPDFTemplate::checkMixupColumnsKeyframes() {
  // 列名の一覧を取得
  QMap<ExportArea, QStringList> colNamesMap;
  for (auto area : m_columns.keys()) {
    ColumnsData columnsData = m_columns.value(area);
    QStringList colNames;
    for (auto columnData : columnsData) {
      colNames.append(columnData.name);
    }
    colNamesMap.insert(area, colNames);
  }

  // 現在のキーフレームとm_columnsに齟齬があるかをチェックする
  // m_columnsが複数で、内容が違う場合、キーフレームをシェアしていたらシェアを解除する
  if (m_columns.size() == 2 &&
      colNamesMap.values()[0] != colNamesMap.values()[1] &&
      MyParams::instance()->isMixUpColumnsKeyframesShared()) {
    MyParams::instance()->unifyOrSeparateMixupColumnsKeyframes(false);
  }

  bool somethingChanged = false;

  // 各エリアの列数とキーフレームの列数が一致しているか確認する
  for (auto area : colNamesMap.keys()) {
    QStringList currentColNames = colNamesMap[area];

    ExportArea _area = (MyParams::instance()->isMixUpColumnsKeyframesShared())
                           ? ExportArea::Area_Unspecified
                           : area;

    QMap<int, QList<int>> keyframes =
        MyParams::instance()->mixUpColumnsKeyframes(_area);
    if (keyframes.isEmpty()) continue;

    // カラム名一覧が保持されているか？
    QStringList savedColNames = MyParams::instance()->mixUpColumnsNames(_area);
    // 保持されている場合
    if (!savedColNames.isEmpty()) {
      // 内容が現状に一致しているか？
      // 一致している場合
      if (savedColNames == currentColNames) {
        // そのままでOK
        continue;
      }
      // 一致していない場合
      // 組み換えテーブルを作ってキーフレームを編集する
      modifyTerekoKeyFrames(_area, currentColNames, savedColNames);
      // 現状のカラム名をキープする
      MyParams::instance()->setMixUpColumnsNames(_area, currentColNames);
      // ダイアログも出す
      somethingChanged = true;
    }
    // 保持されていない場合
    else {
      // 列数が同じか？
      if (keyframes.begin().value().size() == currentColNames.size()) {
        //// 現状のカラム名をキープする。キーフレームはそのままでOK
        MyParams::instance()->setMixUpColumnsNames(_area, currentColNames);
        continue;
      }
      // 一致していない場合
      // 頭からカラム名を入れた組み換えテーブルを作ってキーフレームを編集する
      modifyTerekoKeyFrames(_area, currentColNames);
      // 現状のカラム名をキープする
      MyParams::instance()->setMixUpColumnsNames(_area, currentColNames);
      // ダイアログも出す
      somethingChanged = true;
    }

    if (MyParams::instance()->isMixUpColumnsKeyframesShared()) break;
  }

  // 違ったら警告を出してキーフレームをクリアする
  // 保存されているテレコ列キーフレームが現在表示されている列と一致しないため、テレコキーフレームが自動的に調整されました。
  // 調整が正しく行われたかどうか、確認が必要かもしれません。
  if (somethingChanged) {
    MyParams::instance()->setFormatDirty(true);
    QMessageBox::warning(
        nullptr, QObject::tr("Warning"),
        QObject::tr(
            "The saved mix-up columns keyframes does not match the currently "
            "displayed columns, "
            "so the mix-up columns keyframes were automatically adjusted.\n"
            "You may need to verify whether the adjustment was performed "
            "correctly."));
  }
}

//---------------------------------------------------------

void XSheetPDFTemplate::checkRepeatColumns() {
  m_repeatColumns.clear();

  // リピート判定される最小の繰り返し長さ
  int minimumRepeatLength = MyParams::instance()->minimumRepeatLength();

  // 各表示エリアについて
  for (auto area : m_columns.keys()) {
    // 動画欄ではリピート表記をしないのでスキップ
    if (area == Area_Cells) continue;

    QMap<int, QList<RepeatInfo>> repeatColumnsInfo;
    ColumnsData data = m_columns[area];
    for (int cId = 0; cId < data.size(); cId++) {
      QList<RepeatInfo> repeatInfos;

      // とりあえずコマの内容をダンプする（Holdコマを展開する）
      QList<QString> dumpCells;
      for (auto cell : data[cId].cells) {
        if (cell.frame.isEmpty()) {
          if (dumpCells.isEmpty())  // 保険
            dumpCells.append(XdtsFrameDataItem::SYMBOL_NULL_CELL);
          else
            dumpCells.append(dumpCells.last());
        } else
          dumpCells.append(cell.frame);
      }

      //--- 止メ列のチェック ---
      bool isAllHoldFrame = true;
      QString firstCell   = dumpCells.first();
      for (auto cell : dumpCells) {
        if (cell != firstCell) {
          isAllHoldFrame = false;
          break;
        }
      }
      if (isAllHoldFrame) {
        if (firstCell !=
            XdtsFrameDataItem::
                SYMBOL_NULL_CELL) {  // 「×止メ」という書き方はしない
          repeatInfos.append(
              {0, 1,
               dumpCells.size()});  // repeatChunkLengthが1なのは止メの場合のみ
          repeatColumnsInfo.insert(cId, repeatInfos);
        }
        continue;  // 次のcIdへ
      }
      //--- 止メ列のチェックここまで ---

      // ひとつ前のリピート明けのフレームを保持しておく
      int previousRepeatEndFrame = 0;

      // チャンクの開始フレーム fn 毎に
      for (int fn = 0; fn < dumpCells.count() - 4; fn++) {
        // チャンクの開始フレームは、Holdコマではない
        if (fn != 0 && data[cId].cells[fn].frame.isEmpty()) continue;
        // ひとつ前のリピート明けのフレーム～fn-1番目のコマにfnと同じ動画番号があったら、
        // 「最初の番号が繰り返し対象外のタイミングにある場合リピート表記しない」の
        // 条件を満たすので、continueしてfnを進める
        QString fn_cell    = dumpCells[fn];
        bool foundSameCell = false;
        for (int tmpf = previousRepeatEndFrame; tmpf < fn; tmpf++) {
          if (dumpCells[tmpf] == fn_cell) {
            foundSameCell = true;
            break;
          }
        }
        if (foundSameCell) continue;

        // チャンクの終了フレーム fm
        //  チャンク内が全て同じ動画番号ではダメなので、fmの探索開始位置は、fnの次のHoldでないコマ
        int fmStart;
        bool foundFmStart = false;
        for (fmStart = fn + 1; fmStart < dumpCells.count() - 3; fmStart++) {
          if (data[cId].cells[fmStart].frame.isEmpty()) continue;
          foundFmStart = true;
          break;
        }
        if (!foundFmStart) continue;

        // チャンクの終了フレーム fm 毎に
        for (int fm = fmStart; fm < dumpCells.count() - 3; fm++) {
          // fnとfmが同じ動画番号であったとき、ここから先fmを進めた場合のチャンク候補は全て
          // 「最初の番号が繰り返しの中に2回以上登場する場合リピート表記しない」の
          // 条件を満たすので、breakしてfnを進める
          if (dumpCells[fm] == fn_cell) break;

          bool repeatFound = false;
          int chunkLength  = fm - fn + 1;

          // このチャンクが繰り返しになっているかをチェックする
          // リピート明けのコマはなにか番号が打たれている必要がある（中割記号でもダメ）ので、
          // 繰り返しを満たし最後に番号が打たれているフレームを記憶する
          int fl_lastNumbered = fm + 1;
          // チャンク以降のフレーム
          for (int fl = fm + 1; fl < dumpCells.count(); fl++) {
            // チャンク内の対応するフレーム位置 fk
            int fk = (fl - fn) % chunkLength + fn;
            // fkとflの動画番号が一致、つまり繰り返している場合、
            if (dumpCells[fk] == dumpCells[fl]) {
              // flが最後のフレームでない場合、continueしてflを進める
              if (fl < dumpCells.count() - 1) {
                // 番号が打たれている場合、fl_lastNumberedを更新
                if (isNotEmptyOrSymbol(data[cId].cells[fl].frame))
                  fl_lastNumbered = fl;
                continue;
              }
              // 最後のフレームに達していたら、リピート終わりのフレームは
              // 最後のフレーム+1にする
              else
                fl_lastNumbered = fl + 1;
            }
            // 一致していない場合、なにか番号が打たれているかを確認
            else {
              if (isNotEmptyOrSymbol(data[cId].cells[fl].frame))
                fl_lastNumbered = fl;
            }
            // 繰り返しを満たし最後に番号が打たれているフレーム fl_lastNumbered
            // を リピート明けのフレームとし、
            // 繰り返し長が十分に長いかどうかをチェックする
            int repeatLength = fl_lastNumbered - fn;
            // チャンクが2回以上繰り返されていて、かつ閾値フレームよりも長い必要
            if (repeatLength >= chunkLength * 2 &&
                repeatLength >= minimumRepeatLength + chunkLength) {
              // リピート発見！！情報を格納する。
              repeatInfos.append({fn, chunkLength, fl_lastNumbered});
              // チャンク開始位置をflまで進める。breakして次の fnへ
              repeatFound            = true;
              fn                     = fl_lastNumbered - 1;
              previousRepeatEndFrame = fl_lastNumbered;
              break;
            }
            // リピートならず。break して次のfmに進む
            else
              break;

          }  // リピート終了フレーム fl

          if (repeatFound) break;

        }  // チャンクの終了フレーム fm

      }  // チャンクの開始フレーム fn

      if (!repeatInfos.isEmpty()) repeatColumnsInfo.insert(cId, repeatInfos);
    }
    m_repeatColumns.insert(area, repeatColumnsInfo);
  }
}
//---------------------------------------------------------

void XSheetPDFTemplate::checkSkippedDrawings() {
  m_skippedDrawingText.clear();
  for (auto area : m_columns.keys()) {
    // 原画欄はスキップ
    if (area == Area_Actions) continue;

    ColumnsData columnsData = m_columns.value(area);
    // セル名毎に、数字のフレームだけを集める
    QMap<QString, QSet<int>> frameLists;
    for (auto columnData : columnsData) {
      QSet<int> frameNumbers;

      for (auto frameData : columnData.cells) {
        bool ok;
        int frameNumber = frameData.frame.toInt(&ok);
        if (ok) frameNumbers.insert(frameNumber);
      }

      if (frameNumbers.isEmpty()) continue;

      // 新しいセル名の場合はリストを新たに登録
      if (!frameLists.contains(columnData.name))
        frameLists.insert(columnData.name, frameNumbers);
      else
        frameLists[columnData.name].unite(frameNumbers);
    }

    // 各セル名について
    for (auto colName : frameLists.keys()) {
      // 番号順にソート
      QList<int> numbers = frameLists.value(colName).toList();
      std::sort(numbers.begin(), numbers.end());
      // 数字が1個以下ならcontinue
      if (numbers.isEmpty()) continue;

      QStringList skippedFrameChunks;
      // 欠番を見つけてテキスト化。欠番の連番チャンクが3コマ以上のときは"～"でつなぐ
      int prevNumber = 0;
      for (int i = 0; i < numbers.size(); i++) {
        int currentNumber = numbers[i];
        // currentとprevの差が2以上の時
        if (currentNumber - prevNumber > 1) {
          if (currentNumber - prevNumber == 2)
            skippedFrameChunks.append(QString::number(prevNumber + 1));
          else if (currentNumber - prevNumber == 3) {
            skippedFrameChunks.append(QString::number(prevNumber + 1));
            skippedFrameChunks.append(QString::number(prevNumber + 2));
          } else
            skippedFrameChunks.append(
                QObject::tr("%1-%2")
                    .arg(QString::number(prevNumber + 1))
                    .arg(QString::number(currentNumber - 1)));
        }
        prevNumber = currentNumber;
      }

      if (!skippedFrameChunks.isEmpty())
        m_skippedDrawingText.append(
            QString(" %1%2 ").arg(colName).arg(skippedFrameChunks.join(",")));
    }

    // 何か欠番が見つかったら、タイトルを入れる
    if (!m_skippedDrawingText.isEmpty())
      m_skippedDrawingText.prepend(QObject::tr("Skipped Frames:"));
  }
}

//---------------------------------------------------------

XSheetPDFTemplate_B4_6sec::XSheetPDFTemplate_B4_6sec(
    const QMap<ExportArea, ColumnsData>& columns, const int duration)
    : XSheetPDFTemplate(columns, duration) {
  m_p.documentPageSize = QPageSize::JisB4;               // 257 * 364 mm
  m_p.documentMargin   = QMarginsF(9.5, 6.0, 8.5, 6.0);  // millimeters

  m_params.insert("BodyWidth", mm2px(117));
  m_params.insert("BodyHeight", mm2px(315.5));
  m_params.insert("BodyAmount", 2);
  m_params.insert("BodyHMargin", mm2px(5.0));
  m_params.insert("BodyTop", mm2px(36.5));
  m_params.insert("HeaderHeight", mm2px(6.5));
  m_params.insert("KeyColWidth", mm2px(5.0));
  m_params.insert("KeyColAmount", 4);
  m_params.insert("LastKeyColWidth", mm2px(7));
  m_params.insert("DialogColWidth", mm2px(10));
  m_params.insert("CellsColWidth", mm2px(10));
  m_params.insert("CellsColAmount", 5);
  m_params.insert("CameraColWidth", mm2px(10));
  m_params.insert("CameraColAmount", 3);
  m_params.insert("RowHeight", mm2px(102.0 / 24.0));
  m_params.insert("1SecHeight", mm2px(103.0));
  m_params.insert("FrameLength", 144);
  m_params.insert("InfoOriginLeft", mm2px(84));
  m_params.insert("InfoOriginTop", mm2px(0));
  m_params.insert("InfoTitleHeight", mm2px(4.5));
  m_params.insert("InfoBodyHeight", mm2px(8.5));
  m_params.insert("MemoLinesAmount", 3);
  m_params.insert("ExtraCellsColAmount", 3);

  addInfo(mm2px(26), QObject::tr("EPISODE", "XSheetPDF"));
  addInfo(mm2px(18.5), QObject::tr("SEQ.", "XSheetPDF"));
  addInfo(mm2px(18.5), QObject::tr("SCENE", "XSheetPDF"), decoSceneInfo);
  addInfo(mm2px(40), QObject::tr("TIME", "XSheetPDF"), decoTimeInfo);
  addInfo(mm2px(26), QObject::tr("NAME", "XSheetPDF"));
  addInfo(mm2px(26), QObject::tr("SHEET", "XSheetPDF"), decoSheetInfo);

  // data rects
  int infoBottom =
      param(InfoOriginTop) + param(InfoTitleHeight) + param(InfoBodyHeight);
  m_dataRects[Data_Memo] = QRect(mm2px(0), infoBottom + mm2px(1), mm2px(239),
                                 param(BodyTop) - infoBottom - mm2px(3));
  m_dataRects[Data_DateTimeAndScenePath] = m_dataRects[Data_Memo];
  m_dataRects[Data_Logo] =
      QRect(mm2px(0), mm2px(0), param(InfoOriginLeft), infoBottom);
}

//---------------------------------------------------------
// read from the settings file
XSheetPDFTemplate_Custom::XSheetPDFTemplate_Custom(
    const QString& fp, const QMap<ExportArea, ColumnsData>& columns,
    const int duration)
    : XSheetPDFTemplate(columns, duration), m_valid(false) {
  QSettings s(fp, QSettings::IniFormat);
  if (!s.childGroups().contains("XSheetPDFTemplate")) return;

  s.beginGroup("XSheetPDFTemplate");
  {
    QString pageStr      = s.value("PageSize").toString();
    m_p.documentPageSize = str2PageSizeId(pageStr);

    QString marginStr = s.value("Margin").toString();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList m = marginStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
#else
    QStringList m = marginStr.split(QLatin1Char(','), QString::SkipEmptyParts);
#endif
    assert(m.size() == 4);
    if (m.size() == 4)
      m_p.documentMargin = QMarginsF(m[0].toDouble(), m[1].toDouble(),
                                     m[2].toDouble(), m[3].toDouble());

    s.beginGroup("Number");
    {
      for (auto key : s.childKeys())
        m_params.insert(key.toStdString(), s.value(key).toInt());
    }
    s.endGroup();

    s.beginGroup("Length");
    {
      for (auto key : s.childKeys())
        m_params.insert(key.toStdString(), mm2px(s.value(key).toDouble()));
    }
    s.endGroup();

    bool translateInfo = param(TranslateInfoLabel, 1) == 1;
    int size           = s.beginReadArray("InfoFormats");
    for (int i = 0; i < size; ++i) {
      s.setArrayIndex(i);
      XSheetPDF_InfoFormat infoFormat;
      infoFormat.width = mm2px(s.value("width").toDouble());
      if (translateInfo)
        infoFormat.label =
            QObject::tr(s.value("label").toString().toLocal8Bit(), "XSheetPDF");
      else
        infoFormat.label = s.value("label").toString();

      infoFormat.decoFunc =
          infoType2DecoFunc(s.value("infoType", "").toString());
      m_p.array_Infos.append(infoFormat);
    }
    s.endArray();

    s.beginGroup("DataFields");
    {
      for (auto key : s.childKeys()) {
        QString rectStr = s.value(key).toString();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList r = rectStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
#else
        QStringList r =
            rectStr.split(QLatin1Char(','), QString::SkipEmptyParts);
#endif
        assert(r.size() == 4);
        if (r.size() == 4)
          m_dataRects[dataStr2Type(key)] =
              QRect(mm2px(r[0].toDouble()), mm2px(r[1].toDouble()),
                    mm2px(r[2].toDouble()), mm2px(r[3].toDouble()));
      }
    }
    s.endGroup();
  }
  s.endGroup();

  m_valid = true;
}

//-----------------------------------------------------------------------------
XsheetPdfPreviewPane::XsheetPdfPreviewPane(QWidget* parent)
    : QWidget(parent)
    , m_scaleFactor(0.2)
    , m_isTablet(false)
    , m_scribbling(false)
    , m_isCanvasValid(true)
    , m_clipRect(QRect()) {
  MyParams::instance()->setToolView(this);
  setMouseTracking(true);
  setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void XsheetPdfPreviewPane::paintEvent(QPaintEvent* event) {
  computeScaledPixmap();
  computeScaledScribblePixmap();

  QPainter painter(this);
#ifndef __MACOS__
  if (!m_clipRect.isEmpty()) {
    painter.setClipping(true);
    painter.setClipRect(m_clipRect);

    painter.drawPixmap(m_clipRect, m_scaledPixmap, m_clipRect);

    painter.setCompositionMode(QPainter::CompositionMode_Multiply);
    painter.drawPixmap(m_clipRect, m_scaledScribblePixmap, m_clipRect);

    releaseClipRect();
  } else
#endif
  {
    painter.fillRect(rect(), Qt::white);
    painter.drawPixmap(0, 0, m_scaledPixmap);

    painter.setCompositionMode(QPainter::CompositionMode_Multiply);
    painter.drawPixmap(0, 0, m_scaledScribblePixmap);
  }

  if (!m_canvasImage.isNull()) {
    if (MyParams::instance()->currentToolId() == Tool_Eraser)
      painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(
        m_canvasPos * m_scaleFactor,
        m_canvasImage.scaled(m_canvasImage.size() * m_scaleFactor,
                             Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }

  MyParams::instance()->currentTool()->draw(painter, m_mousePos, m_scaleFactor);
}

void XsheetPdfPreviewPane::computeScaledPixmap() {
  static double factor = -1.;
  if (m_scaleFactor == factor && !m_scaledPixmap.isNull()) return;

  QSize pmSize((double)m_pixmap.width() * m_scaleFactor,
               (double)m_pixmap.height() * m_scaleFactor);

  m_scaledPixmap =
      m_pixmap.scaled(pmSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  factor = m_scaleFactor;
}

void XsheetPdfPreviewPane::computeScaledScribblePixmap() {
  static double factor = -1.;
  if (m_scaleFactor == factor && !m_scaledScribblePixmap.isNull()) return;

  QSize imgSize((double)m_scribbleImage.width() * m_scaleFactor,
                (double)m_scribbleImage.height() * m_scaleFactor);

  m_scaledScribblePixmap = QPixmap::fromImage(m_scribbleImage.scaled(
      imgSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  factor                 = m_scaleFactor;
}

void XsheetPdfPreviewPane::mousePressEvent(QMouseEvent* e) {
  MyParams::instance()->setBusy(true);
  if (m_isTablet) return;
  m_mousePos = e->localPos();

  if (e->button() == Qt::LeftButton) {
    MyParams::instance()->currentTool()->onPress(m_mousePos, e->modifiers());
    m_scribbling = true;
  }
  update();
}

void XsheetPdfPreviewPane::mouseMoveEvent(QMouseEvent* e) {
  if ((e->buttons() & Qt::LeftButton) && m_scribbling) {
    if (!m_isTablet) {
      MyParams::instance()->currentTool()->onDrag(m_mousePos, e->localPos(),
                                                  e->modifiers());
      // m_pane->drawLine(m_mousePos, e->localPos());
      m_mousePos = e->localPos();
    }
  } else if (e->buttons() & Qt::MiddleButton) {
    QPointF d = m_mousePos - e->localPos();
    m_area->horizontalScrollBar()->setValue(
        m_area->horizontalScrollBar()->value() + (int)d.x());
    m_area->verticalScrollBar()->setValue(m_area->verticalScrollBar()->value() +
                                          (int)d.y());
    m_mousePos = e->localPos() + d;
  } else if (e->buttons() == Qt::NoButton) {
    if (!m_isTablet) {
      m_mousePos = e->localPos();
    }
    MyParams::instance()->currentTool()->onMove(m_mousePos);
    // the canvas is invalidated when the slider moves
    if (!m_isCanvasValid && !m_scribbling) updateCanvas();
  }
  update();
  // update(QRectF(m_mousePos, e->localPos()).toRect().normalized());
}

void XsheetPdfPreviewPane::mouseReleaseEvent(QMouseEvent* e) {
  MyParams::instance()->setBusy(false);
  if (e->button() == Qt::MiddleButton) {
    // 描画キャンバスを更新
    updateCanvas();
  }
  if (m_isTablet) return;
  if ((e->button() == Qt::LeftButton) && m_scribbling) {
    // m_pane->drawLine(m_mousePos, e->localPos());
    MyParams::instance()->currentTool()->onRelease(e->localPos(), m_canvasImage,
                                                   m_canvasPos);

    m_scribbling = false;
  }
  update();
}

void XsheetPdfPreviewPane::tabletEvent(QTabletEvent* event) {
  static bool tabletJustTouched = false;
  switch (event->type()) {
  case QEvent::TabletPress: {
    if (event->button() == Qt::LeftButton) {
      // 早くタッチするとm__mousePosがひとつ前のタッチ位置になることがあるため、使わないことにする
      // m_mousePos = event->posF();
      // std::cout << "tablet left m_mousePos = " << m_mousePos.x() << ", " <<
      // m_mousePos.y() <<
      // std::endl;//ここの値がただしく更新されないようだ？？？
      MyParams::instance()->currentTool()->onPress(event->posF(),
                                                   event->modifiers());
      m_scribbling      = true;
      m_isTablet        = true;
      tabletJustTouched = true;
      event->accept();
    } else {
      event->ignore();
    }
  } break;
  case QEvent::TabletRelease: {
    if (event->button() == Qt::LeftButton) {
      MyParams::instance()->currentTool()->onRelease(
          event->posF(), m_canvasImage, m_canvasPos);
      m_scribbling = false;
      m_isTablet   = false;
      event->accept();
    } else
      event->ignore();
  } break;
  case QEvent::TabletMove: {
    if (tabletJustTouched) {
      m_mousePos        = event->posF();
      tabletJustTouched = false;
      return;
    }
    if ((event->buttons() & Qt::LeftButton) && m_scribbling) {
      MyParams::instance()->currentTool()->onDrag(
          m_mousePos, event->posF(), event->modifiers(), event->pressure());
      // m_pane->drawLine(m_mousePos, event->posF(), event->pressure());
      event->accept();
    }
    m_mousePos = event->posF();
  } break;
  default:
    event->ignore();
    break;
  }
}

void XsheetPdfPreviewPane::leaveEvent(QEvent* event) {
  setCursor(Qt::ArrowCursor);
}

void XsheetPdfPreviewPane::enterEvent(QEvent* event) {
  MyParams::instance()->currentTool()->onEnter();
}

void XsheetPdfPreviewPane::setPixmap(QPixmap pm) {
  m_pixmap       = pm;
  m_scaledPixmap = QPixmap();
  resize(pm.size() * m_scaleFactor);
  updateCanvas();
  update();
}

double XsheetPdfPreviewPane::doZoom(bool zoomIn) {
  double oldScaleFactor = m_scaleFactor;
  m_scaleFactor         = getQuantizedZoomFactor(m_scaleFactor, zoomIn);

  if (MyParams::instance()->isFitToWindow())
    MyParams::instance()->setIsFitToWindow(false);

  resize(m_pixmap.size() * m_scaleFactor);
  invalidateCanvas();
  update();
  return m_scaleFactor / oldScaleFactor;
}

void XsheetPdfPreviewPane::fitScaleTo(QSize size) {
  double tmp_scaleFactor =
      std::min((double)size.width() / (double)m_pixmap.width(),
               (double)size.height() / (double)m_pixmap.height());

  m_scaleFactor = tmp_scaleFactor;
  if (m_scaleFactor > 1.0)
    m_scaleFactor = 1.0;
  else if (m_scaleFactor < 0.1)
    m_scaleFactor = 0.1;

  resize(m_pixmap.size() * m_scaleFactor);
  updateCanvas();
  update();
}

void XsheetPdfPreviewPane::setScribbleMode(bool isErase) {
  // m_scribblePainter.setCompositionMode((isErase) ?
  // QPainter::CompositionMode_DestinationOut
  //   : QPainter::CompositionMode_SourceOver);
}

QRect XsheetPdfPreviewPane::drawLine(const QPointF& _from, const QPointF& _to,
                                     qreal pressure, bool isErase,
                                     double brushSize) {
  // QPoint fp = _from.toPoint();
  // QPointF fFrac = _from - QPointF(fp);
  // QPoint tp = _to.toPoint();
  // QPointF tFrac = _to - QPointF(tp);

  // double brushSize = (isErase) ? 60. : 4.;

  // 画像上の位置を得る
  // QPointF fromP = (QPointF(mapFromParent(fp)) + fFrac) / m_scaleFactor;
  // QPointF toP = (QPointF(mapFromParent(tp)) + tFrac) / m_scaleFactor;
  QPointF fromP = _from / m_scaleFactor;
  QPointF toP   = _to / m_scaleFactor;

  QColor col = (isErase) ? Qt::white : MyParams::instance()->currentColor();
  col.setAlpha((int)(255. * std::min(1., pressure + 0.5)));
  m_canvasPainter.setPen(QPen(col, brushSize * pressure, Qt::SolidLine,
                              Qt::RoundCap, Qt::RoundJoin));
  m_canvasPainter.drawLine(fromP - m_canvasPos, toP - m_canvasPos);

  QRect rect = QRectF(_from, _to).normalized().toAlignedRect();
  int margin = (int)std::ceil(brushSize * m_scaleFactor);
  rect += QMargins(margin, margin, margin, margin);
  setClipRect(rect);

  emit(imageEdited());

  // repaint();
  update();

  return QRectF(fromP, toP).normalized().toAlignedRect();
  // update??
  // int rad = (int)(2. / 2.) + 2;
  // parentWidget()->repaint(QRectF(_from, _to).normalized().toAlignedRect()
  //   .adjusted(-rad, -rad, +rad, +rad));
}

void XsheetPdfPreviewPane::updateCanvas() {
  if (!m_canvasImage.isNull()) m_canvasPainter.end();
  QScrollBar* hb = m_area->horizontalScrollBar();
  QScrollBar* vb = m_area->verticalScrollBar();

  m_canvasPos        = QPointF((double)hb->value() / m_scaleFactor,
                               (double)vb->value() / m_scaleFactor);
  QSize viewableSize = m_area->size() -
                       QSize(vb->sizeHint().width(), hb->sizeHint().height()) -
                       QSize(m_area->frameWidth(), m_area->frameWidth());
  viewableSize /= m_scaleFactor;

  m_canvasImage = QImage(viewableSize, QImage::Format_ARGB32_Premultiplied);
  m_canvasImage.fill(Qt::transparent);
  m_canvasPainter.begin(&m_canvasImage);
  m_canvasPainter.setRenderHints(QPainter::Antialiasing);

  m_isCanvasValid = true;
}

//-----------------------------------------------------------------------------

XsheetPdfPreviewArea::XsheetPdfPreviewArea(QWidget* parent)
    : QScrollArea(parent) {
  connect(horizontalScrollBar(), SIGNAL(actionTriggered(int)), this,
          SLOT(onSliderActionTriggered()));
  connect(verticalScrollBar(), SIGNAL(actionTriggered(int)), this,
          SLOT(onSliderActionTriggered()));
}

void XsheetPdfPreviewArea::onSliderActionTriggered() {
  dynamic_cast<XsheetPdfPreviewPane*>(widget())->invalidateCanvas();
}

void XsheetPdfPreviewArea::contextMenuEvent(QContextMenuEvent* event) {
  QMenu* menu = new QMenu(this);

  MyParams::instance()->currentTool()->addContextMenu(menu);

  QAction* fitAction = menu->addAction(tr("Fit To Window"));
  fitAction->setCheckable(true);
  fitAction->setChecked(MyParams::instance()->isFitToWindow());
  connect(fitAction, SIGNAL(triggered()), this, SLOT(fitToWindow()));
  menu->exec(event->globalPos());
}

void XsheetPdfPreviewArea::fitToWindow() {
  bool wasOn = MyParams::instance()->isFitToWindow();
  MyParams::instance()->setIsFitToWindow(!wasOn);
  if (!wasOn)
    dynamic_cast<XsheetPdfPreviewPane*>(widget())->fitScaleTo(rect().size());
}

void XsheetPdfPreviewArea::wheelEvent(QWheelEvent* event) {
  int delta = 0;
  switch (event->source()) {
  case Qt::MouseEventNotSynthesized: {
    delta = event->angleDelta().y();
  }
  case Qt::MouseEventSynthesizedBySystem: {
    QPoint numPixels  = event->pixelDelta();
    QPoint numDegrees = event->angleDelta() / 8;
    if (!numPixels.isNull()) {
      delta = event->pixelDelta().y();
    } else if (!numDegrees.isNull()) {
      QPoint numSteps = numDegrees / 15;
      delta           = numSteps.y();
    }
    break;
  }

  default:  // Qt::MouseEventSynthesizedByQt,
            // Qt::MouseEventSynthesizedByApplication
  {
    std::cout << "not supported event: Qt::MouseEventSynthesizedByQt, "
                 "Qt::MouseEventSynthesizedByApplication"
              << std::endl;
    break;
  }

  }  // end switch

  if (delta == 0) {
    event->accept();
    return;
  }

  QScrollBar* hScrollBar = horizontalScrollBar();
  QScrollBar* vScrollBar = verticalScrollBar();
  double xRatioPos       = event->position().x() / (double)width();
  double yRatioPos       = event->position().y() / (double)height();
  double xPivot =
      hScrollBar->value() + (double)hScrollBar->pageStep() * xRatioPos;
  double yPivot =
      vScrollBar->value() + (double)vScrollBar->pageStep() * yRatioPos;
  // std::cout << "X xRatioPos = " << xRatioPos << "  pivot =" << xPivot << "
  // pageStep = " << hScrollBar->pageStep() << std::endl;

  double d_scale =
      dynamic_cast<XsheetPdfPreviewPane*>(widget())->doZoom(delta > 0);
  // std::cout << "d_scale = "<< d_scale << std::endl;

  // ensureVisible(xPivot, yPivot);
  hScrollBar->setValue(xPivot * d_scale -
                       (double)hScrollBar->pageStep() * xRatioPos);
  vScrollBar->setValue(yPivot * d_scale -
                       (double)vScrollBar->pageStep() * yRatioPos);

  event->accept();
}

void XsheetPdfPreviewArea::keyPressEvent(QKeyEvent* event) {
  MyParams::instance()->currentTool()->onKeyPress(event);

  if (!event->isAccepted()) QScrollArea::keyPressEvent(event);
}

void XsheetPdfPreviewArea::resizeEvent(QResizeEvent* event) {
  if (MyParams::instance()->isFitToWindow())
    dynamic_cast<XsheetPdfPreviewPane*>(widget())->fitScaleTo(rect().size());

  QScrollArea::resizeEvent(event);
}

void XsheetPdfPreviewArea::zoomIn() {
  QScrollBar* hScrollBar = horizontalScrollBar();
  QScrollBar* vScrollBar = verticalScrollBar();
  double xPivot = hScrollBar->value() + (double)hScrollBar->pageStep() * 0.5;
  double yPivot = vScrollBar->value() + (double)vScrollBar->pageStep() * 0.5;

  double d_scale = dynamic_cast<XsheetPdfPreviewPane*>(widget())->doZoom(true);

  hScrollBar->setValue(xPivot * d_scale - (double)hScrollBar->pageStep() * 0.5);
  vScrollBar->setValue(yPivot * d_scale - (double)vScrollBar->pageStep() * 0.5);
}

void XsheetPdfPreviewArea::zoomOut() {
  QScrollBar* hScrollBar = horizontalScrollBar();
  QScrollBar* vScrollBar = verticalScrollBar();
  double xPivot = hScrollBar->value() + (double)hScrollBar->pageStep() * 0.5;
  double yPivot = vScrollBar->value() + (double)vScrollBar->pageStep() * 0.5;

  double d_scale = dynamic_cast<XsheetPdfPreviewPane*>(widget())->doZoom(false);

  hScrollBar->setValue(xPivot * d_scale - (double)hScrollBar->pageStep() * 0.5);
  vScrollBar->setValue(yPivot * d_scale - (double)vScrollBar->pageStep() * 0.5);
}