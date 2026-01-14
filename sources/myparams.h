#pragma once

#ifndef MYPARAMS_H
#define MYPARAMS_H

#include <QString>
#include <QColor>
#include <QMap>
#include <QHash>
#include <QPair>
#include <QUndoStack>
#include <QCursor>

#include "xdtsio.h"

class XSheetPDFTemplate;
class Tool;
class XsheetPdfPreviewPane;

// 動画欄を先に読み込むために、Cellsを先にもってくる
enum ExportArea { Area_Cells = 0, Area_Actions, Area_Unspecified };
enum ContinuousLineMode { Line_Always = 0, Line_MoreThan3s, Line_None };

// テレコの扱い
enum MixupColumnsType { Mixup_Manual = 0, Mixup_Auto };

enum ToolId {
  Tool_Brush,
  Tool_Eraser,
  Tool_Selection,
  Tool_Stamp,
  Tool_Line,
  Tool_AE,
  Tool_Dye
};

struct ColumnData {
  QVector<FrameData> cells;
  QString name;
  bool isHoldFrame;
};

typedef QList<ColumnData> ColumnsData;
typedef QHash<QPair<int, int>, QColor> DyedCellsData;

enum WorkFlowType { Genga = 0, Douga, LO, RoughGen, WorkFlowTypeCount };

const int GengaLevelsCount_Auto = -1;

class MyParams : public QObject  // singleton
{
  Q_OBJECT

  QMap<QString, QString> m_templatesMap;

  QString m_createdVersion;

  // format settings
  QString m_templateName;
  ExportArea m_exportArea;
  QString m_logoPath;
  // regular expressions using wildcards. separated by semicorons
  // Ex:  "*_e;*_k;*_s;lo"
  QString m_skippedLevelNames;
  bool m_expandColumns;
  int m_cameraColumnAddition;  // カメラ列の追加分（列を拡張する場合）

  MixupColumnsType m_mixUpColumnsType;

  struct MixUpColumnsKeyFramesInfo {
    QStringList
        colNames;  // テレコのつじつまを合わせるために保持する、現在のセル名の一覧
    QMap<int, QList<int>> keyframes;
  };

  QMap<ExportArea, MixUpColumnsKeyFramesInfo> m_mixUpColumnsKeyframes;

  // コマの動画番号に色を付ける
  QMap<ExportArea, DyedCellsData> m_dyedCells;

  bool m_withDenpyo;
  QString m_backsideImgPath;
  QString m_backsideImgPathWithDenpyo;

  bool m_isScannedGengaSheet;  // 原画シートを紙からスキャン
                               // （原画欄の枠線を隠す）

  int m_dougaColumnOffset;  // Deprecated. Is kept only for compatibility:
                            // 動画列の開始位置（原画シートが紙スキャンの場合）
  int m_gengaLevelsCount;   // 原画のセル数

  int m_scannedSheetPageAmount;  // 手動で空ページを追加する

  // OL尺（フレーム長で保存する）
  int m_startOverlapFrameLength;
  int m_endOverlapFrameLength;

  bool m_showSkippedDrawingsInfo;

  bool m_isFormatDirty;

  // preferences
  QString m_language;  // either "en" or "ja"
  QColor m_lineColor;
  QString m_templateFont;
  QString m_contentsFont;
  ContinuousLineMode m_continuousLineMode;
  int m_minimumRepeatLength;
  bool m_serialFrameNumber;
  bool m_levelNameOnBottom;
  bool m_capitalizeFirstLetter;
  QString m_userStampFolderPath;
  QString m_approvalName;
  int m_emptyFrameForAE;

  QMap<WorkFlowType, QString>
      m_suffixes;  // reserved file suffixes by workflows

  // export settings
  QColor m_exportLineColor;
  QString m_exportTemplateFont;
  QString m_exportContentsFont;

  QMap<ExportArea, QString> m_currentXdtsPaths;

  XSheetPDFTemplate* m_currentTmpl;

  // scribble images
  QMap<int, QImage> m_scribbleImages;
  QMap<int, bool> m_dirtyFlags;

  // environment
  QMap<ToolId, Tool*> m_tools;
  ToolId m_currentToolId;
  QColor m_currentColor;
  QCursor m_brushCursor;
  bool m_fitToWindow;

  // undo stack
  QUndoStack* m_undoStack;

  int m_currentPage;

  // suspend undo operation when busy (when dragging)
  bool m_isBusy;

  MyParams();

public:
  static MyParams* instance();
  ~MyParams();
  static int PDF_Resolution;

  // to be called just after instancing the application
  void initialize();
  void initStamps() {
    // マイスタンプの登録ここでやる
    registerDefaultStamps();
    loadUserStamps();
  }

