#include <QMenu>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QFileInfo>
#include <QFileDialog>
#include <QSignalBlocker>
#include <QDir>
#include <QPainter>
#include <QImage>
#include <QIcon>
#include <QBitmap>
#include <QSet>
#include <QSettings>
#include <algorithm>
#include <QRandomGenerator>

#if QT_VERSION < 0x060000
  #include <QDesktopWidget>
#else
  #include <QScreen>
#endif

#include <QDebug>


#include "optionsform.h"
#include "editform.h"
#include "SimboxMain.h"

//-----------------------------------------------------------------------------------------
SimboxMain::SimboxMain(const QString &skinPath, QWidget *parent)
    : QWidget(parent), m_skin(skinPath)
{
  // 1. Strip OS window borders and enable per-pixel alpha transparency
  setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
  setAttribute(Qt::WA_TranslucentBackground);

  // 2. Size the window to match your jukebox PNG (fallback if image isn't found yet)
  if (!m_skin.isNull())
  {
    setFixedSize(m_skin.size());
    setMask(m_skin.mask());
  }
  else
  {
   setFixedSize(430, 420);
  }


 // center form on screen
#if QT_VERSION < 0x060000
  QRect desktopRect = QApplication::desktop()->availableGeometry(this);
  QPoint center = desktopRect.center();
  move(center.x() - width() * 0.5, center.y() - height() * 0.5);
#else
  QScreen *screen = QGuiApplication::primaryScreen();
  QRect  screenGeometry = screen->geometry();
  int height = screenGeometry.height()/2;
  int width = screenGeometry.width()/2;
  move(width-340,height-285);
#endif

#ifndef Q_OS_MAC
    qApp->setWindowIcon(QIcon(":images/icon"));
    setWindowIcon(QIcon(":images/icon"));
#else
    setUnifiedTitleAndToolBarOnMac(true);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif


  m_selected.resize(20);
  m_selected.fill(false);

  m_player = new QMediaPlayer(this);
  m_marqueeTimer = new QTimer(this);
  setupLeds();

  createToolBars();
  createActions();
  createMenus();

  setupSelectionButtons();
  setupSongSlots();
  setupPageButtons();

  // load settings from ini file
  loadSettings();

  QStringList args = QCoreApplication::arguments();
  if (args.size() > 1)
  {
    QFileInfo fi(args[1].trimmed());
    QString ext = fi.suffix().toLower();
    if (fi.exists() && (ext == "m3u" || ext == "sel"))
    {
      loadPlaylist(fi.absoluteFilePath()); // Your existing .m3u + .sel loader
      m_page = 0;
      updateLabels();

      // Find the first selected track and start playing
      for (int i = 0; i < m_playlist.size(); ++i)
      {
        if (m_playlist[i].selected)
        {
          startPlayer(i);
          break;
        }
      }
    }
  }

  updateLabels();

}

//--------------------------------------------------------------------------------------
void SimboxMain::saveSettings()
{
  QSettings settings("SimBox", "settings");
  settings.beginGroup("MainWindow");

  if (settings.contains("pos"))
  {
    QPoint savedPos = settings.value("pos").toPoint();
    if (QGuiApplication::screenAt(savedPos + QPoint(50, 50)))
    {
      move(savedPos);
    }
  }

  PlaylistDir = settings.value("Playlist Dir", QDir::homePath()).toString();
  SongDir     = settings.value("Song Dir", QDir::homePath()).toString();
  settings.endGroup();

  settings.beginGroup("VU Display");
  settings.setValue("tailsDot",      m_tailsDot);
  settings.setValue("tailsBar",      m_tailsBar);
  settings.setValue("peakHoldDot",   m_peakHoldDot);
  settings.setValue("peakHoldBar",   m_peakHoldBar);
  settings.setValue("loopEnabled",   m_loopEnabled);
  settings.setValue("randomLoop",    m_randomLoop);
  settings.setValue("ledIntervalMs", m_ledIntervalMs);
  settings.endGroup();
}

//--------------------------------------------------------------------------------------
void SimboxMain::loadSettings()
{
  QSettings settings("SimBox", "settings");
  settings.beginGroup("MainWindow");

  if (settings.contains("pos"))
  {
    QPoint savedPos = settings.value("pos").toPoint();
    // Only restore if that coordinate is still on an active monitor
    if (QGuiApplication::screenAt(savedPos + QPoint(50, 50)))
    {
      move(savedPos);
    }
  }

  if (settings.contains("Playlist Dir"))
  {
    PlaylistDir = settings.value("Playlist Dir").toString();
  }

  if (settings.contains("Song Dir"))
  {
    SongDir = settings.value("Song Dir").toString();
  }
  settings.endGroup();

  settings.beginGroup("VU Display");
  m_tailsDot      = settings.value("tailsDot", true).toBool();
  m_tailsBar      = settings.value("tailsBar", false).toBool();
  m_peakHoldDot   = settings.value("peakHoldDot", false).toBool();
  m_peakHoldBar   = settings.value("peakHoldBar", false).toBool();
  m_loopEnabled   = settings.value("loopEnabled", true).toBool();
  m_randomLoop    = settings.value("randomLoop", false).toBool();
  m_ledIntervalMs = settings.value("ledIntervalMs", 20).toInt();
  if (m_ledTimer) m_ledTimer->start(m_ledIntervalMs);
  settings.endGroup();
}

