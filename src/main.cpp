#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QScreen>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTextStream>
#include <QWidget>
#include <QWindow>

#include <LayerShellQt/window.h>

#include <csignal>
#include <memory>
#include <unistd.h>
#include <vector>

int currentAlpha = 150; // 0-255

QString levelFilePath() {
  QString dir = QStandardPaths::writableLocation(
      QStandardPaths::GenericConfigLocation) + "/screen-dimmer";
  QDir().mkpath(dir);
  return dir + "/level";
}

void loadAlphaFromFile() {
  QFile f(levelFilePath());
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return;
  QTextStream in(&f);
  bool ok = false;
  int value = in.readLine().trimmed().toInt(&ok);
  if (ok && value >= 0 && value <= 255)
    currentAlpha = value;
}

class Overlay : public QWidget {
public:
  explicit Overlay(QScreen *screen) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool |
                   Qt::WindowTransparentForInput);

    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);

    resize(screen->geometry().size());

    createWinId();

    windowHandle()->setScreen(screen);

    auto *layer = LayerShellQt::Window::get(windowHandle());

    LayerShellQt::Window::Anchors anchors;
    anchors |= LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorBottom;
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorRight;

    layer->setAnchors(anchors);
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setExclusiveZone(-1);
    layer->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);

    layer->setScope("screen-dimmer");
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, currentAlpha));
  }
};

std::vector<std::unique_ptr<Overlay>> overlays;
bool visible = false;

int signalPipe[2];

void signalHandler(int sig) {
  char c = static_cast<char>(sig);
  write(signalPipe[1], &c, 1);
}

void updateVisibility() {
  for (auto &overlay : overlays)
    overlay->setVisible(visible);
}

void repaintAll() {
  for (auto &overlay : overlays)
    overlay->update();
}

void toggleOverlay() {
  visible = !visible;
  updateVisibility();
}

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  loadAlphaFromFile();

  for (QScreen *screen : app.screens())
    overlays.emplace_back(std::make_unique<Overlay>(screen));

  updateVisibility();

  if (pipe(signalPipe) != 0)
    return 1;

  struct sigaction sa{};
  sa.sa_handler = signalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  sigaction(SIGUSR1, &sa, nullptr);
  sigaction(SIGUSR2, &sa, nullptr);

  auto *notifier = new QSocketNotifier(signalPipe[0], QSocketNotifier::Read);

  QObject::connect(notifier, &QSocketNotifier::activated, [](int) {
    char c;
    read(signalPipe[0], &c, 1);
    if (c == SIGUSR1) {
      toggleOverlay();
    } else if (c == SIGUSR2) {
      loadAlphaFromFile();
      repaintAll();
    }
  });

  return app.exec();
}