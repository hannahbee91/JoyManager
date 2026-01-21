#include "FileManagerView.h"
#include "RemoteFileSystemModel.h"
#include "../ble/BleManager.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QMenu>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QProgressDialog>
#include <QFileDialog>
#include <QDragMoveEvent>
#include <QDir>
#include <deque>
#include "DeviceSelectionDialog.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QSettings>
#include <QHeaderView>

// Forward declaration if needed, but we included headers.

class FileManagerView::FileManagerViewPrivate {
public:
    FileManagerView *q;
    BleManager bleManager;
    RemoteFileSystemModel *remoteModel;
    QString lastRequestedPath;
    std::vector<uint8_t> responseBuffer;

    FileManagerViewPrivate(FileManagerView *parent) : q(parent) {}

    enum class OpType { CreateFolder, UploadFile, DownloadFile, DeleteFile };
    struct Operation {
        OpType type;
        QString source;
        QString target;
    };

    std::deque<Operation> opQueue;
    QProgressDialog *progressDialog = nullptr;
    int totalOps = 0;
    int completedOps = 0;
    bool isProcessing = false;

    // Current File State
    std::unique_ptr<QFile> currentFile;
    qint64 currentOffset = 0;
    uint8_t currentFileId = 0;
    static constexpr int CHUNK_SIZE = 200;

    void processNextOperation() {
        if (opQueue.empty()) {
            isProcessing = false;
            if (progressDialog) {
                progressDialog->close();
                progressDialog->deleteLater();
                progressDialog = nullptr;
            }
            // Refresh
            q->onFetchRequested(lastRequestedPath);
            return;
        }

        isProcessing = true;
        Operation op = opQueue.front();
        opQueue.pop_front();
        completedOps++;

        if (progressDialog) {
            progressDialog->setValue(completedOps);
            progressDialog->setLabelText(QString("Processing: %1").arg(QFileInfo(op.source.isEmpty() ? op.target : op.source).fileName()));
            if (progressDialog->wasCanceled()) {
                opQueue.clear();
                processNextOperation();
                return;
            }
        }

        switch (op.type) {
            case OpType::CreateFolder: {
                auto payload = Pixl::Protocol::createStringPayload(op.target.toStdString());
                bleManager.sendCommand(Pixl::Command::CreateFolder, payload);
                break;
            }
            case OpType::UploadFile: {
                currentFile = std::make_unique<QFile>(op.source);
                if (!currentFile->open(QIODevice::ReadOnly)) {
                    processNextOperation();
                    return;
                }
                currentOffset = 0;
                auto payload = Pixl::Protocol::createOpenFilePayload(op.target.toStdString(), 0x16);
                bleManager.sendCommand(Pixl::Command::OpenFile, payload);
                break;
            }
            case OpType::DownloadFile: {
                currentFile = std::make_unique<QFile>(op.target);
                if (!currentFile->open(QIODevice::WriteOnly)) {
                     processNextOperation();
                     return;
                }
                currentOffset = 0;
                auto payload = Pixl::Protocol::createOpenFilePayload(op.source.toStdString(), 0x08);
                bleManager.sendCommand(Pixl::Command::OpenFile, payload);
                break;
            }
            case OpType::DeleteFile: {
                auto payload = Pixl::Protocol::createStringPayload(op.target.toStdString());
                bleManager.sendCommand(Pixl::Command::Remove, payload);
                break;
            }
        }
    }

    void startOperations(const std::vector<Operation>& ops, const QString& title, QWidget* parent) {
        for (const auto& op : ops) opQueue.push_back(op);
        totalOps += ops.size();

        if (!progressDialog) {
            progressDialog = new QProgressDialog(title, "Cancel", 0, totalOps, parent);
            progressDialog->setWindowModality(Qt::WindowModal);
            progressDialog->setMinimumDuration(0);
            progressDialog->show();
            completedOps = 0;
        } else {
            progressDialog->setMaximum(totalOps);
        }

        if (!isProcessing) {
            processNextOperation();
        }
    }

