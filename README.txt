COIL Robot Control Center - Milestone 3
--------------------------------------

This project has been restructured to facilitate the RESTful web server implementation using Crow.

Project Structure:
- src/              : Backend source code (main.cpp, RobotController, Packet, Socket)
- public/           : Frontend assets (index.html, style.css, app.js)
- external/         : External dependencies (Add crow_all.h here)

Compilation Instructions:
1. Ensure you have 'crow_all.h' in the 'external/' directory.
2. Open the solution in Visual Studio.
3. Update the Project configuration to include:
   - Additional Include Directories: ./src; ./external
   - Linker Dependencies: ws2_32.lib
4. Ensure your compiler is set to C++17 or later.
5. Build and Run.

Execution Instructions:
1. Once the server is running, it will listen on http://localhost:18080.
2. Open Robot_Simulator.exe in a separate terminal.
3. Open a web browser and navigate to http://localhost:18080.
4. Input the IP (e.g., 127.0.0.1) and Port (e.g., 29500) and click 'Initialize Connection'.
5. Use the GUI to send Drive, Sleep, and Telemetry commands.

Logging:
Packet traffic is logged both in the server console and displayed in the 'Packet Traffic Log' section of the web GUI.
