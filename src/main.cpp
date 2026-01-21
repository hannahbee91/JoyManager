#include <QApplication>
#include <QMainWindow>
#include "gui/FileManagerView.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("JoyManager");
    window.resize(1024, 600);

    FileManagerView *fileManager = new FileManagerView(&window);
    window.setCentralWidget(fileManager);

    window.show();

    return app.exec();
}
