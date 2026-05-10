#include "squat_detect.h"

#include "../../project_config.h"
#include <math.h>
#include <string.h>

#ifdef ARM_MATH_CM4
#include "arm_math.h"
#endif

/*
 * 深蹲演算法調參說明（中文）
 *
 * 這一區是實體測試時最常調的門檻，原則如下：
 * - 想要更容易判定：把門檻放寬一點
 * - 想要更不容易誤判：把門檻收緊一點
 *
 * 參數意義：
 * - SAMPLE_RATE_HZ：感測取樣率，必須跟主迴圈讀 sensor 的頻率一致
 * - FIR_TAPS：平滑程度，越大越平滑但延遲越大
 * - T_DOWN_G：開始下蹲的負向門檻，越小越容易進入 DESCEND
 * - T_UP_G：起身回站立的正向門檻，越小越容易回到 STAND
 * - T_DEPTH_SCORE：深蹲「夠深」判定，越大越嚴格
 * - BOTTOM_HOLD_MS：底部停留時間，越大越不容易把晃動當成深蹲
 * - REP_MIN_INTERVAL_MS：兩次有效計數的最短間隔，避免抖動重複算
 * - STAND_BAND_G：回站立時允許的震盪帶，越小越嚴格
 * - STAND_HOLD_MS：回站立後需要穩定維持的時間
 */
#ifndef SQUAT_SAMPLE_RATE_HZ
#define SQUAT_SAMPLE_RATE_HZ 50u /* 50Hz = 每 20ms 一筆；若主迴圈/感測頻率不同，這裡也要一起對齊 */
#endif
#ifndef SQUAT_FIR_TAPS
#define SQUAT_FIR_TAPS 7u /* FIR 濾波器 tap 數：越大越平滑，但反應越慢 */
#endif
#ifndef SQUAT_T_DOWN_G
#define SQUAT_T_DOWN_G 0.5f /* 下蹲起始門檻(g)：越小越容易進入下蹲；太大會漏算 */
#endif
#ifndef SQUAT_T_UP_G
#define SQUAT_T_UP_G 0.4f /* 起身門檻(g)：越小越容易回到站立；太大會拖慢結束判定 */
#endif
#ifndef SQUAT_T_DEPTH_SCORE
#define SQUAT_T_DEPTH_SCORE 0.85f /* 深度分數門檻(0~1)：越大越要求「蹲得更深」 */
#endif
#ifndef SQUAT_DOWN_CONFIRM_MS
#define SQUAT_DOWN_CONFIRM_MS 120u /* 下蹲確認時間(ms)：連續低於下蹲門檻多久才算真的開始下蹲 */
#endif
#ifndef SQUAT_DESCEND_MIN_MS
#define SQUAT_DESCEND_MIN_MS 120u /* 下降最短時間(ms)：避免瞬間甩動一下就直接走到底部 */
#endif
#ifndef SQUAT_BOTTOM_HOLD_MS
#define SQUAT_BOTTOM_HOLD_MS 300u /* 底部停留時間(ms)：越大越不容易把快速彈動誤判成深蹲 */
#endif
#ifndef SQUAT_REP_MIN_INTERVAL_MS
#define SQUAT_REP_MIN_INTERVAL_MS 400u /* 兩次有效計數最小間隔(ms)：避免連點重複計數 */
#endif
#ifndef SQUAT_STAND_BAND_G
#define SQUAT_STAND_BAND_G 0.05f /* 站立容許帶(g)：越小越嚴格，越不容易提早算完成 */
#endif
#ifndef SQUAT_STAND_HOLD_MS
#define SQUAT_STAND_HOLD_MS 120u /* 回到站立後需穩定維持時間(ms) */
#endif

typedef struct
{
    float grav_x;
    float grav_y;
    float grav_z;
    float vert_filt;
    float rms_hist[16];
    uint8_t rms_idx;
    uint8_t rms_count;

    float valley;
    float peak;
    uint32_t phase_enter_ms;
    uint32_t down_candidate_ms;
    uint32_t last_rep_ms;
    uint16_t count;
    SquatPhase phase;

    SquatFeatureSnapshot feat;
} SquatCtx;