    void sendNextChunk() {
        if (!currentFile) return;
        currentFile->seek(currentOffset);
        QByteArray data = currentFile->read(CHUNK_SIZE);
        if (data.isEmpty()) {
            std::vector<uint8_t> payload;
            payload.push_back(currentFileId);
            bleManager.sendCommand(Pixl::Command::CloseFile, payload);
            return;
        }
        std::vector<uint8_t> payload;
        payload.push_back(currentFileId);
        payload.insert(payload.end(), data.begin(), data.end());
        bleManager.sendCommand(Pixl::Command::WriteFile, payload);
    }

    void recursiveScan(const QString& localPath, const QString& remotePath, std::vector<Operation>& ops) {
        QFileInfo fi(localPath);
        if (fi.isDir()) {
            ops.push_back({OpType::CreateFolder, localPath, remotePath});
            QDir dir(localPath);
            for (const QString& entry : dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
                recursiveScan(dir.absoluteFilePath(entry), remotePath + (remotePath.endsWith("/") ? "" : "/") + entry, ops);
            }
        } else {
            ops.push_back({OpType::UploadFile, localPath, remotePath});
        }
    }
};

FileManagerView::FileManagerView(QWidget *parent) : QWidget(parent) {
    d = new FileManagerViewPrivate(this);
    setupUi();
    
    // Connect Signals
    connect(connectButton, &QPushButton::clicked, this, &FileManagerView::onConnectClicked);
    
    // Setup BLE callbacks
    d->bleManager.initialize();
    
    d->bleManager.setDataReceivedCallback([this](const std::vector<uint8_t>& data) {
        // Handle data on UI thread
        QMetaObject::invokeMethod(this, [this, data]() {
            handleBleData(data);
        }, Qt::QueuedConnection);
    });
    
     d->bleManager.setDisconnectedCallback([this]() {
          QMetaObject::invokeMethod(this, [this]() {
             connectButton->setText("Connect to Device");
             connectButton->setEnabled(true);
             d->remoteModel->clear();
             QMessageBox::warning(this, "Disconnected", "Device disconnected");
         }, Qt::QueuedConnection);
     });
    
    // Connect Model
    connect(d->remoteModel, &RemoteFileSystemModel::fetchRequested, this, &FileManagerView::onFetchRequested);
}

FileManagerView::~FileManagerView() {
    QSettings settings("Joysfusion", "JoyManager");
    settings.setValue("localPath", localModel->filePath(localView->rootIndex()));
    settings.setValue("localHeaderState", localView->header()->saveState());
    settings.setValue("remoteHeaderState", remoteView->header()->saveState());
    delete d;
}

