#include "crow_all.h"
#include "RobotController.h"
#include <curl/curl.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <mutex>

using namespace std;

RobotController controller;

// --- Routing Table State ---
struct RoutingTarget {
    string ip;
    int port = 0;
    bool enabled = false;
};
RoutingTarget routingTarget;
mutex routingMutex;

// --- libcurl write callback ---
static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// --- Forward a command to the next-hop Web Service via HTTP ---
string forwardToNextHop(const string& endpoint, const string& method, const string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) return "{\"success\":false,\"message\":\"curl init failed\"}";

    string url = "http://" + routingTarget.ip + ":" + to_string(routingTarget.port) + endpoint;
    string response;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "{\"success\":false,\"message\":\"Routing forward failed: " + string(curl_easy_strerror(res)) + "\"}";
    }
    return response;
}

// --- Ping a target web service to check reachability before enabling routing ---
bool pingTarget(const string& ip, int port, string& errorMsg) {
    CURL* curl = curl_easy_init();
    if (!curl) { errorMsg = "curl init failed"; return false; }

    string url = "http://" + ip + ":" + to_string(port) + "/";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);       // HEAD request - no body needed
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);      // 3 second timeout
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        errorMsg = string(curl_easy_strerror(res));
        return false;
    }
    // HTTP 200-499 all mean the server is up (even 404 means it responded)
    if (httpCode >= 200 && httpCode < 500) return true;

    errorMsg = "Server returned HTTP " + to_string(httpCode);
    return false;
}

void addStatus(crow::json::wvalue& res) {
    res["stats"]["sent"] = controller.getSentCount();
    res["stats"]["accepted"] = controller.getAckOKCount();
    res["stats"]["rejected"] = controller.getAckBadCount();
    res["stats"]["pktCount"] = controller.getPktCount();
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    crow::SimpleApp app;

    // --- Route 1: Root (Serve GUI) ---
    CROW_ROUTE(app, "/")([]() {
        std::ifstream f("index.html");
        if (!f.is_open()) return crow::response(404, "index.html not found");
        std::stringstream ss;
        ss << f.rdbuf();
        return crow::response(ss.str());
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
        string protocol = x.has("protocol") ? std::string(x["protocol"].s()) : "udp";

        bool success = controller.configure(ip, port, protocol);

        crow::json::wvalue res;
        res["success"] = success;
        res["message"] = success ? "Robot connection configured." : "Failed to configure robot connection.";
        res["ip"] = ip;
        res["port"] = port;
        res["protocol"] = protocol;
        addStatus(res);

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
        addStatus(res);

        // --- Forward to next hop if routing is enabled ---
        {
            lock_guard<mutex> lock(routingMutex);
            if (routingTarget.enabled && !routingTarget.ip.empty()) {
                string fwdResponse = forwardToNextHop("/telecommand/", "PUT", req.body);
                res["routing"]["forwarded"] = true;
                res["routing"]["target"] = routingTarget.ip + ":" + to_string(routingTarget.port);
                res["routing"]["response"] = fwdResponse;
            } else {
                res["routing"]["forwarded"] = false;
            }
        }

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
        addStatus(res);

        if (success) {
            crow::json::wvalue tel;
            tel["lastPktCounter"] = (int)telemetry.LastPktCounter;
            tel["currentGrade"] = (int)telemetry.CurrentGrade;
            tel["hitCount"] = (int)telemetry.HitCount;
            tel["heading"] = (int)telemetry.Heading;
            tel["lastCmd"] = (int)telemetry.LastCmd;
            tel["lastCmdValue"] = (int)telemetry.LastCmdValue;
            tel["lastCmdPower"] = (int)telemetry.LastCmdPower;
            res["telemetry"] = std::move(tel);
        }

        // Also forward telemetry request if routing enabled
        {
            lock_guard<mutex> lock(routingMutex);
            if (routingTarget.enabled && !routingTarget.ip.empty()) {
                string fwdResponse = forwardToNextHop("/telementry_request/", "GET", "");
                res["routing"]["forwarded"] = true;
                res["routing"]["target"] = routingTarget.ip + ":" + to_string(routingTarget.port);
                res["routing"]["telemetry_response"] = fwdResponse;
            }
        }

        return crow::response(res);
    });

    // --- Route 5: Routing Table ---
    CROW_ROUTE(app, "/routing_table/").methods("GET"_method, "POST"_method, "DELETE"_method)(
        [](const crow::request& req) {
            crow::json::wvalue res;

            if (req.method == crow::HTTPMethod::GET) {
                // Return current routing config
                lock_guard<mutex> lock(routingMutex);
                res["success"] = true;
                res["enabled"] = routingTarget.enabled;
                res["target_ip"] = routingTarget.ip;
                res["target_port"] = routingTarget.port;
                res["message"] = routingTarget.enabled
                    ? "Routing active -> " + routingTarget.ip + ":" + to_string(routingTarget.port)
                    : "Routing disabled.";

            } else if (req.method == crow::HTTPMethod::POST) {
                // Configure routing target — verify reachability first
                auto x = crow::json::load(req.body);
                if (!x) return crow::response(400);

                string newIp   = x.has("target_ip")   ? string(x["target_ip"].s()) : "";
                int    newPort = x.has("target_port")  ? x["target_port"].i() : 6769;
                bool   enable  = x.has("enabled")      ? x["enabled"].b() : true;

                if (enable && !newIp.empty()) {
                    string pingErr;
                    bool reachable = pingTarget(newIp, newPort, pingErr);
                    if (!reachable) {
                        // Target not reachable - do NOT enable routing
                        res["success"] = false;
                        res["enabled"] = false;
                        res["target_ip"] = newIp;
                        res["target_port"] = newPort;
                        res["message"] = "Cannot reach " + newIp + ":" + to_string(newPort) + " - " + pingErr;
                        return crow::response(res);
                    }
                }

                lock_guard<mutex> lock(routingMutex);
                routingTarget.ip      = newIp;
                routingTarget.port    = newPort;
                routingTarget.enabled = enable;

                res["success"]     = true;
                res["enabled"]     = routingTarget.enabled;
                res["target_ip"]   = routingTarget.ip;
                res["target_port"] = routingTarget.port;
                res["message"] = routingTarget.enabled
                    ? "Routing enabled -> " + routingTarget.ip + ":" + to_string(routingTarget.port)
                    : "Routing disabled.";

            } else if (req.method == crow::HTTPMethod::Delete) {
                // Clear routing
                lock_guard<mutex> lock(routingMutex);
                routingTarget = RoutingTarget{};
                res["success"] = true;
                res["enabled"] = false;
                res["message"] = "Routing table cleared.";
            }

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

    curl_global_cleanup();
}

