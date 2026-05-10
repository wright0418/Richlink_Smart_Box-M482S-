# RL_SPORT 深蹲模式設計規劃（8x8 RGB + MX4000/MXC400 + CMSIS-DSP）

> 目的：在既有 `RL_SPORT` 韌體上新增一個可用 `define` 開關的 **深蹲模式**，使用 **8x8 RGB LED** 顯示次數，利用 **MX4000/MXC400x G-sensor** 做深蹲計數，並可透過 **BLE AT REPL** 將 RAW data / feature / state 丟回電腦觀察。

---

## 1. 目前專案可直接復用的基礎

### 已經存在的硬體/韌體能力
- `drivers/ws2812b.*`
  - 已有 8x8 / 16x16 WS2812B 顯示底層。
  - 已支援 `WS2812B_SetPixel()`、`WS2812B_FillRgb()`、`WS2812B_Refresh()`。
- `drivers/gsensor.*`
  - 已支援 `SC7U22` 與 `MXC400` 自動偵測。
  - `GsensorReadAxis()` 已可取得三軸資料。
  - `Gsensor_CalcMagnitude_g_from_raw()` 已做不同 sensor backend 的比例換算。
- `ble_at_repl.c`
  - 已有 `SENSOR_READ`、`SENSOR_STREAM`、`KEY_READ` 等遠端觀察能力。
- `main.c`
  - 已有 PB3 心跳燈節奏：**每 3 秒亮 0.1 秒**。
  - 已有按鍵事件入口：`Sys_GetKeyAFlag()`。
- CMSIS-DSP
  - `VSCode/RL_SPORT.cproject.yml` 已經帶入 `ARM_MATH_CM4` 與 `arm_cortexM4lf_math.lib`。
  - 不需要額外改 toolchain，直接可做 M4 DSP 演算法。

### 關鍵結論
這個功能**不是從零開始**，建議是在既有架構上新增：
- 一個 **深蹲模式 app module**
- 一個 **深蹲演算法 module**
- 一組 **8x8 數字/狀態顯示 API**
- 少量 **BLE AT REPL 擴充命令**

---

## 2. 建議的模式切分

### compile-time define 建議
放在 `project_config.h`：

```c
#define USE_SQUAT_MODE 1
#define SQUAT_USE_RGB_8X8 1
#define SQUAT_USE_CMSIS_DSP 1
#define SQUAT_SENSOR_FORCE_MXC400 1
#define SQUAT_ENABLE_REPL_RAW_STREAM 1
#define SQUAT_ENABLE_REPL_FEATURE_STREAM 1
#define SQUAT_ENABLE_REPL_STATE_STREAM 1
#define SQUAT_ENABLE_PROGRESS_BAR 1
#define SQUAT_ENABLE_REP_FLASH 1
#define SQUAT_DISPLAY_MAX_COUNT 99u
```

### 與既有模式的關係
建議互斥：
- `USE_MOLE_GAME`
- `USE_GSENSOR_JUMP_DETECT`
- `USE_HALL_ANTICHEAT`
- `USE_SQUAT_MODE`

建議規則：
- `USE_SQUAT_MODE=1` 時，主迴圈走 **Squat Mode**。
- 其他模式保持不變，避免互相污染。

### sensor 後端建議
因需求指明 **MX4000/MXC400**，建議新增一個「強制 backend」define：

```c
#define GSENSOR_FORCE_DEVICE_NONE   0u
#define GSENSOR_FORCE_DEVICE_SC7U22 1u
#define GSENSOR_FORCE_DEVICE_MXC400 2u
#define GSENSOR_FORCE_DEVICE GSENSOR_FORCE_DEVICE_MXC400
```

> 這樣之後即使板上同時焊了 `SC7U22`，深蹲模式仍可固定用 `MXC400` 做驗證與量測，避免演算法測試混到別的 sensor backend。

---

## 3. 使用者行為與狀態機

### 需求對應
1. 開機後：
   - PB3 綠燈維持 **每 3 秒亮 0.1 秒** 心跳。
   - 8x8 RGB 面板預設關閉或只顯示待機小圖示。
2. 按下 `KEY`：
   - 進入深蹲模式，計數歸零。
   - 顯示 `GO` / `00` 後開始計數。
3. 再按一次 `KEY`：
   - **歸零並重新計算**。
   - 不需要回待機，直接留在可計數狀態最符合需求。

