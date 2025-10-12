#include "ble_mesh_at.h"
#include "NuMicro.h"
#include <string.h>

// UART1 發送字串（阻塞式）
static void uart1_send_string(const char *str)
{
    while (*str)
    {
        while (UART_GET_TX_FULL(UART1))
            ;
        UART_WRITE(UART1, (uint8_t)*str++);
    }
}

// 處理接收到的單個位元組
static void handle_rx_byte(ble_mesh_at_controller_t *controller, uint8_t ch)
{
    if (controller->line_ready)
    {
        // 尚未處理上一行，丟棄新資料以避免覆蓋
        controller->diag.drop_byte_count++;
        return;
    }

    if (ch == '\r')
    {
        // 將 CR 視為行結束（有些模組僅回 CR 或 CRLF）
        controller->rx_line[controller->rx_line_len] = '\0';
        // 快照
        controller->diag.last_line_len = controller->rx_line_len;
        {
            uint32_t i;
            for (i = 0; i < sizeof(controller->diag.last_line) - 1 && controller->rx_line[i]; ++i)
                controller->diag.last_line[i] = controller->rx_line[i];
            controller->diag.last_line[i] = '\0';
        }
        controller->diag.line_count++;
        controller->line_ready = true;
        controller->seen_cr = false;
        controller->rx_line_len = 0;
        return;
    }

    if (ch == '\n')
    {
        // 將 LF 也視為行結束（容忍僅 LF 結尾）
        controller->rx_line[controller->rx_line_len] = '\0';
        // 快照
        controller->diag.last_line_len = controller->rx_line_len;
        {
            uint32_t i;
            for (i = 0; i < sizeof(controller->diag.last_line) - 1 && controller->rx_line[i]; ++i)
                controller->diag.last_line[i] = controller->rx_line[i];
            controller->diag.last_line[i] = '\0';
        }
        controller->diag.line_count++;
        controller->line_ready = true;
        controller->seen_cr = false;
        controller->rx_line_len = 0;
        return;
    }

    controller->seen_cr = false;
    if (controller->rx_line_len < sizeof(controller->rx_line) - 1)
    {
        controller->rx_line[controller->rx_line_len++] = (char)ch;
    }
    else
    {
        // 超長，丟棄這行直到下一個 CRLF
        controller->rx_line_len = 0;
    }
}

// 初始化 BLE MESH AT 控制器
void ble_mesh_at_init(ble_mesh_at_controller_t *controller,
                      const ble_mesh_at_config_t *config,
                      void (*event_callback)(ble_mesh_at_event_t, const char *),
                      uint32_t (*get_time_ms)(void))
{
    if (!controller || !config || !get_time_ms)
        return;

    // 複製配置
    controller->config = *config;
    controller->event_callback = event_callback;
    controller->get_time_ms = get_time_ms;

    // 初始化狀態
    controller->state = BLE_MESH_AT_STATE_IDLE;
    controller->line_ready = false;
    controller->rx_line_len = 0;
    controller->seen_cr = false;
    controller->ver_sent = false;
    controller->last_command_time = 0;
    controller->response_timeout_ms = 5000; // 5 秒逾時

    // 清診斷欄位
    memset(&controller->diag, 0, sizeof(controller->diag));

    // 預設未綁定，清空 UID
    controller->is_bound = false;
    controller->uid[0] = '\0';

    // 初始化 UART1
    SYS_UnlockReg(); // 解鎖系統暫存器
    CLK_EnableModuleClock(UART1_MODULE);
    CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UART1SEL_HXT, CLK_CLKDIV0_UART1(1));

    // 設定腳位 MFP（PA.8=RX, PA.9=TX）
    SYS->GPA_MFPH &= ~(SYS_GPA_MFPH_PA8MFP_Msk | SYS_GPA_MFPH_PA9MFP_Msk);
    SYS->GPA_MFPH |= (SYS_GPA_MFPH_PA8MFP_UART1_RXD | SYS_GPA_MFPH_PA9MFP_UART1_TXD);
    SYS_LockReg(); // 重新上鎖

    // 開啟 UART1
    UART_Open(UART1, controller->config.baudrate);

    // 啟用中斷
    UART_ENABLE_INT(UART1, (UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk));
    NVIC_EnableIRQ(UART1_IRQn);

    controller->initialized = true;

    // 可於偵錯器觀察 controller->diag 與 state
}

