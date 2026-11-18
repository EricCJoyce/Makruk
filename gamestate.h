#ifndef __GAMESTATE_H
#define __GAMESTATE_H

#include <ctype.h>
#include <math.h>                                                   /* Needed for INFINITY and tanh. */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define _NONE                        64

#define _EMPTY                     0x00
#define _WHITE_PAWN                0x01
#define _WHITE_KNIGHT              0x02
#define _WHITE_BISHOP              0x03
#define _WHITE_ROOK                0x04
#define _WHITE_QUEEN               0x05
#define _WHITE_KING                0x06
#define _BLACK_PAWN                0x07
#define _BLACK_KNIGHT              0x08
#define _BLACK_BISHOP              0x09
#define _BLACK_ROOK                0x0A
#define _BLACK_QUEEN               0x0B
#define _BLACK_KING                0x0C

#define _WHITE_TO_MOVE                0
#define _BLACK_TO_MOVE                1

#define GAME_ONGOING                  0
#define GAME_OVER_WHITE_WINS          1
#define GAME_OVER_BLACK_WINS          2
#define GAME_OVER_STALEMATE           3

#define _GAMESTATE_BYTE_SIZE         65                             /* Number of bytes needed to store a GameState structure. */
#define _MOVE_BYTE_SIZE               2                             /* Number of bytes needed to store a Move structure. */
#define _MAX_NUM_TARGETS             32                             /* A (generous) upper bound on how many distinct destinations (not distinct moves)
                                                                       may be available to a player from a single index. */
#define _MAX_MOVES                  256                             /* A (generous) upper bound on how many moves are available to a team in a single turn. */

#define _REPETITION_STATE_BYTE_SIZE  64                             /* Bytes needed for repetition-detection encoding. */
#define _MAX_STATE_REPETITION         3                             /* Sources disagree on Makruk's draw-by-repetition rules, but this implementation is
                                                                       going to enforce the policy that the 3rd occurrence of a game state forces a draw. */

/**************************************************************************************************
 Typedefs  */

typedef struct GameStateType                                        //  TOTAL: 65 bytes.
  {
    bool whiteToMove;                                               //  True: white to move. False: black to move.
    unsigned char board[_NONE];                                     //  Array of characters.

    unsigned char moveCtr;                                          //  When neither side has any PAWNS, the game must be completed within a certain number of
                                                                    //  moves or it is declared a draw.
  } GameState;

typedef struct MoveType                                             //  TOTAL: 2 bytes.
  {
    unsigned char from;                                             //  Index in [0, 64).
    unsigned char to;                                               //  Index in [0, 64).
  } Move;

/**************************************************************************************************
 Prototypes  */

void copyGameState(GameState*, GameState*);

void makeMove(Move*, GameState*);
void makeNullMove(GameState*);
char nowToMove(GameState*);
char nextToMove(GameState*);
bool inCheckBy(unsigned char, unsigned char, GameState*);
unsigned int getMoves(GameState*, Move*);
unsigned int getMovesIndex(unsigned char, GameState*, Move*);

unsigned int getPawnMoves(unsigned char, GameState*, Move*);
unsigned int getPawnAttackable(unsigned char, GameState*, Move*);
unsigned char attackersOfSquare(unsigned char, unsigned char, GameState*, Move*);

unsigned int getKnightMoves(unsigned char, GameState*, Move*);
unsigned int getBishopMoves(unsigned char, GameState*, Move*);
unsigned int getRookMoves(unsigned char, GameState*, Move*);
unsigned int getQueenMoves(unsigned char, GameState*, Move*);
unsigned int getKingMoves(unsigned char, GameState*, Move*);

unsigned char isWin(GameState*);
unsigned char countLimit(GameState*);
bool insufficientMaterial(GameState*);
bool terminal(GameState*);

bool isEmpty(unsigned char, GameState*);
bool isWhite(unsigned char, GameState*);
bool isBlack(unsigned char, GameState*);
bool sameSide(unsigned char, unsigned char, GameState*);
bool opposed(unsigned char, unsigned char, GameState*);
char getTeam(unsigned char, GameState*);
bool isPawn(unsigned char, GameState*);
bool isKnight(unsigned char, GameState*);
bool isBishop(unsigned char, GameState*);
bool isRook(unsigned char, GameState*);
bool isQueen(unsigned char, GameState*);
bool isKing(unsigned char, GameState*);

bool isCapture(Move*, GameState*);

unsigned char uSet(unsigned char, char*, GameState*, unsigned char*);
unsigned char dSet(unsigned char, char*, GameState*, unsigned char*);
unsigned char lSet(unsigned char, char*, GameState*, unsigned char*);
unsigned char rSet(unsigned char, char*, GameState*, unsigned char*);

unsigned char u(unsigned char);
unsigned char d(unsigned char);
unsigned char l(unsigned char);
unsigned char r(unsigned char);
unsigned char ul(unsigned char);
unsigned char ur(unsigned char);
unsigned char dl(unsigned char);
unsigned char dr(unsigned char);
unsigned char row(unsigned char);
unsigned char col(unsigned char);