### 建議狀態機

```text
BOOT/IDLE
  └─(KEY short press)→ SQUAT_START_ANIM
                          └→ SQUAT_RUNNING
SQUAT_RUNNING
  └─(KEY short press)→ SQUAT_RESET_ANIM
                          └→ SQUAT_RUNNING
```

### 每個狀態的行為
- `BOOT/IDLE`
  - PB3：3 秒 / 0.1 秒心跳。
  - 8x8：全黑，或中心一顆藍點慢閃（可用 define 開關）。
- `SQUAT_START_ANIM`
  - 8x8：顯示 `GO` 或綠色展開動畫 300~500 ms。
  - 計數器、DSP buffer、baseline 全部清掉。
- `SQUAT_RUNNING`
  - 顯示目前次數。
  - 顯示下蹲深度 / phase。
  - 判定一次完整深蹲後 `count++`。
- `SQUAT_RESET_ANIM`
  - 8x8：紅色清空動畫 150~250 ms。
  - 重置計數與演算法狀態後，回 `SQUAT_RUNNING` 顯示 `00`。

---

## 4. 8x8 RGB 顯示規劃（建議版）

## 核心想法
8x8 很小，如果只顯示大數字，使用者看得到次數，但看不到「目前有沒有真的蹲到位」。
所以建議把畫面拆成：
- **中間：兩位數字**（主資訊）
- **最上排：phase 狀態列**（目前在站立/下蹲/底部/起身）
- **最下排：深度 progress bar**（是否蹲夠深）

### 版面配置

```text
Row0 : phase bar / state color
Row1 : digit tens   digit ones
Row2 : digit tens   digit ones
Row3 : digit tens   digit ones
Row4 : digit tens   digit ones
Row5 : digit tens   digit ones
Row6 : reserved / confidence marker
Row7 : squat depth progress bar (0~8)
```

### 數字字型建議
- 使用 **3x5 digit font**。
- 左邊 3 欄顯示十位數。
- 中間 1 欄空白。
- 右邊 3 欄顯示個位數。
- 共 7 欄，剛好放在 8x8 中央，剩 1 欄可做右邊狀態點或保留。

### 顏色規劃
- `Idle`：全黑 / 中央藍點
- `Ready/Start`：青色 `00`
- `Descending`：黃色數字 + 黃色 top bar
- `Bottom reached`：橘色 top bar
- `Ascending`：綠色 top bar
- `Rep accepted`：全畫面綠閃 80~120 ms，再回當前 count
- `Reset`：全畫面紅閃 100~150 ms，再回 `00`
- `Error / unstable`：紅色 top bar 或右上角紅點

### Progress bar 規劃（Row7）
- 使用 `0~8` 顆 LED 表示目前下蹲深度。
- 越蹲越深，Row7 由左到右亮起。
- 達到深蹲門檻時，Row7 全綠或最後兩顆變亮色提示「到底」。

### 顯示 count 的建議細節
- `0~99`：固定雙位數顯示，例如 `00, 01, 02 ... 99`
- `>99`：先不做滾動字，直接 **飽和顯示 `99`**，並在 top bar 用紫色提示 overflow。
  - 這樣最省 code size，也最適合第一版。

### 為什麼我推薦這個顯示方案
相比單純把整個 8x8 拿來顯示大數字：
- 使用者**看得到次數**。
- 使用者**也看得到現在是正在下蹲還是起身**。
- 調機時能從 progress bar 直接看出 threshold 是否合理。
- 之後若要改成只顯示 count，也只要關掉 `SQUAT_ENABLE_PROGRESS_BAR`。

---

## 5. 深蹲演算法規劃（M4 CMSIS-DSP）

> 目標不是做健身房等級 3D biomechanics，而是做一個 **在 M4 上穩定、可調、可 debug** 的 embedded squat counter。

### 5.1 輸入資料
- Sensor：`MX4000/MXC400x` 三軸加速度
- 取樣率：建議 `50 Hz`
- 每筆資料：`ax, ay, az`

### 5.2 CMSIS-DSP 建議使用項目
- `arm_fir_f32`：低通 / band-pass / smoothing
- `arm_mean_f32`：baseline / moving mean
- `arm_var_f32` 或 `arm_rms_f32`：活動量 / 穩定度 / noise level
- `arm_dot_prod_f32`：向量投影（把加速度投影到重力方向）

### 5.3 建議的 feature pipeline

