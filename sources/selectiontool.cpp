#include "tool.h"

#include "xsheetpreviewarea.h"

#include <QUndoCommand>
#include <QGuiApplication>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QMenu>
#include <QKeyEvent>
#include <QColorDialog>
#include <QSettings>

bool operator<(const QColor& a, const QColor& b) {
  return a.redF() < b.redF() || a.greenF() < b.greenF() ||
         a.blueF() < b.blueF() || a.alphaF() < b.alphaF();
}

namespace {

QImage removeMatte(QImage& image, QColor matteColor) {
  double mc[3] = {matteColor.blueF(), matteColor.greenF(), matteColor.redF()};

  double b_len = std::sqrt(mc[0] * mc[0] + mc[1] * mc[1] + mc[2] * mc[2]);

  double b[3] = {-matteColor.blueF() / b_len, -matteColor.greenF() / b_len,
                 -matteColor.redF() / b_len};

  QMap<QColor, QColor> resultMap;

  QImage retImage(image.size(), QImage::Format_ARGB32_Premultiplied);
  uchar* s_p = image.bits();
  uchar* d_p = retImage.bits();
  for (int i = 0; i < image.width() * image.height(); i++) {
    QColor sc((int)s_p[2], (int)s_p[1], (int)s_p[0], (int)s_p[3]);
    if (s_p[3] == 0 || sc == matteColor) {
      for (int c = 0; c < 4; c++, d_p++, s_p++) *d_p = 0;
      continue;
    }

    if (resultMap.contains(sc)) {
      QColor resultColor = resultMap.value(sc);
      d_p[0]             = (uchar)resultColor.blue();
      d_p[1]             = (uchar)resultColor.green();
      d_p[2]             = (uchar)resultColor.red();
      d_p[3]             = (uchar)resultColor.alpha();
      s_p += 4;
      d_p += 4;
      continue;
    }

    // unpremultiply
    double color[4];
    color[3] = (double)s_p[3] / 255.;
    for (int c = 0; c < 3; c++) color[c] = ((double)s_p[c] / 255.) / color[3];

    double a[3] = {color[0] - mc[0], color[1] - mc[1], color[2] - mc[2]};

    double alpha = (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) / b_len;
    if (alpha < 0.)
      alpha = 0.;
    else if (alpha > 1.)
      alpha = 1.;

    uchar alpha_c = (uchar)(alpha * 255.);

    // blue
    d_p[0] = 0;
    // green
    d_p[1] = 0;
    // red
    d_p[2] = 0;
    // alpha
    d_p[3] = alpha_c;

    QColor dc((int)d_p[2], (int)d_p[1], (int)d_p[0], (int)d_p[3]);
    resultMap.insert(sc, dc);

    s_p += 4;
    d_p += 4;
  }

  return retImage;
}
}  // namespace

class SelectionRectSelectUndo : public QUndoCommand {
  SelectionTool* m_tool;
  QRect m_rect;
  QImage m_selectedImg;

public:
  SelectionRectSelectUndo(SelectionTool* tool, QRect rect, QImage selectedImg)
      : m_tool(tool), m_rect(rect), m_selectedImg(selectedImg) {}
  void undo() {
    m_tool->pasteImage(m_rect, m_selectedImg, true);
    m_tool->setSelectedImage(QImage());
    m_tool->clearSelection();
  }
  void redo() {
    m_tool->pasteImage(m_rect, QImage(), true);
    m_tool->setSelectedImage(m_selectedImg);
    m_tool->select(m_rect, m_rect);
  }
};

