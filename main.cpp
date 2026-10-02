#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using socket_t = int;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

using namespace std;

void closeSocket(socket_t socketValue)
{
#ifdef _WIN32
    closesocket(socketValue);
#else
    close(socketValue);
#endif
}

// ================= VEHICLE CLASS =================

class Vehicle
{
private:
    string name;
    string type;
    double rent;

public:
    Vehicle(string n, string t, double r)
        : name(n), type(t), rent(r) {}

    ~Vehicle()
    {
        cout << "Vehicle object destroyed: " << name << endl;
    }

    double calculateRent(int days)
    {
        return rent * days;
    }

    string getName()
    {
        return name;
    }

    string getType()
    {
        return type;
    }

    double getRent()
    {
        return rent;
    }
};

// ================= CUSTOMER CLASS =================

class Customer
{
private:
    string name;
    string phone;

public:
    Customer(string n, string p)
        : name(n), phone(p) {}

    ~Customer()
    {
        cout << "Customer object destroyed: " << name << endl;
    }

    void showCustomer()
    {
        cout << "Customer Name: " << name << endl;
        cout << "Phone: " << phone << endl;
    }
};

// ================= URL DECODE =================

string urlDecode(const string& value)
{
    string result;

    for (size_t i = 0; i < value.length(); i++)
    {
        if (value[i] == '%' && i + 2 < value.length())
        {
            string hex = value.substr(i + 1, 2);

            char ch =
                static_cast<char>(
                    strtol(hex.c_str(), nullptr, 16)
                );

            result += ch;
            i += 2;
        }
        else if (value[i] == '+')
        {
            result += ' ';
        }
        else
        {
            result += value[i];
        }
    }

    return result;
}

// ================= URL ENCODE =================

string urlEncode(const string& value)
{
    ostringstream encoded;

    for (unsigned char c : value)
    {
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded << c;
        }
        else
        {
            encoded << '%'
                    << uppercase
                    << hex
                    << setw(2)
                    << setfill('0')
                    << static_cast<int>(c)
                    << nouppercase
                    << dec;
        }
    }

    return encoded.str();
}

// ================= GET FORM VALUE =================

string getValue(const string& body, const string& key)
{
    string searchKey = key + "=";

    size_t start = body.find(searchKey);

    if (start == string::npos)
        return "";

    start += searchKey.length();

    size_t end = body.find("&", start);

    if (end == string::npos)
        return urlDecode(body.substr(start));

    return urlDecode(body.substr(start, end - start));
}

// ================= HTML ESCAPE =================

string htmlEscape(const string& value)
{
    string result;

    for (char c : value)
    {
        switch (c)
        {
        case '&':
            result += "&amp;";
            break;

        case '<':
            result += "&lt;";
            break;

        case '>':
            result += "&gt;";
            break;

        case '"':
            result += "&quot;";
            break;

        case '\'':
            result += "&#039;";
            break;

        default:
            result += c;
        }
    }

    return result;
}

// ================= BOOKING ID =================

string generateBookingID()
{
    time_t now = time(nullptr);
    tm localTime{};

#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    ostringstream datePart;

    datePart << put_time(&localTime, "%Y%m%d");

    int bookingNumber = 1;

    ifstream file("bookings.txt");

    string line;

    while (getline(file, line))
    {
        if (line.find("Booking ID:") != string::npos)
            bookingNumber++;
    }

    ostringstream id;

    id << "DG-" << datePart.str() << "-"
       << setw(3)
       << setfill('0')
       << bookingNumber;

    return id.str();
}

// ================= FIND BOOKING BLOCK =================

string getBookingBlock(const string& bookingID)
{
    ifstream file("bookings.txt");

    if (!file)
        return "";

    string line;
    string block;

    bool found = false;

    while (getline(file, line))
    {
        if (line.find("Booking ID: " + bookingID) != string::npos)
        {
            found = true;
            block = line + "\n";
            continue;
        }

        if (found)
        {
            if (line.find("========================================") != string::npos)
                break;

            block += line + "\n";
        }
    }

    return found ? block : "";
}

// ================= EXTRACT BOOKING FIELD =================