void FileManagerView::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    splitter = new QSplitter(Qt::Horizontal, this);
    
    QSettings settings("Joysfusion", "JoyManager");

    // Setup Local Pane
    localWidget = new QWidget(this);
    localLayout = new QVBoxLayout(localWidget);
    localLabel = new QLabel("Local Files", localWidget);
    localLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #333;");
    
    localUpButton = new QPushButton("Up", localWidget);
    localPathLabel = new QLabel(localWidget);
    localPathLabel->setStyleSheet("font-weight: bold; padding: 4px; background: #eee; border: 1px solid #ccc; border-radius: 4px; color: #555;");
    
    localView = new QTreeView(localWidget);
    localModel = new QFileSystemModel(this);
    localModel->setRootPath(QDir::homePath());
    localView->setModel(localModel);
    
    QString lastLocalPath = settings.value("localPath", QDir::homePath()).toString();
    localView->setRootIndex(localModel->index(lastLocalPath));
    localPathLabel->setText(lastLocalPath);

    localView->setRootIsDecorated(false);
    localView->setItemsExpandable(false);
    localView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    localView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    
    if (settings.contains("localHeaderState")) {
        localView->header()->restoreState(settings.value("localHeaderState").toByteArray());
    }
    
    localLayout->addWidget(localLabel);
    localLayout->addWidget(localUpButton);
    localLayout->addWidget(localPathLabel);
    localLayout->addWidget(localView);
    
    // Setup Remote Pane
    remoteWidget = new QWidget(this);
    remoteLayout = new QVBoxLayout(remoteWidget);
    remoteLabel = new QLabel("Device Files", remoteWidget);
    remoteLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #333;");
    
    remoteUpButton = new QPushButton("Up", remoteWidget);
    remotePathLabel = new QLabel("/", remoteWidget);
    remotePathLabel->setStyleSheet("font-weight: bold; padding: 4px; background: #eee; border: 1px solid #ccc; border-radius: 4px; color: #555;");
    
    remoteView = new QTreeView(remoteWidget);
    connectButton = new QPushButton("Connect to Device", remoteWidget);
    
    d->remoteModel = new RemoteFileSystemModel(this);
    d->remoteModel->setBleManager(&d->bleManager);
    remoteView->setModel(d->remoteModel);
    remoteView->setRootIsDecorated(false);
    remoteView->setItemsExpandable(false);
    remoteView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    remoteView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    
    if (settings.contains("remoteHeaderState")) {
        remoteView->header()->restoreState(settings.value("remoteHeaderState").toByteArray());
    }
    
    remoteLayout->addWidget(remoteLabel);
    remoteLayout->addWidget(connectButton);
    remoteLayout->addWidget(remoteUpButton);
    remoteLayout->addWidget(remotePathLabel);
    remoteLayout->addWidget(remoteView);
    
    // Add to Splitter
    splitter->addWidget(localWidget);
    splitter->addWidget(remoteWidget);
    
    mainLayout->addWidget(splitter);

    // Enable context menus
    localView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(localView, &QTreeView::customContextMenuRequested, [this](const QPoint &pos) {
        QMenu menu;
        menu.addAction("Upload", [this]() {
            QModelIndex index = localView->currentIndex();
            QString localPath = localModel->filePath(index);
            if (localPath.isEmpty()) return;
            
            QFileInfo fi(localPath);
            QString remotePath = d->lastRequestedPath + (d->lastRequestedPath.endsWith("/") ? "" : "/") + fi.fileName();
            
            std::vector<FileManagerViewPrivate::Operation> ops;
            d->recursiveScan(localPath, remotePath, ops);
            d->startOperations(ops, "Uploading...", this);
        });
        menu.exec(localView->mapToGlobal(pos));
    });
    
    localView->setDragEnabled(true);
    localView->setAcceptDrops(true);
    localView->setDropIndicatorShown(true);
    localView->setDragDropMode(QAbstractItemView::DragDrop);

    remoteView->setDragEnabled(true);
    remoteView->setAcceptDrops(true);
    remoteView->setDropIndicatorShown(true);
    remoteView->setDragDropMode(QAbstractItemView::DragDrop);

    // Event filter for drops
    localView->viewport()->installEventFilter(this);
    remoteView->viewport()->installEventFilter(this);

    remoteView->setContextMenuPolicy(Qt::CustomContextMenu);
    remoteView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(remoteView, &QTreeView::customContextMenuRequested, [this](const QPoint &pos) {
        QMenu menu;
        menu.addAction("Download", [this]() {
            QModelIndexList selected = remoteView->selectionModel()->selectedRows();
            if (selected.isEmpty()) return;

            QString targetDir = QFileDialog::getExistingDirectory(this, "Select Download Directory");
            if (targetDir.isEmpty()) return;

            std::vector<FileManagerViewPrivate::Operation> ops;
            for (const auto& index : selected) {
                QString remotePath = d->remoteModel->filePath(index);
                QFileInfo fi(remotePath);
                QString localPath = QDir(targetDir).absoluteFilePath(fi.fileName());
                ops.push_back({FileManagerViewPrivate::OpType::DownloadFile, remotePath, localPath});
            }
            d->startOperations(ops, "Downloading...", this);
        });
        menu.addAction("Delete", [this]() {
            QModelIndexList selected = remoteView->selectionModel()->selectedRows();
            if (selected.isEmpty()) return;
            
            auto reply = QMessageBox::question(this, "Confirm Deletion", 
                                             QString("Are you sure you want to delete %1 selected items?").arg(selected.size()),
                                             QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                std::vector<FileManagerViewPrivate::Operation> ops;
                for (const auto& index : selected) {
                    QString path = d->remoteModel->filePath(index);
                    ops.push_back({FileManagerViewPrivate::OpType::DeleteFile, "", path});
                }
                d->startOperations(ops, "Deleting...", this);
            }
        });
        menu.exec(remoteView->mapToGlobal(pos));
    });

    // Navigation Logic
    connect(localUpButton, &QPushButton::clicked, [this]() {
        QModelIndex parent = localView->rootIndex().parent();
        if (parent.isValid()) {
            localView->setRootIndex(parent);
            localPathLabel->setText(localModel->filePath(parent));
        }
    });

    connect(remoteUpButton, &QPushButton::clicked, [this]() {
        QModelIndex parent = remoteView->rootIndex().parent();
        remoteView->setRootIndex(parent);
        remotePathLabel->setText(d->remoteModel->filePath(parent));
    });

    connect(localView, &QTreeView::doubleClicked, [this](const QModelIndex &index) {
        if (localModel->isDir(index)) {
            localView->setRootIndex(index);
            localPathLabel->setText(localModel->filePath(index));
        }
    });

    connect(remoteView, &QTreeView::doubleClicked, [this](const QModelIndex &index) {
        if (d->remoteModel->isDir(index)) {
            remoteView->setRootIndex(index);
            remotePathLabel->setText(d->remoteModel->filePath(index));
        }
    });
}