//-----------------------------------------------------------------------------------------
bool SimboxMain::eventFilter(QObject *obj, QEvent *event)
{
  if (event->type() == QEvent::MouseButtonDblClick)
  {
    QPushButton *slot = qobject_cast<QPushButton*>(obj);
    if (slot && slot->property("slotIndex").isValid())
    {
      int offset = (m_page * 20) + slot->property("slotIndex").toInt();
      if (offset < m_playlist.size())
      {
        startPlayer(offset);
      }
      else
      {
        openFilesClicked(); // Double-click a blank card to add tracks
      }
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

//-----------------------------------------------------------------------------------------
void SimboxMain::createActions()
{
  openFileAction = new QAction(tr("&Open File..."), this);
  loadPlAction = new QAction(tr("&Load"), this);
  clearPlAction = new QAction(tr("&Clear"), this);
  savePlAction = new QAction(tr("&Save..."), this);
  editPlAction = new QAction(tr("&Edit..."), this);
  selectAllAction = new QAction(tr("Select &All"), this);
  deselectAllAction = new QAction(tr("&Deselect All"), this);
  quitAction = new QAction(tr("E&xit"), this);
  vuTimerAction = new QAction(tr("&Timer..."), this);
  moreOptionsAction = new QAction(tr("&More Options..."), this);

  playerDisplayAction = new QAction(tr("&Display"), this);
  playerDisplayAction->setCheckable(true);
  playerDisplayAction->setChecked(false);

  connect(playerDisplayAction, SIGNAL(toggled(bool)), this, SLOT(togglePlayerDisplay(bool)));
  connect(playButton,     SIGNAL(clicked()), this, SLOT(playClicked()));
  connect(pauseButton,    SIGNAL(clicked()), this, SLOT(pauseClicked()));
  connect(stopButton,     SIGNAL(clicked()), this, SLOT(stopClicked()));
  connect(previousButton, SIGNAL(clicked()), this, SLOT(prevClicked()));
  connect(nextButton,     SIGNAL(clicked()), this, SLOT(nextClicked()));

  connect(m_player, SIGNAL(durationChanged(qint64)), this, SLOT(onDurationChanged(qint64)));
  connect(m_player, SIGNAL(positionChanged(qint64)), this, SLOT(onPositionChanged(qint64)));
  connect(m_player, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)),this,
                    SLOT(onMediaStatusChanged(QMediaPlayer::MediaStatus)));
  connect(m_marqueeTimer, SIGNAL(timeout()), this, SLOT(onMarqueeTimer()));

  vuBarAction = new QAction(tr("&Bar"), this);
  vuDotAction = new QAction(tr("&Dot"), this);
  vuOffAction = new QAction(tr("Display &OFF"), this);
  vuBarAction->setCheckable(true);
  vuDotAction->setCheckable(true);
  vuOffAction->setCheckable(true);
  vuBarAction->setChecked(true);

  connect(vuTimerAction, SIGNAL(triggered()), this, SLOT(timerSpeedClicked()));
  connect(openFileAction, SIGNAL(triggered()), this, SLOT(openFilesClicked()));
  connect(loadPlAction, SIGNAL(triggered()), this, SLOT(loadPlaylistClicked()));
  connect(clearPlAction, SIGNAL(triggered()), this, SLOT(clearPlaylistClicked()));
  connect(savePlAction, SIGNAL(triggered()), this, SLOT(savePlaylistClicked()));
  connect(editPlAction, SIGNAL(triggered()), this, SLOT(editPlaylistClicked()));
  connect(selectAllAction, SIGNAL(triggered()), this, SLOT(selectAllClicked()));
  connect(deselectAllAction, SIGNAL(triggered()), this, SLOT(deselectAllClicked()));
  connect(moreOptionsAction, SIGNAL(triggered()), this, SLOT(moreOptionsClicked()));
  connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
  connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(saveSettings()));
}

//-----------------------------------------------------------------------------------------
void SimboxMain::createMenus()
{
  mainMenu = new QMenu(this);
  mainMenu->addAction(openFileAction);

  QMenu *plSubMenu = mainMenu->addMenu(tr("&Play List"));
  plSubMenu->addAction(loadPlAction);
  plSubMenu->addAction(clearPlAction);
  plSubMenu->addAction(savePlAction);
  plSubMenu->addAction(editPlAction);

  mainMenu->addAction(selectAllAction);
  mainMenu->addAction(deselectAllAction);
  mainMenu->addSeparator();
  mainMenu->addAction(quitAction);

 // 2. Bottom Player Controls Menu (over the lower grille)
  playControlMenu = new QMenu(this);
  playControlMenu->addAction(playerDisplayAction);
  playControlMenu->addSeparator();
  playControlMenu->addAction(quitAction);

  // 3. LED VU Meter Menu (over the left/right vertical LED columns)
  displayMenu = new QMenu(this);
  QActionGroup *vuGroup = new QActionGroup(this);
  vuGroup->addAction(vuBarAction);
  vuGroup->addAction(vuDotAction);
  vuGroup->addAction(vuOffAction);

  displayMenu->addAction(vuBarAction);
  displayMenu->addAction(vuDotAction);
  displayMenu->addAction(vuOffAction);
  displayMenu->addSeparator();
  displayMenu->addAction(vuTimerAction);
  displayMenu->addSeparator();
  displayMenu->addAction(moreOptionsAction);
}

//-----------------------------------------------------------------------------------------
void SimboxMain::createToolBars()
{
  // 1. Container panel placed over the bottom grille
  controlPanel = new QWidget(this);
  controlPanel->setGeometry(85, 340, 260, 90);// (x, y, w, h)
  controlPanel->setStyleSheet("background: transparent;");
//  controlPanel->setStyleSheet("background-color: rgba(200,200,200,50);");
  controlPanel->setVisible(false);

  // 2. Time & Marquee labels (coordinates relative to controlPanel)
  Label_Elapsed = new QLabel("00:00", controlPanel);
  Label_Elapsed->setGeometry(10, 4, 45, 14);
  Label_Elapsed->setStyleSheet("color: #ffff00; font-size: 10px; font-weight: bold;");

  Label_Total = new QLabel("00:00", controlPanel);
  Label_Total->setGeometry(205, 4, 45, 14);
  Label_Total->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  Label_Total->setStyleSheet("color: #ffff00; font-size: 10px; font-weight: bold;");

  Label_Marquee = new QLabel("Song Title", controlPanel);
  Label_Marquee->setGeometry(10, 20, 240, 30);
  Label_Marquee->setAlignment(Qt::AlignCenter);
  Label_Marquee->setStyleSheet("color: #ffff00; font-size: 20px; font-weight: bold;");

  // 3. Transport buttons (coordinates relative to controlPanel)
  previousButton = new QPushButton(controlPanel);
  previousButton->setGeometry(36, 55, 32, 32);
  previousButton->setStyleSheet("QPushButton { border-image: url(:/images/bt_previous); }"
                                "QPushButton:hover { border-image: url(:/images/bt_previous_hover); }");

  playButton = new QPushButton(controlPanel);
  playButton->setGeometry(76, 55, 32, 32);
  playButton->setStyleSheet("QPushButton { border-image: url(:/images/bt_play); }"
                            "QPushButton:hover { border-image: url(:/images/bt_play_hover); }");

  pauseButton = new QPushButton(controlPanel);
  pauseButton->setGeometry(116, 55, 32, 32);
  pauseButton->setStyleSheet("QPushButton { border-image: url(:/images/bt_pause); }"
                             "QPushButton:hover { border-image: url(:/images/bt_pause_hover); }");

  stopButton = new QPushButton(controlPanel);
  stopButton->setGeometry(156, 55, 32, 32);
  stopButton->setStyleSheet("QPushButton { border-image: url(:/images/bt_stop); }"
                            "QPushButton:hover { border-image: url(:/images/bt_stop_hover); }");

  nextButton = new QPushButton(controlPanel);
  nextButton->setGeometry(196, 55, 32, 32);
  nextButton->setStyleSheet("QPushButton { border-image: url(:/images/bt_next); }"
                            "QPushButton:hover { border-image: url(:/images/bt_next_hover); }");
}

