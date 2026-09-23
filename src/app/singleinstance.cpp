#include "singleinstance.hpp"

#include <QLocalSocket>

SingleInstance::SingleInstance(const QString& key, QObject* parent) : QObject(parent), key_(key) {
    QLocalSocket probe;
    probe.connectToServer(key_);
    if (probe.waitForConnected(300)) {
        probe.disconnectFromServer();
        return;
    }
    // A crashed primary leaves a stale socket behind on Unix; removing it is harmless on Windows.
    QLocalServer::removeServer(key_);
    if (!server_.listen(key_)) {
        return;
    }
    primary_ = true;
    connect(&server_, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* client = server_.nextPendingConnection()) {
            connect(client, &QLocalSocket::readyRead, this, [this, client] {
                emit commandReceived(client->readAll().trimmed());
                client->disconnectFromServer();
            });
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        }
    });
}

bool SingleInstance::notifyPrimary(const QByteArray& command) {
    QLocalSocket socket;
    socket.connectToServer(key_);
    if (!socket.waitForConnected(1000)) {
        return false;
    }
    socket.write(command + '\n');
    socket.flush();
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}
