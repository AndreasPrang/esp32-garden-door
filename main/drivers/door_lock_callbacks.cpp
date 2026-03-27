/*
 * SPDX-License-Identifier: MIT
 * Garden Door Controller – Minimal Door Lock cluster callbacks
 *
 * Provides the mandatory callbacks required by the Matter Door Lock cluster
 * server. This is a simplified implementation without PIN/credential support –
 * any lock/unlock command is accepted.
 */

#include <esp_log.h>
#include <lib/core/DataModelTypes.h>
#include <app/clusters/door-lock-server/door-lock-server.h>

using namespace chip;
using namespace chip::app::Clusters::DoorLock;

static const char *TAG = "lock_cb";

/* Called when the Door Lock cluster is initialized on an endpoint */
void emberAfDoorLockClusterInitCallback(EndpointId endpoint)
{
    DoorLockServer::Instance().InitServer(endpoint);
    ESP_LOGI(TAG, "Door Lock cluster initialized on endpoint %d", endpoint);
}

/* Handle Lock command – always succeeds (no PIN required) */
bool emberAfPluginDoorLockOnDoorLockCommand(EndpointId endpointId,
                                            const Nullable<FabricIndex> &fabricIdx,
                                            const Nullable<NodeId> &nodeId,
                                            const Optional<ByteSpan> &pinCode,
                                            OperationErrorEnum &err)
{
    ESP_LOGI(TAG, "Lock command on endpoint %d", endpointId);
    DoorLockServer::Instance().SetLockState(endpointId,
        DlLockState::kLocked, OperationSourceEnum::kRemote,
        NullNullable, NullNullable, fabricIdx, nodeId);
    return true;
}

/* Handle Unlock command – always succeeds (no PIN required) */
bool emberAfPluginDoorLockOnDoorUnlockCommand(EndpointId endpointId,
                                              const Nullable<FabricIndex> &fabricIdx,
                                              const Nullable<NodeId> &nodeId,
                                              const Optional<ByteSpan> &pinCode,
                                              OperationErrorEnum &err)
{
    ESP_LOGI(TAG, "Unlock command on endpoint %d", endpointId);
    DoorLockServer::Instance().SetLockState(endpointId,
        DlLockState::kUnlocked, OperationSourceEnum::kRemote,
        NullNullable, NullNullable, fabricIdx, nodeId);
    return true;
}

/* Auto-relock callback */
void emberAfPluginDoorLockOnAutoRelock(EndpointId endpointId)
{
    ESP_LOGI(TAG, "Auto-relock on endpoint %d", endpointId);
    DoorLockServer::Instance().SetLockState(endpointId,
        DlLockState::kLocked, OperationSourceEnum::kAuto);
}
