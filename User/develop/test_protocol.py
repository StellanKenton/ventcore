"""Host regression for MCM wire framing, caches and ventilation settings binding."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''

/************************************************************************************
* @file     : protocol_test.c
* @brief    : Production protocol framing and device binding regression.
***********************************************************************************/
#include <assert.h>
#include <string.h>
#include <math.h>
#include "ProtoclOfMcm.h"
#include "breathscheduler.h"
#include "phasecontroller.h"
#include "monitorengine.h"
#include "controldata.h"
#include "rtos.h"
#include "log.h"
static uint8_t gRx[2048], gTx[256];
static uint16_t gRxSize, gTxSize;
static uint8_t gRunning;
static eVentMode gMode = VENT_MD_IDLE;
static unsigned gUpdates;
static bool gUartBusy;
static uint32_t gHeartbeatTx;
void repRtosEnterCritical(void) {}
void repRtosExitCritical(void) {}
void logWrite(eLogLevel level, const char *tag, const char *format, ...) {}
bool uartIsTxBusy(uint8_t instance) { return gUartBusy; }
uint16_t uartGetRxDataCount(uint8_t instance) { return gRxSize; }
int8_t uartGetRxData(uint8_t instance, uint8_t *data, uint16_t length) {
    assert(length <= gRxSize); memcpy(data, gRx, length);
    memmove(gRx, gRx + length, gRxSize - length); gRxSize -= length;
    return UART_STATUS_OK;
}
int8_t uartSendData(uint8_t instance, const uint8_t *data, uint16_t length) {
    if (gUartBusy) { return UART_ERROR_BUSY; }
    memcpy(gTx, data, length); gTxSize = length;
    if (length == 7U && data[0] == 0xFE && data[1] == 0xFF && data[2] == 0x7F) {
        assert(data[3] == 1U && data[4] == 0U && ProtocolCheckCRC(data));
        ++gHeartbeatTx;
    }
    return UART_STATUS_OK;
}
uint8_t breathSchedulerRunningGet(void) { return gRunning; }
eVentMode breathSchedulerModeGet(void) { return gMode; }
int8_t breathSchedulerSettingsUpdate(eVentMode mode) {
    if (mode != VENT_MD_PAC && mode != VENT_MD_VAC && mode != VENT_MD_CPAP_PSV && mode != VENT_MD_PSV_ST) { return -1; }
    gMode = mode; ++gUpdates; return 1;
}
int8_t breathSchedulerStart(eVentMode mode) {
    int8_t status = breathSchedulerSettingsUpdate(mode);
    if (status == 1) { gRunning = 1; } return status;
}
int8_t breathSchedulerStop(void) { gRunning = 0; return 1; }
ePhaseControllerState phaseControllerStateGet(void) { return PHASE_INSP; }
float controlDataGet(ControlData_Index_EnumDef index) { return index == PAT_REAL_PRS ? 12.3f : -25.0f; }
float monitorEngineGet(eMonitorDataType type) { return 456.0f; }

/** Deliver bytes through the production UART-to-parser path. */
static void feed(const uint8_t *data, uint16_t length) {
    memcpy(gRx + gRxSize, data, length); gRxSize += length;
    assert(ProtocolReceiveData(0) == PROTOCOL_OK);
}
/** Create a single-sub-ID MCM frame using the original wire format. */
static uint16_t frame(uint8_t *data, uint8_t mid, uint8_t id, uint32_t value, uint8_t size, uint8_t scale) {
    SubIDCache_t item = {.m_id=id, .m_value=value, .m_size=size, .m_scale=scale};
    return ProtocolCreateSubIdData(data, PROTOCOL_ADDR_MCM_TO_VCM, false, mid, (const uint8_t *)&item, 1);
}
/** Receive one setting, then let VentTask consume it. */
static void send(uint8_t mid, uint8_t id, uint32_t value, uint8_t size, uint8_t scale) {
    uint8_t bytes[32]; uint16_t length = frame(bytes, mid, id, value, size, scale);
    feed(bytes, length); assert(ProtocolProcessRxData(0) == PROTOCOL_OK);
    protocolApplyReceivedSettings();
}
/** Exercise the full task processing path with continuous and burst heartbeats. */
static void testHeartbeat(void) {
    uint8_t heartbeat[7];
    stProtocolHeartbeatStats stats;
    gRunning = 0;
    ProtocolProcessInit(0);
    uint16_t length = ProtocolCreateDirectData(heartbeat, PROTOCOL_ADDR_MCM_TO_VCM, false, 0x7F, NULL, 0);
    assert(length == 7);
    /* Every request receives exactly one response, without an ACK/retry storm. */
    for (unsigned i = 0; i < 1000; ++i) {
        memcpy(gRx, heartbeat, length); gRxSize = length;
        ProtocolProcessMain(0);
        assert(gHeartbeatTx == i + 1U);
    }
    for (unsigned i = 0; i < 510; ++i) { ProtocolProcessMain(0); }
    assert(gHeartbeatTx == 1000U && !ProtocolIsMCMConnected());
    /* A burst cannot collapse into one reply. Busy transport must retain replies. */
    gUartBusy = true;
    for (unsigned i = 0; i < 8; ++i) { memcpy(gRx + i * 7, heartbeat, 7); }
    gRxSize = 56;
    for (unsigned i = 0; i < 8; ++i) { ProtocolProcessMain(0); }
    assert(gHeartbeatTx == 1000U && ProtocolIsMCMConnected());
    gUartBusy = false;
    for (unsigned i = 0; i < 8; ++i) { ProtocolProcessMain(0); }
    assert(gHeartbeatTx == 1008U);
    /* Fill the high-priority queue; keep pending responses until space is available. */
    gUartBusy = true;
    for (unsigned i = 0; i < 100; ++i) {
        memcpy(gRx, heartbeat, 7); gRxSize = 7; ProtocolProcessMain(0);
    }
    protocolHeartbeatStatsGet(&stats);
    assert(stats.received == 1108U && stats.pending > 0U && stats.overflow == 0U);
    gUartBusy = false;
    for (unsigned i = 0; i < 110; ++i) { ProtocolProcessMain(0); }
    protocolHeartbeatStatsGet(&stats);
    assert(stats.received == 1108U && stats.transmitted == 1108U && stats.pending == 0U);
    /* CRC-corrupt and fragmented requests cannot elicit an early reply. */
    heartbeat[6] ^= 1;
    memcpy(gRx, heartbeat, 7); gRxSize = 7; ProtocolProcessMain(0);
    assert(gHeartbeatTx == 1108U);
    ProtocolProcessInit(0);
    heartbeat[6] ^= 1;
    memcpy(gRx, heartbeat, 4); gRxSize = 4; ProtocolProcessMain(0);
    assert(gHeartbeatTx == 1108U);
    memcpy(gRx, heartbeat + 4, 3); gRxSize = 3; ProtocolProcessMain(0);
    assert(gHeartbeatTx == 1109U);
    /* A request with needAck retains generic echo behavior plus one heartbeat reply. */
    ProtocolCreateDirectData(heartbeat, PROTOCOL_ADDR_MCM_TO_VCM, true, 0x7F, NULL, 0);
    memcpy(gRx, heartbeat, 7); gRxSize = 7; ProtocolProcessMain(0);
    assert(gTxSize == 7 && memcmp(gTx, heartbeat, 7) == 0);
    ProtocolProcessMain(0); assert(gHeartbeatTx == 1110U);
    for (unsigned i = 0; i < 510; ++i) { ProtocolProcessMain(0); }
    assert(gHeartbeatTx == 1110U);
}

int main(void) {
    uint8_t bytes[256];
    ProtocolPacket_t packet;
    assert(ProtocolProcessInit(0) == PROTOCOL_OK);
    assert(!ProtocolIsMCMConnected());
    float fixedVt = GetVentVacSettings()->tidalVolume;
    float fixedHigh = GetVentLimitSettings()->pressureHigh;
    uint16_t length = frame(bytes, 0xAF, 0x0E, 650, 2, 0);
    feed(bytes, 4); assert(ProtocolProcessRxData(0) == PROTOCOL_INVALID_PACKET);
    assert(!ProtocolGetRxVentParamsCache()->m_valid[14]);
    feed(bytes + 4, length - 4); assert(ProtocolProcessRxData(0) == PROTOCOL_OK);
    assert(ProtocolGetRxVentParamsCache()->m_tidalVolume == 650);
    assert(ProtocolIsMCMConnected());
    protocolApplyReceivedSettings(); assert(GetVentVacSettings()->tidalVolume == fixedVt);
    send(0xAF, 1, 1, 1, 0); /* VAC mode alone cannot start ventilation. */
    assert(!gRunning);
    send(0xAC, 1, 550, 2, 1);
    assert(ProtocolGetRxAlarmLimitsCache()->m_pAirwayHigh == 550);
    assert(GetVentLimitSettings()->pressureHigh == fixedHigh);
    GetVentPatientSettings()->useHostSettings = 1;
    protocolApplyReceivedSettings();
    assert(GetVentVacSettings()->tidalVolume == 650);
    assert(GetVentLimitSettings()->pressureHigh == 55.0f);
    send(0xAF, 0x17, 125, 2, 2); assert(GetVentVacSettings()->inspTimeMs == 1250);
    send(0xAF, 0x12, (uint16_t)-20, 2, 1); assert(GetVentVacSettings()->pressureTriggerCmh2o == -2.0f);
    send(0xAE, 0, 1, 1, 0); assert(gRunning && gMode == VENT_MD_VAC);
    gTxSize = 0; ProtocolWaveDataProcess(0, 20); ProtocolSchedulerProcess(0);
    assert(gTxSize == 18 && gTx[0] == 0xFE && gTx[1] == 0xFF && gTx[2] == 0xAF);
    assert(ProtocolCheckCRC(gTx) && gTx[4] == 11 && gTx[5] == 1);
    assert((gTx[6] | (gTx[7] << 8)) == 123);
    assert((gTx[8] | (gTx[9] << 8)) == 1750);
    assert((gTx[10] | (gTx[11] << 8)) == 456);
    assert(gTx[12] == 0 && gTx[13] == 0 && gTx[14] == 0 && gTx[15] == 20);
    send(0xAE, 0, 0, 1, 0); assert(!gRunning);
    gTxSize = 0; ProtocolWaveDataProcess(0, 40); ProtocolSchedulerProcess(0); assert(gTxSize == 0);
    send(0xAE, 0, 2, 1, 0); assert(!gRunning && ProtocolGetRxVentSwitchCache()->m_command == 0);
    GetVentPatientSettings()->useHostSettings = 0; protocolApplyReceivedSettings();
    assert(GetVentVacSettings()->tidalVolume == fixedVt && GetVentLimitSettings()->pressureHigh == fixedHigh);
    send(0xAE, 0, 1, 1, 0); assert(gRunning && gMode == VENT_MD_VAC);
    unsigned updates = gUpdates;
    send(0xAF, 0x0E, 700, 2, 0); assert(gUpdates == updates && GetVentVacSettings()->tidalVolume == fixedVt);
    send(0xAE, 0, 0, 1, 0);
    GetVentPatientSettings()->useHostSettings = 1; protocolApplyReceivedSettings();
    assert(GetVentVacSettings()->tidalVolume == 700);
    /* Bad CRC must not alter a valid cached value. */
    length = frame(bytes, 0xAF, 14, 800, 2, 0); bytes[length-1] ^= 1;
    feed(bytes, length); assert(ProtocolProcessRxData(0) == PROTOCOL_CRC_ERROR);
    assert(ProtocolGetRxVentParamsCache()->m_tidalVolume == 700);
    ProtocolProcessInit(0); /* Clear transport only. */
    /* CRC-valid malformed payload must fail atomically before callback/ACK. */
    length = frame(bytes, 0xAF, 14, 800, 2, 0); bytes[6] = ProtocolGetSubIdLength(3, true, 0);
    uint16_t crc = Crc16Compute(bytes + 2, length - 4); bytes[length-2] = crc; bytes[length-1] = crc >> 8;
    feed(bytes, length); assert(ProtocolProcessRxData(0) == PROTOCOL_ERROR);
    assert(ProtocolGetRxVentParamsCache()->m_tidalVolume == 700);
    assert(ProtocolParsePacket(bytes, 4, &packet) != PROTOCOL_OK);
    assert(ProtocolParsePacket(bytes, length-1, &packet) != PROTOCOL_OK);
    /* Valid requested ACK echoes the exact received frame. */
    length = frame(bytes, 0xAF, 14, 750, 2, 0); bytes[3] = 1;
    crc = Crc16Compute(bytes + 2, length - 4); bytes[length-2] = crc; bytes[length-1] = crc >> 8;
    feed(bytes, length); assert(ProtocolProcessRxData(0) == PROTOCOL_OK);
    ProtocolSchedulerProcess(0); assert(gTxSize == length && memcmp(bytes, gTx, length) == 0);
    testHeartbeat();
    return 0;
}
/**************************End of file********************************/

'''

def main():
    compiler = os.environ.get("CC") or shutil.which("gcc") or shutil.which("clang")
    if not compiler:
        compiler = next((str(path) for path in (
            Path("C:/msys64/mingw64/bin/gcc.exe"),
            Path("C:/Qt/Tools/mingw1310_64/bin/gcc.exe"),
        ) if path.is_file()), None)
    if not compiler:
        raise SystemExit("Set CC to a native GCC or Clang compiler for this host test.")
    with tempfile.TemporaryDirectory(prefix="ventcore-protocol-") as directory:
        harness = Path(directory) / "protocol_test.c"
        harness.write_text(HARNESS, encoding="utf-8", newline="\n")
        executable = Path(directory) / "protocol_test.exe"
        includes = ["user/app/protocol", "user/bsp/uart", "user/app/databus", "user/app/ventlogic",
                    "user/app/ventalgo", "user/module/log", "user/module/rtos", "user/tools/ringbuffer",
                    "user/tools/controller"]
        sources = ["user/app/protocol/ProtoclOfMcm.c", "user/app/protocol/ProtoclOfPackets.c",
                   "user/app/protocol/ProtoclOfProcess.c", "user/app/databus/settingdata.c",
                   "user/tools/ringbuffer/ringbuffer.c"]
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                   *[f"-I{ROOT / path}" for path in includes], str(harness),
                   *[str(ROOT / path) for path in sources], "-o", str(executable)]
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment["PATH"]
        subprocess.run(command, check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
    print("PASS: fragmented RX, CRC rejection, parameter/alarm caches, scaling, source switching, start/stop, waveform, ACK, 1110 heartbeat replies, bursts, TX backpressure and reconnect")

if __name__ == "__main__":
    main()
