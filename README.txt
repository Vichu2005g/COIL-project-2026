COIL Robot Command & Control GUI - Milestone 3
--------------------------------------

To build and run the Robot Command & Control GUI using Docker, open your terminal (PowerShell, CMD, or bash) inside the root directory and run:

1. Build the Docker Image:
docker build -t coil-robot-gui .

2. Run the Container:
docker run -d --rm -p 6769:6769 --name robot-gui-final coil-robot-gui

3. Access the GUI:
Once the container is running, open any web browser and navigate to: http://localhost:6769

4. Connect to Robot Simulator (Or physical robot):
On the GUI, enter IP: 10.172.41.150 (Or 127.0.0.1 for local testing) and Port: 29500 (UDP).
Click 'Initialize Connection' to begin routing commands.

5. Stopping the Application:
docker stop robot-gui-final

6. Fast Re-deploy (Development Tip):
docker build -t coil-robot-gui . ; docker rm -f robot-gui-final ; docker run -d --rm -p 6769:6769 --name robot-gui-final coil-robot-gui
