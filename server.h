#pragma once

/*
 * Server always start at ServerState::INIT.
 * GetState() is only called when server starts, to know other's state. Until
 * this information is known, no client request will be served.
 * Both HeartbeatClient::GetState()'s return && HeartbeatService::GetState()
 * (request handling code) can change ServerState.
 * - ServerState::INIT:
 *   - (BlockStoreService) Return "I am INIT" message.
 *   - (HeartbeatClient) keep issuing GetState() to ask the other node's state
 *     until getting the answer.
 *     - Recheck state is still INIT. Otherwise no-op.
 *     - If the other node is BACKUP: This occurs when both start as INIT and my
 * node number is smaller than the other. Turn myself to PRIMARY.
 *     - If the other node is PRIMARY: It means I was a recovered BACKUP. Turn
 * my state to BACKUP.
 *   - (HeartbeatService)
 *     - on receiving GetState():
 *       - Both nodes are in INIT. If node_num == 0, change state to
 * PRIMARY; if node_num == 1, change state to BACKUP. Return changed state.
 *     - on receiving RepliWrite():
 *       - Change state to BACKUP. Assert this (addr, data) is empty (i.e. this
 * is a heartbeat, not a write request). Ack.
 *
 * - ServerState::BACKUP:
 *   - (BlockStoreService) return "I am Backup" message.
 *   - (HeartbeatService) receive GetState():
 *     - previous PRIMARY was dead. Change itself to PRIMARY and return
 *   - (HeartbeatClient) should never be used.
 *
 * - ServerState::PRIMARY
 *   - (BlockStoreService) Do the logic.
 *   - (HeartbeatService) 
 *     - receive GetState(): return PRIMARY
 *     - receive RepliWrite(): this could happen when network failure:
 *       - change state to INIT
 *       - reply grpc::status (ABORTED)
 *   - (HeartbeatClient) Do the logic.
 */

enum ServerState { INIT, PRIMARY, BACKUP };