static SquatCtx s_ctx;

#ifdef ARM_MATH_CM4
static arm_fir_instance_f32 s_fir;
static float s_fir_state[SQUAT_FIR_TAPS + 1u];
static const float s_fir_coeffs[SQUAT_FIR_TAPS] = {0.06f, 0.12f, 0.2f, 0.24f, 0.2f, 0.12f, 0.06f};
#endif

/*
 * clampf(v, lo, hi)
 * - 用途：把數值限制在範圍內
 * - 調整方式：通常不需要改它，只是保護 depth_score 不會超出 0~1
 */
static float clampf(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/*
 * vec_mag(x, y, z)
 * - 用途：計算三軸向量長度
 * - 在這裡代表加速度總量，可用來觀察 sensor 整體變化
 */
static float vec_mag(float x, float y, float z)
{
    return sqrtf(x * x + y * y + z * z);
}

/*
 * fir_step(in)
 * - 用途：對垂直加速度做平滑
 * - 調整方式：要更靈敏就減少 FIR_TAPS；要更穩就增加 FIR_TAPS
 */
static float fir_step(float in)
{
#ifdef ARM_MATH_CM4
    float out = 0.0f;
    arm_fir_f32(&s_fir, &in, &out, 1u);
    return out;
#else
    return in;
#endif
}

/*
 * rms_recent(v)
 * - 用途：計算最近一段訊號的活動量(RMS)
 * - 調整方式：RMS 越大，代表動作變化越明顯；可用來輔助 depth_score
 */
static float rms_recent(float v)
{
    float sum_sq = 0.0f;
    s_ctx.rms_hist[s_ctx.rms_idx] = v;
    s_ctx.rms_idx = (uint8_t)((s_ctx.rms_idx + 1u) % (uint8_t)(sizeof(s_ctx.rms_hist) / sizeof(s_ctx.rms_hist[0])));
    if (s_ctx.rms_count < (uint8_t)(sizeof(s_ctx.rms_hist) / sizeof(s_ctx.rms_hist[0])))
    {
        s_ctx.rms_count++;
    }

#ifdef ARM_MATH_CM4
    arm_rms_f32(s_ctx.rms_hist, s_ctx.rms_count, &sum_sq);
    return sum_sq;
#else
    for (uint8_t i = 0u; i < s_ctx.rms_count; i++)
    {
        sum_sq += s_ctx.rms_hist[i] * s_ctx.rms_hist[i];
    }
    return (s_ctx.rms_count > 0u) ? sqrtf(sum_sq / (float)s_ctx.rms_count) : 0.0f;
#endif
}

void SquatDetect_Init(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.phase = SQUAT_PHASE_IDLE;
#ifdef ARM_MATH_CM4
    memset(s_fir_state, 0, sizeof(s_fir_state));
    arm_fir_init_f32(&s_fir, SQUAT_FIR_TAPS, (float *)s_fir_coeffs, s_fir_state, 1u);
#endif
}

void SquatDetect_Reset(void)
{
    uint16_t keep_count = 0u;
    SquatDetect_Init();
    s_ctx.count = keep_count;
    s_ctx.phase = SQUAT_PHASE_STAND;
}

uint8_t SquatDetect_ProcessSample(int16_t ax, int16_t ay, int16_t az, uint32_t now_ms)
{
    /*
     * alpha：重力低通追蹤速度
     * - 越大：重力方向跟得更快，但也更容易被手抖/動作影響
     * - 越小：更穩，但姿態改變時反應比較慢
     *
     * 實測調整方向：
     * - 動作開始/結束反應太慢 -> 把 alpha 調大一點
     * - 誤判太多、重力方向飄很快 -> 把 alpha 調小一點
     */
    const float alpha = 0.04f;
    const float x = (float)ax;
    const float y = (float)ay;
    const float z = (float)az;

    float gmag;
    float ux;
    float uy;
    float uz;
    float linx;
    float liny;
    float linz;
    float vert;
    float vert_f;
    float rms;
    float depth;

    s_ctx.grav_x = (1.0f - alpha) * s_ctx.grav_x + alpha * x;
    s_ctx.grav_y = (1.0f - alpha) * s_ctx.grav_y + alpha * y;
    s_ctx.grav_z = (1.0f - alpha) * s_ctx.grav_z + alpha * z;

    gmag = vec_mag(s_ctx.grav_x, s_ctx.grav_y, s_ctx.grav_z);
    if (gmag < 1.0f)
    {
        gmag = 1.0f;
    }

    ux = s_ctx.grav_x / gmag;
    uy = s_ctx.grav_y / gmag;
    uz = s_ctx.grav_z / gmag;

    linx = x - s_ctx.grav_x;
    liny = y - s_ctx.grav_y;
    linz = z - s_ctx.grav_z;
    vert = (linx * ux + liny * uy + linz * uz) / 1024.0f;
    vert_f = fir_step(vert);
    rms = rms_recent(vert_f);

    if (vert_f < s_ctx.valley)
        s_ctx.valley = vert_f;
    if (vert_f > s_ctx.peak)
        s_ctx.peak = vert_f;

    /*
     * depth_score：深蹲深度綜合分數
     * - valley/peak：看垂直加速度的上下振幅
     * - rms：看最近一段時間的動作能量
     *
     * 調整方式：
     * - 想更容易判定「有蹲夠深」：把 1.2f / 0.8f 調大，或把 SQUAT_T_DEPTH_SCORE 調小
     * - 想更嚴格：把 1.2f / 0.8f 調小，或把 SQUAT_T_DEPTH_SCORE 調大
     */
    depth = clampf((fabsf(s_ctx.valley) + fabsf(s_ctx.peak)) * 1.2f + rms * 0.8f, 0.0f, 1.0f);

    s_ctx.feat.raw_mag_g = vec_mag(x, y, z) / 1024.0f;
    s_ctx.feat.grav_mag_g = gmag / 1024.0f;
    s_ctx.feat.vert_acc_g = vert_f;
    s_ctx.feat.depth_score = depth;
    s_ctx.feat.rms_g = rms;

    switch (s_ctx.phase)
    {
    case SQUAT_PHASE_IDLE:
        s_ctx.phase = SQUAT_PHASE_STAND;
        s_ctx.phase_enter_ms = now_ms;
        s_ctx.valley = 0.0f;
        s_ctx.peak = 0.0f;
        break;

    case SQUAT_PHASE_STAND:
        /*
         * 進入下蹲條件：
         * - vert_f < -SQUAT_T_DOWN_G 表示重力方向上的動態加速度開始往下
         * - 但必須持續 SQUAT_DOWN_CONFIRM_MS，避免快速甩動只碰到一瞬間就被算進來
         * - 如果覺得太難進入下蹲，請把 SQUAT_T_DOWN_G 調小
         * - 如果走動/晃動也會被當成下蹲，請把它調大
         */
        if (vert_f < -SQUAT_T_DOWN_G)
        {
            if (s_ctx.down_candidate_ms == 0u)
            {
                s_ctx.down_candidate_ms = now_ms;
            }

            if ((now_ms - s_ctx.down_candidate_ms) >= SQUAT_DOWN_CONFIRM_MS)
            {
                s_ctx.phase = SQUAT_PHASE_DESCEND;
                s_ctx.phase_enter_ms = now_ms;
                s_ctx.down_candidate_ms = 0u;
                s_ctx.valley = vert_f;
                s_ctx.peak = 0.0f;
            }
        }
        else
        {
            s_ctx.down_candidate_ms = 0u;
        }
        break;

    case SQUAT_PHASE_DESCEND:
        /*
         * 下降中判定：
         * - depth >= SQUAT_T_DEPTH_SCORE：代表蹲得夠深
         * - fabsf(vert_f) <= SQUAT_STAND_BAND_G：代表接近底部、速度變慢
         * - 下降時間至少要達到 SQUAT_DESCEND_MIN_MS，避免瞬間甩動直接撞到底部條件
         *
         * 調整方式：
         * - 想更容易進入 BOTTOM：降低 SQUAT_T_DEPTH_SCORE 或放大 SQUAT_STAND_BAND_G
         * - 想減少誤判到底：提高 SQUAT_T_DEPTH_SCORE 或縮小 SQUAT_STAND_BAND_G
         */
        if ((now_ms - s_ctx.phase_enter_ms) < SQUAT_DESCEND_MIN_MS)
        {
            if (vert_f > SQUAT_T_UP_G)
            {
                s_ctx.phase = SQUAT_PHASE_STAND;
                s_ctx.phase_enter_ms = now_ms;
                s_ctx.down_candidate_ms = 0u;
                s_ctx.valley = 0.0f;
                s_ctx.peak = 0.0f;
            }
            break;
        }

        if ((depth >= SQUAT_T_DEPTH_SCORE) && (fabsf(vert_f) <= SQUAT_STAND_BAND_G))
        {
            s_ctx.phase = SQUAT_PHASE_BOTTOM;
            s_ctx.phase_enter_ms = now_ms;
        }
        else if (vert_f > SQUAT_T_UP_G)
        {
            s_ctx.phase = SQUAT_PHASE_STAND;
            s_ctx.phase_enter_ms = now_ms;
            s_ctx.down_candidate_ms = 0u;
            s_ctx.valley = 0.0f;
            s_ctx.peak = 0.0f;
        }
        break;

    case SQUAT_PHASE_BOTTOM:
        /*
         * 底部停留條件：
         * - 先待滿 SQUAT_BOTTOM_HOLD_MS
         * - 再確認 vert_f > SQUAT_T_UP_G，才算開始起身
         *
         * 調整方式：
         * - 快速動作被漏算：可把 SQUAT_BOTTOM_HOLD_MS 調小一點
         * - 太多下壓抖動被當成完成：把它調大一點
         */
        if ((now_ms - s_ctx.phase_enter_ms) >= SQUAT_BOTTOM_HOLD_MS)
        {
            if (vert_f > SQUAT_T_UP_G)
            {
                s_ctx.phase = SQUAT_PHASE_ASCEND;
                s_ctx.phase_enter_ms = now_ms;
            }
        }
        break;

    case SQUAT_PHASE_ASCEND:
        /*
         * 回到站立完成條件：
         * - fabsf(vert_f) <= SQUAT_STAND_BAND_G：表示回到穩定區
         * - 維持 SQUAT_STAND_HOLD_MS：避免短暫穿越站立區就誤算完成
         * - 再加上 SQUAT_REP_MIN_INTERVAL_MS：避免連續抖動造成重複計數
         */
        if ((fabsf(vert_f) <= SQUAT_STAND_BAND_G) && ((now_ms - s_ctx.phase_enter_ms) >= SQUAT_STAND_HOLD_MS))
        {
            if ((now_ms - s_ctx.last_rep_ms) >= SQUAT_REP_MIN_INTERVAL_MS)
            {
                s_ctx.count++;
                s_ctx.last_rep_ms = now_ms;
                s_ctx.phase = SQUAT_PHASE_STAND;
                s_ctx.phase_enter_ms = now_ms;
                s_ctx.down_candidate_ms = 0u;
                s_ctx.valley = 0.0f;
                s_ctx.peak = 0.0f;
                return 1u;
            }
            s_ctx.phase = SQUAT_PHASE_STAND;
            s_ctx.phase_enter_ms = now_ms;
            s_ctx.down_candidate_ms = 0u;
        }
        break;

    default:
        s_ctx.phase = SQUAT_PHASE_STAND;
        s_ctx.down_candidate_ms = 0u;
        break;
    }

    return 0u;
}

uint16_t SquatDetect_GetCount(void)
{
    return s_ctx.count;
}

SquatPhase SquatDetect_GetPhase(void)
{
    return s_ctx.phase;
}

uint8_t SquatDetect_GetProgress8(void)
{
    float p = clampf(s_ctx.feat.depth_score, 0.0f, 1.0f);
    return (uint8_t)(p * 8.0f + 0.5f);
}

void SquatDetect_GetFeatureSnapshot(SquatFeatureSnapshot *out)
{
    if (out == NULL)
    {
        return;
    }
    *out = s_ctx.feat;
}
