/*

Game logic module for the human player.

sudo docker run --rm -v $(pwd):/src -u $(id -u):$(id -g) --mount type=bind,source=$(pwd),target=/home/src c-wasm emcc -Os -s STANDALONE_WASM -s INITIAL_HEAP=1048576 -s EXPORTED_FUNCTIONS="['_getCurrentState','_getMovesBuffer','_sideToMove_client','_isWhite_client','_isBlack_client','_isEmpty_client','_isPawn_client','_isKnight_client','_isBishop_client','_isRook_client','_isQueen_client','_isKing_client','_getMovesIndex_client','_makeMove_client','_isTerminal_client','_isWin_client','_repetitionBarrier_client','_draw']" -Wl,--no-entry "gamelogic.c" -o "gamelogic.wasm"

*/

#include "gamestate.h"

/**************************************************************************************************
 Typedefs  */


/**************************************************************************************************
 Prototypes  */

__attribute__((import_module("env"), import_name("_printRow"))) void printRow(char a, char b, char c, char d, char e, char f, char g, char h);
__attribute__((import_module("env"), import_name("_printGameStateData"))) void printGameStateData(bool wToMove, unsigned char moveCtr);

unsigned char* getCurrentState(void);
unsigned char* getMovesBuffer(void);
void serialize(GameState*);
void deserialize(GameState*);

unsigned char sideToMove_client(void);
bool isWhite_client(unsigned char);
bool isBlack_client(unsigned char);
bool isEmpty_client(unsigned char);
bool isPawn_client(unsigned char);
bool isKnight_client(unsigned char);
bool isBishop_client(unsigned char);
bool isRook_client(unsigned char);
bool isQueen_client(unsigned char);
bool isKing_client(unsigned char);

unsigned int getMovesIndex_client(unsigned char);
void makeMove_client(unsigned char, unsigned char);
bool isTerminal_client(void);
unsigned char isWin_client(void);
unsigned char repetitionBarrier_client(void);

void draw(void);

/**************************************************************************************************
 Globals  */

unsigned char currentState[_GAMESTATE_BYTE_SIZE];                   //  Global array containing the serialized game state.
unsigned char movesBuffer[_MAX_NUM_TARGETS];                        //  Global array containing the unique destination-indices
                                                                    //  (not necessarily the number of unique moves) available.

/**************************************************************************************************
 Functions  */

/* Expose the global array declared here to JavaScript.  */
unsigned char* getCurrentState(void)
  {
    return &currentState[0];
  }

/* Expose the global array declared here to JavaScript.  */
unsigned char* getMovesBuffer(void)
  {
    return &movesBuffer[0];
  }

/* Game State Encoding & Decoding

   Byte [     0] = Side to move:  [7][6][5][4][3][2][1][0]
                                   ^  ^  ^  ^  ^  ^  ^  ^
                                   |  |  |  |  |  |  |  +--- reserved
                                   |  |  |  |  |  |  +------ reserved
                                   |  |  |  |  |  +--------- reserved
                                   |  |  |  |  +------------ reserved
                                   |  |  |  +--------------- reserved
                                   |  |  +------------------ reserved
                                   |  +--------------------- reserved
                                   +------------------------ ON: white to move; OFF: black to move.
   Byte [     1] = Move counter.
   Bytes[ 2, 65] = Board encoding. */

/* Pack a GameState into the unsigned-char buffer "currentState". */
void serialize(GameState* gs)
  {
    unsigned char i = 0, j;
    unsigned char ch;

    //////////////////////////////////////////////////////////////////  (1 byte) Encode side to move.
    if(gs->whiteToMove)                                             //  Set bit: white to move.
      ch = 128;
    else
      ch = 0;

    currentState[i++] = ch;                                         //  Write byte.

    //////////////////////////////////////////////////////////////////  (64 bytes) Encode the board.
    for(j = 0; j < _NONE; j++)
      {
        ch = 0;
        if(isWhite(j, gs))
          {
            if(isPawn(j, gs))
              ch = _WHITE_PAWN;
            else if(isKnight(j, gs))
              ch = _WHITE_KNIGHT;
            else if(isBishop(j, gs))
              ch = _WHITE_BISHOP;
            else if(isRook(j, gs))
              ch = _WHITE_ROOK;
            else if(isQueen(j, gs))
              ch = _WHITE_QUEEN;
            else
              ch = _WHITE_KING;
          }
        else if(isBlack(j, gs))
          {
            if(isPawn(j, gs))
              ch = _BLACK_PAWN;
            else if(isKnight(j, gs))
              ch = _BLACK_KNIGHT;
            else if(isBishop(j, gs))
              ch = _BLACK_BISHOP;
            else if(isRook(j, gs))
              ch = _BLACK_ROOK;
            else if(isQueen(j, gs))
              ch = _BLACK_QUEEN;
            else
              ch = _BLACK_KING;
          }

        currentState[i++] = ch;                                     //  Write byte.
      }

    //////////////////////////////////////////////////////////////////  (1 byte) Encode the move counter.
    ch = gs->moveCtr;                                               //  Write the move counter.

    currentState[i++] = ch;                                         //  Write byte.

    return;                                                         //  TOTAL: 66 bytes.
  }

