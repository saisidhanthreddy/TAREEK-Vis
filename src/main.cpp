#include <QApplication>
#include <QSurfaceFormat>
#include <QMessageBox>
#include <QFileInfo>
#include "ui/main_window.h"
#include "core/logger.h"

int main(int argc, char* argv[]) {
    try {
        simvis::Logger::init();
        LOG_INFO("TAREEK-Vis starting up");

        // Set OpenGL version before creating application
        QSurfaceFormat format;
        format.setVersion(3, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setDepthBufferSize(24);
        format.setSamples(4);  // Multisampling for smoother lines
        QSurfaceFormat::setDefaultFormat(format);

        QApplication app(argc, argv);
        app.setApplicationName("TAREEK-Vis");
        app.setApplicationVersion("1.0.0");
        app.setOrganizationName("TAREEK-Vis");

        LOG_INFO("QApplication created");

        simvis::MainWindow mainWindow;
        mainWindow.show();

        // CLI: `TAREEK-Vis <folder>` auto-loads network/events/transit from folder
        if (argc >= 3) {
            mainWindow.loadNetworkFile(argv[1]);
            mainWindow.loadEventsFile(argv[2]);
        } else if (argc == 2 && QFileInfo(QString::fromLocal8Bit(argv[1])).isDir()) {
            QMetaObject::invokeMethod(&mainWindow, [&mainWindow, path = QString::fromLocal8Bit(argv[1])]() {
                mainWindow.loadFolder(path);
            }, Qt::QueuedConnection);
        }

        int ret = app.exec();
        LOG_INFO(QString("TAREEK-Vis exiting, code %1").arg(ret));
        return ret;

    } catch (const std::exception& e) {
        qCritical() << "Unhandled exception in main():" << e.what();
        QMessageBox::critical(nullptr, "Fatal Error",
            QString("Unhandled exception:\n%1").arg(e.what()));
        return 1;
    } catch (...) {
        qCritical() << "Unknown unhandled exception in main()";
        return 1;
    }
}
