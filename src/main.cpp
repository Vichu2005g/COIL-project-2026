#include "crow_all.h"
#include "RobotController.h"
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

RobotController controller;

int main() {
    crow::SimpleApp app;

    // --- Route 1: Root (Serve GUI) ---
    CROW_ROUTE(app, "/")([]() {
        return crow::mustache::load_text("index.html");
    });

    // --- Route 1.1: Static Assets ---
    CROW_ROUTE(app, "/style.css")([]() {
        std::ifstream f("style.css");
        std::stringstream ss;
        ss << f.rdbuf();
        return crow::response(ss.str());
    });

    CROW_ROUTE(app, "/app.js")([]() {
        std::ifstream f("app.js");
        std::stringstream ss;
        ss << f.rdbuf();
        return crow::response(ss.str());
    });

    // --- Route 2: Connect (POST) ---
    CROW_ROUTE(app, "/connect").methods("POST"_method)([](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400);

        string ip = x["ip"].s();
        int port = x["port"].i();
        string protocol = x.has("protocol") ? x["protocol"].s() : "udp";

        bool success = controller.configure(ip, port, protocol);

        crow::json::wvalue res;
        res["success"] = success;
        res["message"] = success ? "Robot connection configured." : "Failed to configure robot connection.";
        res["ip"] = ip;
        res["port"] = port;
        res["protocol"] = protocol;

        return crow::response(res);
    });

    // --- Route 3: Telecommand (PUT) ---
    CROW_ROUTE(app, "/telecommand/").methods("PUT"_method)([](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400);

        string command = x["command"].s();
        int duration = x.has("duration") ? x["duration"].i() : 0;
        int power = x.has("power") ? x["power"].i() : 100;

        string message;
        bool ack = false;
        int pktCount = 0;
        bool success = false;

        if (command == "forward") {
            success = controller.sendDrive(FORWARD, duration, power, message, ack, pktCount);
        } else if (command == "backward") {
            success = controller.sendDrive(BACKWARD, duration, power, message, ack, pktCount);
        } else if (command == "left") {
            success = controller.sendTurn(LEFT, duration, message, ack, pktCount);
        } else if (command == "right") {
            success = controller.sendTurn(RIGHT, duration, message, ack, pktCount);
        } else if (command == "sleep") {
            success = controller.sendSleep(message, ack, pktCount);
        }

        crow::json::wvalue res;
        res["success"] = success;
        res["command"] = command;
        res["ack"] = ack;
        res["message"] = message;
        res["pktCount"] = pktCount;

        return crow::response(res);
    });

    // --- Route 4: Telemetry Request (GET) ---
    CROW_ROUTE(app, "/telementry_request/")([]() {
        string message;
        bool ack = false;
        int pktCount = 0;
        TelemetryBody telemetry;
        
        bool success = controller.requestTelemetry(message, ack, pktCount, telemetry);

        crow::json::wvalue res;
        res["success"] = success;
        res["ack"] = ack;
        res["message"] = message;
        res["pktCount"] = pktCount;

        if (success) {
            crow::json::wvalue tel;
            tel["lastPktCounter"] = telemetry.LastPktCounter;
            tel["currentGrade"] = telemetry.CurrentGrade;
            tel["hitCount"] = telemetry.HitCount;
            tel["heading"] = telemetry.Heading;
            tel["lastCmd"] = telemetry.LastCmd;
            tel["lastCmdValue"] = telemetry.LastCmdValue;
            tel["lastCmdPower"] = telemetry.LastCmdPower;
            res["telemetry"] = std::move(tel);
        }

        return crow::response(res);
    });

    // --- Route 5: Routing Table (Placeholder) ---
    CROW_ROUTE(app, "/routing_table/").methods("GET"_method, "POST"_method)([](const crow::request& req) {
        crow::json::wvalue res;
        res["success"] = true;
        res["enabled"] = false;
        res["message"] = "Routing table feature is currently passive.";
        return crow::response(res);
    });

    // --- Extra Route: Logs (GET) ---
    CROW_ROUTE(app, "/logs")([]() {
        auto logs = controller.getLogs();
        crow::json::wvalue res;
        vector<crow::json::wvalue> logList;
        for (const auto& entry : logs) {
            crow::json::wvalue item;
            item["timestamp"] = entry.timestamp;
            item["direction"] = entry.direction;
            item["content"] = entry.content;
            logList.push_back(std::move(item));
        }
        res["logs"] = std::move(logList);
        return crow::response(res);
    });

    cout << "COIL Robot Control Center starting on http://localhost:6769" << endl;
    app.port(6769).multithreaded().run();
}