// 更新函數（在主迴圈中調用）
void ble_mesh_at_update(ble_mesh_at_controller_t *controller)
{
    if (!controller || !controller->initialized)
        return;

    uint32_t current_time = controller->get_time_ms();

    // 處理接收到的行
    if (controller->line_ready)
    {
        // 記憶體屏障，保證 line_ready 與 rx_line 的順序
        __DSB();

        // 分析接收到的行（容忍前後可能有其他資訊）
        if (strstr(controller->rx_line, "VER-MSG SUCCESS") != NULL)
        {
            controller->state = BLE_MESH_AT_STATE_VER_OK;
            controller->ver_sent = false; // 完成本次命令
            controller->diag.last_event = BLE_MESH_AT_EVENT_VER_SUCCESS;
            if (controller->event_callback)
            {
                controller->event_callback(BLE_MESH_AT_EVENT_VER_SUCCESS, controller->rx_line);
            }
        }
        else if (strstr(controller->rx_line, "REBOOT-MSG SUCCESS") != NULL)
        {
            // 重新啟動成功的回覆
            controller->diag.last_event = BLE_MESH_AT_EVENT_REBOOT_SUCCESS;
            controller->state = BLE_MESH_AT_STATE_IDLE; // 等待後續 SYS-MSG 狀態
            controller->ver_sent = false;
            if (controller->event_callback)
            {
                controller->event_callback(BLE_MESH_AT_EVENT_REBOOT_SUCCESS, controller->rx_line);
            }
        }
        else if (strstr(controller->rx_line, "SYS-MSG DEVICE PROV-ED") != NULL ||
                 strstr(controller->rx_line, "PROV-MSG SUCCESS") != NULL)
        {
            // 嘗試擷取 UID（尋找倒數第一個以 0x 開始的 token）
            const char *line = controller->rx_line;
            const char *uid_ptr = NULL;
            // 從尾端往前掃描空白分隔，簡化：找到第一個包含 "0x" 的子字串
            const char *p = line;
            const char *last_0x = NULL;
            while ((p = strstr(p, "0x")) != NULL)
            {
                last_0x = p;
                p += 2;
            }
            if (last_0x)
            {
                uid_ptr = last_0x;
                // 複製到 controller->uid，遇到空白或行尾停止，限制長度
                uint32_t i = 0;
                while (uid_ptr[i] && uid_ptr[i] != ' ' && i < sizeof(controller->uid) - 1)
                {
                    controller->uid[i] = uid_ptr[i];
                    i++;
                }
                controller->uid[i] = '\0';
            }
            controller->is_bound = true;
            controller->diag.last_event = BLE_MESH_AT_EVENT_PROV_BOUND;
            controller->state = BLE_MESH_AT_STATE_IDLE;
            controller->ver_sent = false;
            if (controller->event_callback)
            {
                controller->event_callback(BLE_MESH_AT_EVENT_PROV_BOUND, controller->rx_line);
            }
        }
        else if (strstr(controller->rx_line, "SYS-MSG DEVICE UNPROV") != NULL)
        {
            controller->is_bound = false;
            controller->uid[0] = '\0';
            controller->diag.last_event = BLE_MESH_AT_EVENT_PROV_UNBOUND;
            controller->state = BLE_MESH_AT_STATE_IDLE;
            controller->ver_sent = false;
            if (controller->event_callback)
            {
                controller->event_callback(BLE_MESH_AT_EVENT_PROV_UNBOUND, controller->rx_line);
            }
        }
        else if (strstr(controller->rx_line, "NR-MSG SUCCESS") != NULL)
        {
            // 自我解除綁定的回覆可忽略（不改變 LED 與狀態），但允許再次發送
            controller->state = BLE_MESH_AT_STATE_IDLE;
            controller->ver_sent = false;
            controller->diag.last_event = BLE_MESH_AT_EVENT_LINE_RECEIVED; // 維持一般行事件
        }
        else
        {
            // 一般回應：回到 IDLE 以允許重新發送
            controller->state = BLE_MESH_AT_STATE_IDLE;
            controller->ver_sent = false;
            controller->diag.last_event = BLE_MESH_AT_EVENT_LINE_RECEIVED;
            if (controller->event_callback)
            {
                controller->event_callback(BLE_MESH_AT_EVENT_LINE_RECEIVED, controller->rx_line);
            }
        }

        controller->line_ready = false;
    }

    // 檢查命令逾時
    if (controller->state == BLE_MESH_AT_STATE_WAITING_RESPONSE)
    {
        if (current_time - controller->last_command_time > controller->response_timeout_ms)
        {
            controller->state = BLE_MESH_AT_STATE_ERROR;
            controller->ver_sent = false; // 逾時允許重送
            controller->diag.timeout_count++;
            controller->diag.last_event = BLE_MESH_AT_EVENT_TIMEOUT;
            if (controller->event_callback)
            {
                controller->event_callback(BLE_MESH_AT_EVENT_TIMEOUT, "Command timeout");
            }
        }
    }
}