/* Recover a GameState from the unsigned-char buffer "currentState". */
void deserialize(GameState* gs)
  {
    unsigned char i = 0, j;

    for(j = 0; j < _NONE; j++)                                      //  Fill-in/blank-out.
      gs->board[j] = _EMPTY;

    //////////////////////////////////////////////////////////////////  (1 byte) Decode side to move.
    gs->whiteToMove = ((currentState[i++] & 128) == 128);           //  Recover side to move.

    //////////////////////////////////////////////////////////////////  (64 bytes) Decode the board.
    for(j = 0; j < _NONE; j++)
      {
        if(currentState[i] == _WHITE_PAWN)
          gs->board[j] = _WHITE_PAWN;
        else if(currentState[i] == _WHITE_KNIGHT)
          gs->board[j] = _WHITE_KNIGHT;
        else if(currentState[i] == _WHITE_BISHOP)
          gs->board[j] = _WHITE_BISHOP;
        else if(currentState[i] == _WHITE_ROOK)
          gs->board[j] = _WHITE_ROOK;
        else if(currentState[i] == _WHITE_QUEEN)
          gs->board[j] = _WHITE_QUEEN;
        else if(currentState[i] == _WHITE_KING)
          gs->board[j] = _WHITE_KING;
        else if(currentState[i] == _BLACK_PAWN)
          gs->board[j] = _BLACK_PAWN;
        else if(currentState[i] == _BLACK_KNIGHT)
          gs->board[j] = _BLACK_KNIGHT;
        else if(currentState[i] == _BLACK_BISHOP)
          gs->board[j] = _BLACK_BISHOP;
        else if(currentState[i] == _BLACK_ROOK)
          gs->board[j] = _BLACK_ROOK;
        else if(currentState[i] == _BLACK_QUEEN)
          gs->board[j] = _BLACK_QUEEN;
        else if(currentState[i] == _BLACK_KING)
          gs->board[j] = _BLACK_KING;

        i++;
      }

    //////////////////////////////////////////////////////////////////  (1 byte) Recover the move counter.
    gs->moveCtr = currentState[i++];

    return;                                                         //  TOTAL: 66 bytes.
  }

/* Answer the client-side question, Whose turn is it? */
unsigned char sideToMove_client(void)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return (gs.whiteToMove) ? _WHITE_TO_MOVE : _BLACK_TO_MOVE;
  }

bool isWhite_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isWhite(index, &gs);
  }

bool isBlack_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isBlack(index, &gs);
  }

bool isEmpty_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isEmpty(index, &gs);
  }

bool isPawn_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isPawn(index, &gs);
  }

bool isKnight_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isKnight(index, &gs);
  }

bool isBishop_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isBishop(index, &gs);
  }

bool isRook_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isRook(index, &gs);
  }

bool isQueen_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isQueen(index, &gs);
  }

bool isKing_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isKing(index, &gs);
  }

bool isTerminal_client(void)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return terminal(&gs);
  }

/* Returns unsigned char in {GAME_ONGOING         = 0,
                             GAME_OVER_WHITE_WINS = 1,
                             GAME_OVER_BLACK_WINS = 2,
                             GAME_OVER_STALEMATE  = 3}. */
unsigned char isWin_client(void)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isWin(&gs);
  }

/* Given an index, recover the game state from the global buffer "currentState", and compute the moves available to the piece at "index."
   The number of moves is returned, and that many bytes in "movesBuffer" will contain a destinations.
   This function is intended to answer queries from the human player. */
unsigned int getMovesIndex_client(unsigned char index)
  {
    GameState gs;
    Move moves[_MAX_NUM_TARGETS];                                   //  Generous upper bound assumes that a single piece could reach half of all squares.
    unsigned int len, i = 0, j;
    unsigned int ctr;
    unsigned char indices[_MAX_NUM_TARGETS];

    deserialize(&gs);                                               //  Recover GameState from buffer.
    len = getMovesIndex(index, &gs, moves);

    ctr = 0;
    for(i = 0; i < len; i++)                                        //  Iterate through moves for index and identify unique destination indices.
      {
        j = 0;
        while(j < ctr && indices[j] != moves[i].to)
          j++;
        if(j == ctr)
          indices[ctr++] = moves[i].to;
      }

    i = 0;                                                          //  Reset. 'i' now iterates into 'movesBuffer'.
    for(len = 0; len < ctr; len++)
      movesBuffer[i++] = indices[len];

    return ctr;
  }

/* Update "currentState" according to the given move data (if those data are indeed valid!) */
void makeMove_client(unsigned char from, unsigned char to)
  {
    GameState gs;
    Move moves[_NONE];                                              //  Generous assumption that every square is reachable.
    Move move;
    unsigned int len, i;

    deserialize(&gs);                                               //  Recover GameState from buffer.
    len = getMovesIndex(from, &gs, moves);                          //  Make sure that this move is legal.
    i = 0;                                                          //  Otherwise, ignore it. Cheaters lose their turns!
    while(i < len && !(moves[i].from == from && moves[i].to == to))
      i++;
    if(i < len)
      {
        move.from = from;
        move.to = to;
        makeMove(&move, &gs);
      }

    serialize(&gs);                                                 //  Write updated GameState back to buffer.

    return;
  }

/* Indicate whether a repeated-state tracker may be reset. */
unsigned char repetitionBarrier_client(void)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return gs.moveCtr == 0;
  }

/* Draw the board to the JavaScript console.
   r n b q k b n r
   . . . . . . . .
   p p p p p p p p
   . . . . . . . .
   . . . . . . . .
   P P P P P P P P
   . . . . . . . .
   R N B K Q B N R */
void draw(void)
  {
    GameState gs;
    signed char y;

    deserialize(&gs);                                               //  Recover GameState from buffer.

    for(y = 7; y >= 0; y--)
      printRow(gs.board[y * 8], gs.board[y * 8 + 1], gs.board[y * 8 + 2], gs.board[y * 8 + 3], gs.board[y * 8 + 4], gs.board[y * 8 + 5], gs.board[y * 8 + 6], gs.board[y * 8 + 7]);

    printGameStateData(gs.whiteToMove, gs.moveCtr);
    return;
  }
