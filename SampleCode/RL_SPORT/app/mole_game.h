#ifndef MOLE_GAME_H
#define MOLE_GAME_H

#include <stdint.h>

void MoleGame_Init(void);
void MoleGame_Process(uint32_t now_ms);
void MoleGame_OnButtonEvent(uint32_t now_ms);
void MoleGame_ShutdownOutputs(void);
void MoleGame_ResetFrameState(void);

#endif /* MOLE_GAME_H */
