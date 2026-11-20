#ifndef SIMBOXMAIN_H
#define SIMBOXMAIN_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QVector>
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QContextMenuEvent>
#include <QMediaPlayer>
#include <QTimer>

#include <QAudioProbe>
#include <QAudioBuffer>
#include <QVector>

class SimboxMain : public QWidget
{
  Q_OBJECT

public:
  explicit SimboxMain(const QString &skinPath, QWidget *parent = nullptr);
  QStringList playlist;

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onSongSlotClicked(int slotIndex);
  void onSelectionButtonToggled(bool checked);
  void printData(QStringList);
  void togglePlayerDisplay(bool visible);
  void saveSettings();

  void openFilesClicked();
  void loadPlaylistClicked();
  void clearPlaylistClicked();
  void savePlaylistClicked();
  void editPlaylistClicked();
  void selectAllClicked();
  void deselectAllClicked();

  void playClicked();
  void pauseClicked();
  void stopClicked();
  void prevClicked();
  void nextClicked();
  void onDurationChanged(qint64 duration);
  void onPositionChanged(qint64 position);
  void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
  void onMarqueeTimer();
  void prevPageClicked();
  void nextPageClicked();

  void onAudioBufferProbed(const QAudioBuffer &buffer);
  void onLedDecayTimer();

private:
  struct PlaylistItem
  {
    QString filePath;
    bool selected = true; // Replaces song[i]
  };

  QMenu *mainMenu;
  QMenu *playControlMenu;
  QMenu *displayMenu;

  QAction *openFileAction;
  QAction *loadPlAction;
  QAction *clearPlAction;
  QAction *savePlAction;
  QAction *editPlAction;
  QAction *selectAllAction;
  QAction *deselectAllAction;
  QAction *quitAction;
  QAction *playerDisplayAction;
  QAction *vuBarAction;
  QAction *vuDotAction;
  QAction *vuOffAction;

  QWidget *controlPanel;

  QLabel *Label_Marquee;
  QLabel *Label_Elapsed;
  QLabel *Label_Total;

  QPushButton *m_prevPageBtn = nullptr;
  QPushButton *m_nextPageBtn = nullptr;
  QPushButton *playButton;
  QPushButton *pauseButton;
  QPushButton *stopButton;
  QPushButton *previousButton;
  QPushButton *nextButton;
  QMediaPlayer *m_player = nullptr;
  QTimer *m_marqueeTimer = nullptr;
  QAudioProbe *m_audioProbe = nullptr;
  QTimer *m_ledTimer = nullptr;

  QPixmap m_skin;
  QPoint m_dragOffset;
  QString m_marqueeText;
  int m_currentSong = -1;
  int m_page=0;
  int m_leftLevel = 0;   // 0..13 active LED count
  int m_rightLevel = 0;  // 0..13 active LED count
  double m_leftAvg = 0.0;
  double m_rightAvg = 0.0;
  bool m_isDragging = false;

  QVector<PlaylistItem> m_playlist; // Grows dynamically to any size
  QVector<QPushButton*> m_songSlots;
  QVector<QPushButton*> m_selButtons;
  QVector<bool> m_selected;
  QVector<QPoint> m_leftLedCoords;   // 13 LEDs along the left side of the arch
  QVector<QPoint> m_rightLedCoords;  // 13 LEDs along the right side of the arc

  QIcon createSlicedButtonIcon(const QRect &rect);

  QString formatTime(qint64 ms);

  void createActions();
  void createMenus();
  void createToolBars();
  void setupSongSlots();
  void updateSlotStyle(int slotIndex);
  void loadPlaylist(const QString &m3uPath);
  void setupSelectionButtons();
  void updatePageDisplay();
  void updateSlotLabelStyle(int slotIndex, bool selected);
  void updateLabels();
  void contextMenuEvent(QContextMenuEvent *event);
  void showMainMenu(const QPoint &globalPos);
  void savePlaylist(const QString &m3uPath);
  void startPlayer(int index);
  void setupPageButtons();
  void setupLeds();
  void loadSettings();

};

#endif // SIMBOXMAIN_H