#### Step A：重力估測（低通）
對 raw accel 做低通：

$$
\mathbf{g}_{lp}(n) = LPF(\mathbf{a}_{raw}(n))
$$

用途：
- 估計裝置當前的重力方向。
- 讓演算法不必假設板子永遠水平放置。

#### Step B：線性加速度

$$
\mathbf{a}_{lin}(n) = \mathbf{a}_{raw}(n) - \mathbf{g}_{lp}(n)
$$

用途：
- 去除靜態重力，只保留身體上下移動造成的動態成分。

#### Step C：垂直方向投影
將動態加速度投影到重力方向單位向量：

$$
\hat{u}_g = \frac{\mathbf{g}_{lp}}{\|\mathbf{g}_{lp}\|}
$$

$$
a_v(n) = \mathbf{a}_{lin}(n) \cdot \hat{u}_g
$$

用途：
- 取得「相對上下方向」的動作訊號。
- 比只看 magnitude 更適合深蹲這種低頻、上下往返的動作。

#### Step D：平滑 / band-pass
對 `a_v` 再做一次平滑或 band-pass：
- squat 典型頻率約 `0.25 ~ 2 Hz`
- 第一版可先用：
  - `LPF`：降低高頻雜訊
  - 或 `band-pass`：只留下深蹲節奏相關能量

#### Step E：狀態機判定
用一個簡單但穩定的 FSM：

```text
STAND
  -> DESCEND   : a_v < -T_down 且 progress 開始增加
DESCEND
  -> BOTTOM    : depth >= T_depth 且速度趨近 0
BOTTOM
  -> ASCEND    : a_v > +T_up
ASCEND
  -> STAND     : 回到 baseline 附近且維持 T_hold
                 => count++
```

### 5.4 建議的有效深蹲條件
必須同時滿足：
1. 有明確 `下蹲 -> 底部 -> 起身` 三段相位
2. 底部停留超過最小時間，例如 `80~150 ms`
3. 深度 proxy 達到門檻
4. 回站立後才算 +1
5. 每次 rep 間隔至少 `300~500 ms`，避免抖動重複計數

### 5.5 第一版深度 proxy 建議
第一版不要做雙重積分位移，太容易 drift。
建議用：
- `vertical_acc` 的 valley + peak 振幅
- 搭配低通後的姿態變化量 / moving RMS
- 組成一個 `depth_score`

例如：

$$
depth\_score = w_1 \cdot |valley| + w_2 \cdot rms(a_v) + w_3 \cdot posture\_delta
$$

只要 `depth_score > threshold` 就視為「有蹲夠深」。

### 5.6 第一版建議不要做的事
- 不要一開始就做步態/姿態分類 ML
- 不要一開始就做 double integration 求位移
- 不要讓演算法直接依賴固定某一軸，例如只看 `Z`
  - 因為裝置貼法可能改變

---

## 6. BLE AT REPL 規劃

### 已有能力
現有：
- `AT+TEST,SENSOR_READ`
- `AT+TEST,SENSOR_STREAM,START,GSENSOR,200`

所以**RAW data 其實已經能送回 PC**。

### 建議新增命令

#### 基本資訊
- `AT+TEST,SQUAT_INFO`
  - 回傳 mode on/off、count、state、sensor backend、取樣率

#### 控制
- `AT+TEST,SQUAT_START`
- `AT+TEST,SQUAT_STOP`
- `AT+TEST,SQUAT_RESET`

#### 串流
- `AT+TEST,SQUAT_STREAM,RAW,<ms>`
  - 回傳：`ax, ay, az, mag`
- `AT+TEST,SQUAT_STREAM,FEATURE,<ms>`
  - 回傳：`grav_x, grav_y, grav_z, vert_acc, rms, depth_score`
- `AT+TEST,SQUAT_STREAM,STATE,<ms>`
  - 回傳：`state, count, phase, progress, flags`
- `AT+TEST,SQUAT_STREAM,STOP`

### 建議回傳格式

#### RAW
```text
+DATA,SQUAT_RAW,AX=123,AY=-456,AZ=980,MAG=1.03
```

#### FEATURE
```text
+DATA,SQUAT_FEAT,GX=0.01,GY=-0.02,GZ=0.99,VACC=-0.31,RMS=0.18,DEPTH=6
```

#### STATE
```text
+DATA,SQUAT_STATE,STATE=DESCEND,COUNT=12,PROG=5,VALID=1
```

