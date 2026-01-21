#include "RemoteFileSystemModel.h"
#include <QIcon>
#include <algorithm>

RemoteFileSystemModel::RemoteFileSystemModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    rootNode = new RemoteFileNode{"/", "/", 0, true};
}

RemoteFileSystemModel::~RemoteFileSystemModel()
{
    delete rootNode;
}

void RemoteFileSystemModel::clear()
{
    beginResetModel();
    qDeleteAll(rootNode->children);
    rootNode->children.clear();
    rootNode->fetched = false;
    rootNode->fetching = false;
    endResetModel();
}

void RemoteFileSystemModel::setBleManager(BleManager *manager)
{
    bleManager = manager;
}

QVariant RemoteFileSystemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    RemoteFileNode *node = nodeFromIndex(index);

    if (role == Qt::DisplayRole) {
        if (index.column() == 0) return node->name;
        if (index.column() == 1) return QString::number(node->size);
    }
    if (role == Qt::DecorationRole && index.column() == 0) {
        return node->isDir ? QIcon::fromTheme("folder") : QIcon::fromTheme("text-x-generic");
    }

    return QVariant();
}

Qt::ItemFlags RemoteFileSystemModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant RemoteFileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section == 0) return "Name";
        if (section == 1) return "Size";
    }
    return QVariant();
}

QModelIndex RemoteFileSystemModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    RemoteFileNode *parentNode = nodeFromIndex(parent);
    if (row < parentNode->children.count()) {
        return createIndex(row, column, parentNode->children[row]);
    }

    return QModelIndex();
}

QModelIndex RemoteFileSystemModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    RemoteFileNode *childNode = nodeFromIndex(index);
    RemoteFileNode *parentNode = childNode->parent;

    if (parentNode == rootNode || parentNode == nullptr)
        return QModelIndex();

    // Find row in grandparent
    RemoteFileNode *grandparentNode = parentNode->parent;
    int row = grandparentNode->children.indexOf(parentNode);
    return createIndex(row, 0, parentNode);
}

int RemoteFileSystemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) return 0;
    RemoteFileNode *node = nodeFromIndex(parent);
    return node->children.count();
}

int RemoteFileSystemModel::columnCount(const QModelIndex &parent) const
{
    return 2;
}

bool RemoteFileSystemModel::hasChildren(const QModelIndex &parent) const
{
    RemoteFileNode *node = nodeFromIndex(parent);
    return node->isDir; 
}

bool RemoteFileSystemModel::canFetchMore(const QModelIndex &parent) const
{
    RemoteFileNode *node = nodeFromIndex(parent);
    return node->isDir && !node->fetched;
}

void RemoteFileSystemModel::fetchMore(const QModelIndex &parent)
{
    RemoteFileNode *node = nodeFromIndex(parent);
    if (node->fetched || node->fetching) return;
    
    node->fetching = true;
    emit fetchRequested(node->path);
}

void RemoteFileSystemModel::refresh(const QModelIndex &parent)
{
    RemoteFileNode *node = nodeFromIndex(parent);
    node->fetched = false;
    emit fetchRequested(node->path); // Re-fetch
}

QString RemoteFileSystemModel::filePath(const QModelIndex &index) const
{
    return nodeFromIndex(index)->path;
}

bool RemoteFileSystemModel::isDir(const QModelIndex &index) const
{
    return nodeFromIndex(index)->isDir;
}

QModelIndex RemoteFileSystemModel::indexFromPath(const QString &path) const
{
    QString normalizedSearch = path;
    if (normalizedSearch.endsWith("/") && normalizedSearch.length() > 3) normalizedSearch.chop(1);

    QList<RemoteFileNode*> queue;
    queue.append(rootNode);

    while(!queue.isEmpty()) {
        RemoteFileNode* curr = queue.takeFirst();
        QString currNormalized = curr->path;
        if (currNormalized.endsWith("/") && currNormalized.length() > 3) currNormalized.chop(1);

        if (currNormalized == normalizedSearch) {
            if (curr == rootNode) return QModelIndex();
            int row = curr->parent->children.indexOf(curr);
            return createIndex(row, 0, curr);
        }
        for(auto child : curr->children) {
            queue.append(child);
        }
    }
    return QModelIndex();
}

void RemoteFileSystemModel::onDirectoryListing(const QString &path, const std::vector<Pixl::FileEntry> &entries)
{
    auto sortedEntries = entries;
    std::sort(sortedEntries.begin(), sortedEntries.end(), [](const Pixl::FileEntry& a, const Pixl::FileEntry& b) {
        if (a.type != b.type) return a.type > b.type; // Dirs first
        return a.name < b.name; // Alphabetical
    });

    // Find node by path. Currently only supports finding via traversal or assuming it's for the currently requested one.
    // Simplifying: we traverse from root.
    // In a real app, we might map paths to nodes.
    
    // BFS/DFS to find node with path
    RemoteFileNode* target = nullptr;
    QString normalizedPath = path;
    if (normalizedPath.endsWith("/") && normalizedPath.length() > 3) normalizedPath.chop(1); // Keep drive root as is
    
    QList<RemoteFileNode*> queue;
    queue.append(rootNode);
    
    while(!queue.isEmpty()) {
        RemoteFileNode* curr = queue.takeFirst();
        QString currNormalized = curr->path;
        if (currNormalized.endsWith("/") && currNormalized.length() > 3) currNormalized.chop(1);
        
        if (currNormalized == normalizedPath) {
            target = curr;
            break;
        }
        for(auto child : curr->children) {
            if (child->isDir) queue.append(child);
        }
    }
    
    if (!target) return;
    target->fetching = false;
    
    QModelIndex parentIndex = (target == rootNode) ? QModelIndex() : createIndex(target->parent->children.indexOf(target), 0, target);
    
    // Update children
    // BeginInsert...
    // For simplicity, remove all and add new (if refreshing)
    if (!target->children.isEmpty()) {
        beginRemoveRows(parentIndex, 0, target->children.count() - 1);
        qDeleteAll(target->children);
        target->children.clear();
        endRemoveRows();
    }
    
    if (!sortedEntries.empty()) {
        beginInsertRows(parentIndex, 0, sortedEntries.size() - 1);
        for (const auto& entry : sortedEntries) {
            RemoteFileNode* child = new RemoteFileNode;
            child->name = QString::fromStdString(entry.name);
            if (path == "/") {
                child->path = child->name;
            } else {
                child->path = path + (path.endsWith("/") ? "" : "/") + child->name;
            }
            child->size = entry.size;
            child->isDir = (entry.type == 1);
            child->parent = target;
            target->children.append(child);
        }
        endInsertRows();
    }
    
    target->fetched = true;
}

RemoteFileNode *RemoteFileSystemModel::nodeFromIndex(const QModelIndex &index) const
{
    if (!index.isValid()) return rootNode;
    return static_cast<RemoteFileNode*>(index.internalPointer());
}
