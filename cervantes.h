#ifndef __CERVANTES_H
#define __CERVANTES_H

#include <stdint.h>

/**************************************************************************************************
 Prototypes  */

extern "C"
  {
    unsigned char* getInputGameStateBuffer(void);
    unsigned char* getInputMoveBuffer(void);
    unsigned char* getOutputGameStateBuffer(void);
    unsigned char* getOutputMovesBuffer(void);
    unsigned char* getOutputRepetitionStateBuffer(void);

    uintptr_t getWeightsBuffer(void);
    unsigned int weightsBufferSize(void);

    unsigned int featureCount_eval(void);
    unsigned char sideToMove_eval(void);
    unsigned char isTerminal_eval(void);
    unsigned char isSideToMoveInCheck_eval(void);
    unsigned char nonPawnMaterial_eval(void);
    void makeMove_eval(void);
    void makeNullMove_eval(void);
    float heuristicLogit_eval(void);
    float neuralLogit_eval(void);
    float evaluate_eval(void);
    unsigned int getMoves_eval(void);
    void repetitionState_eval(void);
    unsigned char historyVerdict_eval(unsigned int);
  }

#endif