string extractBookingField(
    const string& block,
    const string& label
)
{
    size_t pos = block.find(label);

    if (pos == string::npos)
        return "";

    pos += label.length();

    size_t end = block.find('\n', pos);

    if (end == string::npos)
        end = block.length();

    string value = block.substr(pos, end - pos);

    while (!value.empty() &&
           (value.back() == '\r' ||
            value.back() == ' '))
    {
        value.pop_back();
    }

    return value;
}

// ================= CANCEL BOOKING =================

bool cancelBooking(const string& bookingID)
{
    ifstream input("bookings.txt");

    if (!input)
        return false;

    string all;
    string line;

    while (getline(input, line))
        all += line + "\n";

    input.close();

    string marker = "Booking ID: " + bookingID;

    size_t idPos = all.find(marker);

    if (idPos == string::npos)
        return false;

    size_t blockStart =
        all.rfind("========================================", idPos);

    if (blockStart == string::npos)
        return false;

    size_t blockEnd =
        all.find(
            "========================================",
            idPos + marker.length()
        );

    if (blockEnd == string::npos)
        return false;

    size_t statusPos =
        all.find("Status: Confirmed", idPos);

    if (statusPos == string::npos ||
        statusPos > blockEnd)
    {
        return false;
    }

    all.replace(
        statusPos,
        string("Status: Confirmed").length(),
        "Status: Cancelled"
    );

    ofstream output("bookings.txt", ios::trunc);

    if (!output)
        return false;

    output << all;

    output.close();

    return true;
}

// ================= RECEIVE REQUEST =================

string receiveRequest(socket_t clientSocket)
{
    string request;

    char buffer[4096];

    while (true)
    {
        int received =
            recv(
                clientSocket,
                buffer,
                sizeof(buffer),
                0
            );

        if (received <= 0)
            break;

        request.append(buffer, received);

        size_t headerEnd =
            request.find("\r\n\r\n");

        if (headerEnd != string::npos)
        {
            size_t position =
                request.find("Content-Length:");

            int contentLength = 0;

            if (position != string::npos)
            {
                size_t valueStart =
                    position + 15;

                size_t valueEnd =
                    request.find(
                        "\r\n",
                        valueStart
                    );

                string lengthText =
                    request.substr(
                        valueStart,
                        valueEnd - valueStart
                    );

                contentLength =
                    atoi(lengthText.c_str());
            }

            size_t bodyStart =
                headerEnd + 4;

            if (
                request.size() - bodyStart >=
                static_cast<size_t>(contentLength)
            )
            {
                break;
            }
        }

        if (request.length() > 1000000)
            break;
    }

    return request;
}

// ================= SEND TEXT RESPONSE =================

void sendResponse(
    socket_t clientSocket,
    const string& body,
    const string& status = "200 OK"
)
{
    string response =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Length: " +
        to_string(body.length()) +
        "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        body;

    send(
        clientSocket,
        response.c_str(),
        static_cast<int>(response.length()),
        0
    );
}

// ================= SEND HTML RESPONSE =================

void sendHtmlResponse(
    socket_t clientSocket,
    const string& body,
    const string& status = "200 OK"
)
{
    string response =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: " +
        to_string(body.length()) +
        "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        body;

    send(
        clientSocket,
        response.c_str(),
        static_cast<int>(response.length()),
        0
    );
}

// ================= SEND 401 =================

void sendUnauthorized(socket_t clientSocket)
{
    string body =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<title>DriveGo Admin Login</title>"
        "<style>"
        "body{"
        "font-family:Arial;"
        "background:#f5f5f5;"
        "display:flex;"
        "justify-content:center;"
        "align-items:center;"
        "height:100vh;"
        "}"
        ".box{"
        "background:white;"
        "padding:35px;"
        "border-radius:10px;"
        "box-shadow:0 5px 20px rgba(0,0,0,.15);"
        "text-align:center;"
        "}"
        "h2{color:#e63946;}"
        "</style>"
        "</head>"
        "<body>"
        "<div class='box'>"
        "<h2>DriveGo Admin</h2>"
        "<p>Authentication required.</p>"
        "<p>Please login using your admin credentials.</p>"
        "</div>"
        "</body>"
        "</html>";

    string response =
        "HTTP/1.1 401 Unauthorized\r\n"
        "WWW-Authenticate: Basic realm=\"DriveGo Admin\"\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: " +
        to_string(body.length()) +
        "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        body;

    send(
        clientSocket,
        response.c_str(),
        static_cast<int>(response.length()),
        0
    );
}

