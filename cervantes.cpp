/*

sudo docker run --rm -v $(pwd):/src -u $(id -u):$(id -g) --mount type=bind,source=$(pwd),target=/home/src c-wasm em++ -std=c++17 -O3 -msimd128 -I/usr/include/eigen3 -DEIGEN_NO_DEBUG -DEIGEN_DONT_PARALLELIZE -s STANDALONE_WASM -s INITIAL_HEAP=1048576 -s EXPORTED_FUNCTIONS="['_getInputGameStateBuffer','_getInputMoveBuffer','_getOutputGameStateBuffer','_getOutputMovesBuffer','_getOutputRepetitionStateBuffer','_getWeightsBuffer','_weightsBufferSize','_featureCount_eval','_sideToMove_eval','_isTerminal_eval','_isSideToMoveInCheck_eval','_nonPawnMaterial_eval','_makeMove_eval','_makeNullMove_eval','_heuristicLogit_eval','_neuralLogit_eval','_evaluate_eval','_getMoves_eval','_repetitionState_eval','_historyVerdict_eval']" -Wl,--no-entry cervantes.cpp -o eval.wasm

*/

#include <Eigen/Dense>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "gamestate.h"
#include "cervantes.h"


namespace
  {
    /**********************************************************************************************
     Constants  */

    constexpr int SQUARES = 64;
    constexpr int PLANES = 18;
    constexpr int CHANNELS = 32;
    constexpr int BLOCKS = 4;
    constexpr int HIDDEN = 64;
    constexpr int ACTIVATIONS = CHANNELS * SQUARES;
    constexpr int STEM_WEIGHT = 0;
    constexpr int STEM_BIAS = STEM_WEIGHT + CHANNELS * PLANES;
    constexpr int BLOCK_START = STEM_BIAS + CHANNELS;
    constexpr int BLOCK_FLOATS = 2 * (CHANNELS * 9 + CHANNELS + CHANNELS * CHANNELS + CHANNELS);
    constexpr int HIDDEN_WEIGHT = BLOCK_START + BLOCKS * BLOCK_FLOATS;
    constexpr int HIDDEN_BIAS = HIDDEN_WEIGHT + HIDDEN * ACTIVATIONS;
    constexpr int VALUE_WEIGHT = HIDDEN_BIAS + HIDDEN;
    constexpr int VALUE_BIAS = VALUE_WEIGHT + HIDDEN;
    constexpr int PARAMETER_FLOATS = VALUE_BIAS + 1;

    constexpr int FEATURE_OWN_PAWN_ATTACK = 12;
    constexpr int FEATURE_ENEMY_PAWN_ATTACK = 13;
    constexpr int FEATURE_COUNT_BOARD = 14;
    constexpr int FEATURE_COUNT_OWN_MATES = 15;
    constexpr int FEATURE_COUNT_ENEMY_MATES = 16;
    constexpr int FEATURE_CLOCK = 17;

    using InputMatrix = Eigen::Matrix<float, CHANNELS, SQUARES, Eigen::RowMajor>;
    using PlaneMatrix = Eigen::Matrix<float, PLANES, SQUARES, Eigen::RowMajor>;
    using StemWeights = Eigen::Matrix<float, CHANNELS, PLANES, Eigen::RowMajor>;
    using BlockWeights = Eigen::Matrix<float, CHANNELS, CHANNELS, Eigen::RowMajor>;
    using Vec32 = Eigen::Matrix<float, CHANNELS, 1>;
    using Vec64 = Eigen::Matrix<float, HIDDEN, 1>;
    using DenseWeights = Eigen::Matrix<float, HIDDEN, ACTIVATIONS, Eigen::RowMajor>;
    using FlatActivations = Eigen::Matrix<float, ACTIVATIONS, 1>;

    alignas(16) static float weights[PARAMETER_FLOATS] = {};
    alignas(16) static float input[PLANES * SQUARES] = {};
    alignas(16) static float activationA[ACTIVATIONS] = {};
    alignas(16) static float activationB[ACTIVATIONS] = {};
    alignas(16) static float activationC[ACTIVATIONS] = {};
    alignas(16) static float hidden[HIDDEN] = {};
                                                                    //  Baseline piece values in centipawn-like units; NOT calibrated Makruk values.
    constexpr float PAWN_VALUE = 100.0f;
    constexpr float QUEEN_VALUE = 170.0f;
    constexpr float BISHOP_VALUE = 270.0f;
    constexpr float KNIGHT_VALUE = 280.0f;
    constexpr float ROOK_VALUE = 520.0f;

    /**********************************************************************************************
     Typedefs  */

    struct HeuristicParts
      {
        float material = 0;
        float mobility = 0;
        float kingSafety = 0;
        float pawnStructure = 0;
        float see = 0; //  TODO: legal static exchange evaluation, shared with move ordering.
      };

    /**********************************************************************************************
     Functions  */

    /* Lookup the material value of the given piece. */
    float pieceValue(unsigned char piece)
      {
        switch(piece)
          {
            case _WHITE_PAWN:
            case _BLACK_PAWN:   return PAWN_VALUE;

            case _WHITE_QUEEN:
            case _BLACK_QUEEN:  return QUEEN_VALUE;

            case _WHITE_BISHOP:
            case _BLACK_BISHOP: return BISHOP_VALUE;

            case _WHITE_KNIGHT:
            case _BLACK_KNIGHT: return KNIGHT_VALUE;

            case _WHITE_ROOK:
            case _BLACK_ROOK:   return ROOK_VALUE;

            default:            return 0.0f;
          }
      }

    /* To which side does the given piece belong? */
    int sideOf(unsigned char p)
      {
        if(p >= _WHITE_PAWN && p <= _WHITE_KING)
          return 1;
        if(p >= _BLACK_PAWN && p <= _BLACK_KING)
          return -1;
        return 0;
      }

    /* Regardless of side, what kind of piece is the given byte? */
    int kindOf(unsigned char p)
      {
        return p == 0 ? -1 : (p - 1) % 6;
      }

    /* What is the given "index" from the point of view of the indicated side? */
    int canonicalSquare(int index, bool whiteToMove)
      {
        return whiteToMove ? index : ((7 - index / 8) * 8 + index % 8);
      }

    /* Extract pseudo-legal movement/attack targets.
       King safety is approximated; these are not full chess attack maps (friendly occupied targets omitted). */
    unsigned movementTargets(int index, GameState* gs, Move* out)
      {
        unsigned char p = gs->board[index];
        switch(kindOf(p))
          {
            case 0:  return getPawnAttackable(index, gs, out);
            case 1:  return getKnightMoves(index, gs, out);
            case 2:  return getBishopMoves(index, gs, out);
            case 3:  return getRookMoves(index, gs, out);
            case 4:  return getQueenMoves(index, gs, out);
            case 5:  return getKingMoves(index, gs, out);
            default: return 0;
          }
      }

    /* Compute the hand-coded heuristics two be complemented by network evaluation. */
    HeuristicParts heuristicParts(GameState* gs)
      {
        float whiteMaterial = 0, blackMaterial = 0;
        float whitePawn = 0, blackPawn = 0;
        int mobility[2] = {0, 0};
        int attacks[2][SQUARES] = {};
        int kingSq[2] = {-1, -1};
        int pawnFiles[2][8] = {};

        int square, side;

        Move targets[_NONE];
        HeuristicParts parts;

        for(square = 0; square < SQUARES; square++)
          {
            side = sideOf(gs->board[square]);
            if(!side)
              continue;

            const int t = side == 1 ? 0 : 1;
            const int kind = kindOf(gs->board[square]);

            if(kind == 5)
              kingSq[t] = square;
            if(kind == 0)
              pawnFiles[t][square % 8]++;

            const float value = pieceValue(gs->board[square]);

            if(t == 0)
              whiteMaterial += value;
            else
              blackMaterial += value;

            const unsigned n = movementTargets(square, gs, targets);

            if(kind != 0 && kind != 5)
              mobility[t] += static_cast<int>(n);

            for(unsigned j = 0; j < n; j++)
              {
                if(targets[j].to < SQUARES)
                  attacks[t][targets[j].to]++;
              }
          }

        for(square = 0; square < SQUARES; square++)
          {
            if(kindOf(gs->board[square]) != 0)
              continue;

            const bool white = sideOf(gs->board[square]) == 1;
            const int t = white ? 0 : 1;
            const int file = square % 8;
            const int rank = square / 8;

            float value = 0;
            if(pawnFiles[t][file] > 1)
              value -= 12.0f * (pawnFiles[t][file] - 1) / pawnFiles[t][file];
            if((file == 0 || pawnFiles[t][file - 1] == 0) && (file == 7 || pawnFiles[t][file + 1] == 0))
              value -= 10.0f;

            const int supportRank = white ? rank - 1 : rank + 1;
            if(supportRank >= 0 && supportRank < 8)
              {
                for(int df : {-1, 1})
                  {
                    if(file + df >= 0 && file + df < 8 && gs->board[supportRank * 8 + file + df] == gs->board[square])
                      {
                        value += 9.0f;
                        break;
                      }
                  }
              }

            bool passed = true;
            for(int other = 0; other < SQUARES; other++)
              {
                if(kindOf(gs->board[other]) != 0 || sideOf(gs->board[other]) != (white ? -1 : 1))
                  continue;

                const int theirFile = other % 8;

                if(std::abs(theirFile - file) > 1)
                  continue;

                if(white ? other / 8 > rank : other / 8 < rank)
                  {
                    passed = false;
                    break;
                  }
              }
            if(passed)
              {
                const int distance = white ? 5 - rank : rank - 2;
                value += 15.0f + 14.0f * std::max(0, 3 - distance);
              }

            if(t == 0)
              whitePawn += value;
            else
              blackPawn += value;
          }

        auto danger = [&](int who)
          {
            if(kingSq[who] < 0)
              return 0.0f;

            const int rank = kingSq[who] / 8;
            const int file = kingSq[who] % 8;

            float pressure = 0.0f;

            for(int dr = -1; dr <= 1; dr++)
              {
                for(int df = -1; df <= 1; df++)
                  {
                    const int r = rank + dr, f = file + df;
                    if(r < 0 || r > 7 || f < 0 || f > 7)
                      continue;

                    pressure += std::min(2, attacks[1 - who][r * 8 + f]) * (dr == 0 && df == 0 ? 5.0f : 1.5f);
                  }
              }

            return pressure;
          };

        const float orient = gs->whiteToMove ? 1.0f : -1.0f;

        parts.material = (whiteMaterial - blackMaterial) * orient / 400.0f;
        parts.mobility = static_cast<float>(mobility[0] - mobility[1]) * orient * 0.005f;
        parts.kingSafety = (danger(1) - danger(0)) * orient * 0.012f;
        parts.pawnStructure = (whitePawn - blackPawn) * orient / 400.0f;

        return parts;
      }

    /* Sum up (BUT DO NOT APPLY AN ACTIVATION FUNCTION) the hand-coded heuristic evaluations. */
    float heuristicLogit(GameState* gs)
      {
        const auto p = heuristicParts(gs);
        return p.material + p.mobility + p.kingSafety + p.pawnStructure + p.see;
      }

    /* Encode the given GameState into network-ready input.
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . 1 . . . . 1 .
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    1 1 1 1 1 1 1 1    . . . . . . . .
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .
       1 1 1 1 1 1 1 1    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .
       . . . . . . . .    . 1 . . . . 1 .    . . 1 . . 1 . .    1 . . . . . . 1    . . . . 1 . . .    . . . 1 . . . .    . . . . . . . .    . . . . . . . .
       0: Friendly        1: Friendly        2: Friendly        3: Friendly        4: Friendly        5: King            6: Opponent        7: Opponent
          pawns              knights            bishops            rooks              queens                                pawns              knights

       . . 1 . . 1 . .    1 . . . . . . 1    . . . 1 . . . .    . . . . 1 . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    1 1 1 1 1 1 1 1    . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    1 1 1 1 1 1 1 1    . . . . . . . .    . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .    . . . . . . . .
       8: Opponent        9: Opponent        10: Opponent       11: Opponent       12: Friendly       13: Opponent       14: Counting       15: Player to move
          bishops            rooks               queens             king               pawn attacks       pawn attacks       active?            must mate?

       . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .
       . . . . . . . .    . . . . . . . .
       16: Opponent       17: Normalized
           must mate?         countdown      */
    void encode(GameState* gs, float* features)
      {
      	int square;

        std::memset(features, 0, PLANES * SQUARES * sizeof(float)); //  Blank out.

        for(square = 0; square < SQUARES; square++)
          {
            const auto p = gs->board[square];
            if(!p)
              continue;

            const int kind = kindOf(p);
            const int plane = sideOf(p) == (gs->whiteToMove ? 1 : -1) ? kind : 6 + kind;

            features[plane * SQUARES + canonicalSquare(square, gs->whiteToMove)] = 1.0f;
            if(kind == 0)
              {
                Move targets[_NONE];
                const unsigned n = getPawnAttackable(square, gs, targets);
                const int attackPlane = plane == 0 ? FEATURE_OWN_PAWN_ATTACK : FEATURE_ENEMY_PAWN_ATTACK;
                for(unsigned i = 0; i < n; i++)
                  {
                    if(targets[i].to < SQUARES)
                      features[attackPlane * SQUARES + canonicalSquare(targets[i].to, gs->whiteToMove)] = 1.0f;
                  }
              }
          }

        const CountPhase phase = countPhase(gs);
        int phasePlane = -1;

        if(phase == COUNT_BOARD)
          phasePlane = FEATURE_COUNT_BOARD;
        if(phase == COUNT_WHITE_MATES || phase == COUNT_BLACK_MATES)
          {
            const bool ownMates = (phase == COUNT_WHITE_MATES) == gs->whiteToMove;
            phasePlane = ownMates ? FEATURE_COUNT_OWN_MATES : FEATURE_COUNT_ENEMY_MATES;
          }

        for(square = 0; square < SQUARES; square++)
          {
            if(phasePlane >= 0)
              features[phasePlane * SQUARES + square] = 1.0f;

            features[FEATURE_CLOCK * SQUARES + square] = phase == COUNT_NONE ? 0.0f : gs->moveCtr / 128.0f;
          }

        return;
      }

    /* Initial processing layer. */
    void stem(const float* x, float* out)
      {
        Eigen::Map<const PlaneMatrix> a(x);
        Eigen::Map<const StemWeights> w(weights + STEM_WEIGHT);
        Eigen::Map<const Vec32> b(weights + STEM_BIAS);
        Eigen::Map<InputMatrix> y(out);
        y.noalias() = w * a;                                        //  Apply weights.
        y.colwise() += b;                                           //  Broadcast bias.
        y = y.cwiseMax(0.0f);                                       //  ReLU.
        return;
      }

    /* 3 x 3 convolution. */
    void depthwise(const float* x, float* out, const float* w, const float* b)
      {
        for(int ch = 0; ch < CHANNELS; ch++)
          {
            for(int row = 0; row < 8; row++)
              {
                for(int col = 0; col < 8; col++)
                  {
                    float sum = b[ch];
                    for(int kr = 0; kr < 3; kr++)
                      {
                        const int r = row + kr - 1;
                        if(r < 0 || r >= 8)
                          continue;

                        for(int kc = 0; kc < 3; kc++)
                          {
                            const int c = col + kc - 1;
                            if(c < 0 || c >= 8)
                              continue;

                            sum += x[ch * SQUARES + r * 8 + c] * w[ch * 9 + kr * 3 + kc];
                          }
                      }
                    out[ch * SQUARES + row * 8 + col] = std::max(0.0f, sum);
                  }
              }
          }

        return;
      }

    /* Combine across channel. */
    void pointwise(const float* x, float* out, const float* wptr, const float* bptr, const float* residual)
      {
        Eigen::Map<const InputMatrix> a(x);
        Eigen::Map<const BlockWeights> w(wptr);
        Eigen::Map<const Vec32> b(bptr);
        Eigen::Map<InputMatrix> y(out);
        y.noalias() = w * a;
        y.colwise() += b;

        if(residual)                                                //  Residual connection.
          {
            Eigen::Map<const InputMatrix> r(residual);
            y += r;
          }

        y = y.cwiseMax(0.0f);                                       //  ReLU.
        return;
      }

    /* Forward. */
    float neuralLogit(const float* features)
      {
        stem(features, activationA);                                //  Run stem.

        float* current = activationA;
        float* scratch1 = activationB;
        float* scratch2 = activationC;

        for(int block = 0; block < BLOCKS; block++)                 //  Run residual blocks.
          {
            const int base = BLOCK_START + block * BLOCK_FLOATS;
            depthwise(current, scratch1, weights + base, weights + base + 9 * CHANNELS);

            const int pw1 = base + 9 * CHANNELS + CHANNELS;
            pointwise(scratch1, scratch2, weights + pw1, weights + pw1 + CHANNELS * CHANNELS, nullptr);

            const int dw2 = pw1 + CHANNELS * CHANNELS + CHANNELS;
            depthwise(scratch2, scratch1, weights + dw2, weights + dw2 + 9 * CHANNELS);

            const int pw2 = dw2 + 9 * CHANNELS + CHANNELS;
            pointwise(scratch1, scratch2, weights + pw2, weights + pw2 + CHANNELS * CHANNELS, current);

            std::swap(current, scratch2);
          }

        Eigen::Map<const FlatActivations> flat(current);
        Eigen::Map<const DenseWeights> hw(weights + HIDDEN_WEIGHT);
        Eigen::Map<const Vec64> hb(weights + HIDDEN_BIAS);
        Eigen::Map<Vec64> h(hidden);

        h.noalias() = hw * flat;
        h += hb;
        h = h.cwiseMax(0.0f);                                       //  ReLU.
        Eigen::Map<const Vec64> vw(weights + VALUE_WEIGHT);

        return vw.dot(h) + weights[VALUE_BIAS];
      }
  }                                                                 //  END anonymous namespace