class SelectionFreeHandSelectUndo : public QUndoCommand {
  SelectionTool* m_tool;
  QRect m_rect;
  QPainterPath m_path;
  QImage m_selectedImg;

public:
  SelectionFreeHandSelectUndo(SelectionTool* tool, QRect rect,
                              QPainterPath path, QImage selectedImg)
      : m_tool(tool), m_rect(rect), m_path(path), m_selectedImg(selectedImg) {
    QImage maskImg(rect.size(), QImage::Format_ARGB32_Premultiplied);
    maskImg.fill(Qt::black);
    QPainter mp(&maskImg);
    mp.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    mp.fillPath(m_path.translated(QPointF(-rect.topLeft())), QBrush(Qt::black));
    mp.end();

    QPainter p(&m_selectedImg);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    p.drawImage(QPoint(), maskImg);
    // p.fillPath(m_path.translated(QPointF(-rect.topLeft())),
    // QBrush(Qt::black));
    p.end();
  }
  void undo() {
    m_tool->pasteImage(m_rect, m_selectedImg, false);
    m_tool->setSelectedImage(QImage());
    m_tool->clearSelection();
  }
  void redo() {
    m_tool->clearImage(m_path);
    m_tool->setSelectedImage(m_selectedImg);
    m_tool->select(m_path);
  }
};
// 選択の解除のUndo
class SelectionReleaseUndo : public QUndoCommand {
  SelectionTool* m_tool;
  QRect m_selectedRect, m_currentRect;
  QImage m_selectedImg;
  QImage m_beforePasteImg;
  QPainterPath m_path;

public:
  SelectionReleaseUndo(SelectionTool* tool, QRect selectedRect,
                       QRect currentRect, QPainterPath path, QImage selectedImg,
                       QImage beforePasteImg, QUndoCommand* parent = nullptr)
      : QUndoCommand(parent)
      , m_tool(tool)
      , m_selectedRect(selectedRect)
      , m_currentRect(currentRect)
      , m_selectedImg(selectedImg)
      , m_beforePasteImg(beforePasteImg)
      , m_path(path) {}

  void undo() {
    if (!m_selectedImg.isNull()) {
      // 前の絵を貼り付け
      m_tool->pasteImage(m_currentRect, m_beforePasteImg, true,
                         m_currentRect.size() != m_selectedRect.size());
      // 選択画像を戻す
      m_tool->setSelectedImage(m_selectedImg);
    }
    if (m_path.isEmpty())
      // Rectを選択状態に戻す
      m_tool->select(m_selectedRect, m_currentRect);
    else
      m_tool->select(m_path);
  }
  void redo() {
    if (!m_selectedImg.isNull()) {
      // 選択範囲を貼り付け
      m_tool->pasteImage(m_currentRect, m_selectedImg, false,
                         m_currentRect.size() != m_selectedRect.size());
      // 選択画像をクリア
      m_tool->setSelectedImage(QImage());
    }
    // 選択解除
    m_tool->clearSelection();
  }
};

class SelectionDragToolUndo : public QUndoCommand {
  SelectionTool* m_tool;
  QRect m_selectedRect, m_currentRect;
  QPainterPath m_selectedPath, m_currentPath;

public:
  SelectionDragToolUndo(SelectionTool* tool, QRect selectedRect,
                        QRect currentRect, QPainterPath path)
      : m_tool(tool)
      , m_selectedRect(selectedRect)
      , m_currentRect(currentRect)
      , m_selectedPath(path) {
    if (!path.isEmpty()) {
      QTransform t;
      t.translate(m_currentRect.left(), m_currentRect.top());
      t.scale((qreal)m_currentRect.width() / (qreal)m_selectedRect.width(),
              (qreal)m_currentRect.height() / (qreal)m_selectedRect.height());
      t.translate(-m_selectedRect.left(), -m_selectedRect.top());
      m_currentPath = t.map(path);
    }
  }

  void undo() {
    if (m_selectedPath.isEmpty())
      m_tool->select(m_selectedRect, m_selectedRect);
    else
      m_tool->select(m_selectedPath);
  }
  void redo() {
    if (m_selectedPath.isEmpty())
      m_tool->select(m_currentRect, m_currentRect);
    else
      m_tool->select(m_currentPath);
  }
};

//---------------------------------------