  QString createdVersion() const { return m_createdVersion; }

  void setTemplateName(const QString& val) { m_templateName = val; }
  QString templateName() const { return m_templateName; }

  void registerTemplate(const QString& name, const QString& path) {
    m_templatesMap.insert(name, path);
  }
  const QMap<QString, QString>& availableTemplates() { return m_templatesMap; }
  QString templatePath() const;

  void setIsExpamdColumns(bool on) { m_expandColumns = on; }
  bool isExpandColumns() { return m_expandColumns; }

  void setMixUpColumns(bool isAuto) {
    m_mixUpColumnsType = (isAuto) ? Mixup_Auto : Mixup_Manual;
  }
  MixupColumnsType mixUpColumnsType() { return m_mixUpColumnsType; }

  QMap<int, QList<int>>& mixUpColumnsKeyframes(ExportArea area);
  QStringList mixUpColumnsNames(ExportArea area) const;
  void setMixupColumnsKeyframe(ExportArea area, int frame, QList<int> data) {
    mixUpColumnsKeyframes(area).insert(frame, data);
  }
  void setMixUpColumnsNames(const ExportArea area, const QStringList colNames) {
    m_mixUpColumnsKeyframes[area].colNames = colNames;
  }
  void deleteMixUpColumnsKeyframeAt(ExportArea area, int frame) {
    mixUpColumnsKeyframes(area).remove(frame);
  }
  void clearMixUpColumnsKeyframes(ExportArea area) {
    mixUpColumnsKeyframes(area).clear();
  }
  bool isMixUpColumnsKeyframesShared() {
    return m_mixUpColumnsKeyframes.size() == 0 ||
           (m_mixUpColumnsKeyframes.size() == 1 &&
            m_mixUpColumnsKeyframes.keys()[0] == Area_Unspecified);
  }
  void unifyOrSeparateMixupColumnsKeyframes(bool unify);

  DyedCellsData& dyedCells(ExportArea area) { return m_dyedCells[area]; }

  void setExportArea(const ExportArea val) { m_exportArea = val; }
  ExportArea exportArea() const { return m_exportArea; }

  void setLogoPath(const QString& val) { m_logoPath = val; }
  QString logoPath(bool asIs = false) const;

  void setWithDenpyo(bool withDenpyo) { m_withDenpyo = withDenpyo; }
  bool withDenpyo() { return m_withDenpyo; }

  void setBacksideImgPath(const QString& val);
  QString backsideImgPath(bool asIs = false) const;

  void setIsScannedGengaSheet(bool on) { m_isScannedGengaSheet = on; }
  bool isScannedGengaSheet() { return m_isScannedGengaSheet; }

  // DEPRECATED
  // void setDougaColumnOffset(int val) { m_dougaColumnOffset = val; }
  // int dougaColumnOffset_Param() { return m_dougaColumnOffset; }
  // int getDougaColumnOffset() {
  //  return (m_isScannedGengaSheet) ? m_dougaColumnOffset : 0;
  //}
  void setGengaLevelsCount(const int val) { m_gengaLevelsCount = val; }
  int gengaLevelsCount_Param() { return m_gengaLevelsCount; }
  int getGengaLevelsCount();

  void setCameraColumnAddition(int val) { m_cameraColumnAddition = val; }
  int cameraColumnAddition_Param() { return m_cameraColumnAddition; }
  int getCameraColumnAddition() {
    return (m_expandColumns) ? m_cameraColumnAddition : 0;
  }

  void setScannedSheetPageAmount(int val) { m_scannedSheetPageAmount = val; }
  int scannedSheetPageAmount_Param() { return m_scannedSheetPageAmount; }
  int getScannedSheetPageAmount() {
    return (m_isScannedGengaSheet) ? m_scannedSheetPageAmount : 0;
  }

  int startOverlapFrameLength() { return m_startOverlapFrameLength; }
  void setStartOverlapFrameLength(int val) { m_startOverlapFrameLength = val; }
  int endOverlapFrameLength() { return m_endOverlapFrameLength; }
  void setEndOverlapFrameLength(int val) { m_endOverlapFrameLength = val; }
  bool hasOverlap() {
    return m_startOverlapFrameLength > 0 || m_endOverlapFrameLength < 0;
  }

  void setShowSkippedDrawingsInfo(bool show) {
    m_showSkippedDrawingsInfo = show;
  }
  bool showSkippedDrawingsInfo() { return m_showSkippedDrawingsInfo; }

  void setSkippedLevelNames(const QString& val) { m_skippedLevelNames = val; }
  QString skippedLevelNames() const { return m_skippedLevelNames; }

