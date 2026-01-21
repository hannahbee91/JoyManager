#pragma once

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QSplitter>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

class FileManagerView : public QWidget {
    Q_OBJECT

public:
    explicit FileManagerView(QWidget *parent = nullptr);
    ~FileManagerView() override;
    void setBleManager(void* manager); // Pointer to key logic

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUi();
    
    QSplitter *splitter;
    
    // Left Pane (Local)
    QWidget *localWidget;
    QVBoxLayout *localLayout;
    QLabel *localLabel;
    QPushButton *localUpButton;
    QLabel *localPathLabel;
    QTreeView *localView;
    QFileSystemModel *localModel;

    // Right Pane (Remote)
    QWidget *remoteWidget;
    QVBoxLayout *remoteLayout;
    QLabel *remoteLabel;
    QPushButton *remoteUpButton;
    QLabel *remotePathLabel;
    QTreeView *remoteView;
    QPushButton *connectButton; // Temporary placeholder until main window handles it?

private slots:
    void onConnectClicked();
    void onFetchRequested(const QString &path);
    void handleBleData(const std::vector<uint8_t>& data);

private:
    class FileManagerViewPrivate;
    FileManagerViewPrivate *d;
};