extern "C"
  {
    /**********************************************************************************************
     Globals  */

    unsigned char inputGameStateBuffer[_GAMESTATE_BYTE_SIZE] = {};  //  Global array containing the serialized INPUT game state.

    unsigned char inputMoveBuffer[_MOVE_BYTE_SIZE] = {};            //  Global array containing the serialized INPUT move.

    unsigned char outputGameStateBuffer[_GAMESTATE_BYTE_SIZE] = {}; //  Global array containing the serialized OUTPUT game state.

                                                                    //  Global array containing up to _MAX_MOVES moves.
                                                                    //  Rather than encode the number of moves in the array itself, we return an integer.
                                                                    //  Each move is represented as a byte sub-array encoding:
                                                                    //    _MOVE_BYTE_SIZE  :  bytes encoding a single move,
                                                                    //    4                :  bytes for signed integer, which is rough score.
                                                                    //    1                :  byte (should be Boolean) indicating whether move is "quiet".
    unsigned char outputMovesBuffer[_MAX_MOVES * (_MOVE_BYTE_SIZE + 5)] = {};

                                                                    //  Global array containing a specially-encoded game state for repetition testing.
                                                                    //  (For Makruk, we are enforcing a 3-count limit on repeated states.)
    unsigned char outputRepetitionStateBuffer[_REPETITION_STATE_BYTE_SIZE] = {};

    /**********************************************************************************************
     Functions  */

    /* Expose the global array declared here to JavaScript.  */
    unsigned char* getInputGameStateBuffer(void)
      {
  	    return inputGameStateBuffer;
      }

    /* Expose the global array declared here to JavaScript.  */
    unsigned char* getInputMoveBuffer(void)
      {
  	    return inputMoveBuffer;
      }

    /* Expose the global array declared here to JavaScript.  */
    unsigned char* getOutputGameStateBuffer(void)
      {
  	    return outputGameStateBuffer;
      }

    /* Expose the global array declared here to JavaScript.  */
    unsigned char* getOutputMovesBuffer(void)
      {
  	    return outputMovesBuffer;
      }

    /* Expose the global array declared here to JavaScript.  */
    unsigned char* getOutputRepetitionStateBuffer(void)
      {
  	    return outputRepetitionStateBuffer;
      }

    /* Expose the global array declared here to JavaScript.  */
    uintptr_t getWeightsBuffer(void)
      {
  	    return reinterpret_cast<uintptr_t>(weights);
      }

    unsigned int weightsBufferSize(void)
      {
  	    return PARAMETER_FLOATS * sizeof(float);
      }

    unsigned int featureCount_eval(void)
      {
  	    return PLANES * SQUARES;
      }
                                                                    //  Enforce spec.
    static_assert(_GAMESTATE_BYTE_SIZE == 66, "Expected Makruk 66-byte game state.");
    static_assert(_REPETITION_STATE_BYTE_SIZE == 65, "Expected 65-byte Makruk repetition identity.");
    static_assert(sizeof(float) == 4 && sizeof(int32_t) == 4, "Expected 32-bit floats and scores.");
    static_assert(_MOVE_BYTE_SIZE == 2, "Expected Makruk 2-byte moves.");

    /* Decode GameState from the input buffer. */
    void decodeGameState(GameState* gs)
      {
        gs->whiteToMove = (inputGameStateBuffer[0] & 128) != 0;     //  Recover side to move.
        std::memcpy(gs->board, inputGameStateBuffer + 1, SQUARES);  //  Recover the board contents.
        gs->moveCtr = inputGameStateBuffer[65];                     //  Recover the move counter.
      }

    /* Encode GameState to the output buffer. */
    void encodeGameState(const GameState* gs)
      {
        outputGameStateBuffer[0] = gs->whiteToMove ? 128 : 0;       //  Encode the side to move.
        std::memcpy(outputGameStateBuffer + 1, gs->board, SQUARES); //  Encode the board's contents.
        outputGameStateBuffer[65] = gs->moveCtr;                    //  Encode the move counter.
      }

    /* Indicate which side is now to move. */
    unsigned char sideToMove_eval()
      {
      	                                                            //  No need to decode; just peek at the top byte.
        return (inputGameStateBuffer[0] & 128) ? _WHITE_TO_MOVE : _BLACK_TO_MOVE;
      }

    /* Indicate whether the input gamestate is terminal. */
    unsigned char isTerminal_eval()
      {
        GameState gs;
        decodeGameState(&gs);                                       //  Recover GameState from buffer.
        return terminal(&gs) ? 1 : 0;
      }

    /* Indicate whether the side to move in the input gamestate has its king in check. */
    unsigned char isSideToMoveInCheck_eval()
      {
        GameState gs;
        decodeGameState(&gs);                                       //  Recover GameState from buffer.

        const unsigned char king = gs.whiteToMove ? _WHITE_KING : _BLACK_KING;
        const unsigned char opponent = gs.whiteToMove ? 'b' : 'w';

        unsigned char index = 0;
        while(index < _NONE && gs.board[index] != king)
          index++;

        if(index == _NONE)
          return 0;

        return inCheckBy(index, opponent, &gs) ? 1 : 0;
      }

    /* Provide a rough evaluation of non-pawn material for the input gamestate's side to move. */
    unsigned char nonPawnMaterial_eval()
      {
        GameState gs;
        unsigned char knights = 0;
        unsigned char bishops = 0;
        unsigned char rooks = 0;
        unsigned char queens = 0;
        unsigned char i;

        decodeGameState(&gs);                                       //  Recover GameState from buffer.

        if(gs.whiteToMove)
          {
            for(i = 0; i < _NONE; i++)
              {
                if(isWhite(i, &gs))
                  {
                    if(isKnight(i, &gs))
                      knights++;
                    else if(isBishop(i, &gs))
                      bishops++;
                    else if(isRook(i, &gs))
                      rooks++;
                    else if(isQueen(i, &gs))
                      queens++;
                  }
              }
          }
        else
          {
            for(i = 0; i < _NONE; i++)
              {
                if(isBlack(i, &gs))
                  {
                    if(isKnight(i, &gs))
                      knights++;
                    else if(isBishop(i, &gs))
                      bishops++;
                    else if(isRook(i, &gs))
                      rooks++;
                    else if(isQueen(i, &gs))
                      queens++;
                  }
              }
          }

        return 2 * (queens + bishops) + 3 * knights + 5 * rooks;
      }

    /* Apply the move in the move-input buffer to the GameState in the input buffer.
       Write the resulting GameState to the output buffer. */
    void makeMove_eval()
      {
        GameState gs;

        decodeGameState(&gs);                                       //  Recover GameState from buffer.
        Move mv {inputMoveBuffer[0], inputMoveBuffer[1]};           //  Recover Move from buffer.
                                                                    //  The searcher must pass a move returned by getMoves_eval().
        if(mv.from < _NONE && mv.to < _NONE)
          makeMove(&mv, &gs);

        encodeGameState(&gs);                                       //  Write GameState to output buffer.
        return;
      }

    /* Apply a null-move to the GameState in the input buffer.
       Write the result to the output buffer. */
    void makeNullMove_eval()
      {
        GameState gs;
        decodeGameState(&gs);                                       //  Recover GameState from buffer.
        makeNullMove(&gs);
        encodeGameState(&gs);                                       //  Write GameState to output buffer.
        return;
      }

    /* Compute the hand-coded heuristic evaluation of the GameState in the input buffer. */
    float heuristicLogit_eval()
      {
        GameState gs;
        decodeGameState(&gs);                                       //  Recover GameState from buffer.
        return heuristicLogit(&gs);
      }

    /* Compute the network evaluation of the GameState in the input buffer--DO NOT APPLY THE FINAL ACTIVATION HERE. */
    float neuralLogit_eval()
      {
        GameState gs;
        decodeGameState(&gs);                                       //  Recover GameState from buffer.
        encode(&gs, input);                                         //  Encode network input from GameState.
        return neuralLogit(input);                                  //  Return network evaluation.
      }

    /* Compute an evaluation of the GameState in the input buffer using BOTH hand-coded heuristics and network evaluation.
       THIS FUNCTION DOES APPLY THE FINAL ACTIVATION FUNCTION. */
    float evaluate_eval()
      {
        GameState gs;
        decodeGameState(&gs);                                       //  Recover GameState from buffer.
        const auto result = isWin(&gs);
        if(result != GAME_ONGOING)
          {
            if(result == GAME_OVER_STALEMATE)
              return 0.0f;
            return ((result == GAME_OVER_WHITE_WINS) == gs.whiteToMove) ? 1.0f : -1.0f;
          }

        encode(&gs, input);                                         //  Encode network input from GameState.
        return std::tanh(heuristicLogit(&gs) + neuralLogit(input)); //  Apply the ACTIVATION FUNCTION TANH to the weighted sum of network and heuristic evaluations.
      }

    /*  Canonical repetition identity: side to move + 64 board squares.
        The 66-byte game state's countdown is deliberately excluded. */
    void repetitionState_eval()
      {
        outputRepetitionStateBuffer[0] = inputGameStateBuffer[0] & 128;
        std::memcpy(outputRepetitionStateBuffer + 1, inputGameStateBuffer + 1, SQUARES);
      }

    /*  Automatic draw on the third occurrence of the same canonical position. */
    unsigned char historyVerdict_eval(unsigned int occurrences)
      {
        return occurrences >= _MAX_STATE_REPETITION ? 1 : 0;
      }

    /* Compute the moves available for the GameState in the input buffer.
       Write a rough-scored list of results to the moves-output buffer. */
    unsigned int getMoves_eval()
      {
        GameState gs;
        decodeGameState(&gs);                                       //  Recover GameState from buffer.
        Move moves[_MAX_MOVES];

        const unsigned int len = getMoves(&gs, moves);
        for(unsigned int i = 0; i < len; i++)
          {
            const Move& mv = moves[i];
            const unsigned char victim = gs.board[mv.to];
            const bool promotes = kindOf(gs.board[mv.from]) == 0 && (mv.to / 8 == (gs.whiteToMove ? 5 : 2));
            const bool capture = victim != _EMPTY;
                                                                    //  Reward captures and promotions to encourage cuttoffs in negamax search.
            const int32_t orderScore = static_cast<int32_t>( (capture ? 10.0f * pieceValue(victim) - pieceValue(gs.board[mv.from]) : 0.0f) +
                                                             (promotes ? 10.0f * (QUEEN_VALUE - PAWN_VALUE) : 0.0f)                        );
            const unsigned int base = i * (_MOVE_BYTE_SIZE + 5);

            outputMovesBuffer[base] = mv.from;                      //  Write move to buffer.
            outputMovesBuffer[base + 1] = mv.to;

            std::memcpy(outputMovesBuffer + base + 2, &orderScore, 4);
            outputMovesBuffer[base + 6] = (!capture && !promotes) ? 0 : 1;
          }

        return len;
      }
  }                                                                 //  END extern "C"