#include "DeviceSelectionDialog.h"

DeviceSelectionDialog::DeviceSelectionDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Select Pixl.js Device");
    resize(400, 300);

    auto *layout = new QVBoxLayout(this);
    
    layout->addWidget(new QLabel("Devices Found:", this));
    
    deviceList = new QListWidget(this);
    layout->addWidget(deviceList);
    
    scanButton = new QPushButton("Rescan", this);
    layout->addWidget(scanButton);
    
    auto *btnLayout = new QHBoxLayout();
    cancelButton = new QPushButton("Cancel", this);
    connectButton = new QPushButton("Connect", this);
    connectButton->setEnabled(false);
    
    btnLayout->addWidget(cancelButton);
    btnLayout->addWidget(connectButton);
    layout->addLayout(btnLayout);

    connect(deviceList, &QListWidget::itemSelectionChanged, [this]() {
        connectButton->setEnabled(!deviceList->selectedItems().isEmpty());
    });
    
    connect(deviceList, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
        selectedAddress = item->data(Qt::UserRole).toString();
        accept();
    });

    connect(connectButton, &QPushButton::clicked, [this]() {
        if (!deviceList->selectedItems().isEmpty()) {
            selectedAddress = deviceList->selectedItems().first()->data(Qt::UserRole).toString();
            accept();
        }
    });

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    connect(scanButton, &QPushButton::clicked, this, &DeviceSelectionDialog::scanRequested);
}

void DeviceSelectionDialog::addDevice(const QString& name, const QString& address) {
    // Check if exists
    for(int i=0; i<deviceList->count(); ++i) {
        if (deviceList->item(i)->data(Qt::UserRole).toString() == address) {
            return;
        }
    }
    
    QString label = name.isEmpty() ? "Unknown Device" : name;
    label += " [" + address + "]";
    
    QListWidgetItem *item = new QListWidgetItem(label, deviceList);
    item->setData(Qt::UserRole, address);
}

QString DeviceSelectionDialog::getSelectedAddress() const {
    return selectedAddress;
}

void DeviceSelectionDialog::clearDevices() {
    deviceList->clear();
}