/**************************************************************************************************
 Globals  */


/**************************************************************************************************
 Functions  */

void copyGameState(GameState* src, GameState* dst)
  {
    unsigned char i;

    dst->whiteToMove = src->whiteToMove;

    for(i = 0; i < _NONE; i++)
      dst->board[i] = src->board[i];

    dst->moveCtr = src->moveCtr;

    return;
  }

/**************************************************************************************************
 Move generation and Application  */

void makeMove(Move* move, GameState* gs)
  {
    unsigned char prevLimit, newLimit;

    prevLimit = countLimit(gs);                                     //  What was the limit BEFORE this move?

                                                                    //  Pawn promotion
    if(isPawn(move->from, gs) && (row(move->to) == 5 || row(move->to) == 2))
      {
        if(isWhite(move->from, gs))
          gs->board[move->to] = _WHITE_QUEEN;
        else
          gs->board[move->to] = _BLACK_QUEEN;
        gs->board[move->from] = _EMPTY;
      }
    else
      {
        gs->board[move->to] = gs->board[move->from];
        gs->board[move->from] = _EMPTY;
      }

    newLimit = countLimit(gs);                                      //  Check again, after the move.
    if(prevLimit != newLimit)                                       //  Does a new limit apply?
      gs->moveCtr = 0;                                              //  Reset for new conditions.
    else if(newLimit > 0)                                           //  Don't increment if we are not counting.
      gs->moveCtr++;                                                //  Increment under standing conditions.

    gs->whiteToMove = !gs->whiteToMove;                             //  Flip flag.

    return;
  }

/* Does not apply to real chess, but this is convenient for tree-search. */
void makeNullMove(GameState* gs)
  {
    gs->whiteToMove = !gs->whiteToMove;                             //  Flip flag.
    return;
  }

/* Return a character indicating who is to move now. */
char nowToMove(GameState* gs)
  {
    return (gs->whiteToMove) ? 'w' : 'b';
  }

/* Return a character indicating who is next to move (once the side to move has played). */
char nextToMove(GameState* gs)
  {
    return (gs->whiteToMove) ? 'b' : 'w';
  }

/* 'team' is holding 'index' in check. */
bool inCheckBy(unsigned char index, unsigned char team, GameState* gs)
  {
    bool ret = false;
    unsigned char enemytargets[_MAX_MOVES];                         //  Generous upper bound attacks every square four times.
    unsigned int enemyStrikeCtr = 0;
    unsigned char i;
    unsigned int j, len = 0;
    Move tmp[_NONE];                                                //  Assumes generous upper bound of all squares being reachable.

    for(i = 0; i < _NONE; i++)
      {
        if((isWhite(i, gs) && team == 'w') || (isBlack(i, gs) && team == 'b'))
          {
            if(isPawn(i, gs))
              len = getPawnAttackable(i, gs, tmp);
            else if(isKnight(i, gs))
              len = getKnightMoves(i, gs, tmp);
            else if(isBishop(i, gs))
              len = getBishopMoves(i, gs, tmp);
            else if(isRook(i, gs))
              len = getRookMoves(i, gs, tmp);
            else if(isQueen(i, gs))
              len = getQueenMoves(i, gs, tmp);
            else
              len = getKingMoves(i, gs, tmp);

            for(j = 0; j < len; j++)
              enemytargets[enemyStrikeCtr + j] = tmp[j].to;

            enemyStrikeCtr += len;                                  //  Count up strikes
          }
      }

    j = 0;
    while(j < enemyStrikeCtr && enemytargets[j] != index)
      j++;

    if(j < enemyStrikeCtr)
      ret = true;

    return ret;
  }

/* THIS FUNCTION FILTERS FOR CHECK!!
   Return number of moves. Actual Move objects stored in given buffer. */
unsigned int getMoves(GameState* gs, Move* buffer)
  {
    unsigned int movesCtr = 0;
    Move potentialmoves[_MAX_MOVES];                                //  Assumes generous upper bound of moves per piece.
    unsigned int potentialmovesCtr = 0;
    unsigned int i;
    unsigned char index;

    for(index = 0; index < _NONE; index++)
      {
        if((gs->whiteToMove && isWhite(index, gs)) || (!gs->whiteToMove && isBlack(index, gs)))
          {
            potentialmovesCtr = getMovesIndex(index, gs, potentialmoves);
            if(potentialmovesCtr > 0)
              {
                for(i = 0; i < potentialmovesCtr; i++)
                  {
                    buffer[movesCtr].from = potentialmoves[i].from;
                    buffer[movesCtr].to = potentialmoves[i].to;
                    movesCtr++;
                  }
              }
          }
      }

    return movesCtr;
  }

/* THIS FUNCTION FILTERS FOR CHECK!!
   Return number of moves. Actual Move objects stored in given buffer. */
