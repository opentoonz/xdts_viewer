#pragma once
#ifndef TOOL_H
#define TOOL_H

#include <QPointF>
#include <QPainterPath>
#include <QPixmap>
#include <QList>
#include <QPair>
#include <QObject>

#include "myparams.h"

class XsheetPdfPreviewPane;
class QMenu;
class QKeyEvent;
class QSettings;

class Tool : public QObject {
protected:
  XsheetPdfPreviewPane* m_view;

public:
  Tool(ToolId id);
  virtual void setView(XsheetPdfPreviewPane* view) { m_view = view; }
  void updateView();

  virtual void onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) {}
  virtual void onDrag(const QPointF& from, const QPointF& to,
                      Qt::KeyboardModifiers modifiers, qreal pressure = 0.5) {}
  virtual void onMove(const QPointF& pos) {}
  virtual void onRelease(const QPointF& pos, QImage& canvasImg,
                         const QPointF& canvasPos) {}
  virtual void onDeactivate() {}
  virtual bool onActivate() { return true; }  // ツール変更可能ならtrueを返す
  virtual void draw(QPainter& painter, QPointF pos, double scaleFactor) {}
  virtual void addContextMenu(QMenu* menu) {}
  virtual void onEnter() {}
  virtual void onKeyPress(QKeyEvent*) {}
  virtual void saveToolState(QSettings& settings) {};
  virtual void loadToolState(QSettings& settings) {};
};

class BrushUndo : public QUndoCommand {
  int m_page;
  QImage m_before, m_after;
  QRect m_boundingBox;
  XsheetPdfPreviewPane* m_view;
  bool m_onPush;

public:
  BrushUndo(const QImage& before, const QImage& after, const QRect& boundingBox,
            XsheetPdfPreviewPane* view);

  void setImage(bool isUndo);
  void undo() override;
  void redo() override;
};

class BrushTool : public Tool {
  QImage m_imageBefore;
  QRect m_boundingBox;
  QPointF m_lastPos;
  double m_brushSize;

public:
  enum BrushSize { Small = 0, Large };

  BrushTool();
  void onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) override;
  void onDrag(const QPointF& from, const QPointF& to,
              Qt::KeyboardModifiers modifiers, qreal pressure = 0.5) override;
  void onRelease(const QPointF& pos, QImage& canvasImg,
                 const QPointF& canvasPos) override;
  bool onActivate() override;
  void setSize(BrushSize sizeId);
  void onEnter() override;

  void saveToolState(QSettings& settings) override;
  void loadToolState(QSettings& settings) override;
  BrushSize brushSizeId();
};

class EraserTool : public Tool {
  QImage m_imageBefore;
  QRect m_boundingBox;

public:
  EraserTool();
  void onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) override;
  void onDrag(const QPointF& from, const QPointF& to,
              Qt::KeyboardModifiers modifiers, qreal pressure = 0.5) override;
  void onRelease(const QPointF& pos, QImage& canvasImg,
                 const QPointF& canvasPos) override;
  void draw(QPainter& painter, QPointF pos, double scaleFactor) override;
  bool onActivate() override;
};

class SelectionDragTool {
protected:
  QRect m_rect;

public:
  SelectionDragTool(QRect rect) : m_rect(rect) {}
  virtual void onClick(QPoint p) {};
  virtual QRect onDrag(QPoint p, QRect r,
                       const Qt::KeyboardModifiers modifiers) {
    return QRect();
  };
  virtual void onRelease() {};
};

class SelectionTool : public Tool {
  Q_OBJECT
public:
  enum SelectionMode { Rect = 0, FreeHand };

private:
  QImage m_imageBefore;
  QRect m_selectedRect;
  QRect m_currentRect;

  // rect
  QPoint m_dragStartPos;
  // freehand
  QPainterPath m_freeHandPath;

  SelectionDragTool* m_dragTool;

  QImage m_selectedImage;

  SelectionMode m_mode;

  enum MouseOver {
    Over_None = 0,
    Over_Body,
    Over_TopLeft,
    Over_TopRight,
    Over_BottomLeft,
    Over_BottomRight
  } m_mouseOver;

public:
  SelectionTool();
  void onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) override;
  void onDrag(const QPointF& from, const QPointF& to,
              Qt::KeyboardModifiers modifiers, qreal pressure = 0.5) override;
  void onMove(const QPointF& pos) override;
  void onRelease(const QPointF& pos, QImage& canvasImg,
                 const QPointF& canvasPos) override;
  void draw(QPainter& painter, QPointF pos, double scaleFactor) override;
  void addContextMenu(QMenu* menu) override;
  void saveToolState(QSettings& settings) override;
  void loadToolState(QSettings& settings) override;

  void select(const QRect& selectedRect, const QRect& currentRect);
  void select(const QPainterPath& path);
  void setSelectedImage(QImage img) { m_selectedImage = img; }
  void clearSelection();
  void pasteImage(const QRect& rect, const QImage& img, bool replace,
                  bool smoothing = false);
  void clearImage(const QPainterPath&);
  void setMode(SelectionMode mode) { m_mode = mode; }
  SelectionMode mode() const { return m_mode; }

  void onCut();
  void onCopy();
  void onPaste();
  void pasteImage(const QImage&, QRect rect = QRect());
  void onDelete();
  void onKeyPress(QKeyEvent*);

protected slots:
  void onFill();
  void onRemoveMatte();
};

class StampTool : public Tool {
  int m_stampId;

  QList<QPair<QString, QPixmap>> m_stampImgs;

  double m_tmpScaleFactor;
  bool m_validFlag;

public:
  StampTool();
  void onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) override;
  void draw(QPainter& painter, QPointF pos, double scaleFactor) override;
  void saveToolState(QSettings& settings) override;
  void loadToolState(QSettings& settings) override;
  void registerStamp(const QString name, const QPixmap pm);
  void setStampId(int id) {
    m_stampId   = id;
    m_validFlag = false;
  }
  int stampId() const { return m_stampId; }
  QPixmap& prepareScaledStamp(double scaleFactor);

  QPair<QString, QPixmap> getStampInfo(int id) const {
    assert(0 <= id && id < m_stampImgs.size());
    return m_stampImgs[id];
  }
  int stampCount() { return m_stampImgs.size(); }
};

enum LineId {
  ThinLine,
  ThickLine,
  SmallArrow,
  LargeArrow,
  DoubledLine,
  DottedLine,
  TypeCount
};

class LineTool : public Tool {
  LineId m_lineId;

  QPoint m_dragStartPos, m_currentPos;

  void doDrawLine(QPainter&);

public:
  LineTool();
  void onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) override;
  void onDrag(const QPointF& from, const QPointF& to,
              Qt::KeyboardModifiers modifiers, qreal pressure = 0.5) override;
  void onRelease(const QPointF& pos, QImage& canvasImg,
                 const QPointF& canvasPos) override;
  void draw(QPainter& painter, QPointF pos, double scaleFactor) override;
  void saveToolState(QSettings& settings) override;
  void loadToolState(QSettings& settings) override;
  void setType(LineId id) { m_lineId = id; }
  LineId type() const { return m_lineId; }
};
#endif