  void setLanguage(const QString& lang) { m_language = lang; }
  QString language() const { return m_language; }

  void setLineColor(const QColor& val) { m_lineColor = val; }
  QColor lineColor() const { return m_lineColor; }

  void setTemplateFont(const QString& val) { m_templateFont = val; }
  QString templateFont() const { return m_templateFont; }

  void setContentsFont(const QString& val) { m_contentsFont = val; }
  QString contentsFont() const { return m_contentsFont; }

  void setContinuousLineMode(const ContinuousLineMode val) {
    m_continuousLineMode = val;
  }
  ContinuousLineMode continuousLineMode() const { return m_continuousLineMode; }

  void setMinimumRepeatLength(const int length) {
    m_minimumRepeatLength = length;
  }
  int minimumRepeatLength() { return m_minimumRepeatLength; }

  void setIsSerialFrameNumber(bool val) { m_serialFrameNumber = val; }
  bool isSerialFrameNumber() const { return m_serialFrameNumber; }

  void setIsLevelNameOnBottom(bool val) { m_levelNameOnBottom = val; }
  bool isLevelNameOnBottom() { return m_levelNameOnBottom; }

  void setIsCapitalizeFirstLetter(bool val) { m_capitalizeFirstLetter = val; }
  bool isCapitalizeFirstLetter() { return m_capitalizeFirstLetter; }

  const QString& userStampFolderPath() { return m_userStampFolderPath; }
  void setUserStampFolderPath(const QString& path) {
    m_userStampFolderPath = path;
  }

  const QString& approvalName() { return m_approvalName; }
  void setApprovalName(const QString& name) { m_approvalName = name; }

  const int emptyFrameForAE() { return m_emptyFrameForAE; }
  void setEmptyFrameForAE(const int val) { m_emptyFrameForAE = val; }

  const QString& suffix(WorkFlowType type) { return m_suffixes[type]; }
  void setSuffix(WorkFlowType type, const QString& val) {
    m_suffixes.insert(type, val);
  }

  QColor exportLineColor() const { return m_exportLineColor; }
  QString exportTemplateFont() const { return m_exportTemplateFont; }
  QString exportContentsFont() const { return m_exportContentsFont; }

  QImage scribbleImage(int page);
  void setScribbleImage(int page, const QImage& image);
  void resetValues();

  QPixmap backsidePixmap(bool forExportImage = false);

  XSheetPDFTemplate* currentTemplate() { return m_currentTmpl; }
  void setCurrentTemplate(XSheetPDFTemplate* tmpl);

  QString currentXdtsPath(ExportArea id = Area_Unspecified);
  void setCurrentXdtsPath(const QString& path);
  bool isAreaSpecified();
  QList<ExportArea> exposeAreas() { return m_currentXdtsPaths.keys(); }

  QString getImageFolderPath(QString xdtsPath = "");
  void setImageDirty(int page);
  void setFormatDirty(bool dirty);

  bool loadFormatSettingsIfExists();
  void saveFormatSettings();
  bool loadUserSettingsIfExists();
  void saveUserSettings();
  void saveWindowGeometry(const QRect geometry);
  QRect loadWindowGeometry() const;
  void convertCompatibleParameters();

  void registerDefaultStamps();
  void loadUserStamps();

  void registerTool(ToolId id, Tool* tool) { m_tools.insert(id, tool); }
  void setCurrentTool(ToolId id);
  Tool* currentTool() { return getTool(m_currentToolId); }
  const ToolId currentToolId() { return m_currentToolId; }
  Tool* getTool(const ToolId id) { return m_tools.value(id); }
  void setToolView(XsheetPdfPreviewPane*);
  const QCursor& getBrushToolCursor() { return m_brushCursor; }

  const QColor currentColor() { return m_currentColor; }
  void setCurrentColor(QColor col) { m_currentColor = col; }

  const bool isFitToWindow() { return m_fitToWindow; }
  void setIsFitToWindow(const bool on) { m_fitToWindow = on; }

  QUndoStack* undoStack() { return m_undoStack; }

  int currentPage() { return m_currentPage; }
  void setCurrentPage(int page) { m_currentPage = page; }

  bool isBusy() { return m_isBusy; }
  void setBusy(bool busy) { m_isBusy = busy; }

  bool somethingIsDirty();
  void saveChanges();
  bool saveUntitled();
signals:
  void dirtyStateChanged();
  void toolSwitched();
  void backsideImgPathChanged();
  // triggered by settings / preferences dialogs
  void templateSwitched();
  void somethingChanged();

public:
  void notifyTemplateSwitched() { emit templateSwitched(); }
  void notifySomethingChanged() { emit somethingChanged(); }
};

#endif