// ================= BASE64 DECODE =================

string base64Decode(const string& encoded)
{
    const string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    string result;

    int value = 0;
    int bits = -8;

    for (unsigned char c : encoded)
    {
        if (c == '=')
            break;

        size_t pos = chars.find(c);

        if (pos == string::npos)
            continue;

        value =
            (value << 6) +
            static_cast<int>(pos);

        bits += 6;

        if (bits >= 0)
        {
            result +=
                static_cast<char>(
                    (value >> bits) & 0xFF
                );

            bits -= 8;
        }
    }

    return result;
}

// ================= ADMIN AUTHENTICATION =================

bool isAdminAuthenticated(const string& request)
{
    const char* adminUserEnv =
        getenv("ADMIN_USER");

    const char* adminPasswordEnv =
        getenv("ADMIN_PASSWORD");

    if (adminUserEnv == nullptr ||
        adminPasswordEnv == nullptr)
    {
        return false;
    }

    string expectedUser =
        adminUserEnv;

    string expectedPassword =
        adminPasswordEnv;

    size_t position =
        request.find("Authorization: Basic ");

    if (position == string::npos)
        return false;

    position +=
        string("Authorization: Basic ").length();

    size_t end =
        request.find("\r\n", position);

    if (end == string::npos)
        return false;

    string encoded =
        request.substr(
            position,
            end - position
        );

    string decoded =
        base64Decode(encoded);

    string expected =
        expectedUser + ":" + expectedPassword;

    return decoded == expected;
}

// ================= GET QUERY VALUE =================

string getQueryValue(
    const string& request,
    const string& key
)
{
    size_t question =
        request.find('?');

    if (question == string::npos)
        return "";

    size_t end =
        request.find(' ', question);

    if (end == string::npos)
        end = request.length();

    string query =
        request.substr(
            question + 1,
            end - question - 1
        );

    return getValue(query, key);
}

// ================= ADMIN BOOKINGS PAGE =================

