#pragma once
/// Stable Android thermal status ABI; newer nonnegative values remain observations rather than quality policies.
enum AThermalStatus {
    ATHERMAL_STATUS_ERROR=-1,ATHERMAL_STATUS_NONE=0,ATHERMAL_STATUS_LIGHT=1,ATHERMAL_STATUS_MODERATE=2,
    ATHERMAL_STATUS_SEVERE=3,ATHERMAL_STATUS_CRITICAL=4,ATHERMAL_STATUS_EMERGENCY=5,ATHERMAL_STATUS_SHUTDOWN=6
};
/// SDK manager remains opaque to the production reader.
struct AThermalManager;
