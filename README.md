
# The C Minus Minus Code Submission


## Introduction
This project is a multiplayer game developed entirely in C. It was designed to be played by two players in real time, using a client-server model to allow gameplay across multiple devices. 

## Gameplay

### Single-Device Version (dance.c)
In the single-device version:
- Player 1 controls their character using **WASD** keys.
- Player 2 controls their character using the **Arrow** keys.
- The game runs smoothly with real-time interactions between players on a single device.

### Client-Server Version (client.c & server.c)
In the client-server setup:
- We successfully implemented a connection between the client and server.
- However, due to synchronization limitations, the gameplay currently executes the connection but doesnt load the game efficiently.

## Files Provided
- **dance.c**: Contains the fully functional, real-time single-device version of the game.
- **client.c** and **server.c**: These files set up a client-server connection.
- **additional files** contain all the image and audio files necessary for the execution of the code 

## How to Run

### Running the Single-Device Version
1. Compile `dance.c`:
   ```bash
   gcc dance.c -o dance -lraylib -lm
   ```
2. Run the game:
   ```bash
   ./dance
   ```
3. Use **WASD** keys for Player 1 and **Arrow** keys for Player 2 to control their characters.

### Running the Client-Server Setup
1. Compile `server.c`:
   ```bash
   gcc server.c -o server -pthreads
   ```
2. Run the server:
   ```bash
   ./server
   ```
3. In a new terminal, compile and run `client.c`:
   ```bash
   gcc client.c -o client -lraylib -lm -pthreads
   ./client
   ```
4. Note: Gameplay might not execute but the connection will be established