unsigned int getMovesIndex(unsigned char index, GameState* gs, Move* buffer)
  {
    Move potentialmoves[_MAX_MOVES];                                //  Assume a generous upper bound of moves.
    unsigned int potentialmovesCtr = 0;
    unsigned int movesCtr = 0;
    unsigned int i;
    unsigned char j;
    GameState tmp;

    if(!isEmpty(index, gs))
      {
        if(isPawn(index, gs))
          potentialmovesCtr = getPawnMoves(index, gs, potentialmoves);
        else if(isKnight(index, gs))
          potentialmovesCtr = getKnightMoves(index, gs, potentialmoves);
        else if(isBishop(index, gs))
          potentialmovesCtr = getBishopMoves(index, gs, potentialmoves);
        else if(isRook(index, gs))
          potentialmovesCtr = getRookMoves(index, gs, potentialmoves);
        else if(isQueen(index, gs))
          potentialmovesCtr = getQueenMoves(index, gs, potentialmoves);
        else
          potentialmovesCtr = getKingMoves(index, gs, potentialmoves);

        if(isWhite(index, gs))                                      //  Piece is white, check for checks on the King by Black
          {
            for(i = 0; i < potentialmovesCtr; i++)                  //  For every move, make that move, then test the resultant board
              {
                copyGameState(gs, &tmp);                            //  Copy the board
                makeMove(potentialmoves + i, &tmp);                 //  Apply the candidate move

                j = 0;                                              //  Locate the king on the new board
                while(j < _NONE && tmp.board[j] != _WHITE_KING)
                  j++;

                if(!inCheckBy(j, 'b', &tmp))                        //  If king not in check, then move is allowed
                  {
                    buffer[movesCtr].from = potentialmoves[i].from;
                    buffer[movesCtr].to = potentialmoves[i].to;
                    movesCtr++;
                  }
              }
          }
        else                                                        //  Piece is black, check for checks on the King by White
          {
            for(i = 0; i < potentialmovesCtr; i++)                  //  For every move, make that move, then test the resultant board
              {
                copyGameState(gs, &tmp);                            //  Copy the board
                makeMove(potentialmoves + i, &tmp);                 //  Apply the candidate move

                j = 0;                                              //  Locate the king on the new board
                while(j < _NONE && tmp.board[j] != _BLACK_KING)
                  j++;

                if(!inCheckBy(j, 'w', &tmp))                        //  If king not in check, then move is allowed
                  {
                    buffer[movesCtr].from = potentialmoves[i].from;
                    buffer[movesCtr].to = potentialmoves[i].to;
                    movesCtr++;
                  }
              }
          }
      }

    return movesCtr;
  }

/**************************************************************************************************
 Pawn functions  */

