#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

class DeviceSelectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit DeviceSelectionDialog(QWidget *parent = nullptr);
    void addDevice(const QString& name, const QString& address);
    QString getSelectedAddress() const;
    void clearDevices();

signals:
    void scanRequested();

private:
    QListWidget *deviceList;
    QPushButton *connectButton;
    QPushButton *cancelButton;
    QPushButton *scanButton;
    QString selectedAddress;
};
