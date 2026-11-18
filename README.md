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
