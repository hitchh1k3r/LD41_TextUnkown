**Ludum Dare 41 :: TextUnkown's AdventureGrounds**

This is my Ludum Dare 41 game. Compiled with emscripten:
emcc -O3 -g4 src/main.c -s WASM=1 -s ASSERTIONS=2 -o game.js