void sendAdminBookings(
    socket_t clientSocket,
    const string& request
)
{
    if (!isAdminAuthenticated(request))
    {
        sendUnauthorized(clientSocket);
        return;
    }

    string status =
        getQueryValue(request, "status");

    if (status.empty())
        status = "all";

    if (status != "all" &&
        status != "confirmed" &&
        status != "cancelled")
    {
        status = "all";
    }

    ifstream file("bookings.txt");

    vector<string> blocks;

    if (file)
    {
        string line;
        string currentBlock;

        while (getline(file, line))
        {
            if (line.find(
                    "========================================"
                ) != string::npos)
            {
                if (!currentBlock.empty())
                {
                    blocks.push_back(currentBlock);
                    currentBlock.clear();
                }
            }
            else if (!line.empty())
            {
                currentBlock += line + "\n";
            }
        }

        if (!currentBlock.empty())
            blocks.push_back(currentBlock);
    }

    string html;

    html +=
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' "
        "content='width=device-width,initial-scale=1.0'>"
        "<title>DriveGo Admin - Bookings</title>"

        "<style>"

        "*{box-sizing:border-box;}"

        "body{"
        "margin:0;"
        "font-family:Arial,sans-serif;"
        "background:#f5f6f8;"
        "color:#222;"
        "}"

        ".header{"
        "background:#111;"
        "color:white;"
        "padding:22px 7%;"
        "display:flex;"
        "justify-content:space-between;"
        "align-items:center;"
        "}"

        ".logo{"
        "font-size:25px;"
        "font-weight:bold;"
        "}"

        ".logo span{"
        "color:#e63946;"
        "}"

        ".container{"
        "padding:40px 6%;"
        "}"

        "h1{"
        "margin-bottom:8px;"
        "}"

        ".subtitle{"
        "color:#666;"
        "margin-bottom:25px;"
        "}"

        ".filters{"
        "display:flex;"
        "gap:10px;"
        "margin-bottom:25px;"
        "flex-wrap:wrap;"
        "}"

        ".filter{"
        "padding:11px 20px;"
        "background:white;"
        "border:1px solid #ddd;"
        "border-radius:6px;"
        "text-decoration:none;"
        "color:#333;"
        "font-weight:bold;"
        "}"

        ".filter.active{"
        "background:#e63946;"
        "color:white;"
        "border-color:#e63946;"
        "}"

        ".table-container{"
        "background:white;"
        "border-radius:10px;"
        "box-shadow:0 5px 20px rgba(0,0,0,.08);"
        "overflow-x:auto;"
        "}"

        "table{"
        "width:100%;"
        "border-collapse:collapse;"
        "min-width:1100px;"
        "}"

        "th{"
        "background:#222;"
        "color:white;"
        "padding:14px;"
        "text-align:left;"
        "}"

        "td{"
        "padding:13px;"
        "border-bottom:1px solid #eee;"
        "}"

        "tr:hover{"
        "background:#fafafa;"
        "}"

        ".confirmed{"
        "color:green;"
        "font-weight:bold;"
        "}"

        ".cancelled{"
        "color:#e63946;"
        "font-weight:bold;"
        "}"

        ".empty{"
        "padding:35px;"
        "text-align:center;"
        "color:#777;"
        "}"

        "@media(max-width:700px){"
        ".container{padding:25px 4%;}"
        ".header{padding:18px 4%;}"
        "}"

        "</style>"
        "</head>"
        "<body>";

    html +=
        "<div class='header'>"
        "<div class='logo'>Drive<span>Go</span></div>"
        "<div>Admin Panel</div>"
        "</div>";

    html +=
        "<div class='container'>"
        "<h1>Booking Management</h1>"
        "<p class='subtitle'>"
        "View all DriveGo customer bookings."
        "</p>";

    html += "<div class='filters'>";

    html +=
        "<a class='filter " +
        string(status == "all" ? "active" : "") +
        "' href='/admin?status=all'>"
        "All Bookings</a>";

    html +=
        "<a class='filter " +
        string(status == "confirmed" ? "active" : "") +
        "' href='/admin?status=confirmed'>"
        "Confirmed</a>";

    html +=
        "<a class='filter " +
        string(status == "cancelled" ? "active" : "") +
        "' href='/admin?status=cancelled'>"
        "Cancelled</a>";

    html += "</div>";

    html +=
        "<div class='table-container'>"
        "<table>"
        "<thead>"
        "<tr>"
        "<th>Booking ID</th>"
        "<th>Status</th>"
        "<th>Customer</th>"
        "<th>Phone</th>"
        "<th>Location</th>"
        "<th>Pickup Date</th>"
        "<th>Return Date</th>"
        "<th>Vehicle</th>"
        "<th>Days</th>"
        "<th>Total Rent</th>"
        "</tr>"
        "</thead>"
        "<tbody>";

    int shownBookings = 0;

    for (const string& block : blocks)
    {
        string bookingID =
            extractBookingField(
                block,
                "Booking ID: "
            );

        string bookingStatus =
            extractBookingField(
                block,
                "Status: "
            );

        string customer =
            extractBookingField(
                block,
                "Customer Name: "
            );

        string phone =
            extractBookingField(
                block,
                "Phone: "
            );

        string location =
            extractBookingField(
                block,
                "Pickup Location: "
            );

        string pickupDate =
            extractBookingField(
                block,
                "Pickup Date: "
            );

        string returnDate =
            extractBookingField(
                block,
                "Return Date: "
            );

        string vehicle =
            extractBookingField(
                block,
                "Vehicle: "
            );

        string days =
            extractBookingField(
                block,
                "Rental Days: "
            );

        string total =
            extractBookingField(
                block,
                "Total Rent: "
            );

        string statusLower =
            bookingStatus;

        for (char& c : statusLower)
            c = static_cast<char>(
                tolower(
                    static_cast<unsigned char>(c)
                )
            );

        if (status != "all" &&
            statusLower != status)
        {
            continue;
        }

        shownBookings++;

        string statusClass =
            statusLower == "cancelled"
            ? "cancelled"
            : "confirmed";

        html += "<tr>";

        html +=
            "<td><strong>" +
            htmlEscape(bookingID) +
            "</strong></td>";

        html +=
            "<td class='" +
            statusClass +
            "'>" +
            htmlEscape(bookingStatus) +
            "</td>";

        html +=
            "<td>" +
            htmlEscape(customer) +
            "</td>";

        html +=
            "<td>" +
            htmlEscape(phone) +
            "</td>";

        html +=
            "<td>" +
            htmlEscape(location) +
            "</td>";

        html +=
            "<td>" +
            htmlEscape(pickupDate) +
            "</td>";

        html +=
            "<td>" +
            htmlEscape(returnDate) +
            "</td>";

        html +=
            "<td>" +
            htmlEscape(vehicle) +
            "</td>";

        html +=
            "<td>" +
            htmlEscape(days) +
            "</td>";

        html +=
            "<td><strong>â‚¹" +
            htmlEscape(total) +
            "</strong></td>";

        html += "</tr>";
    }

    if (shownBookings == 0)
    {
        html +=
            "<tr>"
            "<td colspan='10' class='empty'>"
            "No bookings found."
            "</td>"
            "</tr>";
    }

    html +=
        "</tbody>"
        "</table>"
        "</div>"
        "</div>"
        "</body>"
        "</html>";

    sendHtmlResponse(clientSocket, html);
}