void FileManagerView::setBleManager(void* manager) {
    // Unused for now as we manage it internally
}

void FileManagerView::onConnectClicked() {
    if (d->bleManager.isConnected()) {
        d->bleManager.disconnect();
        return;
    }
    
    // Show Dialog
    DeviceSelectionDialog dialog(this);
    
    // Start scan immediately or on dialog open
    d->bleManager.startScan([&dialog](const std::string& name, const std::string& address) {
        QMetaObject::invokeMethod(&dialog, [&dialog, name, address]() {
            dialog.addDevice(QString::fromStdString(name), QString::fromStdString(address));
        });
    });
    
    // Handle rescan
    connect(&dialog, &DeviceSelectionDialog::scanRequested, [this, &dialog]() {
         d->bleManager.stopScan();
         dialog.clearDevices();
         d->bleManager.startScan([&dialog](const std::string& name, const std::string& address) {
            QMetaObject::invokeMethod(&dialog, [&dialog, name, address]() {
                dialog.addDevice(QString::fromStdString(name), QString::fromStdString(address));
            });
        });
    });
    
    if (dialog.exec() == QDialog::Accepted) {
        d->bleManager.stopScan();
        QString address = dialog.getSelectedAddress();
        if (!address.isEmpty()) {
             connectButton->setEnabled(false);
             connectButton->setText("Connecting...");
             
             // Use QFutureWatcher to handle async result
             auto watcher = new QFutureWatcher<bool>(this);
             connect(watcher, &QFutureWatcher<bool>::finished, [this, watcher]() {
                 bool success = watcher->result();
                 if (success) {
                     connectButton->setText("Disconnect");
                     connectButton->setEnabled(true);
                     
                     // Step 1: Get Version (Triggered from background or here)
                     d->bleManager.sendCommand(Pixl::Command::GetVersion);
                 } else {
                     connectButton->setText("Connect to Device");
                     connectButton->setEnabled(true);
                     QMessageBox::warning(this, "Connection Failed", "Could not connect to device.");
                 }
                 watcher->deleteLater();
             });

             // Run connect in background
             QFuture<bool> future = QtConcurrent::run([this, address]() {
                 return d->bleManager.connect(address.toStdString());
             });
             watcher->setFuture(future);
        }
    } else {
        d->bleManager.stopScan();
    }
}

void FileManagerView::onFetchRequested(const QString &path) {
    QString actualPath = path;
    // Ensure path format is correct. 
    // If it's just a drive letter like "E", fix to "E:/"
    if (actualPath.length() == 1 && actualPath[0].isLetter()) {
        actualPath += ":/";
    }
    
    d->lastRequestedPath = actualPath;
    qDebug() << "Requesting ReadDir for:" << actualPath;
    
    auto payload = Pixl::Protocol::createStringPayload(actualPath.toStdString());
    d->bleManager.sendCommand(Pixl::Command::ReadDir, payload);
}

