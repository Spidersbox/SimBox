#ifndef OPTIONSFORM_H
#define OPTIONSFORM_H

#include <QDialog>

namespace Ui { class OptionsForm; }

class OptionsForm : public QDialog
{
  Q_OBJECT

public:
  explicit OptionsForm(QWidget *parent = nullptr);
  ~OptionsForm();

  void setValues(bool tailsDot, bool tailsBar, bool peakDot, bool peakBar,
                 bool loop, bool randLoop, int intervalMs);

  bool tailsDot() const;
  bool tailsBar() const;
  bool peakDot() const;
  bool peakBar() const;
  bool loopEnabled() const;
  bool randomLoop() const;
  int ledIntervalMs() const;

signals:
  void randomizeNowRequested();

private:
  Ui::OptionsForm *ui;
};

#endif // OPTIONSFORM_H