// ================= SEND FILE =================

void sendFile(
    socket_t clientSocket,
    const string& fileName
)
{
    ifstream file(fileName, ios::binary);

    if (!file)
    {
        sendResponse(
            clientSocket,
            "File not found",
            "404 Not Found"
        );

        return;
    }

    file.seekg(0, ios::end);

    streamsize fileSize =
        file.tellg();

    file.seekg(0, ios::beg);

    string content(
        static_cast<size_t>(fileSize),
        '\0'
    );

    if (fileSize > 0)
        file.read(&content[0], fileSize);

    file.close();

    string contentType =
        "application/octet-stream";

    if (fileName == "index.html" || fileName == "login.html")
        contentType = "text/html";

    else if (fileName == "style.css")
        contentType = "text/css";

    else if (fileName == "script.js")
        contentType = "application/javascript";

    else if (fileName == "logo.png")
        contentType = "image/png";

    string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: " +
        contentType +
        "\r\n"
        "Content-Length: " +
        to_string(content.size()) +
        "\r\n"
        "Connection: close\r\n"
        "\r\n";

    send(
        clientSocket,
        response.c_str(),
        static_cast<int>(response.length()),
        0
    );

    size_t totalSent = 0;

    while (totalSent < content.size())
    {
        int sent =
            send(
                clientSocket,
                content.data() + totalSent,
                static_cast<int>(
                    content.size() - totalSent
                ),
                0
            );

        if (sent <= 0)
            break;

        totalSent += sent;
    }
}

// ================= MAIN =================

int main()
{
    cout << "=====================================\n";
    cout << "       VEHICLE RENTAL SYSTEM\n";
    cout << "=====================================\n";

    Vehicle car(
        "Maruti Swift",
        "Hatchback",
        1500
    );

    Vehicle suv(
        "Hyundai Creta",
        "SUV",
        2500
    );

    Vehicle bike(
        "Royal Enfield",
        "Motorcycle",
        800
    );

    const char* environmentPort =
        getenv("PORT");

    int port = 8080;

    if (environmentPort != nullptr)
        port = atoi(environmentPort);

#ifdef _WIN32

    WSADATA wsa;

    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &wsa
        ) != 0
    )
    {
        cerr << "WSAStartup failed.\n";
        return 1;
    }