/* Pawn moves are all moves a pawn may make, including attacks. */
unsigned int getPawnMoves(unsigned char index, GameState* gs, Move* buffer)
  {
    unsigned int movesCtr = 0;
    unsigned int i, len = 0;

    if(isWhite(index, gs))
      {
                                                                    //  White Pawn forward.
        if(u(index) < _NONE && isEmpty(u(index), gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = u(index);
            movesCtr++;
          }
                                                                    //  White Pawn attack forward-left.
        if(ul(index) < _NONE && row(ul(index)) == row(index) + 1 && col(ul(index)) == col(index) - 1 && isBlack(ul(index), gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = ul(index);
            movesCtr++;
          }
                                                                    //  White Pawn attack forward-right.
        if(ur(index) < _NONE && row(ur(index)) == row(index) + 1 && col(ur(index)) == col(index) + 1 && isBlack(ur(index), gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = ur(index);
            movesCtr++;
          }
      }
    else
      {
                                                                    //  Black Pawn forward.
        if(d(index) < _NONE && isEmpty(d(index), gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = d(index);
            movesCtr++;
          }
                                                                    //  Black Pawn attack forward-left.
        if(dl(index) < _NONE && row(dl(index)) == row(index) - 1 && col(dl(index)) == col(index) - 1 && isWhite(dl(index), gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = dl(index);
            movesCtr++;
          }
                                                                    //  Black Pawn attack forward-right.
        if(dr(index) < _NONE && row(dr(index)) == row(index) - 1 && col(dr(index)) == col(index) + 1 && isWhite(dr(index), gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = dr(index);
            movesCtr++;
          }
      }

    return movesCtr;
  }

/* This function only tests that the forward diagonals of "index" do not fall off the board.
   This function does not consider whether the attackable squares are occupied,
   does not consider pawn promotion, and does not consider en-passant attacks. */
unsigned int getPawnAttackable(unsigned char index, GameState* gs, Move* buffer)
  {
    unsigned char movesCtr = 0;

    if(isWhite(index, gs))
      {
        if(ul(index) < _NONE)
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = ul(index);
            movesCtr++;
          }
        if(ur(index) < _NONE)
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = ur(index);
            movesCtr++;
          }
      }
    else if(isBlack(index, gs))
      {
        if(dl(index) < _NONE)
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = dl(index);
            movesCtr++;
          }
        if(dr(index) < _NONE)
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = dr(index);
            movesCtr++;
          }
      }

    return movesCtr;
  }

/* Build an array of indices of all pieces belonging to "team" that can attack "index" on given GameState "gs". */
unsigned char attackersOfSquare(unsigned char index, unsigned char team, GameState* gs, Move* buffer)
  {
    unsigned char len = 0;
    unsigned char i;
    unsigned int lenMoves, j;
    Move moves[_MAX_MOVES];                                         //  Assumes generous upper bound of moves.

    for(i = 0; i < _NONE; i++)                                      //  Scan every square.
      {
        if(getTeam(i, gs) == team)                                  //  If this square contains a piece belonging to "team", proceed.
          {
            lenMoves = getMovesIndex(i, gs, moves);                 //  All (LEGAL) moves for this piece.
            for(j = 0; j < lenMoves; j++)
              {
                if(moves[j].to == index)                            //  If the piece at "i" can attack "index", add "i" to the array.
                  {
                    buffer[len].from = i;
                    buffer[len].to = index;
                    len++;
                  }
              }
          }
      }

    return len;
  }

/**************************************************************************************************
 Non-Pawn pieces' functions  */

/* Return number of moves. Actual Move objects stored in given buffer. */
unsigned int getKnightMoves(unsigned char index, GameState* gs, Move* buffer)
  {
    unsigned int movesCtr = 0;

    if(ul(u(index)) < _NONE && !sameSide(ul(u(index)), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = ul(u(index));
        movesCtr++;
      }
    if(ur(u(index)) < _NONE && !sameSide(ur(u(index)), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = ur(u(index));
        movesCtr++;
      }

    if(ur(r(index)) < _NONE && !sameSide(ur(r(index)), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = ur(r(index));
        movesCtr++;
      }
    if(ul(l(index)) < _NONE && !sameSide(ul(l(index)), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = ul(l(index));
        movesCtr++;
      }

    if(dl(d(index)) < _NONE && !sameSide(dl(d(index)), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = dl(d(index));
        movesCtr++;
      }
    if(dr(d(index)) < _NONE && !sameSide(dr(d(index)), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = dr(d(index));
        movesCtr++;
      }

    if(dr(r(index)) < _NONE && !sameSide(dr(r(index)), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = dr(r(index));
        movesCtr++;
      }
    if(dl(l(index)) < _NONE && !sameSide(dl(l(index)), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = dl(l(index));
        movesCtr++;
      }

    return movesCtr;
  }

/* Return number of moves. Actual Move objects stored in given buffer. */
unsigned int getBishopMoves(unsigned char index, GameState* gs, Move* buffer)
  {
    unsigned int movesCtr = 0;

    if(isWhite(index, gs))
      {
        if(ul(index) < _NONE && !sameSide(ul(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = ul(index);
            movesCtr++;
          }
        if(u(index) < _NONE && !sameSide(u(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = u(index);
            movesCtr++;
          }
        if(ur(index) < _NONE && !sameSide(ur(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = ur(index);
            movesCtr++;
          }
        if(dl(index) < _NONE && !sameSide(dl(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = dl(index);
            movesCtr++;
          }
        if(dr(index) < _NONE && !sameSide(dr(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = dr(index);
            movesCtr++;
          }
      }
    else
      {
        if(dl(index) < _NONE && !sameSide(dl(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = dl(index);
            movesCtr++;
          }
        if(d(index) < _NONE && !sameSide(d(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = d(index);
            movesCtr++;
          }
        if(dr(index) < _NONE && !sameSide(dr(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = dr(index);
            movesCtr++;
          }
        if(ul(index) < _NONE && !sameSide(ul(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = ul(index);
            movesCtr++;
          }
        if(ur(index) < _NONE && !sameSide(ur(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = ur(index);
            movesCtr++;
          }
      }

    return movesCtr;
  }

/* Return number of moves. Actual Move objects stored in given buffer. */
unsigned int getRookMoves(unsigned char index, GameState* gs, Move* buffer)
  {
    unsigned int movesCtr = 0;
    unsigned char len = 0;
    unsigned char i;
    unsigned char tmpBuff[8];                                       //  Upper bound in any direction is 8 on an 8 x 8 board.
    char* flags;

    if(isWhite(index, gs))
      flags = "B";
    else
      flags = "W";

    len = uSet(index, flags, gs, tmpBuff);                          //  Up
    for(i = 0; i < len; i++)
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = tmpBuff[i];
        movesCtr++;
      }

    len = rSet(index, flags, gs, tmpBuff);                          //  Right
    for(i = 0; i < len; i++)
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = tmpBuff[i];
        movesCtr++;
      }

    len = dSet(index, flags, gs, tmpBuff);                          //  Down
    for(i = 0; i < len; i++)
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = tmpBuff[i];
        movesCtr++;
      }

    len = lSet(index, flags, gs, tmpBuff);                          //  Left
    for(i = 0; i < len; i++)
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = tmpBuff[i];
        movesCtr++;
      }

    return movesCtr;
  }

/* Return number of moves. Actual Move objects stored in given buffer. */
unsigned int getQueenMoves(unsigned char index, GameState* gs, Move* buffer)
  {
    unsigned int movesCtr = 0;

        if(ul(index) < _NONE && !sameSide(ul(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = ul(index);
            movesCtr++;
          }
        if(ur(index) < _NONE && !sameSide(ur(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = ur(index);
            movesCtr++;
          }
        if(dl(index) < _NONE && !sameSide(dl(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = dl(index);
            movesCtr++;
          }
        if(dr(index) < _NONE && !sameSide(dr(index), index, gs))
          {
            buffer[movesCtr].from = index;
            buffer[movesCtr].to = dr(index);
            movesCtr++;
          }

    return movesCtr;
  }

/* Return number of moves. Actual Move objects stored in given buffer. */
unsigned int getKingMoves(unsigned char index, GameState* gs, Move* buffer)
  {
    unsigned int movesCtr = 0;

    if(u(index) < _NONE && !sameSide(u(index), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = u(index);
        movesCtr++;
      }
    if(ur(index) < _NONE && !sameSide(ur(index), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = ur(index);
        movesCtr++;
      }
    if(r(index) < _NONE && !sameSide(r(index), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = r(index);
        movesCtr++;
      }
    if(dr(index) < _NONE && !sameSide(dr(index), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = dr(index);
        movesCtr++;
      }
    if(d(index) < _NONE && !sameSide(d(index), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = d(index);
        movesCtr++;
      }
    if(dl(index) < _NONE && !sameSide(dl(index), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = dl(index);
        movesCtr++;
      }
    if(l(index) < _NONE && !sameSide(l(index), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = l(index);
        movesCtr++;
      }
    if(ul(index) < _NONE && !sameSide(ul(index), index, gs))
      {
        buffer[movesCtr].from = index;
        buffer[movesCtr].to = ul(index);
        movesCtr++;
      }

    return movesCtr;
  }

/**************************************************************************************************
 End-State Testing  */

/*  Given a board array, return
     GAME_ONGOING         if the state is not a win for either player
     GAME_OVER_WHITE_WINS if the state is a win for White
     GAME_OVER_BLACK_WINS if the state is a win for Black
     GAME_OVER_STALEMATE  if the state is a stalemate */
unsigned char isWin(GameState* gs)
  {
    Move moves[_MAX_MOVES];                                         //  Generous upper-bound assumption.
    unsigned int len;
    unsigned char kpos = 0;
    unsigned char limit;

    len = getMoves(gs, moves);                                      //  Get moves for side to move

    if(len == 0)                                                    //  Game is over if side to move cannot move
      {
        if(nowToMove(gs) == 'w')
          {
            while(kpos < _NONE && gs->board[kpos] != _WHITE_KING)
              kpos++;
          }
        else
          {
            while(kpos < _NONE && gs->board[kpos] != _BLACK_KING)
              kpos++;
          }

        if(nowToMove(gs) == 'w')
          {
            if(inCheckBy(kpos, 'b', gs))
              return GAME_OVER_BLACK_WINS;
            return GAME_OVER_STALEMATE;
          }
        else
          {
            if(inCheckBy(kpos, 'w', gs))
              return GAME_OVER_WHITE_WINS;
            return GAME_OVER_STALEMATE;
          }
      }
    else if(insufficientMaterial(gs))                               //  Game is over if there is insufficient material to mate.
      return GAME_OVER_STALEMATE;

    limit = countLimit(gs);                                         //  What is the current limit?

    if(limit > 0 && gs->moveCtr >= limit)                           //  A limit applies, and we have reached it.
      return GAME_OVER_STALEMATE;

    return GAME_ONGOING;
  }

/* When neither side has any PAWNS, the game must be completed within a certain number of moves or it is declared a draw.
   When a piece is captured the count restarts only if it is the last piece of a player in the game.

   When neither player has any PAWNS left, mate must be achieved in 64 moves.

   When the last piece that is not the KING of the disadvantaged player is captured, the count may be started, or restarted from the aforementioned counting,
     by the weaker player.

   The stronger player now has a maximum number of moves based on the pieces left:
     - 2 ROOKS:                                       8 moves
     - 1 ROOK:                                       16 moves
     - 0 ROOKS, 2 BISHOPS:                           22 moves
     - 0 ROOKS, 1 BISHOP:                            44 moves
     - 0 ROOKS, 0 BISHOPS, 2 KNIGHTS:                32 moves
     - 0 ROOKS, 0 BISHOPS, 1 KNIGHT:                 64 moves
     - 0 ROOKS, 0 BISHOPS, 0 KNIGHTS, n >= 1 QUEENS: 64 moves
*/
unsigned char countLimit(GameState* gs)
  {
    unsigned char i;

    unsigned char wRookCtr = 0;
    unsigned char bRookCtr = 0;

    unsigned char wBishopCtr = 0;
    unsigned char bBishopCtr = 0;

    unsigned char wKnightCtr = 0;
    unsigned char bKnightCtr = 0;

    unsigned char wQueenCtr = 0;
    unsigned char bQueenCtr = 0;

    unsigned char wPawnCtr = 0;
    unsigned char bPawnCtr = 0;

    unsigned char wMaterialCtr, bMaterialCtr;                       //  Non-pawn, non-king material.

    for(i = 0; i < _NONE; i++)
      {
        if(isWhite(i, gs))
          {
            switch(gs->board[i])
              {
                case _WHITE_PAWN:    wPawnCtr++;    break;
                case _WHITE_QUEEN:   wQueenCtr++;   break;
                case _WHITE_KNIGHT:  wKnightCtr++;  break;
                case _WHITE_BISHOP:  wBishopCtr++;  break;
                case _WHITE_ROOK:    wRookCtr++;    break;
              }
          }
        else if(isBlack(i, gs))
          {
            switch(gs->board[i])
              {
                case _BLACK_PAWN:    bPawnCtr++;    break;
                case _BLACK_QUEEN:   bQueenCtr++;   break;
                case _BLACK_KNIGHT:  bKnightCtr++;  break;
                case _BLACK_BISHOP:  bBishopCtr++;  break;
                case _BLACK_ROOK:    bRookCtr++;    break;
              }
          }
      }

    wMaterialCtr = wQueenCtr + wKnightCtr + wBishopCtr + wRookCtr;
    bMaterialCtr = bQueenCtr + bKnightCtr + bBishopCtr + bRookCtr;

    if(wPawnCtr == 0 && bPawnCtr == 0)                              //  No pawns on either side; now the count matters.
      {
        if(wMaterialCtr == 0 && bMaterialCtr >= 1)                  //  Advantage Black.
          {
            if(bRookCtr == 2)
              return 8;
            if(bRookCtr == 1)
              return 16;
            if(bBishopCtr == 2)
              return 22;
            if(bBishopCtr == 1)
              return 44;
            if(bKnightCtr == 2)
              return 32;
            if(bKnightCtr == 1)
              return 64;
            return 64;
          }
        else if(wMaterialCtr >= 1 && bMaterialCtr == 0)             //  Advantage White.
          {
            if(wRookCtr == 2)
              return 8;
            if(wRookCtr == 1)
              return 16;
            if(wBishopCtr == 2)
              return 22;
            if(wBishopCtr == 1)
              return 44;
            if(wKnightCtr == 2)
              return 32;
            if(wKnightCtr == 1)
              return 64;
            return 64;
          }
      }

    return 0;
  }

/* Check for King versus King. */
bool insufficientMaterial(GameState* gs)
  {
    unsigned char i;

    for(i = 0; i < _NONE; i++)
      {
        if(isRook(i, gs) || isBishop(i, gs) || isKnight(i, gs) || isQueen(i, gs) || isPawn(i, gs))
          return false;
      }

    return true;
  }

bool terminal(GameState* gs)
  {
    unsigned char win;

    win = isWin(gs);

    return (win != GAME_ONGOING);
  }

/**************************************************************************************************
 Identities and Tests  */

/*  Is the given index i vacant? */
bool isEmpty(unsigned char i, GameState* gs)
  {
    return (gs->board[i] == _EMPTY);
  }

/*  Is the given index i occupied by a Black piece? */
bool isBlack(unsigned char i, GameState* gs)
  {
    return (gs->board[i] >= _BLACK_PAWN && gs->board[i] <= _BLACK_KING);
  }

/*  Is the given index i occupied by a White piece? */
bool isWhite(unsigned char i, GameState* gs)
  {
    return (gs->board[i] >= _WHITE_PAWN && gs->board[i] <= _WHITE_KING);
  }

/*  Is index i the same as index j
    in terms of both being White or both being Black or both being Empty? */
bool sameSide(unsigned char i, unsigned char j, GameState* gs)
  {
    return ((isWhite(i, gs) && isWhite(j, gs)) || (isBlack(i, gs) && isBlack(j, gs)));
  }

/*  More specific than same(), this function asks,
    "Are i and j on opposite teams?" */
bool opposed(unsigned char i, unsigned char j, GameState* gs)
  {
    return ((isWhite(i, gs) && isBlack(j, gs)) || (isBlack(i, gs) && isWhite(j, gs)));
  }

/* Return a character indicating which team 'index' belongs to. */
char getTeam(unsigned char index, GameState* gs)
  {
    if(isWhite(index, gs))
      return 'w';
    if(isBlack(index, gs))
      return 'b';
    return 'e';
  }

/*  Is the given index i occupied by a Pawn? */
bool isPawn(unsigned char i, GameState* gs)
  {
    if(i < _NONE)
      return (gs->board[i] == _WHITE_PAWN || gs->board[i] == _BLACK_PAWN);
    return false;
  }

/*  Is the given index i occupied by a Knight? */
bool isKnight(unsigned char i, GameState* gs)
  {
    if(i < _NONE)
      return (gs->board[i] == _WHITE_KNIGHT || gs->board[i] == _BLACK_KNIGHT);
    return false;
  }

/*  Is the given index i occupied by a Bishop? */
bool isBishop(unsigned char i, GameState* gs)
  {
    if(i < _NONE)
      return (gs->board[i] == _WHITE_BISHOP || gs->board[i] == _BLACK_BISHOP);
    return false;
  }

/*  Is the given index i occupied by a Rook? */
bool isRook(unsigned char i, GameState* gs)
  {
    if(i < _NONE)
      return (gs->board[i] == _WHITE_ROOK || gs->board[i] == _BLACK_ROOK);
    return false;
  }

/*  Is the given index i occupied by a Queen? */
bool isQueen(unsigned char i, GameState* gs)
  {
    if(i < _NONE)
      return (gs->board[i] == _WHITE_QUEEN || gs->board[i] == _BLACK_QUEEN);
    return false;
  }

/*  Is the given index i occupied by an King? */
bool isKing(unsigned char i, GameState* gs)
  {
    if(i < _NONE)
      return (gs->board[i] == _WHITE_KING || gs->board[i] == _BLACK_KING);
    return false;
  }

/* Is the given move a capture on the given GameState? */
bool isCapture(Move* move, GameState* gs)
  {
    return !isEmpty(move->to, gs);
  }

/**************************************************************************************************
 Set builders
   index:  starting position
   Flags:  w = pass through White
           W = stop and include White (as one would when attacking White)
           b = pass through Black
           B = stop and include Black (as one would when attacking Black)
   buffer: provided by the parent function, the set built ends up here, with length returned */

/* Build list of upward squares (e.g. 0, 8, 16, 24, ... ) */
unsigned char uSet(unsigned char index, char* fstr, GameState* gs, unsigned char* buffer)
  {
    unsigned char len = 0;
    unsigned char dst = u(index);

    bool w = false;                                                 //  Scan flag-string
    bool W = false;
    bool b = false;
    bool B = false;
    unsigned char i = 0;
    if((unsigned char)strlen(fstr) > 0)
      {
        while(fstr[i] != '\0')
          {
            if(fstr[i] == 'w')
              w = true;
            else if(fstr[i] == 'W')
              W = true;
            else if(fstr[i] == 'b')
              b = true;
            else if(fstr[i] == 'B')
              B = true;
            i++;
          }
      }

    while(dst < _NONE)
      {
        if(isEmpty(dst, gs))
          {
            buffer[len] = dst;
            len++;
            dst = u(dst);
          }
        else if(isWhite(dst, gs))
          {
            if(w)                                                   //  Pass through White
              {
                buffer[len] = dst;
                len++;
                dst = u(dst);
              }
            else if(W)                                              //  Stop and include White
              {
                buffer[len] = dst;
                len++;
                break;
              }
            else
              break;
          }
        else if(isBlack(dst, gs))
          {
            if(b)                                                   //  Pass through Black
              {
                buffer[len] = dst;
                len++;
                dst = u(dst);
              }
            else if(B)                                              //  Stop and include Black
              {
                buffer[len] = dst;
                len++;
                break;
              }
            else
              break;
          }
      }

    return len;
  }

/* Build list of downward squares (e.g. 56, 48, 40, 32, ... ) */
unsigned char dSet(unsigned char index, char* fstr, GameState* gs, unsigned char* buffer)
  {
    unsigned char len = 0;
    unsigned char dst = d(index);

    bool w = false;                                                 //  Scan flag-string
    bool W = false;
    bool b = false;
    bool B = false;
    unsigned char i = 0;
    if((unsigned char)strlen(fstr) > 0)
      {
        while(fstr[i] != '\0')
          {
            if(fstr[i] == 'w')
              w = true;
            else if(fstr[i] == 'W')
              W = true;
            else if(fstr[i] == 'b')
              b = true;
            else if(fstr[i] == 'B')
              B = true;
            i++;
          }
      }

    while(dst < _NONE)
      {
        if(isEmpty(dst, gs))
          {
            buffer[len] = dst;
            len++;
            dst = d(dst);
          }
        else if(isWhite(dst, gs))
          {
            if(w)                                                   //  Pass through White
              {
                buffer[len] = dst;
                len++;
                dst = d(dst);
              }
            else if(W)                                              //  Stop and include White
              {
                buffer[len] = dst;
                len++;
                break;
              }
            else
              break;
          }
        else if(isBlack(dst, gs))
          {
            if(b)                                                   //  Pass through Black
              {
                buffer[len] = dst;
                len++;
                dst = d(dst);
              }
            else if(B)                                              //  Stop and include Black
              {
                buffer[len] = dst;
                len++;
                break;
              }
            else
              break;
          }
      }

    return len;
  }

/* Build list of leftward squares (e.g. 39, 38, 37, 36, ... ) */
unsigned char lSet(unsigned char index, char* fstr, GameState* gs, unsigned char* buffer)
  {
    unsigned char len = 0;
    unsigned char dst = l(index);

    bool w = false;                                                 //  Scan flag-string
    bool W = false;
    bool b = false;
    bool B = false;
    unsigned char i = 0;
    if((unsigned char)strlen(fstr) > 0)
      {
        while(fstr[i] != '\0')
          {
            if(fstr[i] == 'w')
              w = true;
            else if(fstr[i] == 'W')
              W = true;
            else if(fstr[i] == 'b')
              b = true;
            else if(fstr[i] == 'B')
              B = true;
            i++;
          }
      }

    while(dst < _NONE)
      {
        if(isEmpty(dst, gs))
          {
            buffer[len] = dst;
            len++;
            dst = l(dst);
          }
        else if(isWhite(dst, gs))
          {
            if(w)                                                   //  Pass through White
              {
                buffer[len] = dst;
                len++;
                dst = l(dst);
              }
            else if(W)                                              //  Stop and include White
              {
                buffer[len] = dst;
                len++;
                break;
              }
            else
              break;
          }
        else if(isBlack(dst, gs))
          {
            if(b)                                                   //  Pass through Black
              {
                buffer[len] = dst;
                len++;
                dst = l(dst);
              }
            else if(B)                                              //  Stop and include Black
              {
                buffer[len] = dst;
                len++;
                break;
              }
            else
              break;
          }
      }

    return len;
  }

/* Build list of rightward squares (e.g. 32, 33, 34, 35, ... ) */
unsigned char rSet(unsigned char index, char* fstr, GameState* gs, unsigned char* buffer)
  {
    unsigned char len = 0;
    unsigned char dst = r(index);

    bool w = false;                                                 //  Scan flag-string
    bool W = false;
    bool b = false;
    bool B = false;
    unsigned char i = 0;
    if((unsigned char)strlen(fstr) > 0)
      {
        while(fstr[i] != '\0')
          {
            if(fstr[i] == 'w')
              w = true;
            else if(fstr[i] == 'W')
              W = true;
            else if(fstr[i] == 'b')
              b = true;
            else if(fstr[i] == 'B')
              B = true;
            i++;
          }
      }

    while(dst < _NONE)
      {
        if(isEmpty(dst, gs))
          {
            buffer[len] = dst;
            len++;
            dst = r(dst);
          }
        else if(isWhite(dst, gs))
          {
            if(w)                                                   //  Pass through White
              {
                buffer[len] = dst;
                len++;
                dst = r(dst);
              }
            else if(W)                                              //  Stop and include White
              {
                buffer[len] = dst;
                len++;
                break;
              }
            else
              break;
          }
        else if(isBlack(dst, gs))
          {
            if(b)                                                   //  Pass through Black
              {
                buffer[len] = dst;
                len++;
                dst = r(dst);
              }
            else if(B)                                              //  Stop and include Black
              {
                buffer[len] = dst;
                len++;
                break;
              }
            else
              break;
          }
      }

    return len;
  }

/**************************************************************************************************
 Board logic  */

/*  Return the index UP from the given i */
unsigned char u(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i + 8) == row(i) + 1)
          return i + 8;
      }
    return _NONE;
  }

/*  Return the index DOWN from the given i */
unsigned char d(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i - 8) == row(i) - 1 && row(i) != _NONE)
          return i - 8;
      }
    return _NONE;
  }

/*  Return the index LEFT from the given i */
unsigned char l(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i - 1) == row(i))
          return i - 1;
      }
    return _NONE;
  }

/*  Return the index RIGHT from the given i */
unsigned char r(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i + 1) == row(i))
          return i + 1;
      }
    return _NONE;
  }

/*  Return the index UP-LEFT from the given i */
unsigned char ul(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i + 7) == row(i) + 1)
          return i + 7;
      }
    return _NONE;
  }

/*  Return the index UP-RIGHT from the given i */
unsigned char ur(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i + 9) == row(i) + 1)
          return i + 9;
      }
    return _NONE;
  }

/*  Return the index DOWN-LEFT from the given i */
unsigned char dl(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i - 9) == row(i) - 1 && row(i) != _NONE)
          return i - 9;
      }
    return _NONE;
  }

/*  Return the index DOWN-RIGHT from the given i */
unsigned char dr(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i - 7) == row(i) - 1 && row(i) != _NONE)
          return i - 7;
      }
    return _NONE;
  }

/*  Compute the COLUMN in which given index is included */
unsigned char col(unsigned char i)
  {
    if(i < _NONE)
      return i & 7;                                                 //  i & 7 == i % 8 because 8 is a power of 2
    return _NONE;
  }

/*  Compute the ROW in which given index is included */
unsigned char row(unsigned char i)
  {
    if(i < _NONE)
      return (i - (i & 7)) / 8;                                     //  i & 7 == i % 8 because 8 is a power of 2
    return _NONE;
  }

#endif