class SelectionDragTranslate : public SelectionDragTool {
  QPoint m_pos;

public:
  SelectionDragTranslate(QRect rect) : SelectionDragTool(rect) {}
  void onClick(QPoint p) override { m_pos = p; }
  QRect onDrag(QPoint p, QRect r,
               const Qt::KeyboardModifiers modifiers) override {
    if (modifiers & Qt::ShiftModifier) {
      // 移動量の大きい方にスナップ
      QPoint dif = p - m_pos;
      if (std::abs(dif.x()) > std::abs(dif.y()))
        p.setY(m_pos.y());
      else
        p.setX(m_pos.x());
    }
    return r.translated(p - m_pos);
  }
  void onRelease() override {}
};

class SelectionDragScale : public SelectionDragTool {
  QPoint m_handleOffset, m_pivotPos;

public:
  SelectionDragScale(QRect rect, QPoint handlePos, QPoint pivotPos)
      : SelectionDragTool(rect)
      , m_pivotPos(pivotPos)
      , m_handleOffset(handlePos) {}
  void onClick(QPoint p) override { m_handleOffset = p - m_handleOffset; }
  QRect onDrag(QPoint p, QRect r,
               const Qt::KeyboardModifiers modifiers) override {
    return QRect(p - m_handleOffset, m_pivotPos).normalized();
  }
  void onRelease() override {}
};

//---------------------------------------

SelectionTool::SelectionTool()
    : Tool(Tool_Selection)
    , m_selectedRect()
    , m_currentRect()
    , m_dragTool(nullptr)
    , m_mouseOver(Over_None)
    , m_mode(Rect) {}

void SelectionTool::onPress(const QPointF& pos,
                            Qt::KeyboardModifiers modifiers) {
  m_imageBefore = m_view->scribbleImage().copy();
  QPoint p      = (pos / m_view->scaleFactor()).toPoint();
  if (m_selectedRect.isNull()) {
    if (m_mode == Rect)
      m_dragStartPos = p;
    else {  // freehand
      m_dragStartPos = p;
      m_freeHandPath.clear();
      m_freeHandPath.moveTo(p);
    }
    // std::cout << "start from " << p.x() << ", " << p.y() << std::endl;
  } else {
    assert(!m_currentRect.isNull());
    // クリック位置によって操作を変える

    // 選択範囲内をクリックしたとき、移動モード
    if (m_mouseOver == Over_Body) {
      m_dragTool = new SelectionDragTranslate(m_currentRect);
    } else if (m_mouseOver >= Over_TopLeft && m_mouseOver <= Over_BottomRight) {
      // 反対側の点
      QPoint handlePos, pivotPos;
      switch (m_mouseOver) {
      case Over_TopLeft:
        handlePos = m_currentRect.topLeft();
        pivotPos  = m_currentRect.bottomRight();
        break;
      case Over_TopRight:
        handlePos = m_currentRect.topRight();
        pivotPos  = m_currentRect.bottomLeft();
        break;
      case Over_BottomLeft:
        handlePos = m_currentRect.bottomLeft();
        pivotPos  = m_currentRect.topRight();
        break;
      case Over_BottomRight:
        handlePos = m_currentRect.bottomRight();
        pivotPos  = m_currentRect.topLeft();
        break;
      }
      m_dragTool = new SelectionDragScale(m_currentRect, handlePos, pivotPos);
    }

    if (m_dragTool) {
      m_dragTool->onClick(p);
    }
    // それ以外をクリックしたとき、画像の確定
    else {
      // 登録時redoが実行される
      MyParams::instance()->undoStack()->push(new SelectionReleaseUndo(
          this, m_selectedRect, m_currentRect, m_freeHandPath, m_selectedImage,
          m_view->scribbleImage().copy(m_currentRect)));

      m_dragStartPos = QPoint();
    }
  }
}

void SelectionTool::onDrag(const QPointF& from, const QPointF& to,
                           Qt::KeyboardModifiers modifiers, qreal pressure) {
  if (m_dragStartPos.isNull() && m_freeHandPath.isEmpty() && !m_dragTool)
    return;
  // QPoint p = m_view->mapFromParent(to.toPoint()) / m_view->scaleFactor();
  QPoint p = (to / m_view->scaleFactor()).toPoint();
  if (m_currentRect.isNull()) {
    if (m_mode == Rect)
      m_selectedRect = QRect(m_dragStartPos, p).normalized();
    else {  // freehand
      m_freeHandPath.lineTo(p);
      m_selectedRect = m_freeHandPath.boundingRect().toAlignedRect();
    }
    // std::cout << "move to "<<p.x()<<", " << p.y() << std::endl;
  } else if (m_dragTool) {
    m_currentRect = m_dragTool->onDrag(p, m_selectedRect, modifiers);
  }
}

