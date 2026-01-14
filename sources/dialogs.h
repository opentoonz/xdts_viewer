#pragma once
#ifndef DIALOGS_H
#define DIALOGS_H

#include <QDialog>
#include <QMap>

class QComboBox;
class QLineEdit;
class QLabel;
class QColorDialog;
class QFontComboBox;
class QCheckBox;
class QGroupBox;

class SettingsDialog : public QDialog {
  Q_OBJECT
  // format
  QComboBox* m_templateCombo;
  QGroupBox* m_expandColumnsGB;
  QLineEdit* m_cameraColumnAdditionEdit;
  QComboBox* m_mixUpColumnsCombo;
  QPushButton* m_mixupKeyBtn;
  QLineEdit* m_logoImgPathField;
  QComboBox* m_exportAreaCombo;
  QLineEdit* m_skippedLevelNamesEdit;
  QCheckBox* m_withDenpyoCB;
  QCheckBox* m_showSkippedDrawingsCB;
  QLineEdit* m_backsideImgPathField;
  QGroupBox* m_scannedGengaSheetGB;
  QLineEdit* m_gengaLevelsCountEdit;
  // QLineEdit* m_dougaColumnOffsetEdit; // deprecated
  QLineEdit* m_scannedSheetPageAmountEdit;
  QLabel* m_pageInfoLbl;

  QComboBox* m_startOLSecCombo;
  QComboBox* m_startOLKomaCombo;
  QComboBox* m_endOLSecCombo;
  QComboBox* m_endOLKomaCombo;

  int m_duration;
  //  QStringList m_levelNames;

public:
  SettingsDialog(QWidget* parent);
  // SettingsDialog(QWidget* parent, const QStringList& levelNames, int
  // duration);
  void syncUIs();
  void setDuration(int duration) { m_duration = duration; }

protected slots:
  void onTemplateSwitched(int);
  void onExpandColumnsSwitched();
  void onMixUpActivated();
  void openMixupKeyDialog();
  void onFormatSettingsChanged();
  void onLogoImgBrowserButtonClicked();
  void onBacksideImgBrowserButtonClicked();
  void onSkippedLevelNameChanged();
  void onGengaLevelsCountEdited();
  void onCameraColumnAdditionEdited();
  void onStartOLComboChanged(int);
  void onEndOLComboChanged(int);
  void onDenpyoCheckboxClicked(bool);
  void onShowSkippedDrawingsClicked(bool);
};

class PreferencesDialog : public QDialog {
  Q_OBJECT

  // user
  QComboBox* m_languageCombo;
  QPushButton* m_lineColorButton;
  QColorDialog* m_lineColorDialog;
  // QFontComboBox* m_templateFontCB, * m_contentsFontCB;
  QComboBox* m_continuousLineCombo;
  QLineEdit* m_minimumRepeatLengthField;
  QCheckBox *m_serialFrameNumberCB, *m_levelNameOnBottomCB,
      *m_capitalizeFirstLetterCB;
  QLineEdit* m_stampFolderPathField;
  QLineEdit* m_approvalNameField;
  QLineEdit* m_emptyFrameForAEField;
  QMap<int, QLineEdit*> m_suffixEdits;

public:
  PreferencesDialog(QWidget* parent);
  void syncUIs();
protected slots:
  void onLanguageSwitched();
  void onViewPreferencesChanged();
  void onLineColorChanged(const QColor&);
  void updateLineColorButton();
  void onStampBrowserButtonClicked();
  void onStampPathChanged();
  void onApprovalNameChanged();
  void onEmptyFrameForAEChanged();
  void onCapitalizeFirstLetterSwitched();
  void onSuffixEdited();
};

class AboutDialog : public QDialog {
  Q_OBJECT

public:
  AboutDialog(QWidget* parent);

protected:
  void paintEvent(QPaintEvent*) override;
};
#endif