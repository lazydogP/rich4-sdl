#include "app/monthly_finance_flow.h"
#include "core/rng.h"

int rich4_monthly_animation_repeats(int character, int unlucky)
{
    /* BYTE1 of 0x4759F7 + 24*c, consumed by 0x450CED/0x45144F. */
    static const int repeats[12][2] = {
        {6,1},{4,6},{6,6},{6,3},{4,6},{4,2},
        {6,6},{4,1},{4,1},{4,3},{4,6},{4,4}
    };
    return character >= 0 && character < 12 ? repeats[character][unlucky != 0] : 1;
}

void rich4_monthly_banker_update(GameSession *s, int speaking)
{
    static const int eyes[5][3] = {{21,20,21},{28,27,28},{34,33,34},
                                  {0,0,0},{39,40,39}};
    static const int open_mouth[6] = {23,30,36,0,0,44};
    static const int closed_mouth[6] = {22,29,35,0,0,43};
    GameBankSettlementPresentationState *p = &s->presentation.bank_settlement;
    Rich4Rng rng = {s->state.ai_rng_state};
    int stage = p->display_stage, mode, n, step;
    mode = stage == 1 || stage == 5 ? 0 : stage == 2 ? 1 :
           stage == 6 || stage == 8 ? 2 : stage == 7 ? 3 :
           stage == 9 || stage == 16 ? 4 : stage == 17 || stage == 18 ? 5 : 6;
    if (p->face_mode != mode) {
        p->face_mode = mode;
        p->eye_chunk = p->mouth_chunk = 0;
    }
    /* 0x43919D: idle consumes rand even for poses without eye sprites. */
    if ((p->eye_state & 15) == 0) {
        n = rich4_rng_original_state_next(&rng) >> 10;
        if (n == 0) p->eye_state |= 1;
        else if (n == 1) p->eye_state |= 2;
    }
    n = p->eye_state & 15;
    if (mode < 5 && mode != 3 && n) {
        if (n == 1) {
            step = (p->eye_state >> 4) & 3;
            p->eye_chunk = step < 3 ? eyes[mode][step] : mode == 1 ? 25 : 0;
            if (step == 3) p->eye_state = 0;
            else p->eye_state = (p->eye_state + 16) & 255;
        } else if (mode == 1) {
            n = rich4_rng_original_state_next(&rng) & 1;
            if (n != (int)(p->eye_state >> 6)) {
                p->eye_chunk = 25 + n;
                p->eye_state |= (unsigned)n << 6;
            }
            p->eye_state &= 64;
        } else p->eye_state = 0;
    }
    if (mode < 6 && open_mouth[mode] && (speaking || p->mouth_ticks)) {
        if (p->mouth_ticks) {
            if (--p->mouth_ticks == 0)
                /* At 0x43974D..0x439759 the second stack load occurs after
                 * the first push, so the actual arguments remain Rect.left
                 * and Rect.top despite IDA naming them right/bottom. */
                p->mouth_chunk = closed_mouth[mode] << 8;
        } else if ((rich4_rng_original_state_next(&rng) >> 11) < 4) {
            n = open_mouth[mode];
            if (mode == 0 && !(rich4_rng_original_state_next(&rng) & 1)) n = 0;
            p->mouth_chunk = n;
            p->mouth_ticks = (rich4_rng_original_state_next(&rng) & 7) + 1;
        }
    }
    s->state.ai_rng_state = rng.state;
}
