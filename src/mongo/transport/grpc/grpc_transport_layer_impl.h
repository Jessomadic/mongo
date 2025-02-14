/**
 *    Copyright (C) 2023-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include <boost/optional.hpp>
#include <memory>

#include "mongo/transport/client_transport_observer.h"
#include "mongo/transport/grpc/client.h"
#include "mongo/transport/grpc/grpc_transport_layer.h"
#include "mongo/transport/grpc/reactor.h"
#include "mongo/transport/session_manager.h"
#include "mongo/util/duration.h"

namespace mongo::transport::grpc {

class Server;

class GRPCTransportLayerImpl : public GRPCTransportLayer {
public:
    // Note that passing `nullptr` for {sessionManager} will disallow ingress usage.
    GRPCTransportLayerImpl(ServiceContext* svcCtx,
                           Options options,
                           std::unique_ptr<SessionManager> sessionManager);

    /**
     * Create a GRPCTransportLayerImpl instance suitable for ingress (and optionally egress).
     * The instantiated TL will have CommandService pre-attached to route requests via
     * sessionManager->startSession().
     *
     * Note that this TransportLayer will throw during `setup()`
     * if no tlsCertificateKeyFile is available when ingress mode is set.
     */
    static std::unique_ptr<GRPCTransportLayerImpl> createWithConfig(
        ServiceContext*,
        Options options,
        std::vector<std::shared_ptr<ClientTransportObserver>> observers);

    Status registerService(std::unique_ptr<Service> svc) override;

    Status setup() override;

    Status start() override;

    void shutdown() override;

    void stopAcceptingSessions() override;

    StatusWith<std::shared_ptr<Session>> connectWithAuthToken(
        HostAndPort peer,
        ConnectSSLMode sslMode,
        Milliseconds timeout,
        boost::optional<std::string> authToken = boost::none) override;

    StatusWith<std::shared_ptr<Session>> connect(
        HostAndPort peer,
        ConnectSSLMode sslMode,
        Milliseconds timeout,
        const boost::optional<TransientSSLParams>& transientSSLParams = boost::none) override;

    Future<std::shared_ptr<Session>> asyncConnectWithAuthToken(
        HostAndPort peer,
        ConnectSSLMode sslMode,
        const ReactorHandle& reactor,
        Milliseconds timeout,
        std::shared_ptr<ConnectionMetrics> connectionMetrics,
        boost::optional<std::string> authToken = boost::none) override;

    Future<std::shared_ptr<Session>> asyncConnect(
        HostAndPort peer,
        ConnectSSLMode sslMode,
        const ReactorHandle& reactor,
        Milliseconds timeout,
        std::shared_ptr<ConnectionMetrics> connectionMetrics,
        std::shared_ptr<const SSLConnectionContext> transientSSLContext) override;

    void appendStatsForServerStatus(BSONObjBuilder* bob) const override {
        if (!_client) {
            return;
        }

        _client->appendStats(bob);
    }

#ifdef MONGO_CONFIG_SSL
    Status rotateCertificates(std::shared_ptr<SSLManagerInterface> manager,
                              bool asyncOCSPStaple) override;
#endif

    ReactorHandle getReactor(WhichReactor which) override {
        switch (which) {
            case TransportLayer::kIngress:
                MONGO_UNIMPLEMENTED;
            case TransportLayer::kEgress:
                return _egressReactor;
            case TransportLayer::kNewReactor:
                return std::make_shared<GRPCReactor>();
        }

        MONGO_UNREACHABLE;
    }

    const std::vector<HostAndPort>& getListeningAddresses() const override;

    SessionManager* getSessionManager() const override {
        return _sessionManager.get();
    }

    std::shared_ptr<SessionManager> getSharedSessionManager() const override {
        return _sessionManager;
    }

    bool isIngress() const override {
        return _options.enableIngress;
    }

    bool isEgress() const override {
        return _options.enableEgress;
    }

private:
    mutable stdx::mutex _mutex;
    bool _isShutdown = false;

    std::shared_ptr<Client> _client;
    std::unique_ptr<Server> _server;
    ServiceContext* const _svcCtx;

    /**
     * The GRPCTransportLayer starts an egress reactor on an _ioThread that is provided to
     * streams/sessions on calls to the synchronous connect() function. Providing this default
     * reactor allows use of the synchronous transport layer APIs with gRPC's async completion queue
     * API.
     */
    stdx::thread _ioThread;
    std::shared_ptr<GRPCReactor> _egressReactor;

    // Invalidated after setup().
    std::vector<std::unique_ptr<Service>> _services;
    Options _options;
    std::shared_ptr<SessionManager> _sessionManager;
};

}  // namespace mongo::transport::grpc