void SelectionTool::onMove(const QPointF& pos) {
  if (m_currentRect.isNull()) {
    m_view->setCursor(Qt::ArrowCursor);
    return;
  }

  QMap<MouseOver, QPoint> handles = {
      {Over_TopLeft, m_currentRect.topLeft()},
      {Over_TopRight, m_currentRect.topRight()},
      {Over_BottomLeft, m_currentRect.bottomLeft()},
      {Over_BottomRight, m_currentRect.bottomRight()}};
  m_mouseOver = Over_None;
  for (auto hId : handles.keys()) {
    // 画面上の位置
    QPointF handlePos = QPointF(handles.value(hId)) * m_view->scaleFactor();
    if ((handlePos - pos).manhattanLength() <= 5.) {
      m_mouseOver = hId;
      break;
    }
  }
  if (m_mouseOver == Over_None) {
    QPoint p = (pos / m_view->scaleFactor()).toPoint();
    if (m_currentRect.contains(p)) m_mouseOver = Over_Body;
  }

  Qt::CursorShape cursor;
  switch (m_mouseOver) {
  case Over_None:
    cursor = Qt::ArrowCursor;
    break;
  case Over_Body:
    cursor = Qt::SizeAllCursor;
    break;
  case Over_TopLeft:
  case Over_BottomRight:
    cursor = Qt::SizeFDiagCursor;
    break;
  case Over_TopRight:
  case Over_BottomLeft:
    cursor = Qt::SizeBDiagCursor;
    break;
  default:
    cursor = Qt::ArrowCursor;
    break;
  }
  m_view->setCursor(cursor);
}

void SelectionTool::onRelease(const QPointF& pos, QImage&, const QPointF&) {
  if (m_dragStartPos.isNull() && m_freeHandPath.isEmpty() && !m_dragTool)
    return;
  if (m_currentRect.isNull()) {
    // 登録時redoが実行される
    if (m_mode == Rect) {
      MyParams::instance()->undoStack()->push(new SelectionRectSelectUndo(
          this, m_selectedRect, m_view->scribbleImage().copy(m_selectedRect)));
    } else {
      m_freeHandPath = m_freeHandPath.simplified();
      MyParams::instance()->undoStack()->push(new SelectionFreeHandSelectUndo(
          this, m_selectedRect, m_freeHandPath,
          m_view->scribbleImage().copy(m_selectedRect)));
    }
  } else if (m_dragTool) {
    m_dragTool->onRelease();
    delete m_dragTool;
    m_dragTool = nullptr;
    // 登録時redoが実行される
    MyParams::instance()->undoStack()->push(new SelectionDragToolUndo(
        this, m_selectedRect, m_currentRect, m_freeHandPath));
  }
  // std::cout << "release at " << p.x() << ", " << p.y() << std::endl;
}

