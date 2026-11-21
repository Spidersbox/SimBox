#include "optionsform.h"
#include "ui_optionsform.h"

OptionsForm::OptionsForm(QWidget *parent)
    : QDialog(parent), ui(new Ui::OptionsForm)
{
  ui->setupUi(this);
  connect(ui->btnRandomizeNow, &QPushButton::clicked,
          this, &OptionsForm::randomizeNowRequested);
}

OptionsForm::~OptionsForm() { delete ui; }

void OptionsForm::setValues(bool tailsDot, bool tailsBar, bool peakDot, bool peakBar,
                            bool loop, bool randLoop, int intervalMs)
{
  ui->chkTailsDot->setChecked(tailsDot);
  ui->chkTailsBar->setChecked(tailsBar);
  ui->chkPeakDot->setChecked(peakDot);
  ui->chkPeakBar->setChecked(peakBar);
  ui->chkLoop->setChecked(loop);
  ui->chkRandomLoop->setChecked(randLoop);
  ui->spinLedSpeed->setValue(intervalMs);
}

bool OptionsForm::tailsDot() const      { return ui->chkTailsDot->isChecked(); }
bool OptionsForm::tailsBar() const      { return ui->chkTailsBar->isChecked(); }
bool OptionsForm::peakDot() const       { return ui->chkPeakDot->isChecked(); }
bool OptionsForm::peakBar() const       { return ui->chkPeakBar->isChecked(); }
bool OptionsForm::loopEnabled() const   { return ui->chkLoop->isChecked(); }
bool OptionsForm::randomLoop() const    { return ui->chkRandomLoop->isChecked(); }
int OptionsForm::ledIntervalMs() const  { return ui->spinLedSpeed->value(); }