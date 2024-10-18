#include "dialogs.h"
#include "myparams.h"
#include "pathutils.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QColorDialog>
#include <QFontComboBox>
#include <QCheckBox>
#include <QIntValidator>
#include <QMessageBox>
#include <QImageReader>
#include <QFileDialog>
#include <QDir>
#include <QPainter>
#include <QTextEdit>
#include <QMessageBox>

namespace {
void frame2SecKoma(int frame, int& sec, int& koma) {
  sec  = frame / 24;
  koma = frame % 24;
}
int secKoma2Frame(int sec, int koma) { return sec * 24 + koma; }

}  // namespace
//-----------------------------------------------------------------------------

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    // , m_levelNames(levelNames)
    , m_duration(0) {
  setWindowTitle(tr("Settings"));

  m_templateCombo           = new QComboBox(this);
  m_logoImgPathField        = new QLineEdit(this);
  QPushButton* browseButton = new QPushButton("...", this);
  m_exportAreaCombo         = new QComboBox(this);
  m_pageInfoLbl             = new QLabel(this);
  m_skippedLevelNamesEdit   = new QLineEdit(this);
  m_expandColumnsCB = new QCheckBox(tr("Adaptively Expand Columns "), this);
  m_mixUpColumnsCB  = new QCheckBox(tr("Mix-up Columns "), this);
  m_withDenpyoCB    = new QCheckBox(tr("Attach Composite Voucher"), this);
  m_backsideImgPathField = new QLineEdit(this);
  m_scannedGengaSheetGB =
      new QGroupBox(tr("Scanned Genga Sheet Mode ( Hide Genga Area Lines )"));
  m_dougaColumnOffsetEdit           = new QLineEdit(this);
  m_scannedSheetPageAmountEdit      = new QLineEdit(this);
  QPushButton* backsideBrowseButton = new QPushButton("...", this);
  QPushButton* closeButton          = new QPushButton(tr("OK"), this);

  m_startOLSecCombo  = new QComboBox(this);
  m_startOLKomaCombo = new QComboBox(this);
  m_endOLSecCombo    = new QComboBox(this);
  m_endOLKomaCombo   = new QComboBox(this);

  MyParams* p                    = MyParams::instance();
  QMap<QString, QString> tmplMap = p->availableTemplates();
  for (auto name : tmplMap.keys()) {
    m_templateCombo->addItem(name, tmplMap.value(name));
  }
  browseButton->setFixedSize(20, 20);
  browseButton->setFocusPolicy(Qt::NoFocus);
  backsideBrowseButton->setFixedSize(20, 20);
  backsideBrowseButton->setFocusPolicy(Qt::NoFocus);
  m_exportAreaCombo->addItem(tr("ACTIONS"), Area_Actions);
  m_exportAreaCombo->addItem(tr("CELLS"), Area_Cells);

  m_scannedGengaSheetGB->setCheckable(true);
  QIntValidator* validator = new QIntValidator();
  validator->setBottom(0);
  m_dougaColumnOffsetEdit->setValidator(validator);
  m_dougaColumnOffsetEdit->setMaximumWidth(50);
  QIntValidator* pagesValidator = new QIntValidator();
  pagesValidator->setBottom(1);
  m_scannedSheetPageAmountEdit->setMaximumWidth(50);
  m_scannedSheetPageAmountEdit->setValidator(pagesValidator);

  for (int i = 0; i < 24; i++) {
    m_startOLSecCombo->addItem(QString::number(i), i);
    m_startOLKomaCombo->addItem(QString::number(i), i);
    m_endOLSecCombo->addItem(QString::number(i), i);
    m_endOLKomaCombo->addItem(QString::number(i), i);
  }

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setMargin(10);
  mainLay->setSpacing(10);
  {
    QGridLayout* formatLay = new QGridLayout();
    formatLay->setMargin(10);
    formatLay->setHorizontalSpacing(5);
    formatLay->setVerticalSpacing(10);
    {
      formatLay->addWidget(new QLabel(tr("Template:"), this), 0, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      formatLay->addWidget(m_templateCombo, 0, 1, 1, 2,
                           Qt::AlignLeft | Qt::AlignVCenter);

      formatLay->addWidget(m_expandColumnsCB, 1, 0, 1, 3,
                           Qt::AlignLeft | Qt::AlignVCenter);

      formatLay->addWidget(m_mixUpColumnsCB, 2, 0, 1, 3,
                           Qt::AlignLeft | Qt::AlignVCenter);

      formatLay->addWidget(new QLabel(tr("Logo Image:"), this), 3, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      QHBoxLayout* logoPathLay = new QHBoxLayout();
      logoPathLay->setMargin(0);
      logoPathLay->setSpacing(1);
      {
        logoPathLay->addWidget(m_logoImgPathField, 1);
        logoPathLay->addWidget(browseButton, 0);
      }
      formatLay->addLayout(logoPathLay, 3, 1, 1, 2);

      formatLay->addWidget(m_withDenpyoCB, 4, 0, 1, 3);

      formatLay->addWidget(new QLabel(tr("Backside Image:"), this), 5, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      QHBoxLayout* backsidePathLay = new QHBoxLayout();
      backsidePathLay->setMargin(0);
      backsidePathLay->setSpacing(1);
      {
        backsidePathLay->addWidget(m_backsideImgPathField, 1);
        backsidePathLay->addWidget(backsideBrowseButton, 0);
      }
      formatLay->addLayout(backsidePathLay, 5, 1, 1, 2);

      formatLay->addWidget(new QLabel(tr("Output area:"), this), 6, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      formatLay->addWidget(m_exportAreaCombo, 6, 1);
      formatLay->addWidget(m_pageInfoLbl, 6, 2);

      formatLay->addWidget(new QLabel(tr("Skipped Levels:"), this), 7, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      formatLay->addWidget(m_skippedLevelNamesEdit, 7, 1, 1, 2);

      formatLay->addWidget(m_scannedGengaSheetGB, 8, 0, 1, 3);
      QHBoxLayout* scannedGengaLay = new QHBoxLayout();
      scannedGengaLay->setMargin(5);
      scannedGengaLay->setSpacing(5);
      {
        scannedGengaLay->addWidget(new QLabel(tr("Douga Column Offset:"), this),
                                   0);
        scannedGengaLay->addWidget(m_dougaColumnOffsetEdit, 1,
                                   Qt::AlignLeft | Qt::AlignVCenter);
        scannedGengaLay->addSpacing(10);
        scannedGengaLay->addWidget(new QLabel(tr("Page Amount:"), this), 0);
        scannedGengaLay->addWidget(m_scannedSheetPageAmountEdit, 1,
                                   Qt::AlignLeft | Qt::AlignVCenter);
      }
      m_scannedGengaSheetGB->setLayout(scannedGengaLay);

      // Overlap frame length
      formatLay->addWidget(new QLabel(tr("Overlap At the Start:"), this), 9, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      QHBoxLayout* startOlLay = new QHBoxLayout();
      startOlLay->setMargin(0);
      startOlLay->setSpacing(5);
      {
        startOlLay->addWidget(m_startOLSecCombo, 0);
        startOlLay->addWidget(new QLabel(tr("sec + "), this), 0);
        startOlLay->addWidget(m_startOLKomaCombo, 0);
        startOlLay->addWidget(new QLabel(tr("k"), this), 0);
        startOlLay->addStretch(1);
      }
      formatLay->addLayout(startOlLay, 9, 1, 1, 2);

      formatLay->addWidget(new QLabel(tr("Overlap At the End:"), this), 10, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      QHBoxLayout* endOlLay = new QHBoxLayout();
      endOlLay->setMargin(0);
      endOlLay->setSpacing(5);
      {
        endOlLay->addWidget(m_endOLSecCombo, 0);
        endOlLay->addWidget(new QLabel(tr("sec + "), this), 0);
        endOlLay->addWidget(m_endOLKomaCombo, 0);
        endOlLay->addWidget(new QLabel(tr("k"), this), 0);
        endOlLay->addStretch(1);
      }
      formatLay->addLayout(endOlLay, 10, 1, 1, 2);
    }
    mainLay->addLayout(formatLay, 0);

    mainLay->addWidget(closeButton, 0, Qt::AlignCenter);
  }
  setLayout(mainLay);

  connect(m_templateCombo, SIGNAL(activated(int)), this,
          SLOT(onTemplateSwitched(int)));
  connect(m_expandColumnsCB, SIGNAL(clicked(bool)), this,
          SLOT(onExpandColumnsSwitched()));
  connect(m_mixUpColumnsCB, SIGNAL(clicked(bool)), this,
          SLOT(onMixUpSwitched()));
  connect(m_logoImgPathField, SIGNAL(editingFinished()), this,
          SLOT(onFormatSettingsChanged()));
  connect(browseButton, SIGNAL(clicked()), this,
          SLOT(onLogoImgBrowserButtonClicked()));
  connect(m_withDenpyoCB, SIGNAL(clicked(bool)), this,
          SLOT(onDenpyoCheckboxClicked(bool)));
  connect(m_backsideImgPathField, SIGNAL(editingFinished()), this,
          SLOT(onFormatSettingsChanged()));
  connect(backsideBrowseButton, SIGNAL(clicked()), this,
          SLOT(onBacksideImgBrowserButtonClicked()));
  connect(m_exportAreaCombo, SIGNAL(activated(int)), this,
          SLOT(onFormatSettingsChanged()));
  connect(m_skippedLevelNamesEdit, SIGNAL(editingFinished()), this,
          SLOT(onSkippedLevelNameChanged()));
  connect(m_scannedGengaSheetGB, SIGNAL(clicked(bool)), this,
          SLOT(onFormatSettingsChanged()));
  connect(m_dougaColumnOffsetEdit, SIGNAL(editingFinished()), this,
          SLOT(onDougaColumnOffsetEdited()));
  connect(m_scannedSheetPageAmountEdit, SIGNAL(editingFinished()), this,
          SLOT(onFormatSettingsChanged()));

  connect(m_startOLSecCombo, SIGNAL(activated(int)), this,
          SLOT(onStartOLComboChanged(int)));
  connect(m_startOLKomaCombo, SIGNAL(activated(int)), this,
          SLOT(onStartOLComboChanged(int)));
  connect(m_endOLSecCombo, SIGNAL(activated(int)), this,
          SLOT(onEndOLComboChanged(int)));
  connect(m_endOLKomaCombo, SIGNAL(activated(int)), this,
          SLOT(onEndOLComboChanged(int)));

  connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
}

void SettingsDialog::syncUIs() {
  MyParams* p = MyParams::instance();
  m_templateCombo->setCurrentText(p->templateName());
  m_expandColumnsCB->setChecked(p->isExpandColumns());
  m_mixUpColumnsCB->setChecked(p->isMixUpColumns());
  m_logoImgPathField->setText(p->logoPath(true));
  m_withDenpyoCB->setChecked(p->withDenpyo());
  m_backsideImgPathField->setText(p->backsideImgPath(true));
  m_exportAreaCombo->setCurrentIndex(
      m_exportAreaCombo->findData(p->exportArea()));
  m_exportAreaCombo->setDisabled(p->isAreaSpecified());
  m_skippedLevelNamesEdit->setText(p->skippedLevelNames());
  m_scannedGengaSheetGB->setChecked(p->isScannedGengaSheet());
  m_dougaColumnOffsetEdit->setText(
      QString::number(p->dougaColumnOffset_Param()));
  m_scannedSheetPageAmountEdit->setText(
      QString::number(p->scannedSheetPageAmount_Param()));

  int sec, koma;
  frame2SecKoma(p->startOverlapFrameLength(), sec, koma);
  m_startOLSecCombo->setCurrentIndex(m_startOLSecCombo->findData(sec));
  m_startOLKomaCombo->setCurrentIndex(m_startOLKomaCombo->findData(koma));
  frame2SecKoma(p->endOverlapFrameLength(), sec, koma);
  m_endOLSecCombo->setCurrentIndex(m_endOLSecCombo->findData(sec));
  m_endOLKomaCombo->setCurrentIndex(m_endOLKomaCombo->findData(koma));
}

void SettingsDialog::onTemplateSwitched(int) {
  MyParams::instance()->setTemplateName(m_templateCombo->currentText());
  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

void SettingsDialog::onExpandColumnsSwitched() {
  MyParams::instance()->setIsExpamdColumns(m_expandColumnsCB->isChecked());
  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

void SettingsDialog::onMixUpSwitched() {
  MyParams::instance()->setMixUpColumns(m_mixUpColumnsCB->isChecked());
  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

void SettingsDialog::onDougaColumnOffsetEdited() {
  MyParams::instance()->setDougaColumnOffset(
      m_dougaColumnOffsetEdit->text().toInt());
  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

void SettingsDialog::onStartOLComboChanged(int) {
  int newOLFrameLength =
      secKoma2Frame(m_startOLSecCombo->currentData().toInt(),
                    m_startOLKomaCombo->currentData().toInt());
  int oldOLFrameLength = MyParams::instance()->startOverlapFrameLength();

  if (newOLFrameLength == oldOLFrameLength) return;

  // OL����XDTS�̎ڂ𒴂����Ȃ�
  if (newOLFrameLength > m_duration) {
    QMessageBox::warning(
        this, tr("Warning"),
        tr("Overlap length cannot be longer than XDTS duration."),
        QMessageBox::Ok);
    newOLFrameLength = m_duration;
    int sec, koma;
    frame2SecKoma(newOLFrameLength, sec, koma);
    m_startOLSecCombo->setCurrentIndex(m_startOLSecCombo->findData(sec));
    m_startOLKomaCombo->setCurrentIndex(m_startOLKomaCombo->findData(koma));
    if (newOLFrameLength == oldOLFrameLength) return;
  }

  MyParams::instance()->setStartOverlapFrameLength(newOLFrameLength);

  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

void SettingsDialog::onEndOLComboChanged(int) {
  int newOLFrameLength = secKoma2Frame(m_endOLSecCombo->currentData().toInt(),
                                       m_endOLKomaCombo->currentData().toInt());
  int oldOLFrameLength = MyParams::instance()->endOverlapFrameLength();

  if (newOLFrameLength == oldOLFrameLength) return;

  // OL����XDTS�̎ڂ𒴂����Ȃ�
  if (newOLFrameLength > m_duration) {
    QMessageBox::warning(
        this, tr("Warning"),
        tr("Overlap length cannot be longer than XDTS duration."),
        QMessageBox::Ok);
    newOLFrameLength = m_duration;
    int sec, koma;
    frame2SecKoma(newOLFrameLength, sec, koma);
    m_endOLSecCombo->setCurrentIndex(m_endOLSecCombo->findData(sec));
    m_endOLKomaCombo->setCurrentIndex(m_endOLKomaCombo->findData(koma));
    if (newOLFrameLength == oldOLFrameLength) return;
  }

  MyParams::instance()->setEndOverlapFrameLength(newOLFrameLength);

  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

void SettingsDialog::onFormatSettingsChanged() {
  MyParams* p = MyParams::instance();
  p->setLogoPath(m_logoImgPathField->text());
  p->setBacksideImgPath(m_backsideImgPathField->text());
  p->setExportArea((ExportArea)m_exportAreaCombo->currentData().toInt());
  p->setSkippedLevelNames(m_skippedLevelNamesEdit->text());
  p->setIsScannedGengaSheet(m_scannedGengaSheetGB->isChecked());
  p->setScannedSheetPageAmount(m_scannedSheetPageAmountEdit->text().toInt());

  // OL������ꍇ�̓e���v���[�g����Čv�Z�i�`��ʒu���ς�邱�Ƃ����邽�߁j
  if (MyParams::instance()->hasOverlap())
    MyParams::instance()->notifyTemplateSwitched();
  else
    MyParams::instance()->notifySomethingChanged();
  MyParams::instance()->setFormatDirty(true);
}

void SettingsDialog::onDenpyoCheckboxClicked(bool on) {
  MyParams* p = MyParams::instance();
  p->setWithDenpyo(on);
  m_backsideImgPathField->setText(p->backsideImgPath(true));
  MyParams::instance()->notifySomethingChanged();
  MyParams::instance()->setFormatDirty(true);
}

void SettingsDialog::onLogoImgBrowserButtonClicked() {
  QStringList filters;
  for (QByteArray& format : QImageReader::supportedImageFormats())
    filters += format;
  QString filterStr = tr("Images") + "(";
  for (auto ext : filters) filterStr += " *." + ext;
  filterStr += ")";
  QString ret =
      QFileDialog::getOpenFileName(this, tr("Specify logo image file"),
                                   MyParams::instance()->logoPath(), filterStr);

  if (ret.isEmpty()) return;

  if (ret.startsWith(PathUtils::getResourceDirPath()))
    ret = QDir(PathUtils::getResourceDirPath()).relativeFilePath(ret);

  MyParams::instance()->setLogoPath(ret);
  m_logoImgPathField->setText(ret);

  MyParams::instance()->notifySomethingChanged();
  MyParams::instance()->setFormatDirty(true);
}

void SettingsDialog::onBacksideImgBrowserButtonClicked() {
  QStringList filters;
  for (QByteArray& format : QImageReader::supportedImageFormats())
    filters += format;
  QString filterStr = tr("Images") + "(";
  for (auto ext : filters) filterStr += " *." + ext;
  filterStr += ")";
  QString ret = QFileDialog::getOpenFileName(
      this, tr("Specify logo image file"),
      MyParams::instance()->backsideImgPath(), filterStr);

  if (ret.isEmpty()) return;

  if (ret.startsWith(PathUtils::getResourceDirPath()))
    ret = QDir(PathUtils::getResourceDirPath()).relativeFilePath(ret);

  MyParams::instance()->setBacksideImgPath(ret);
  m_backsideImgPathField->setText(ret);

  MyParams::instance()->notifySomethingChanged();
  MyParams::instance()->setFormatDirty(true);
}

void SettingsDialog::onSkippedLevelNameChanged() {
  MyParams::instance()->setSkippedLevelNames(m_skippedLevelNamesEdit->text());
  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

//-----------------------------------------------------------------------------

PreferencesDialog::PreferencesDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("Preferences"));

  m_languageCombo = new QComboBox(this);

  m_lineColorButton = new QPushButton(this);
  m_lineColorDialog = new QColorDialog(this);

  // m_templateFontCB = new QFontComboBox(this);
  // m_contentsFontCB = new QFontComboBox(this);

  m_continuousLineCombo = new QComboBox(this);
  m_serialFrameNumberCB =
      new QCheckBox(tr("Put Serial Frame Numbers Over Pages"), this);
  m_levelNameOnBottomCB =
      new QCheckBox(tr("Print Level Names On The Bottom"), this);
  m_capitalizeFirstLetterCB = new QCheckBox(
      tr("Capitalize the first letter of level name (ACTIONS area only)"),
      this);
  m_stampFolderPathField         = new QLineEdit(this);
  m_approvalNameField            = new QLineEdit(this);
  QPushButton* stampBrowseButton = new QPushButton("...", this);

  for (int type = (int)Genga; type < (int)WorkFlowTypeCount; type++) {
    QLineEdit* edit = new QLineEdit(this);
    edit->setProperty("WorkFlowType", type);
    m_suffixEdits.insert(type, edit);
  }

  QPushButton* closeButton = new QPushButton(tr("OK"), this);

  m_languageCombo->addItem("English", "en");
  m_languageCombo->addItem(QString::fromLocal8Bit("���{��"), "ja");

  stampBrowseButton->setFixedSize(20, 20);
  stampBrowseButton->setFocusPolicy(Qt::NoFocus);
  m_lineColorDialog->setOption(QColorDialog::NoButtons, true);
  m_continuousLineCombo->addItem(tr("Always"), Line_Always);
  m_continuousLineCombo->addItem(tr("More Than 3 Continuous Cells"),
                                 Line_MoreThan3s);
  m_continuousLineCombo->addItem(tr("None"), Line_None);

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setMargin(10);
  mainLay->setSpacing(10);
  {
    QGridLayout* viewLay = new QGridLayout();
    viewLay->setMargin(10);
    viewLay->setHorizontalSpacing(5);
    viewLay->setVerticalSpacing(10);
    {
      viewLay->addWidget(new QLabel(tr("Language:"), this), 0, 0,
                         Qt::AlignRight | Qt::AlignVCenter);
      viewLay->addWidget(m_languageCombo, 0, 1,
                         Qt::AlignLeft | Qt::AlignVCenter);

      viewLay->addWidget(new QLabel(tr("Line Color:"), this), 1, 0,
                         Qt::AlignRight | Qt::AlignVCenter);
      viewLay->addWidget(m_lineColorButton, 1, 1,
                         Qt::AlignLeft | Qt::AlignVCenter);

      // viewLay->addWidget(new QLabel(tr("Template Font:"), this), 1, 0,
      //   Qt::AlignRight | Qt::AlignVCenter);
      // viewLay->addWidget(m_templateFontCB, 1, 1, Qt::AlignLeft |
      // Qt::AlignVCenter);
      //
      // viewLay->addWidget(new QLabel(tr("Contents Font:"), this), 2, 0,
      //   Qt::AlignRight | Qt::AlignVCenter);
      // viewLay->addWidget(m_contentsFontCB, 2, 1, Qt::AlignLeft |
      // Qt::AlignVCenter);

      viewLay->addWidget(new QLabel(tr("Continuous Line:"), this), 2, 0,
                         Qt::AlignRight | Qt::AlignVCenter);
      viewLay->addWidget(m_continuousLineCombo, 2, 1,
                         Qt::AlignLeft | Qt::AlignVCenter);

      viewLay->addWidget(m_serialFrameNumberCB, 3, 0, 1, 2,
                         Qt::AlignLeft | Qt::AlignVCenter);
      viewLay->addWidget(m_levelNameOnBottomCB, 4, 0, 1, 2,
                         Qt::AlignLeft | Qt::AlignVCenter);
      viewLay->addWidget(m_capitalizeFirstLetterCB, 5, 0, 1, 2,
                         Qt::AlignLeft | Qt::AlignVCenter);

      viewLay->addWidget(new QLabel(tr("User Stamps Folder:"), this), 6, 0,
                         Qt::AlignRight | Qt::AlignVCenter);
      QHBoxLayout* stampPathLay = new QHBoxLayout();
      stampPathLay->setMargin(0);
      stampPathLay->setSpacing(1);
      {
        stampPathLay->addWidget(m_stampFolderPathField, 1);
        stampPathLay->addWidget(stampBrowseButton, 0);
      }
      viewLay->addLayout(stampPathLay, 6, 1);

      viewLay->addWidget(new QLabel(tr("Approval Stamp Name:"), this), 7, 0,
                         Qt::AlignRight | Qt::AlignVCenter);
      viewLay->addWidget(m_approvalNameField, 7, 1,
                         Qt::AlignLeft | Qt::AlignVCenter);

      QGroupBox* suffixBox   = new QGroupBox(tr("File Suffixes"), this);
      QGridLayout* suffixLay = new QGridLayout();
      suffixLay->setMargin(5);
      suffixLay->setHorizontalSpacing(5);
      suffixLay->setVerticalSpacing(10);
      {
        suffixLay->addWidget(new QLabel(tr("LO:"), this), 0, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
        suffixLay->addWidget(m_suffixEdits[(int)LO], 0, 1);
        suffixLay->addWidget(new QLabel(tr("RoughGen:"), this), 0, 2,
                             Qt::AlignRight | Qt::AlignVCenter);
        suffixLay->addWidget(m_suffixEdits[(int)RoughGen], 0, 3);
        suffixLay->addWidget(new QLabel(tr("Genga:"), this), 1, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
        suffixLay->addWidget(m_suffixEdits[(int)Genga], 1, 1);
        suffixLay->addWidget(new QLabel(tr("Douga:"), this), 1, 2,
                             Qt::AlignRight | Qt::AlignVCenter);
        suffixLay->addWidget(m_suffixEdits[(int)Douga], 1, 3);
      }
      suffixLay->setColumnStretch(0, 0);
      suffixLay->setColumnStretch(1, 1);
      suffixLay->setColumnStretch(2, 0);
      suffixLay->setColumnStretch(3, 1);
      suffixBox->setLayout(suffixLay);
      viewLay->addWidget(suffixBox, 8, 0, 1, 2);
    }
    viewLay->setColumnStretch(0, 0);
    viewLay->setColumnStretch(1, 1);
    mainLay->addLayout(viewLay, 0);

    mainLay->addWidget(closeButton, 0, Qt::AlignCenter);
  }
  setLayout(mainLay);

  connect(m_languageCombo, SIGNAL(activated(int)), this,
          SLOT(onLanguageSwitched()));
  connect(m_lineColorButton, SIGNAL(clicked()), m_lineColorDialog,
          SLOT(show()));
  connect(m_lineColorDialog, SIGNAL(currentColorChanged(const QColor&)), this,
          SLOT(onLineColorChanged(const QColor&)));
  // connect(m_lineColorDialog, SIGNAL(colorSelected(const QColor&)), this,
  // SLOT(updateLineColorButton())); connect(m_templateFontCB,
  // SIGNAL(activated(int)), this, SLOT(onViewPreferencesChanged()));
  // connect(m_contentsFontCB, SIGNAL(activated(int)), this,
  // SLOT(onViewPreferencesChanged()));
  connect(m_continuousLineCombo, SIGNAL(activated(int)), this,
          SLOT(onViewPreferencesChanged()));
  connect(m_serialFrameNumberCB, SIGNAL(clicked(bool)), this,
          SLOT(onViewPreferencesChanged()));
  connect(m_levelNameOnBottomCB, SIGNAL(clicked(bool)), this,
          SLOT(onViewPreferencesChanged()));
  connect(m_capitalizeFirstLetterCB, SIGNAL(clicked(bool)), this,
          SLOT(onCapitalizeFirstLetterSwitched()));
  connect(m_stampFolderPathField, SIGNAL(editingFinished()), this,
          SLOT(onStampPathChanged()));
  connect(m_approvalNameField, SIGNAL(editingFinished()), this,
          SLOT(onApprovalNameChanged()));
  connect(stampBrowseButton, SIGNAL(clicked()), this,
          SLOT(onStampBrowserButtonClicked()));
  for (auto edit : m_suffixEdits.values())
    connect(edit, SIGNAL(editingFinished()), this, SLOT(onSuffixEdited()));
  connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
}

void PreferencesDialog::syncUIs() {
  MyParams* p = MyParams::instance();

  m_languageCombo->setCurrentIndex(m_languageCombo->findData(p->language()));

  updateLineColorButton();
  // m_templateFontCB->setCurrentFont(QFont(p->templateFont()));
  // m_contentsFontCB->setCurrentFont(QFont(p->contentsFont()));
  m_continuousLineCombo->setCurrentIndex(
      m_continuousLineCombo->findData(p->continuousLineMode()));

  m_serialFrameNumberCB->setChecked(p->isSerialFrameNumber());
  m_levelNameOnBottomCB->setChecked(p->isLevelNameOnBottom());
  m_capitalizeFirstLetterCB->setChecked(p->isCapitalizeFirstLetter());
  m_stampFolderPathField->setText(p->userStampFolderPath());
  m_approvalNameField->setText(p->approvalName());

  for (auto type : m_suffixEdits.keys()) {
    m_suffixEdits.value(type)->setText(p->suffix((WorkFlowType)type));
  }
}

void PreferencesDialog::updateLineColorButton() {
  QColor color     = MyParams::instance()->lineColor();
  QString txtColor = (color.valueF() > 0.5) ? "black" : "white";
  m_lineColorButton->setText(color.name());
  m_lineColorButton->setStyleSheet(QString("background-color:%1; color:%2;")
                                       .arg(color.name())
                                       .arg(txtColor));
  m_lineColorDialog->setCurrentColor(color);
}

void PreferencesDialog::onLanguageSwitched() {
  QString newLangId = m_languageCombo->currentData().toString();
  MyParams* p       = MyParams::instance();
  if (newLangId == p->language()) return;
  QMessageBox::information(
      this, tr("Language has been changed."),
      tr("Changes will take effect the next time you run XDTS Viewer."));
  p->setLanguage(
      m_languageCombo->currentData().toString());  // en / ja ����������
}

void PreferencesDialog::onViewPreferencesChanged() {
  MyParams* p = MyParams::instance();
  // p->setTemplateFont(m_templateFontCB->currentFont().family());
  // p->setContentsFont(m_contentsFontCB->currentFont().family());
  p->setContinuousLineMode(
      (ContinuousLineMode)m_continuousLineCombo->currentData().toInt());
  p->setIsSerialFrameNumber(m_serialFrameNumberCB->isChecked());
  p->setIsLevelNameOnBottom(m_levelNameOnBottomCB->isChecked());
  p->notifySomethingChanged();
}

void PreferencesDialog::onCapitalizeFirstLetterSwitched() {
  MyParams::instance()->setIsCapitalizeFirstLetter(
      m_capitalizeFirstLetterCB->isChecked());
  MyParams::instance()->notifyTemplateSwitched();
}

void PreferencesDialog::onStampBrowserButtonClicked() {
  QString initialFolder = MyParams::instance()->userStampFolderPath();

  QString ret = QFileDialog::getExistingDirectory(
      this, tr("Specify User Stamp Folder"),
      (initialFolder.isEmpty()) ? "/home" : initialFolder,
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (ret == initialFolder) return;

  m_stampFolderPathField->setText(ret);
  onStampPathChanged();
}

void PreferencesDialog::onStampPathChanged() {
  QString initialFolder = MyParams::instance()->userStampFolderPath();
  if (m_stampFolderPathField->text() == initialFolder) return;
  MyParams::instance()->setUserStampFolderPath(m_stampFolderPathField->text());
  MyParams::instance()->setFormatDirty(true);
  QMessageBox::information(this, tr("Stamp Folder Has Specified."),
                           tr("The Change Will Be Applied When You Launch the "
                              "Application Next Time."));
}

void PreferencesDialog::onApprovalNameChanged() {
  QString initialName = MyParams::instance()->approvalName();
  if (m_approvalNameField->text() == initialName) return;
  MyParams::instance()->setApprovalName(m_approvalNameField->text());
  MyParams::instance()->setFormatDirty(true);
  QMessageBox::information(this, tr("Approval Stamp Name Has Specified."),
                           tr("The Change Will Be Applied When You Launch the "
                              "Application Next Time."));
}

void PreferencesDialog::onLineColorChanged(const QColor& color) {
  MyParams::instance()->setLineColor(color);
  updateLineColorButton();
  MyParams::instance()->notifySomethingChanged();
}

void PreferencesDialog::onSuffixEdited() {
  QLineEdit* edit = qobject_cast<QLineEdit*>(sender());
  if (!edit) return;

  WorkFlowType type = (WorkFlowType)edit->property("WorkFlowType").toInt();

  QString newSuffix = edit->text();
  if (newSuffix.isEmpty()) {
    edit->setText(MyParams::instance()->suffix(type));
    return;
  }

  if (newSuffix == MyParams::instance()->suffix(type)) return;

  MyParams::instance()->setSuffix(type, newSuffix);
}

//-----------------------------------------------------------------------------

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
  setFixedSize(500, 500);
  setWindowTitle(tr("About %1").arg(qApp->applicationName()));

  QString softwareStr =
      qApp->applicationName() + "  Version " + qApp->applicationVersion();
  QString dayTimeStr = tr("Built %1").arg(__DATE__);
  QString copyrightStr =
      "All contributions by Shun Iwasawa :\n"
      "Copyright(c) Shun Iwasawa  All rights reserved.\n"
      "All other contributions : \n"
      "Copyright(c) the respective contributors.  All rights reserved.\n\n"
      "Each contributor holds copyright over their respective "
      "contributions.\n"
      "The project versioning(Git) records all such contribution source "
      "information.";
  QLabel* copyrightLabel = new QLabel(copyrightStr, this);
  copyrightLabel->setAlignment(Qt::AlignLeft);

  QTextEdit* licenseField = new QTextEdit(this);
  licenseField->setLineWrapMode(QTextEdit::NoWrap);
  licenseField->setReadOnly(true);
  QStringList licenseFiles;
  licenseFiles << "LICENSE_opentoonz.txt"
               << "LICENSE_qt.txt";

  for (const QString& fileName : licenseFiles) {
    QString licenseTxt;
    licenseTxt.append("[ " + fileName + " ]\n\n");

    QFile file(PathUtils::getLicenseFolderPath() + "/" + fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

    QTextStream in(&file);
    while (!in.atEnd()) {
      QString line = in.readLine();
      licenseTxt.append(line);
      licenseTxt.append("\n");
    }

    licenseTxt.append("\n-------------------------\n");

    licenseField->append(licenseTxt);
  }
  licenseField->moveCursor(QTextCursor::Start);

  QVBoxLayout* lay = new QVBoxLayout();
  lay->setMargin(15);
  lay->setSpacing(3);
  {
    lay->addSpacing(120);

    lay->addWidget(new QLabel(softwareStr, this), 0, Qt::AlignLeft);
    lay->addWidget(new QLabel(dayTimeStr, this), 0, Qt::AlignLeft);

    lay->addSpacing(10);

    lay->addWidget(copyrightLabel, 0, Qt::AlignLeft);

    lay->addSpacing(10);

    lay->addWidget(new QLabel(tr("Licenses"), this), 0, Qt::AlignLeft);
    lay->addWidget(licenseField, 1);
  }
  setLayout(lay);
}

void AboutDialog::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.drawPixmap(128, 50, QPixmap(":Resources/logo.png"));
}