void SelectionTool::draw(QPainter& painter, QPointF pos, double scaleFactor) {
  // std::cout << "draw" << std::endl;
  if (m_selectedRect.isNull() && m_currentRect.isNull()) return;
  painter.save();
  painter.setRenderHints(QPainter::Antialiasing |
                         QPainter::SmoothPixmapTransform);
  painter.scale(scaleFactor, scaleFactor);
  // rubber-banding
  if (m_currentRect.isNull()) {
    if (m_freeHandPath.isEmpty()) {
      painter.setPen(Qt::white);
      painter.drawRect(m_currentRect);
      painter.setPen(QPen(Qt::black, 1. / scaleFactor, Qt::DashLine));
      painter.drawRect(m_selectedRect);
    }
    // freehand
    else {
      painter.setPen(QPen(Qt::blue, 1. / scaleFactor));
      painter.drawPath(m_freeHandPath);
    }
  }
  // after selected
  else {
    // draw image
    painter.drawImage(m_currentRect, m_selectedImage);
    if (!m_freeHandPath.isEmpty()) {
      QTransform t;
      t.translate(m_currentRect.left(), m_currentRect.top());
      t.scale((qreal)m_currentRect.width() / (qreal)m_selectedRect.width(),
              (qreal)m_currentRect.height() / (qreal)m_selectedRect.height());
      t.translate(-m_selectedRect.left(), -m_selectedRect.top());
      QPainterPath currentPath = t.map(m_freeHandPath);
      painter.setPen(QPen(Qt::white, 1. / scaleFactor));
      painter.drawPath(currentPath);
      painter.setPen(QPen(Qt::blue, 1. / scaleFactor, Qt::DotLine));
      painter.drawPath(currentPath);
    }
    // draw rect
    painter.setPen(QPen(Qt::white, 1. / scaleFactor));
    painter.drawRect(m_currentRect);
    painter.setPen(QPen(Qt::black, 1. / scaleFactor, Qt::DashLine));
    painter.drawRect(m_currentRect);
    // draw handles
    QList<QPoint> handlePositions = {
        m_currentRect.topLeft(), m_currentRect.topRight(),
        m_currentRect.bottomLeft(), m_currentRect.bottomRight()};
    for (auto hp : handlePositions) {
      painter.save();
      painter.translate(hp);
      painter.scale(1. / scaleFactor, 1. / scaleFactor);
      painter.setPen(Qt::black);
      painter.setBrush(Qt::white);
      painter.drawRect(-3, -3, 5, 5);
      painter.restore();
    }
  }
  painter.setPen(QPen(Qt::black));
  painter.restore();
}

void SelectionTool::addContextMenu(QMenu* menu) {
  if (m_currentRect.isNull()) return;
  QAction* fillCmd        = menu->addAction(tr("Fill Selection"));
  QAction* removeMatteCmd = menu->addAction(tr("Remove Matte Color"));
  menu->addSeparator();

  connect(fillCmd, SIGNAL(triggered()), this, SLOT(onFill()));
  connect(removeMatteCmd, SIGNAL(triggered()), this, SLOT(onRemoveMatte()));
}

void SelectionTool::saveToolState(QSettings& settings) {
  settings.setValue("SelectionTool_Mode", (int)m_mode);
}

void SelectionTool::loadToolState(QSettings& settings) {
  m_mode =
      (SelectionMode)settings.value("SelectionTool_Mode", (int)m_mode).toInt();
}

void SelectionTool::select(const QRect& selectedRect,
                           const QRect& currentRect) {
  m_selectedRect = selectedRect;
  m_currentRect  = currentRect;
  m_view->update();
}

void SelectionTool::select(const QPainterPath& path) {
  m_freeHandPath = path;
  m_selectedRect = path.boundingRect().toAlignedRect();
  m_currentRect  = m_selectedRect;
  m_view->update();
}

void SelectionTool::clearSelection() {
  select(QRect(), QRect());
  m_freeHandPath.clear();
}

void SelectionTool::pasteImage(const QRect& rect, const QImage& img,
                               bool replace, bool smoothing) {
  QImage scribbleImg = m_view->scribbleImage().copy();
  QPainter painter(&scribbleImg);
  if (smoothing) {
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);
  }
  // 消去モード
  if (img.isNull()) {
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.fillRect(rect, Qt::black);
  }
  // はりつけ
  else {
    if (replace) painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(rect, img);
  }
  painter.end();
  m_view->setScribbleImage(scribbleImg);
  m_view->update();
  MyParams::instance()->setImageDirty(MyParams::instance()->currentPage());
}

void SelectionTool::clearImage(const QPainterPath& path) {
  QImage scribbleImg = m_view->scribbleImage().copy();
  QPainter painter(&scribbleImg);
  // 消去モード
  painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
  painter.fillPath(path, Qt::black);

  painter.end();
  m_view->setScribbleImage(scribbleImg);
  m_view->update();
  MyParams::instance()->setImageDirty(MyParams::instance()->currentPage());
}