#endif

    socket_t serverSocket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (serverSocket == INVALID_SOCKET)
    {
        cerr << "Socket creation failed.\n";

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

    int option = 1;

    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<char*>(&option),
        sizeof(option)
    );

    sockaddr_in serverAddress{};

    serverAddress.sin_family =
        AF_INET;

    serverAddress.sin_port =
        htons(port);

    serverAddress.sin_addr.s_addr =
        INADDR_ANY;

    if (
        bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)
        ) == SOCKET_ERROR
    )
    {
        cerr << "Bind failed.\n";

        closeSocket(serverSocket);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

    if (
        listen(
            serverSocket,
            10
        ) == SOCKET_ERROR
    )
    {
        cerr << "Listen failed.\n";

        closeSocket(serverSocket);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

    cout << "\nServer Started!\n";
    cout << "Port: " << port << "\n";
    cout << "Open: http://localhost:"
         << port << "\n";

    while (true)
    {
        socket_t clientSocket =
            accept(
                serverSocket,
                nullptr,
                nullptr
            );

        if (clientSocket == INVALID_SOCKET)
            continue;

        string request =
            receiveRequest(clientSocket);

        if (request.empty())
        {
            closeSocket(clientSocket);
            continue;
        }

        cout << "\n===== REQUEST RECEIVED =====\n";
        cout << request << "\n";

        // ================= ADMIN =================

        if (
            request.find("GET /admin") !=
            string::npos
        )
        {
            sendAdminBookings(
                clientSocket,
                request
            );
        }

        // ================= BOOKING =================

        else if (
            request.find("POST /booking") !=
            string::npos
        )
        {
            size_t bodyPosition =
                request.find("\r\n\r\n");

            string body;

            if (bodyPosition != string::npos)
                body =
                    request.substr(
                        bodyPosition + 4
                    );

            string customerName =
                getValue(body, "name");

            string customerPhone =
                getValue(body, "phone");

            string location =
                getValue(body, "location");

            string pickupDate =
                getValue(body, "pickupDate");

            string returnDate =
                getValue(body, "returnDate");

            string vehicleName =
                getValue(body, "vehicle");

            string vehicleType =
                getValue(body, "type");

            string price =
                getValue(body, "price");

            string days =
                getValue(body, "days");

            Customer customer(
                customerName,
                customerPhone
            );

            Vehicle* selectedVehicle =
                nullptr;

            if (
                vehicleName ==
                car.getName()
            )
            {
                selectedVehicle = &car;
            }

            else if (
                vehicleName ==
                suv.getName()
            )
            {
                selectedVehicle = &suv;
            }

            else if (
                vehicleName ==
                bike.getName()
            )
            {
                selectedVehicle = &bike;
            }

            int rentalDays =
                atoi(days.c_str());

            if (rentalDays < 1)
                rentalDays = 1;

            double serverTotal = 0;

            if (selectedVehicle != nullptr)
            {
                serverTotal =
                    selectedVehicle->calculateRent(
                        rentalDays
                    );
            }

            string bookingID =
                generateBookingID();

            cout << "\n===== NEW BOOKING =====\n";

            customer.showCustomer();

            cout << "Booking ID: "
                 << bookingID << "\n";

            cout << "Vehicle: "
                 << vehicleName << "\n";

            cout << "Rental Days: "
                 << rentalDays << "\n";

            cout << "C++ Total: Rs. "
                 << serverTotal << "\n";

            ofstream bookingFile(
                "bookings.txt",
                ios::app
            );

            if (bookingFile)
            {
                bookingFile
                    << "========================================\n";

                bookingFile
                    << "Booking ID: "
                    << bookingID
                    << "\n";

                bookingFile
                    << "Status: Confirmed\n";

                bookingFile
                    << "Customer Name: "
                    << customerName
                    << "\n";

                bookingFile
                    << "Phone: "
                    << customerPhone
                    << "\n";

                bookingFile
                    << "Pickup Location: "
                    << location
                    << "\n";

                bookingFile
                    << "Pickup Date: "
                    << pickupDate
                    << "\n";

                bookingFile
                    << "Return Date: "
                    << returnDate
                    << "\n";

                bookingFile
                    << "Vehicle: "
                    << vehicleName
                    << "\n";

                bookingFile
                    << "Vehicle Type: "
                    << vehicleType
                    << "\n";

                bookingFile
                    << "Price Per Day: "
                    << price
                    << "\n";

                bookingFile
                    << "Rental Days: "
                    << rentalDays
                    << "\n";

                bookingFile
                    << "Total Rent: "
                    << serverTotal
                    << "\n";

                bookingFile
                    << "========================================\n\n";

                bookingFile.close();

                cout
                    << "Booking saved successfully!\n";
            }

            sendResponse(
                clientSocket,
                "Booking received successfully by C++.\n"
                "Booking ID: " +
                bookingID
            );
        }

        // ================= FIND BOOKING =================

        else if (
            request.find("POST /find-booking") !=
            string::npos
        )
        {
            size_t bodyPosition =
                request.find("\r\n\r\n");

            string body;

            if (bodyPosition != string::npos)
                body =
                    request.substr(
                        bodyPosition + 4
                    );

            string bookingID =
                getValue(
                    body,
                    "bookingId"
                );

            string block =
                getBookingBlock(
                    bookingID
                );

            if (block.empty())
            {
                sendResponse(
                    clientSocket,
                    "Booking not found.",
                    "404 Not Found"
                );
            }
            else
            {
                string response =
                    "id=" +
                    urlEncode(
                        extractBookingField(
                            block,
                            "Booking ID: "
                        )
                    ) +
                    "\n" +

                    "status=" +
                    urlEncode(
                        extractBookingField(
                            block,
                            "Status: "
                        )
                    ) +
                    "\n" +

                    "name=" +
                    urlEncode(
                        extractBookingField(
                            block,
                            "Customer Name: "
                        )
                    ) +
                    "\n" +

                    "phone=" +
                    urlEncode(
                        extractBookingField(
                            block,
                            "Phone: "
                        )
                    ) +
                    "\n" +

                    "location=" +
                    urlEncode(
                        extractBookingField(
                            block,
                            "Pickup Location: "
                        )
                    ) +
                    "\n" +

                    "pickupDate=" +
                    urlEncode(
                        extractBookingField(
                            block,
                            "Pickup Date: "
                        )
                    ) +
                    "\n" +

                    "returnDate=" +
                    urlEncode(
                        extractBookingField(
                            block,
                            "Return Date: "
                        )
                    ) +
                    "\n" +

                    "vehicle=" +
                    urlEncode(
                        extractBookingField(
                            block,
                            "Vehicle: "
                        )
                    ) +
                    "\n" +

                    "type=" +
                    urlEncode(
                        extractBookingField(
                            block,
                            "Vehicle Type: "
                        )
                    ) +
                    "\n" +

                    "price=" +
                    urlEncode(
                        extractBookingField(
                            block,
                            "Price Per Day: "
                        )
                    ) +
                    "\n" +

                    "days=" +
                    urlEncode(
                        extractBookingField(
                            block,
                            "Rental Days: "
                        )
                    ) +
                    "\n" +

                    "total=" +
                    urlEncode(
                        extractBookingField(
                            block,
                            "Total Rent: "
                        )
                    );

                sendResponse(
                    clientSocket,
                    response
                );
            }
        }

        // ================= CANCEL BOOKING =================

        else if (
            request.find("POST /cancel-booking") !=
            string::npos
        )
        {
            size_t bodyPosition =
                request.find("\r\n\r\n");

            string body;

            if (bodyPosition != string::npos)
                body =
                    request.substr(
                        bodyPosition + 4
                    );

            string bookingID =
                getValue(
                    body,
                    "bookingId"
                );

            if (
                cancelBooking(
                    bookingID
                )
            )
            {
                sendResponse(
                    clientSocket,
                    "Booking cancelled successfully.\n"
                    "Booking ID: " +
                    bookingID
                );
            }
            else
            {
                sendResponse(
                    clientSocket,
                    "Unable to cancel booking. "
                    "Booking may not exist or "
                    "may already be cancelled.",
                    "400 Bad Request"
                );
            }
        }

        // ================= WEBSITE FILES =================

        else if (
    request.find("GET / ") != string::npos ||
    request.find("GET /index.html") != string::npos
)
{
    sendFile(
        clientSocket,
        "index.html"
    );
}

        else if (
            request.find("GET /login.html") !=
            string::npos
        )
        {
            sendFile(
                clientSocket,
                "login.html"
            );
        }

        else if (
            request.find("GET /style.css") !=
            string::npos
        )
        {
            sendFile(
                clientSocket,
                "style.css"
            );
        }

        else if (
            request.find("GET /script.js") !=
            string::npos
        )
        {
            sendFile(
                clientSocket,
                "script.js"
            );
        }

        else if (
            request.find("GET /logo.png") !=
            string::npos
        )
        {
            sendFile(
                clientSocket,
                "logo.png"
            );
        }

        else if (
            request.find("GET /favicon.ico") !=
            string::npos
        )
        {
            string response =
                "HTTP/1.1 204 No Content\r\n"
                "Connection: close\r\n"
                "\r\n";

            send(
                clientSocket,
                response.c_str(),
                static_cast<int>(
                    response.length()
                ),
                0
            );
        }

        else
        {
            sendResponse(
                clientSocket,
                "404 - Page Not Found",
                "404 Not Found"
            );
        }

        closeSocket(clientSocket);
    }

    closeSocket(serverSocket);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}