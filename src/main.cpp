#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QSocketNotifier>
#include <QWidget>
#include <QWindow>

#include <LayerShellQt/window.h>

#include <csignal>
#include <memory>
#include <unistd.h>
#include <vector>

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
    p.fillRect(rect(), QColor(0, 0, 0, 150));
  }
};

std::vector<std::unique_ptr<Overlay>> overlays;
bool visible = false;

int signalPipe[2];

void signalHandler(int) {
  char c = 1;
  write(signalPipe[1], &c, 1);
}

void updateVisibility() {
  for (auto &overlay : overlays)
    overlay->setVisible(visible);
}

void toggleOverlay() {
  visible = !visible;
  updateVisibility();
}

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

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

  auto *notifier = new QSocketNotifier(signalPipe[0], QSocketNotifier::Read);

  QObject::connect(notifier, &QSocketNotifier::activated, [](int) {
    char c;
    read(signalPipe[0], &c, 1);
    toggleOverlay();
  });

  return app.exec();
}