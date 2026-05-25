#include "services/SpeedTestPortReservation.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

namespace {

const QString kLoopbackAddress = QStringLiteral("127.0.0.1");

} // namespace

namespace SpeedTestPortReservation {

Ports takeAvailable()
{
    for (int attempt = 0; attempt < 64; ++attempt) {
        QTcpServer socksProbe;
        if (!socksProbe.listen(QHostAddress::LocalHost, 0)) {
            continue;
        }

        const int candidate = socksProbe.serverPort();
        if (candidate <= 0) {
            continue;
        }

        QTcpServer httpProbe;
        if (!httpProbe.listen(QHostAddress::LocalHost, 0)) {
            continue;
        }
        const int httpPort = httpProbe.serverPort();
        if (httpPort <= 0 || httpPort == candidate) {
            continue;
        }

        QTcpServer locationProbe;
        if (!locationProbe.listen(QHostAddress::LocalHost, 0)) {
            continue;
        }
        const int locationProbePort = locationProbe.serverPort();
        if (locationProbePort <= 0
            || locationProbePort == candidate
            || locationProbePort == httpPort) {
            continue;
        }

        socksProbe.close();
        httpProbe.close();
        locationProbe.close();
        if (SpeedTestServiceInternal::reserveProxyPorts(candidate, httpPort, locationProbePort)) {
            return Ports{candidate, httpPort, locationProbePort};
        }
    }

    return {};
}

void release(const Ports& ports)
{
    SpeedTestServiceInternal::releaseProxyPorts(
        ports.socksPort,
        ports.httpPort,
        ports.locationProbePort);
}

bool isProxyPortReady(int port)
{
    QTcpSocket socket;
    socket.connectToHost(kLoopbackAddress, port);
    if (socket.waitForConnected(200)) {
        socket.disconnectFromHost();
        return true;
    }

    return false;
}

std::optional<SpeedTestServiceInternal::ReadyProxy> waitForProxy(
    const int socksPort,
    const int httpPort,
    int timeoutMs,
    const std::atomic_bool& cancelled,
    const std::function<bool()>& hasProcessExited)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (cancelled.load()) {
            return std::nullopt;
        }

        if (hasProcessExited && hasProcessExited()) {
            return std::nullopt;
        }

        if (const auto readyProxy = SpeedTestServiceInternal::detectReadyProxy(
                socksPort,
                httpPort,
                [](int port) { return isProxyPortReady(port); });
            readyProxy.has_value()) {
            return readyProxy;
        }

        if (cancelled.load()) {
            return std::nullopt;
        }

        QThread::msleep(50);
    }

    return std::nullopt;
}

} // namespace SpeedTestPortReservation