// 發送 AT+VER 命令
bool ble_mesh_at_send_ver(ble_mesh_at_controller_t *controller)
{
    if (!controller || !controller->initialized)
        return false;

    return ble_mesh_at_send_command(controller, "AT+VER");
}

// 發送通用 AT 命令
bool ble_mesh_at_send_command(ble_mesh_at_controller_t *controller, const char *command)
{
    if (!controller || !controller->initialized || !command)
        return false;

    if (controller->state == BLE_MESH_AT_STATE_WAITING_RESPONSE)
        return false; // 正在等待回應

    // 發送命令
    uart1_send_string(command);
    uart1_send_string("\r\n");

    // 更新狀態
    controller->state = BLE_MESH_AT_STATE_WAITING_RESPONSE;
    controller->last_command_time = controller->get_time_ms();
    controller->ver_sent = true; // 標記為已送出，以避免過快重複
    // 診斷快照
    {
        uint32_t i;
        for (i = 0; i < sizeof(controller->diag.last_cmd) - 1 && command[i]; ++i)
            controller->diag.last_cmd[i] = command[i];
        controller->diag.last_cmd[i] = '\0';
    }
    controller->diag.tx_count++;
    return true;
}

// 狀態查詢函數
ble_mesh_at_state_t ble_mesh_at_get_state(const ble_mesh_at_controller_t *controller)
{
    if (!controller || !controller->initialized)
        return BLE_MESH_AT_STATE_ERROR;

    return controller->state;
}

bool ble_mesh_at_is_ready(const ble_mesh_at_controller_t *controller)
{
    if (!controller || !controller->initialized)
        return false;

    return controller->state == BLE_MESH_AT_STATE_IDLE ||
           controller->state == BLE_MESH_AT_STATE_VER_OK;
}

bool ble_mesh_at_is_ver_ok(const ble_mesh_at_controller_t *controller)
{
    if (!controller || !controller->initialized)
        return false;

    return controller->state == BLE_MESH_AT_STATE_VER_OK;
}

// UART1 中斷處理函數
void ble_mesh_at_uart_irq_handler(ble_mesh_at_controller_t *controller)
{
    if (!controller || !controller->initialized)
    {
        // 記錄錯誤情況
        if (controller)
            controller->diag.error_count++;
        return;
    }

    uint32_t intsts = UART1->INTSTS;
    controller->diag.irq_count++;
    controller->diag.last_intsts = intsts;

    // 接收資料可用或接收逾時
    if (intsts & (UART_INTSTS_RDAIF_Msk | UART_INTSTS_RXTOIF_Msk))
    {
        while (!UART_GET_RX_EMPTY(UART1))
        {
            uint8_t ch = (uint8_t)UART_READ(UART1);
            controller->diag.rx_byte_count++;
            handle_rx_byte(controller, ch);
        }
    }

    // 清錯誤旗標（如有）
    uint32_t fifosts = UART1->FIFOSTS;
    controller->diag.last_fifosts = fifosts;
    if (fifosts & (UART_FIFOSTS_BIF_Msk | UART_FIFOSTS_FEF_Msk |
                   UART_FIFOSTS_PEF_Msk | UART_FIFOSTS_RXOVIF_Msk))
    {
        UART1->FIFOSTS = (UART_FIFOSTS_BIF_Msk | UART_FIFOSTS_FEF_Msk |
                          UART_FIFOSTS_PEF_Msk | UART_FIFOSTS_RXOVIF_Msk);
    }
}

// 發送 AT+REBOOT 命令
bool ble_mesh_at_send_reboot(ble_mesh_at_controller_t *controller)
{
    if (!controller || !controller->initialized)
        return false;
    return ble_mesh_at_send_command(controller, "AT+REBOOT");
}

// 發送 AT+NR（自我解除綁定）
bool ble_mesh_at_send_nr(ble_mesh_at_controller_t *controller)
{
    if (!controller || !controller->initialized)
        return false;
    return ble_mesh_at_send_command(controller, "AT+NR");
}