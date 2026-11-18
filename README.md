# [Makruk](https://www.ericjoycefilm.com/wastesoftime/boardgames/makruk/index.php?lang=en)
Notes on the creation of Makruk

## Docker container to compile C to WebAssembly
Create the container.
```
sudo docker build -t c-wasm .
```

Confirm its existence.
```
sudo docker images
```

Kill the container.
```
sudo docker image rm c-wasm
```

## Zobrist hash generator
This executable (not a WebAssembly module) lives on the server back-end. Compile using GCC. Call it when the page loads to generate a random Zobrist hash for every game.
```
gcc -Wall zgenerate.c -lm -o zgenerate
```

## Opening-book Zobrist hasher
Unlike the in-game hasher, this one is *not* randomly generated for each session.

This executable (not a WebAssembly module) lives on the server back-end. Compile using GCC. Call it from the PHP lookup script.
```
g++ -std=c++11 -Wall hash.cpp -lm -o hash
```

For example:
```
./hash 128 4 2 3 6 5 3 2 4 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 7 7 7 7 7 7 7 7 0 0 0 0 0 0 0 0 10 8 9 11 12 9 8 10 0
```

should produce
```
4190965492281958052
```

To look this position up in the opening book, call:
```
./lookup 4190965492281958052 128 4 2 3 6 5 3 2 4 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 7 7 7 7 7 7 7 7 0 0 0 0 0 0 0 0 10 8 9 11 12 9 8 10 0
```

which produces, for example,
```
SUCCESS,21,29
```












## Citation
If this code was helpful to you, please cite this repository.

```
@misc{boku,
  title={Makruk in C},
  author={Eric C. Joyce},
  year={2026},
  publisher={Github},
  journal={GitHub repository},
  howpublished={\url{https://github.com/EricCJoyce/Makruk}}
}
```
