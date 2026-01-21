#pragma once

#include <QAbstractItemModel>
#include <QVector>
#include <QString>
#include <QVariant>
#include <memory>
#include <map>
#include "../protocol/PixlProtocol.h"

class BleManager;

struct RemoteFileNode {
    QString name;
    QString path;
    uint32_t size;
    bool isDir;
    RemoteFileNode *parent = nullptr;
    QVector<RemoteFileNode*> children;
    bool fetched = false;
    bool fetching = false;

    ~RemoteFileNode() {
        qDeleteAll(children);
    }
};

class RemoteFileSystemModel : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit RemoteFileSystemModel(QObject *parent = nullptr);
    ~RemoteFileSystemModel();

    void setBleManager(BleManager* manager);
    void clear();

    // QAbstractItemModel interface
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

    QString filePath(const QModelIndex &index) const;
    bool isDir(const QModelIndex &index) const;
    QModelIndex indexFromPath(const QString &path) const;

    // Actions
    void refresh(const QModelIndex &parent);

signals:
    void fetchRequested(const QString &path);

public slots:
    void onDirectoryListing(const QString &path, const std::vector<Pixl::FileEntry>& entries);

private:
    BleManager* bleManager = nullptr;
    RemoteFileNode* rootNode;
    
    RemoteFileNode* nodeFromIndex(const QModelIndex &index) const;
};