//---------------------------------------
// 編集コマンド
//---------------------------------------
// ペーストのUndo
class SelectionPasteUndo : public QUndoCommand {
  SelectionTool* m_tool;
  QRect m_rect;
  QImage m_image;

public:
  SelectionPasteUndo(SelectionTool* tool, QRect rect, QImage image,
                     QUndoCommand* parent = nullptr)
      : QUndoCommand(parent), m_tool(tool), m_rect(rect), m_image(image) {}

  void undo() {
    m_tool->clearSelection();
    m_tool->setSelectedImage(QImage());
  }

  void redo() {
    m_tool->select(m_rect, m_rect);
    m_tool->setSelectedImage(m_image);
  }
};

// 選択の消去のUndo
class SelectionDeleteUndo : public QUndoCommand {
  SelectionTool* m_tool;
  QRect m_selectedRect, m_currentRect;
  QImage m_selectedImg;
  QPainterPath m_path;

public:
  SelectionDeleteUndo(SelectionTool* tool, QRect selectedRect,
                      QRect currentRect, QPainterPath path, QImage selectedImg)
      : m_tool(tool)
      , m_selectedRect(selectedRect)
      , m_currentRect(currentRect)
      , m_selectedImg(selectedImg)
      , m_path(path) {}

  void undo() {
    // 選択ツールに切り替え
    MyParams::instance()->setCurrentTool(Tool_Selection);
    if (!m_selectedImg.isNull()) {
      // 選択画像を戻す
      m_tool->setSelectedImage(m_selectedImg);
    }
    if (m_path.isEmpty())
      // Rectを選択状態に戻す
      m_tool->select(m_selectedRect, m_currentRect);
    else
      m_tool->select(m_path);
  }
  void redo() {
    if (!m_selectedImg.isNull()) {
      // 選択画像をクリア
      m_tool->setSelectedImage(QImage());
    }
    // 選択解除
    m_tool->clearSelection();
  }
};

//---------------------------------------
// 選択の塗りつぶしのUndo
class SelectionFillUndo : public QUndoCommand {
  SelectionTool* m_tool;
  QColor m_color;
  QPainterPath m_path;
  QImage m_selectedImg_before, m_selectedImg_after;

public:
  SelectionFillUndo(SelectionTool* tool, QColor color, QRect selectedRect,
                    QPainterPath path, QImage selectedImg)
      : m_tool(tool)
      , m_color(color)
      , m_path(path)
      , m_selectedImg_before(selectedImg) {
    m_selectedImg_after = QImage(m_selectedImg_before.size(),
                                 QImage::Format_ARGB32_Premultiplied);
    if (m_path.isEmpty())
      m_selectedImg_after.fill(color);
    else {
      m_selectedImg_after.fill(Qt::transparent);
      QPainter p(&m_selectedImg_after);
      p.fillPath(m_path.translated(QPointF(-selectedRect.topLeft())), color);
    }
  }

  void undo() {
    m_tool->setSelectedImage(m_selectedImg_before);
    m_tool->updateView();
  }
  void redo() {
    m_tool->setSelectedImage(m_selectedImg_after);
    m_tool->updateView();
  }
};

class SelectionRemoveMatteUndo : public QUndoCommand {
  SelectionTool* m_tool;
  QImage m_selectedImg_before, m_selectedImg_after;
  bool m_isInit;

public:
  SelectionRemoveMatteUndo(SelectionTool* tool, QImage selectedImgBefore,
                           QImage selectedImgAfter)
      : m_tool(tool)
      , m_isInit(true)
      , m_selectedImg_before(selectedImgBefore)
      , m_selectedImg_after(selectedImgAfter) {}

  void undo() {
    m_tool->setSelectedImage(m_selectedImg_before);
    m_tool->updateView();
  }
  void redo() {
    if (m_isInit) {
      m_isInit = false;
      return;
    }
    m_tool->setSelectedImage(m_selectedImg_after);
    m_tool->updateView();
  }
};
//---------------------------------------

void SelectionTool::onCut() {
  onCopy();
  onDelete();
}