//-----------------------------------------------------------------------------------------
void SimboxMain::setupSongSlots()
{
  // Exact Y-coordinates from your original BCB3 SimBoxMain.cpp
  const int yCoords[10] = { 120, 136, 160, 176, 196, 212, 236, 248, 272, 288 };

  for (int i = 0; i < 20; ++i)
  {
    QPushButton *btn = new QPushButton(this);
    int x = (i < 10) ? 96 : 225;
    int y = yCoords[i % 10];

    btn->setGeometry(x, y, 109, 14);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setText(QString("Track %1").arg(i + 1));

    // Tag each card with its 0..19 slot index and route double-clicks to eventFilter
    btn->setProperty("slotIndex", i);
    btn->installEventFilter(this);
    btn->setContextMenuPolicy(Qt::NoContextMenu);

    m_songSlots.append(btn);
    updateSlotStyle(i);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::onSongSlotClicked(int slotIndex)
{
  // Mirrors your SpeedButton1..20 toggle logic
  m_selected[slotIndex] = !m_selected[slotIndex];
  updateSlotStyle(slotIndex);
}

//-----------------------------------------------------------------------------------------
void SimboxMain::updateSlotStyle(int slotIndex)
{
  QPushButton *slot = m_songSlots[slotIndex];
  int offset = (m_page * 20) + slotIndex;
  bool isPlaying = (offset == m_currentSong && offset < m_playlist.size());

  // Warm pale-yellow highlight on the currently playing card, transparent otherwise
  QString bgRule = isPlaying ? "background-color: rgba(231, 253, 142, 210);"
                             : "background-color: transparent;";

  if (m_selected[slotIndex])
  {
    QPixmap dot(8, 8);
    dot.fill(Qt::transparent);
    QPainter p(&dot);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(0, 255, 0));
    p.setPen(QPen(QColor(0, 120, 0), 1));
    p.drawEllipse(1, 1, 6, 6);
    p.end();

    slot->setIcon(QIcon(dot));
    slot->setIconSize(QSize(8, 8));
    slot->setStyleSheet(
        "QPushButton {" + bgRule +
        "  color: black; font-family: Arial; font-size: 8pt; font-weight: bold; text-decoration: underline;"
        "  border: none; text-align: left; padding-left: 2px;"
        "}"
    );
  }
  else
  {
    slot->setIcon(QIcon());
    slot->setStyleSheet(
        "QPushButton {" + bgRule +
        "  color: black; font-family: Arial; font-size: 8pt;"
        "  border: none; text-align: left; padding-left: 10px;"
        "}"
    );
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
  {
    m_isDragging = true;
    m_dragOffset = event->globalPos() - frameGeometry().topLeft();
    setCursor(Qt::ClosedHandCursor);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::mouseMoveEvent(QMouseEvent *event)
{
  if (m_isDragging && (event->buttons() & Qt::LeftButton))
  {
    move(event->globalPos() - m_dragOffset);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
  {
    m_isDragging = false;
    setCursor(Qt::ArrowCursor);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::contextMenuEvent(QContextMenuEvent *event)
{
  int y = event->pos().y();

  if (y >= 320)
  {
    // Bottom grille -> Player Display menu
    playControlMenu->exec(event->globalPos());
  }
  else if (y >= 55 && y < 70)
  {
    // Lower half of the arch (where the LEDs sit, below the page buttons) -> LED Display menu
    displayMenu->exec(event->globalPos());
  }
  else
  {
    // Top peak of the arch (page buttons) & middle card area -> Main menu
    mainMenu->exec(event->globalPos());
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::loadPlaylist(const QString &m3uPath)
{
  m_playlist.clear();
  m_page = 0;

  // 1. Load unlimited lines from .m3u
  QFile m3uFile(m3uPath);
  if (!m3uFile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

  QTextStream in(&m3uFile);
  while (!in.atEnd())
  {
    QString line = in.readLine().trimmed();
    if (!line.isEmpty() && !line.startsWith('#'))
    {
      m_playlist.append({line, true});
    }
  }

  // 2. Load matching .sel file of any length
  QString selPath = QFileInfo(m3uPath).path() + "/" + QFileInfo(m3uPath).completeBaseName() + ".sel";
  QFile selFile(selPath);
  if (selFile.open(QIODevice::ReadOnly))
  {
    QByteArray selData = selFile.readAll();
    int limit = qMin(m_playlist.size(), selData.size());
    for (int i = 0; i < limit; ++i)
    {
      m_playlist[i].selected = (selData.at(i) == 1);
    }
  }
}

//-----------------------------------------------------------------------------------------
QIcon SimboxMain::createSlicedButtonIcon(const QRect &rect)
{
  // 1. Slice the raw button patch directly from the loaded jukebox skin
  QImage baseImg = m_skin.copy(rect).toImage().convertToFormat(QImage::Format_ARGB32);

  QImage outImg = baseImg.copy();
  QImage inImg(rect.size(), QImage::Format_ARGB32);
  inImg.fill(QColor(15, 15, 15)); // Dark inner recess shadow at top/left edge

/*
// Temporary alignment outline drawn directly onto the sliced pixmap
QPainter outline(&outImg);
outline.setPen(QColor(255, 0, 255)); // Bright magenta outline
outline.drawRect(0, 0, outImg.width() - 1, outImg.height() - 1);
outline.end();
*/

  // 2. Make "Out" (unselected) lighter & "In" (selected) darker
  for (int y = 0; y < baseImg.height(); ++y)
  {
    for (int x = 0; x < baseImg.width(); ++x)
    {
      QColor c = baseImg.pixelColor(x, y);
      outImg.setPixelColor(x, y, c.lighter(135)); // 35% brighter when popped out
    }
  }

  // Shift the darkened "In" face by (1, 1) px to give a tactile pressed-in feel
  QPainter p(&inImg);
  QImage darkFace = baseImg.copy();
  for (int y = 0; y < darkFace.height(); ++y)
  {
    for (int x = 0; x < darkFace.width(); ++x)
    {
      darkFace.setPixelColor(x, y, darkFace.pixelColor(x, y).darker(165)); // ~40% darker
    }
  }
  p.drawImage(1, 1, darkFace);
  p.end();

  // 3. Bind both states to a single QIcon (Off = Out, On = In)
  QIcon icon;
  icon.addPixmap(QPixmap::fromImage(outImg), QIcon::Normal, QIcon::Off);
  icon.addPixmap(QPixmap::fromImage(inImg), QIcon::Normal, QIcon::On);
  return icon;
}

//-----------------------------------------------------------------------------------------
void SimboxMain::setupSelectionButtons()
{
  // Individual QRect(x, y, width, height) for each of the 20 buttons (0..9 left, 10..19 right)
  // Tweak any single entry freely to match your stitched jukebox artwork
  const QRect buttonRects[20] = 
  {
    // Left column (Tracks 1 - 10)
    QRect(23, 117, 27, 14),
    QRect(22, 136, 27, 14),
    QRect(22, 155, 27, 14),
    QRect(23, 174, 27, 14),
    QRect(23, 192, 27, 14),
    QRect(23, 210, 27, 14),
    QRect(23, 230, 27, 14),
    QRect(23, 248, 27, 14),
    QRect(23, 266, 27, 14),
    QRect(23, 285, 27, 14),

    // Right column (Tracks 11 - 20)
    QRect(382, 120, 27, 14),
    QRect(382, 136, 27, 14),
    QRect(382, 160, 27, 14),
    QRect(382, 176, 27, 14),
    QRect(382, 196, 27, 14),
    QRect(382, 212, 27, 14),
    QRect(382, 236, 27, 14),
    QRect(382, 248, 27, 14),
    QRect(382, 272, 27, 14),
    QRect(382, 288, 27, 14)
  };

  for (int i = 0; i < 20; ++i)
  {
    const QRect &btnRect = buttonRects[i];

    QPushButton *btn = new QPushButton(this);
    btn->setGeometry(btnRect);
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet("QPushButton { border: none; background: transparent; padding: 0px; }");

    btn->setIcon(createSlicedButtonIcon(btnRect));
    btn->setIconSize(btnRect.size());

    btn->setProperty("slotIndex", i);
    connect(btn, SIGNAL(toggled(bool)), this, SLOT(onSelectionButtonToggled(bool)));

    m_selButtons.append(btn);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::setupLeds()
{
#ifdef Q_OS_WIN
  m_ledSyncDelayMs = 120; // audio delay in windows
#else
  m_ledSyncDelayMs = 0;
#endif

 // 13 left LEDs: index 0 (green) starts at the center -> index 12 (red) at the outer-left edge
  m_leftLedCoords = {
    QPoint(190, 58), QPoint(184, 58), QPoint(178, 58), QPoint(172, 58),
    QPoint(166, 58), QPoint(160, 58), QPoint(154, 58), QPoint(148, 58),
    QPoint(142, 58), QPoint(136, 58), QPoint(130, 58), QPoint(124, 58),
    QPoint(118, 58)
  };

  // 13 right LEDs: index 0 (green) starts at the center -> index 12 (red) at the outer-right edge
  m_rightLedCoords = {
    QPoint(240, 58), QPoint(246, 58), QPoint(252, 58), QPoint(258, 58),
    QPoint(264, 58), QPoint(270, 58), QPoint(276, 58), QPoint(282, 58),
    QPoint(288, 58), QPoint(294, 58), QPoint(300, 58), QPoint(306, 58),
    QPoint(312, 58)
  };

  // Attach QAudioProbe directly to m_player
  m_audioProbe = new QAudioProbe(this);
  if (m_audioProbe->setSource(m_player))
  {
    connect(m_audioProbe, SIGNAL(audioBufferProbed(QAudioBuffer)),
            this, SLOT(onAudioBufferProbed(QAudioBuffer)));
  }

  // Smooth decay timer (~30 fps) so LEDs fall gracefully when paused/stopped
  m_ledTimer = new QTimer(this);
  connect(m_ledTimer, SIGNAL(timeout()), this, SLOT(onLedDecayTimer()));
  m_ledTimer->start(33);
}

//-----------------------------------------------------------------------------------------
void SimboxMain::onSelectionButtonToggled(bool checked)
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int i = btn->property("slotIndex").toInt();
    int offset = (m_page * 20) + i;

    if (offset < m_playlist.size()) {
        m_playlist[offset].selected = checked;
        m_selected[i] = checked;
        updateSlotStyle(i);
    } else if (checked) {
        btn->setChecked(false);
    }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::updateLabels()
{
  // Show/hide page arrows automatically (mirrors Image3->Visible in BCB3)
  if (m_prevPageBtn) m_prevPageBtn->setVisible(m_page > 0);
  if (m_nextPageBtn) m_nextPageBtn->setVisible(((m_page + 1) * 20) < m_playlist.size());

  for (int i = 0; i < 20; ++i)
  {
    int offset = (m_page * 20) + i;
    QSignalBlocker blocker(m_selButtons[i]);

    if (offset < m_playlist.size())
    {
      QFileInfo info(m_playlist[offset].filePath);
      QString songTitle = info.completeBaseName();
      m_songSlots[i]->setText(songTitle);
      m_songSlots[i]->setToolTip(songTitle);

      bool isSel = m_playlist[offset].selected;
      m_selected[i] = isSel;
      m_selButtons[i]->setChecked(isSel);
    }
    else
    {
      m_songSlots[i]->setText("");
      m_songSlots[i]->setToolTip("");
      m_selected[i] = false;
      m_selButtons[i]->setChecked(false);
    }

    updateSlotStyle(i);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::setupPageButtons()
{
  // Left and right top-peak page flip buttons (adjust X/Y/W/H to match your top crown reflectors)
  m_prevPageBtn = new QPushButton("<", this);
  m_prevPageBtn->setGeometry(135, 18, 28, 22);
  m_prevPageBtn->setCursor(Qt::PointingHandCursor);
  m_prevPageBtn->setToolTip("Previous Page");
  m_prevPageBtn->setStyleSheet(
      "QPushButton { background: rgba(0, 0, 0, 120); color: #ffff00; font-weight: bold; border: 1px solid #666; border-radius: 4px; }"
      "QPushButton:hover { background: rgba(255, 255, 0, 80); }"
  );

  m_nextPageBtn = new QPushButton(">", this);
  m_nextPageBtn->setGeometry(267, 18, 28, 22);
  m_nextPageBtn->setCursor(Qt::PointingHandCursor);
  m_nextPageBtn->setToolTip("Next Page");
  m_nextPageBtn->setStyleSheet(
      "QPushButton { background: rgba(0, 0, 0, 120); color: #ffff00; font-weight: bold; border: 1px solid #666; border-radius: 4px; }"
      "QPushButton:hover { background: rgba(255, 255, 0, 80); }"
  );

  connect(m_prevPageBtn, SIGNAL(clicked()), this, SLOT(prevPageClicked()));
  connect(m_nextPageBtn, SIGNAL(clicked()), this, SLOT(nextPageClicked()));
}

//-----------------------------------------------------------------------------------------
void SimboxMain::prevPageClicked()
{
  if (m_page > 0)
  {
    m_page--;
    updateLabels();
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::nextPageClicked()
{
  if (((m_page + 1) * 20) < m_playlist.size())
  {
    m_page++;
    updateLabels();
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::openFilesClicked()
{
  QString startDir = SongDir.isEmpty() ? QDir::homePath() : SongDir;

  QStringList files = QFileDialog::getOpenFileNames(
      this,
      tr("Open Audio Files"),
      startDir,
      tr("Audio Files (*.mp3 *.wav *.mid *.midi);;All Files (*)")
  );

  if (files.isEmpty()) return;

  // Remember the folder for next time
  SongDir = QFileInfo(files.first()).absolutePath();

  bool wasEmpty = m_playlist.isEmpty();

  for (const QString &filePath : files)
  {
    m_playlist.append({filePath, true}); // Selected by default so they play right away
  }

  updateLabels();

  // If the jukebox was empty when you added files, jump to page 0 and start playing
  if (wasEmpty && !m_playlist.isEmpty())
  {
    m_page = 0;
    updateLabels();
//    startPlayer(0);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::loadPlaylistClicked()
{
  QString startDir = PlaylistDir.isEmpty() ? QDir::homePath() : PlaylistDir;
  QString filename = QFileDialog::getOpenFileName(
              this,
              tr("Open Playlist"),
              startDir, //  qApp->applicationDirPath()
              tr("Playlist files (*.m3u)")
  );

  if (filename.isEmpty()) return; // User clicked Cancel

//  PlaylistDir = QFileInfo(filename.first()).absolutePath();
  PlaylistDir=filename;

  QFile file(filename);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    m_playlist.clear();
    m_page = 0;

    QTextStream textStream(&file);
    while (!textStream.atEnd())
    {
      // Strip \r, \n, whitespace, and any legacy BCB3 '\0' null byte
      QString line = textStream.readLine().remove(QChar('\0')).trimmed();
      if (!line.isEmpty() && !line.startsWith('#'))
      {
        m_playlist.append({line, true});
      }
    }
    file.close();

    // Load matching .sel file if it exists (mirrors your original BCB3 LoadPlayList)
    QFileInfo fi(filename);
    QString selPath = fi.path() + "/" + fi.completeBaseName() + ".sel";
    QFile selFile(selPath);
    if (selFile.open(QIODevice::ReadOnly))
    {
      QByteArray selData = selFile.readAll();
      int limit = qMin(m_playlist.size(), selData.size());
      for (int i = 0; i < limit; ++i)
      {
        m_playlist[i].selected = (selData.at(i) == 1);
      }
      selFile.close();
    }

    QApplication::restoreOverrideCursor();
    updateLabels();
  }
  else
  {
    QString mess = "Could not open file: " + filename;
    QMessageBox::warning(this, "SimBox", mess);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::clearPlaylistClicked()
{
  stopClicked();
  playlist.clear();
  m_playlist.clear();
  m_currentSong = -1;
  m_page = 0;
  Label_Total->setText("00:00");
  Label_Marquee->setText("Song Title");
  updateLabels();
}

//-----------------------------------------------------------------------------------------
void SimboxMain::savePlaylistClicked()
{
  QString startDir = PlaylistDir.isEmpty() ? QDir::homePath() : PlaylistDir;
  QString filename = QFileDialog::getSaveFileName(
              this, "Save Playlist", qApp->applicationDirPath(),
              "Playlist files (*.m3u)");

  if (filename.isEmpty()) return;

//  PlaylistDir = QFileInfo(filename).absolutePath();
  PlaylistDir=filename;

  if (!filename.endsWith(".m3u", Qt::CaseInsensitive))
  {
    filename += ".m3u";
  }

  // 1. Write the .m3u playlist
  QFile m3uFile(filename);
  if (!m3uFile.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QMessageBox::warning(this, "SimBox", "Could not save playlist: " + filename);
    return;
  }

  QTextStream out(&m3uFile);
  for (int i = 0; i < m_playlist.size(); ++i)
  {
    out << m_playlist[i].filePath << "\n";
  }
  m3uFile.close();

  // 2. Write the matching .sel selection file (1 byte per track)
  QFileInfo fi(filename);
  QString selPath = fi.path() + "/" + fi.completeBaseName() + ".sel";
  QFile selFile(selPath);
  if (selFile.open(QIODevice::WriteOnly))
  {
    QByteArray selData(m_playlist.size(), 0);
    for (int i = 0; i < m_playlist.size(); ++i)
    {
      selData[i] = m_playlist[i].selected ? 1 : 0;
    }
    selFile.write(selData);
    selFile.close();
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::editPlaylistClicked()
{
  EditForm *dialog = new EditForm();
  for (int t = 0; t < m_playlist.size(); ++t)
  {
    playlist.append(m_playlist[t].filePath);
  }

  dialog->setAttribute(Qt::WA_DeleteOnClose); // Clean up memory when closed
  connect(dialog, SIGNAL(sendData(QStringList)), this, SLOT(printData(QStringList)));

  dialog->receivePlaylist(playlist);
  dialog->show();

}

//-----------------------------------------------------------------------------------------
void SimboxMain::printData(QStringList dat) /** recieves playlist from editor */
{

  m_playlist.clear();
  m_selected.clear();
  for (int t = 0; t < dat.size(); ++t)
  {
    m_playlist.append({dat[t], false});
  }

  m_page = 0;
  updateLabels();
}

//-----------------------------------------------------------------------------------------
void SimboxMain::selectAllClicked()
{
  for (int i = 0; i < m_playlist.size(); ++i)
  {
    m_playlist[i].selected = true;
  }
  updateLabels();
}

//-----------------------------------------------------------------------------------------
void SimboxMain::deselectAllClicked()
{
  for (int i = 0; i < m_playlist.size(); ++i)
  {
    m_playlist[i].selected = false;
  }
  updateLabels();
}

//-----------------------------------------------------------------------------------------
void SimboxMain::togglePlayerDisplay(bool visible)
{
controlPanel->setVisible(visible);
/*
  // Replace with your actual widget member names as you add them
  if (Label_Marquee) Label_Marquee->setVisible(visible);
  if (Label_Elapsed) Label_Elapsed->setVisible(visible);
  if (Label_Total)   Label_Total->setVisible(visible);
  if (playButton)    playButton->setVisible(visible);
  if (pauseButton)   pauseButton->setVisible(visible);
  if (stopButton)    stopButton->setVisible(visible);
  if (previousButton) previousButton->setVisible(visible);
  if (nextButton)    nextButton->setVisible(visible);
*/
}

//-----------------------------------------------------------------------------------------
void SimboxMain::startPlayer(int index)
{
//  m_player->stop();
  m_peakQueue.clear();
  m_rawPeakL = 0.0;
  m_rawPeakR = 0.0;

  if (index < 0 || index >= m_playlist.size()) return;

  m_currentSong = index;
  m_page = m_currentSong / 20;
  updateLabels();

  QString path = m_playlist[m_currentSong].filePath;
  QFileInfo info(path);

  // Set up scrolling marquee text with padding spaces (just like original SongDisplay)
  m_marqueeText = "    " + info.completeBaseName() + "    ";
  Label_Marquee->setText(m_marqueeText);
  m_marqueeTimer->start(180);

  m_player->setMedia(QUrl::fromLocalFile(path));
  m_player->play();
}

//-----------------------------------------------------------------------------------------
void SimboxMain::playClicked()
{
  if (m_player->state() == QMediaPlayer::PausedState)
  {
    m_player->play();
    m_marqueeTimer->start(180);
    return;
  }

  // If nothing is playing yet, start at the first selected song
  if (m_currentSong < 0)
  {
    for (int i = 0; i < m_playlist.size(); ++i)
    {
      if (m_playlist[i].selected) { startPlayer(i); return; }
    }
  }
  else
  {
    startPlayer(m_currentSong);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::pauseClicked()
{
  if (m_player->state() == QMediaPlayer::PlayingState)
  {
    m_player->pause();
    m_marqueeTimer->stop();
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::stopClicked()
{
  m_player->stop();
  m_marqueeTimer->stop();
  Label_Elapsed->setText("00:00");
}

//-----------------------------------------------------------------------------------------
void SimboxMain::nextClicked()
{
  for (int i = m_currentSong + 1; i < m_playlist.size(); ++i)
  {
    if (m_playlist[i].selected) { startPlayer(i); return; }
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::prevClicked()
{
  for (int i = m_currentSong - 1; i >= 0; --i)
  {
    if (m_playlist[i].selected) { startPlayer(i); return; }
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
  // Auto-advance when song finishes or hits an unreadable file
  if (status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia)
  {
    playNextTrack(true);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::playNextTrack(bool autoAdvance)
{
  if (m_playlist.isEmpty()) return;

  // 1. Look for the next selected track after m_currentSong
  for (int i = m_currentSong + 1; i < m_playlist.size(); ++i)
  {
    if (m_playlist[i].selected)
    {
      startPlayer(i);
      return;
    }
  }

  // 2. Reached the end of the playlist
  if (autoAdvance && !m_loopEnabled)
  {
    stopClicked();
    return;
  }

  // Shuffle before starting the next pass if Randomize Loop is checked
  if (autoAdvance && m_loopEnabled && m_randomLoop)
  {
    randomizePlaylist();
  }

  // Wrap around to the first selected track from index 0
  for (int i = 0; i < m_playlist.size(); ++i)
  {
    if (m_playlist[i].selected)
    {
      startPlayer(i);
      return;
    }
  }

  // No tracks are selected at all
  stopClicked();
}

//-----------------------------------------------------------------------------------------
void SimboxMain::onDurationChanged(qint64 duration)
{
  Label_Total->setText(formatTime(duration));
}

//-----------------------------------------------------------------------------------------
void SimboxMain::onPositionChanged(qint64 position)
{
  Label_Elapsed->setText(formatTime(position));
}

//-----------------------------------------------------------------------------------------
QString SimboxMain::formatTime(qint64 ms)
{
  int totalSec = static_cast<int>(ms / 1000);
  int min = totalSec / 60;
  int sec = totalSec % 60;
  return QString("%1:%2").arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
}

//-----------------------------------------------------------------------------------------
void SimboxMain::onMarqueeTimer()
{
  if (m_marqueeText.length() > 1)
  {
    m_marqueeText = m_marqueeText.mid(1) + m_marqueeText.left(1);
    Label_Marquee->setText(m_marqueeText);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::paintEvent(QPaintEvent * /*event*/)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  if (!m_skin.isNull())
  {
    painter.drawPixmap(0, 0, m_skin);
    if (vuOffAction && !vuOffAction->isChecked() && m_leftLedCoords.size() == 13)
    {
      auto drawChannel = [&](const QVector<QPoint> &coords, const double alphas[13])
      {
        for (int i = 0; i < 13; ++i)
        {
          if (alphas[i] <= 0.0) continue;

          QColor ledColor = (i < 4)  ? QColor(0, 255, 0) :     // 4 Green (inner)
                            (i < 10) ? QColor(255, 255, 0) :   // 6 Yellow (middle)
                                       QColor(255, 40, 40);    // 3 Red (outer peak)
          ledColor.setAlphaF(alphas[i]);

          QColor borderColor(0, 0, 0);
          borderColor.setAlphaF(alphas[i]);

          painter.setPen(QPen(borderColor, 1));
          painter.setBrush(ledColor);
          painter.drawRoundedRect(coords[i].x() - 2, coords[i].y() - 4, 5, 9, 2, 2);
        }
      };

      drawChannel(m_leftLedCoords, m_leftLedAlpha);
      drawChannel(m_rightLedCoords, m_rightLedAlpha);

      bool dotOnly = (vuDotAction && vuDotAction->isChecked());
      bool showPeak = dotOnly ? m_peakHoldDot : m_peakHoldBar;
      if (showPeak)
      {
        auto drawPeak = [&](const QVector<QPoint> &coords, double peakPos)
        {
          int idx = static_cast<int>(peakPos) - 1;
          if (idx >= 0 && idx < 13)
          {
            painter.setPen(QPen(Qt::black, 1));
            painter.setBrush(QColor(0, 230, 255)); // Crisp cyan peak-hold marker
            painter.drawRoundedRect(coords[idx].x() - 2, coords[idx].y() - 4, 5, 9, 2, 2);
          }
        };

        drawPeak(m_leftLedCoords, m_peakHoldL);
        drawPeak(m_rightLedCoords, m_peakHoldR);
      }
    }
  }
  else
  {
    // Fallback placeholder shape if skin PNG hasn't been added yet
    painter.setBrush(QColor(40, 20, 20, 230));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 40, 40);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::onAudioBufferProbed(const QAudioBuffer &buffer)
{
 if (!buffer.isValid() || buffer.format().channelCount() < 2) return;

  int frames = buffer.frameCount();
  int channels = buffer.format().channelCount();
  double peakL = 0.0;
  double peakR = 0.0;

  if (buffer.format().sampleType() == QAudioFormat::SignedInt && buffer.format().sampleSize() == 16)
  {
    const qint16 *data = buffer.constData<qint16>();
    for (int i = 0; i < frames; ++i)
    {
      double l = qAbs(static_cast<double>(data[i * channels])) / 32768.0;
      double r = qAbs(static_cast<double>(data[i * channels + 1])) / 32768.0;
      if (l > peakL) peakL = l;
      if (r > peakR) peakR = r;
    }
  }
  else if (buffer.format().sampleType() == QAudioFormat::SignedInt && buffer.format().sampleSize() == 32)
  {
    const qint32 *data = buffer.constData<qint32>();
    for (int i = 0; i < frames; ++i)
    {
      double l = qAbs(static_cast<double>(data[i * channels])) / 2147483648.0;
      double r = qAbs(static_cast<double>(data[i * channels + 1])) / 2147483648.0;
      if (l > peakL) peakL = l;
      if (r > peakR) peakR = r;
    }
  }
  else if (buffer.format().sampleType() == QAudioFormat::Float)
  {
    const float *data = buffer.constData<float>();
    for (int i = 0; i < frames; ++i)
    {
      double l = qAbs(static_cast<double>(data[i * channels]));
      double r = qAbs(static_cast<double>(data[i * channels + 1]));
      if (l > peakL) peakL = l;
      if (r > peakR) peakR = r;
    }
  }

  if (m_ledSyncDelayMs <= 0)
  {
    // Linux: no extra audio driver delay—feed peaks straight to the timer
    if (peakL > m_rawPeakL) m_rawPeakL = peakL;
    if (peakR > m_rawPeakR) m_rawPeakR = peakR;
  }
  else
  {
    qint64 bufMs = (buffer.startTime() >= 0) ? (buffer.startTime() / 1000) : 0;
    // Ignore stale position readings from the previous track during a track switch
    if (!m_peakQueue.isEmpty() && bufMs + 1000 < m_peakQueue.last().timestampMs)
    {
      m_peakQueue.clear();
    }
    m_peakQueue.enqueue({bufMs, peakL, peakR});
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::onLedDecayTimer()
{
  // 1. If paused or stopped, fade everything out smoothly
  if (m_player->state() != QMediaPlayer::PlayingState)
  {
    bool stillFading = false;
    for (int i = 0; i < 13; ++i)
    {
      m_leftLedAlpha[i]  *= 0.7;
      m_rightLedAlpha[i] *= 0.7;
      if (m_leftLedAlpha[i]  < 0.03) m_leftLedAlpha[i]  = 0.0;
      if (m_rightLedAlpha[i] < 0.03) m_rightLedAlpha[i] = 0.0;
      if (m_leftLedAlpha[i] > 0.0 || m_rightLedAlpha[i] > 0.0) stillFading = true;
    }

    if (m_leftLevel > 0 || m_rightLevel > 0 || m_peakHoldL > 0.0 || m_peakHoldR > 0.0 || stillFading)
    {
      m_leftAvg  *= 0.7;
      m_rightAvg *= 0.7;
      m_leftLevel  = static_cast<int>(m_leftAvg * 14.0);
      m_rightLevel = static_cast<int>(m_rightAvg * 14.0);
      m_peakHoldL  = qMax(0.0, m_peakHoldL - 0.5);
      m_peakHoldR  = qMax(0.0, m_peakHoldR - 0.5);
      update();
    }
    return;
  }

  // 2. Grab the latest peak (direct on Linux, timestamp-delayed on Windows)
  double targetL = 0.0;
  double targetR = 0.0;
  bool hasPeak = false;

  if (m_ledSyncDelayMs <= 0)
  {
    if (m_rawPeakL > 0.0 || m_rawPeakR > 0.0)
    {
      targetL = m_rawPeakL;
      targetR = m_rawPeakR;
      m_rawPeakL = 0.0;
      m_rawPeakR = 0.0;
      hasPeak = true;
    }
  }
  else
  {
    qint64 currentPlayPos = m_player->position();
    while (!m_peakQueue.isEmpty() &&
           m_peakQueue.head().timestampMs > currentPlayPos + 1000)
    {
      m_peakQueue.dequeue();
    }
    while (!m_peakQueue.isEmpty() &&
           (m_peakQueue.head().timestampMs + m_ledSyncDelayMs) <= currentPlayPos)
    {
      TimedPeak p = m_peakQueue.dequeue();
      if (p.left > targetL)  targetL = p.left;
      if (p.right > targetR) targetR = p.right;
      hasPeak = true;
    }
  }

  bool dotOnly = (vuDotAction && vuDotAction->isChecked());
  bool tailsOn = dotOnly ? m_tailsDot : m_tailsBar;

  // 3. Smooth attack/decay (snaps down faster when Tails is unchecked)
  if (hasPeak)
  {
    m_leftAvg  = (targetL >= m_leftAvg)  ? targetL : (targetL + m_leftAvg + m_leftAvg) / 3.0;
    m_rightAvg = (targetR >= m_rightAvg) ? targetR : (targetR + m_rightAvg + m_rightAvg) / 3.0;
  }
  else
  {
    double avgFalloff = tailsOn ? 0.90 : 0.65;
    m_leftAvg  *= avgFalloff;
    m_rightAvg *= avgFalloff;
  }

  int newLeft  = qBound(0, static_cast<int>(m_leftAvg * 14.0), 13);
  int newRight = qBound(0, static_cast<int>(m_rightAvg * 14.0), 13);

  // 4. Update phosphor trails using activeFade (0.0 when Tails is unchecked!)
  double activeFade = tailsOn ? 0.82 : 0.0;
  bool anyLit = false;

  for (int i = 0; i < 13; ++i)
  {
    bool isLitL = dotOnly ? (i == newLeft - 1 && newLeft > 0)   : (i < newLeft);
    bool isLitR = dotOnly ? (i == newRight - 1 && newRight > 0) : (i < newRight);

    m_leftLedAlpha[i]  = isLitL ? 1.0 : (m_leftLedAlpha[i]  * activeFade);
    m_rightLedAlpha[i] = isLitR ? 1.0 : (m_rightLedAlpha[i] * activeFade);

    if (m_leftLedAlpha[i]  < 0.03) m_leftLedAlpha[i]  = 0.0;
    if (m_rightLedAlpha[i] < 0.03) m_rightLedAlpha[i] = 0.0;

    if (m_leftLedAlpha[i] > 0.0 || m_rightLedAlpha[i] > 0.0) anyLit = true;
  }

  // 5. Update Peak-Hold markers
  if (newLeft >= m_peakHoldL)
  {
    m_peakHoldL  = newLeft;
    m_peakPauseL = 12;
  }
  else if (m_peakPauseL > 0)
  {
    --m_peakPauseL;
  }
  else
  {
    m_peakHoldL = qMax(0.0, m_peakHoldL - 0.25);
  }

  if (newRight >= m_peakHoldR)
  {
    m_peakHoldR  = newRight;
    m_peakPauseR = 12;
  }
  else if (m_peakPauseR > 0)
  {
    --m_peakPauseR;
  }
  else
  {
    m_peakHoldR = qMax(0.0, m_peakHoldR - 0.25);
  }

  if (anyLit || m_peakHoldL > 0.0 || m_peakHoldR > 0.0 ||
      newLeft != m_leftLevel || newRight != m_rightLevel)
  {
    m_leftLevel  = newLeft;
    m_rightLevel = newRight;
    update();
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::timerSpeedClicked()
{
  bool ok = false;
  int newMs = QInputDialog::getInt(
      this, tr("VU Meter Speed"),
      tr("Refresh interval (ms — lower is faster):"),
      m_ledIntervalMs, 5, 500, 5, &ok);

  if (ok)
  {
    m_ledIntervalMs = newMs;
    m_ledTimer->start(m_ledIntervalMs);
  }
}

//-----------------------------------------------------------------------------------------
void SimboxMain::randomizePlaylist()
{
  if (m_playlist.size() > 1)
  {
    std::shuffle(m_playlist.begin(), m_playlist.end(), *QRandomGenerator::global());
  }
  m_page = 0;
  updateLabels();
}

//-----------------------------------------------------------------------------------------
void SimboxMain::moreOptionsClicked()
{
  OptionsForm dlg(this);
  dlg.setValues(m_tailsDot, m_tailsBar, m_peakHoldDot, m_peakHoldBar,
                m_loopEnabled, m_randomLoop, m_ledIntervalMs);

  connect(&dlg, &OptionsForm::randomizeNowRequested, this, [this]() {
    stopClicked();
    randomizePlaylist();
    for (int i = 0; i < m_playlist.size(); ++i)
    {
      if (m_playlist[i].selected) { startPlayer(i); break; }
    }
  });

  if (dlg.exec() == QDialog::Accepted)
  {
    m_tailsDot    = dlg.tailsDot();
    m_tailsBar    = dlg.tailsBar();
    m_peakHoldDot = dlg.peakDot();
    m_peakHoldBar = dlg.peakBar();
    m_loopEnabled = dlg.loopEnabled();
    m_randomLoop  = dlg.randomLoop();

    if (m_ledIntervalMs != dlg.ledIntervalMs())
    {
      m_ledIntervalMs = dlg.ledIntervalMs();
      m_ledTimer->start(m_ledIntervalMs);
    }
    update();
  }
}

//-----------------------------------------------------------------------------------------

