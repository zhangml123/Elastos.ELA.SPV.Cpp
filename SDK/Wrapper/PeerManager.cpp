// Copyright (c) 2012-2018 The Elastos Open Source Project
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "PeerManager.h"

namespace Elastos {
    namespace SDK {

        namespace {

            typedef boost::weak_ptr<PeerManager::Listener> WeakListener;

            static void syncStarted(void *info) {

                WeakListener *listener = (WeakListener *)info;
                if (!listener->expired()) {
                    listener->lock()->syncStarted();
                }
            }

            static void syncStopped(void *info, int error) {

                WeakListener *listener = (WeakListener *)info;
                if (!listener->expired()) {
                    listener->lock()->syncStopped(error == 0 ? "" : strerror(error));
                }
            }

            static void txStatusUpdate(void *info) {

                WeakListener *listener = (WeakListener *)info;
                if (!listener->expired()) {
                    listener->lock()->txStatusUpdate();
                }
            }

            static void saveBlocks(void *info, int replace, BRMerkleBlock *blocks[], size_t blockCount) {

                WeakListener *listener = (WeakListener *)info;
                if (!listener->expired()) {

                    SharedWrapperList<MerkleBlock, BRMerkleBlock *> coreBlocks;
                    for (size_t i = 0; i < blockCount; ++i) {
                        coreBlocks.push_back(MerkleBlockPtr(new MerkleBlock(BRMerkleBlockCopy(blocks[i]))));
                    }
                    listener->lock()->saveBlocks(replace, coreBlocks);
                }
            }

            static void savePeers(void *info, int replace, const BRPeer peers[], size_t count) {

                WeakListener *listener = (WeakListener *) info;
                if (!listener->expired()) {

                    WrapperList<Peer, BRPeer> corePeers;
                    for (size_t i = 0; i < count; ++i) {
                        corePeers.push_back(Peer(peers[i]));
                    }
                    listener->lock()->savePeers(replace, corePeers);
                }
            }

            static int networkIsReachable(void *info) {

                WeakListener *listener = (WeakListener *) info;
                if (!listener->expired()) {

                    listener->lock()->networkIsReachable();
                }
            }

            static void txPublished(void *info, int error) {

                WeakListener *listener = (WeakListener *) info;
                if (!listener->expired()) {

                    listener->lock()->txPublished(error == 0 ? "" : strerror(error));
                }
            }

            static void threadCleanup(void *info) {

                WeakListener *listener = (WeakListener *) info;
                if (!listener->expired()) {

                    //todo complete releaseEnv
                    //releaseEnv();
                }
            }
        }

        PeerManager::PeerManager(ChainParams &params,
                                 const WalletPtr &wallet,
                                 double earliestKeyTime,
                                 const SharedWrapperList<MerkleBlock, BRMerkleBlock *> &blocks,
                                 const WrapperList<Peer, BRPeer> &peers,
                                 const boost::shared_ptr<PeerManager::Listener> &listener) :
            _wallet(wallet) {

            assert(listener != nullptr);
            _listener = boost::weak_ptr<Listener>(listener);

            _manager = BRPeerManagerNew(
                    params.getRaw(),
                    wallet->getRaw(),
                    uint32_t(earliestKeyTime),
                    blocks.getRawPointerArray().data(),
                    blocks.size(),
                    peers.getRawArray().data(),
                    peers.size()
            );

            BRPeerManagerSetCallbacks (_manager, &_listener,
                                       syncStarted,
                                       syncStopped,
                                       txStatusUpdate,
                                       saveBlocks,
                                       savePeers,
                                       networkIsReachable,
                                       threadCleanup);
        }

        PeerManager::~PeerManager() {
            BRPeerManagerFree(_manager);
        }

        std::string PeerManager::toString() const {
            //todo complete me
            return "";
        }

        BRPeerManager *PeerManager::getRaw() const {
            return _manager;
        }

        Peer::ConnectStatus PeerManager::getConnectStatus() const {
            //todo complete me
            return Peer::Unknown;
        }
    }
}