void SelectionTool::onCopy() {
  // 選択していなければ何もしない
  if (m_selectedRect.isNull()) return;
  // クリップボードに入れる画像はcurrentRectにリサイズしたもの
  QClipboard* clipboard = QGuiApplication::clipboard();
  clipboard->setImage(m_selectedImage.scaled(
      m_currentRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void SelectionTool::onPaste() {
  const QClipboard* clipboard = QApplication::clipboard();
  const QMimeData* mimeData   = clipboard->mimeData();
  assert(mimeData && mimeData->hasImage());

  QImage image = qvariant_cast<QImage>(mimeData->imageData());
  pasteImage(image);
}

// TODO: rectを引数にできるようにしよう
void SelectionTool::pasteImage(const QImage& image, QRect rect) {
  if (rect.isNull()) rect = QRect(QPoint(), image.size());
  rect.moveCenter(m_view->mouseCoord().toPoint());

  // 何か選択されてる場合
  if (!m_currentRect.isNull()) {
    QUndoCommand* uc = new QUndoCommand();
    // まず選択を確定解除
    new SelectionReleaseUndo(this, m_selectedRect, m_currentRect,
                             m_freeHandPath, m_selectedImage,
                             m_view->scribbleImage().copy(m_currentRect), uc);
    new SelectionPasteUndo(this, rect, image.copy(), uc);
    MyParams::instance()->undoStack()->push(uc);
  } else
    // 登録時redoが実行される
    MyParams::instance()->undoStack()->push(
        new SelectionPasteUndo(this, rect, image.copy()));
}

void SelectionTool::onDelete() {
  // 選択していなければ何もしない
  if (m_selectedRect.isNull()) return;
  // 登録時redoが実行される
  MyParams::instance()->undoStack()->push(new SelectionDeleteUndo(
      this, m_selectedRect, m_currentRect, m_freeHandPath, m_selectedImage));
}

void SelectionTool::onKeyPress(QKeyEvent* event) {
  // 選択していなければ何もしない
  if (m_selectedRect.isNull()) return;
  QPoint offset;
  switch (event->key()) {
  case Qt::Key_Up:
    offset.setY(-1);
    break;
  case Qt::Key_Down:
    offset.setY(1);
    break;
  case Qt::Key_Left:
    offset.setX(-1);
    break;
  case Qt::Key_Right:
    offset.setX(1);
    break;
  // release selection
  case Qt::Key_Enter:
  case Qt::Key_Return:
    // 登録時redoが実行される
    MyParams::instance()->undoStack()->push(new SelectionReleaseUndo(
        this, m_selectedRect, m_currentRect, m_freeHandPath, m_selectedImage,
        m_view->scribbleImage().copy(m_currentRect)));
    m_dragStartPos = QPoint();
    event->accept();
    return;
    break;
  default:
    break;
  }

  if (offset.isNull()) return;

  m_currentRect.translate(offset);
  // 登録時redoが実行される
  MyParams::instance()->undoStack()->push(new SelectionDragToolUndo(
      this, m_selectedRect, m_currentRect, m_freeHandPath));
  event->accept();
}

void SelectionTool::onFill() {
  // 選択していなければ何もしない
  if (m_currentRect.isNull()) return;
  QColor color = MyParams::instance()->currentColor();
  // 登録時redoが実行される
  MyParams::instance()->undoStack()->push(new SelectionFillUndo(
      this, color, m_selectedRect, m_freeHandPath, m_selectedImage));
}

void SelectionTool::onRemoveMatte() {
  // 選択していなければ何もしない
  if (m_currentRect.isNull()) return;
  QImage orgImg = m_selectedImage.copy();

  QColor matteColor = QColorDialog::getColor(QColor(255, 241, 150), nullptr,
                                             tr("Choose Matte Color"));

  if (!matteColor.isValid()) return;

  m_selectedImage = removeMatte(m_selectedImage, matteColor);

  MyParams::instance()->undoStack()->push(
      new SelectionRemoveMatteUndo(this, orgImg, m_selectedImage));
}

//---------------------------------------
SelectionTool _selectionTool;