### 設計原則
- RAW / FEATURE / STATE 分三種 stream，避免一次吐太多資料。
- 第一版只要做到能被 PC log 起來畫圖，就夠好用。
- 這會大幅加快 threshold tuning；不然只能「看起來怪怪的」慢慢猜。很痛，真的很痛。

---

## 7. 建議新增模組

### app 層
- `app/squat_mode.h`
- `app/squat_mode.c`
  - 高階狀態機
  - key 行為
  - count/reset
  - 呼叫 display / algorithm / repl hooks

### algorithm 層
- `app/algorithms/squat_detect.h`
- `app/algorithms/squat_detect.c`
  - CMSIS-DSP pipeline
  - feature extraction
  - state machine
  - rep counting

### display 層
- `drivers/ws2812b_digits.h`
- `drivers/ws2812b_digits.c`
  - 3x5 digit font
  - `WS2812B_ShowNumber2Digit()`
  - `WS2812B_ShowProgressBar()`
  - `WS2812B_ShowSquatScreen(count, phase, progress)`

### REPL 擴充
- 在 `ble_at_repl.c` 加 `SQUAT_*` 命令

---

## 8. main.c 掛接建議

### 初始化
- `USE_SQUAT_MODE=1` 時：
  - `SquatMode_Init()`
  - `WS2812B_Init()`
  - `Gsensor_Init(..., FSR_2G)`

### 主迴圈
- PB3 綠燈維持現有 heartbeat。
- 若 `USE_SQUAT_MODE=1`：
  - `MoleGame_Process()` 不跑
  - 改成 `SquatMode_Process(now)`

### 按鍵事件
- `Sys_GetKeyAFlag()` 觸發後：
  - 若 `IDLE` → 開始深蹲模式
  - 若 `RUNNING` → 歸零並重新開始

---

## 9. 第一版實作順序（我建議這樣做）

1. **先做 mode 切換 define**
   - 讓 `USE_SQUAT_MODE` 能獨立編譯進出
2. **做 8x8 數字顯示**
   - 先顯示 `00~99`
   - 再加 top bar / progress bar
3. **做 key start/reset 行為**
4. **做 RAW stream 與 FEATURE stream**
5. **做簡版 squat FSM**
   - 先能抓完整 `down-bottom-up`
6. **再調 threshold**
   - 用 PC 畫圖配 REPL log 調參
7. **最後才加 fancy animation**

---

## 10. 我建議的第一版顯示方案結論

### 最推薦版本
- **PB3 綠燈**：維持現有每 3 秒亮 0.1 秒
- **8x8 RGB 面板**：
  - Idle：全黑
  - Start：綠色 `GO` / 或綠色展開動畫
  - Running：
    - 中央顯示 **兩位數 count**
    - Top row 顯示 phase color
    - Bottom row 顯示 squat depth progress
  - 每計到一次：全綠閃一下，再回數字
  - 按鍵 reset：全紅閃一下，回 `00`

### 為什麼先做這版
- 讀數清楚
- 還能看動作 phase
- 容易 debug
- code size 小
- 跟現有 `WS2812B` driver 最相容

---

## 11. 若要我接著做實作，我會先改哪些檔案

1. `SampleCode/RL_SPORT/project_config.h`
2. `SampleCode/RL_SPORT/main.c`
3. `SampleCode/RL_SPORT/drivers/gsensor.c`（加入 force MXC400 選項）
4. `SampleCode/RL_SPORT/ble_at_repl.c`
5. 新增 `app/squat_mode.*`
6. 新增 `app/algorithms/squat_detect.*`
7. 新增 `drivers/ws2812b_digits.*`

---

## 12. 最終建議

這個需求我建議分兩階段：

### Phase 1：先做穩定可驗證版
- 2-digit count
- progress bar
- key start/reset
- RAW / FEATURE / STATE 透過 BLE AT REPL 回傳
- 使用 MXC400 + CMSIS-DSP FSM 做 rep count

### Phase 2：再做美化與強化
- `GO` / `RST` 動畫
- 更漂亮的顏色映射
- 更準的 depth confidence
- 長按鍵進入 idle / 短按 reset 的 UX 優化

> 結論一句話：**8x8 最適合用「雙位數 + 狀態列 + 深度條」來做深蹲模式**，這樣使用者看得懂、工程師也調得動。