void FileManagerView::handleBleData(const std::vector<uint8_t>& data) {
    try {
        auto pkt = Pixl::Protocol::parsePacket(data);
        
        // Append payload to buffer
        d->responseBuffer.insert(d->responseBuffer.end(), pkt.payload.begin(), pkt.payload.end());
        
        // If more data coming, wait
        if (pkt.hasMoreData()) {
            return;
        }
        
        // Process complete response
        std::vector<uint8_t> fullPayload = std::move(d->responseBuffer);
        d->responseBuffer.clear();
        
        if (pkt.cmd == static_cast<uint8_t>(Pixl::Command::GetVersion)) {
            qDebug() << "Got Version, requesting Drive List...";
            d->bleManager.sendCommand(Pixl::Command::GetDriveList);
        }
        else if (pkt.cmd == static_cast<uint8_t>(Pixl::Command::GetDriveList)) {
            qDebug() << "Got Drive List";
            size_t offset = 0;
            if (fullPayload.empty()) return;
            uint8_t count = fullPayload[offset++]; 
            std::vector<Pixl::FileEntry> entries;
            for (uint8_t i = 0; i < count; ++i) {
                if (offset + 2 > fullPayload.size()) break;
                
                Pixl::FileEntry entry;
                uint8_t status = fullPayload[offset++]; 
                char label = static_cast<char>(fullPayload[offset++]);
                entry.name = std::string(1, label) + ":/";
                std::string longName = Pixl::Protocol::parseString(fullPayload, offset);
                
                entry.meta = longName; 
                entry.size = Pixl::Protocol::parseUInt32(fullPayload, offset);
                uint32_t used = Pixl::Protocol::parseUInt32(fullPayload, offset);

                entry.type = 1;
                entries.push_back(entry);
            }
            d->remoteModel->onDirectoryListing("/", entries);
            
            if (!entries.empty()) {
                QString firstDrivePath = QString::fromStdString(entries[0].name);
                onFetchRequested(firstDrivePath);
                
                // Auto-navigate to first drive
                QModelIndex driveIndex = d->remoteModel->indexFromPath(firstDrivePath);
                if (driveIndex.isValid()) {
                    remoteView->setRootIndex(driveIndex);
                    remotePathLabel->setText(firstDrivePath);
                }
            }
        }
        else if (pkt.cmd == static_cast<uint8_t>(Pixl::Command::ReadDir)) {
            if (pkt.status != 0) {
                qDebug() << "ReadDir Failed with status:" << pkt.status;
                return;
            }
            
            size_t offset = 0;
            std::vector<Pixl::FileEntry> entries;
            while(offset < fullPayload.size()) {
                Pixl::FileEntry entry;
                entry.name = Pixl::Protocol::parseString(fullPayload, offset);
                entry.size = Pixl::Protocol::parseUInt32(fullPayload, offset);
                if (offset < fullPayload.size()) entry.type = fullPayload[offset++];
                if (offset < fullPayload.size()) {
                    uint8_t metaLen = fullPayload[offset++];
                    offset += metaLen;
                }

                if (!entry.name.empty())
                   entries.push_back(entry);
                else break;
            }
            
            d->remoteModel->onDirectoryListing(d->lastRequestedPath, entries);
        }
        else if (pkt.cmd == static_cast<uint8_t>(Pixl::Command::OpenFile)) {
            if (pkt.status != 0) {
                qDebug() << "OpenFile failed with status:" << pkt.status;
                d->processNextOperation();
                return;
            }
            if (fullPayload.size() < 1) return;
            d->currentFileId = fullPayload[0];
            
            // Check op type to decide next step
            if (!d->opQueue.empty()) {
                // Actually, currentFile is already set. We just need to know if we are uploading or downloading.
                // We can check if currentFile is open for read or write.
                if (d->currentFile->openMode() == QIODevice::ReadOnly) {
                    d->sendNextChunk();
                } else {
                    std::vector<uint8_t> payload;
                    payload.push_back(d->currentFileId);
                    d->bleManager.sendCommand(Pixl::Command::ReadFile, payload);
                }
            } else {
                // This shouldn't happen if queue-based?
                // Actually, we popped the op already. Let's look at what we are doing.
                // Wait, I should probably keep the 'currentOpType' or similar. 
                // Alternatively, I can check the file's open mode.
                if (d->currentFile && d->currentFile->openMode() == QIODevice::ReadOnly) {
                    d->sendNextChunk();
                } else if (d->currentFile && d->currentFile->openMode() == QIODevice::WriteOnly) {
                    std::vector<uint8_t> payload;
                    payload.push_back(d->currentFileId);
                    d->bleManager.sendCommand(Pixl::Command::ReadFile, payload);
                }
            }
        }
        else if (pkt.cmd == static_cast<uint8_t>(Pixl::Command::ReadFile)) {
            if (pkt.status != 0) {
                qDebug() << "ReadFile failed with status:" << pkt.status;
                d->processNextOperation();
                return;
            }
            if (d->currentFile) {
                d->currentFile->write(reinterpret_cast<const char*>(fullPayload.data()), fullPayload.size());
                d->currentFile->close();
            }
            std::vector<uint8_t> payload;
            payload.push_back(d->currentFileId);
            d->bleManager.sendCommand(Pixl::Command::CloseFile, payload);
        }
        else if (pkt.cmd == static_cast<uint8_t>(Pixl::Command::WriteFile)) {
            if (pkt.status != 0) {
                qDebug() << "WriteFile failed with status:" << pkt.status;
                d->processNextOperation();
                return;
            }
            d->currentOffset += d->CHUNK_SIZE;
            d->sendNextChunk();
        }
        else if (pkt.cmd == static_cast<uint8_t>(Pixl::Command::CloseFile)) {
            if (d->currentFile) d->currentFile->close();
            d->processNextOperation();
        }
        else if (pkt.cmd == static_cast<uint8_t>(Pixl::Command::CreateFolder)) {
            if (pkt.status != 0 && pkt.status != 1) { // 1 might be "already exists"? 
                qDebug() << "CreateFolder failed with status:" << pkt.status;
            }
            d->processNextOperation();
        }
        else if (pkt.cmd == static_cast<uint8_t>(Pixl::Command::Remove)) {
            if (pkt.status != 0) {
                qDebug() << "Remove failed with status:" << pkt.status;
            }
            d->processNextOperation();
        }
    } catch (const std::exception& e) {
        qDebug() << "Packet parse error:" << e.what();
        d->responseBuffer.clear();
    }
}

