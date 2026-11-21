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
#include <QInputDialog>

#include <QAudioProbe>
#include <QAudioBuffer>
#include <QQueue>
#include <QVector>

class SimboxMain : public QWidget
{
  Q_OBJECT

public:
  explicit SimboxMain(const QString &skinPath, QWidget *parent = nullptr);
  QStringList playlist;
  inline static QString SongDir;     // dir of open file
  inline static QString PlaylistDir; // dir of open playlist

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
  void moreOptionsClicked();
  void onLedDecayTimer();
  void timerSpeedClicked();

private:
  struct PlaylistItem
  {
    QString filePath;
    bool selected = true; // Replaces song[i]
  };

  struct TimedPeak
  {
    qint64 timestampMs;
    double left;
    double right;
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
  QAction *vuTimerAction = nullptr;
  QAction *moreOptionsAction;

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

  QQueue<TimedPeak> m_peakQueue;
  QPixmap m_skin;
  QPoint m_dragOffset;
  QString m_marqueeText;

  // More Options settings
  bool m_tailsDot    = true;
  bool m_tailsBar    = false;
  bool m_peakHoldDot = false;
  bool m_peakHoldBar = false;
  bool m_loopEnabled = true;
  bool m_randomLoop  = false;

  // Peak-hold tracking (0.0 .. 13.0 LED index + hold countdown ticks)
  double m_peakHoldL = 0.0;
  double m_peakHoldR = 0.0;
  int m_peakPauseL   = 0;
  int m_peakPauseR   = 0;

  int m_ledSyncDelayMs = 0;
  int m_currentSong = -1;
  int m_page=0;
  int m_leftLevel = 0;   // 0..13 active LED count
  int m_rightLevel = 0;  // 0..13 active LED count
  int m_ledIntervalMs = 20; // Matches original Sleeper = 20 ms

  double m_rawPeakL = 0.0;
  double m_rawPeakR = 0.0;  double m_leftAvg = 0.0;
  double m_rightAvg = 0.0;
  double m_leftLedAlpha[13]  = {0.0};
  double m_rightLedAlpha[13] = {0.0};
  double m_ledFadeFactor     =  0.82; // 0.0 = instant off, 0.82 = smooth trail, 0.92 = long tail
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
  void savePlaylist(const QString &m3uPath);
  void startPlayer(int index);
  void setupPageButtons();
  void setupLeds();
  void loadSettings();
  void randomizePlaylist();
  void playNextTrack(bool autoAdvance);

};

#endif // SIMBOXMAIN_H