bool FileManagerView::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::DragEnter) {
        auto *de = static_cast<QDragEnterEvent*>(event);
        if (de->mimeData()->hasUrls()) {
            de->acceptProposedAction();
            return true;
        }
    } else if (event->type() == QEvent::DragMove) {
        auto *de = static_cast<QDragMoveEvent*>(event);
        if (de->mimeData()->hasUrls()) {
            de->acceptProposedAction();
            return true;
        }
    } else if (event->type() == QEvent::Drop) {
        auto *de = static_cast<QDropEvent*>(event);
        
        if (obj == remoteView->viewport()) {
            // Drop onto remote
            QString targetDir = d->lastRequestedPath;
            QModelIndex index = remoteView->indexAt(de->position().toPoint());
            if (index.isValid() && d->remoteModel->isDir(index)) {
                targetDir = d->remoteModel->filePath(index);
            }

            if (de->mimeData()->hasUrls()) {
                std::vector<FileManagerViewPrivate::Operation> ops;
                for (const QUrl &url : de->mimeData()->urls()) {
                    QString localPath = url.toLocalFile();
                    if (!localPath.isEmpty()) {
                        QFileInfo fi(localPath);
                        QString remotePath = targetDir + (targetDir.endsWith("/") ? "" : "/") + fi.fileName();
                        d->recursiveScan(localPath, remotePath, ops);
                    }
                }
                if (!ops.empty()) {
                    d->startOperations(ops, "Uploading...", this);
                    de->acceptProposedAction();
                    return true;
                }
            }
        } else if (obj == localView->viewport()) {
            // Drop onto local (Download)
            QModelIndex index = localView->indexAt(de->position().toPoint());
            QString targetDir = localModel->filePath(index);
            if (targetDir.isEmpty() || !QFileInfo(targetDir).isDir()) {
                targetDir = localModel->filePath(localView->rootIndex());
            }

            // Check if source is remoteView
            QModelIndexList selected = remoteView->selectionModel()->selectedRows();
            if (!selected.isEmpty()) {
                std::vector<FileManagerViewPrivate::Operation> ops;
                for (const auto& remoteIdx : selected) {
                    QString remotePath = d->remoteModel->filePath(remoteIdx);
                    QFileInfo fi(remotePath);
                    QString localPath = QDir(targetDir).absoluteFilePath(fi.fileName());
                    ops.push_back({FileManagerViewPrivate::OpType::DownloadFile, remotePath, localPath});
                }
                d->startOperations(ops, "Downloading...", this);
                de->acceptProposedAction();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}



