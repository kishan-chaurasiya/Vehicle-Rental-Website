
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <vector>
#include <cctype>

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
#include <libpq-fe.h>
using socket_t = int;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

using namespace std;

// ==================================================
// DATABASE CONNECTION (RENDER / LINUX)
// ==================================================

#ifndef _WIN32
PGconn* databaseConnection = nullptr;

bool initializeDatabase()
{
    const char* url = getenv("DATABASE_URL");

    if (url == nullptr || string(url).empty())
    {
        cerr << "DATABASE_URL is missing!" << endl;
        return false;
    }

    databaseConnection = PQconnectdb(url);

    if (PQstatus(databaseConnection) != CONNECTION_OK)
    {
        cerr << "Database connection failed: "
             << PQerrorMessage(databaseConnection) << endl;

        PQfinish(databaseConnection);
        databaseConnection = nullptr;
        return false;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS bookings ("
        "booking_id TEXT PRIMARY KEY,"
        "status TEXT NOT NULL DEFAULT 'Confirmed',"
        "customer_name TEXT NOT NULL,"
        "phone TEXT NOT NULL,"
        "pickup_location TEXT NOT NULL,"
        "pickup_date TEXT NOT NULL,"
        "return_date TEXT NOT NULL,"
        "vehicle TEXT NOT NULL,"
        "vehicle_type TEXT NOT NULL,"
        "price_per_day DOUBLE PRECISION NOT NULL,"
        "rental_days INTEGER NOT NULL,"
        "total_rent DOUBLE PRECISION NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()"
        ")";

    PGresult* result = PQexec(databaseConnection, sql);

    bool success =
        PQresultStatus(result) == PGRES_COMMAND_OK;

    if (!success)
        cerr << PQerrorMessage(databaseConnection) << endl;

    PQclear(result);
    return success;
}

bool saveBooking(
    const string& id,
    const string& name,
    const string& phone,
    const string& location,
    const string& pickup,
    const string& returning,
    const string& vehicle,
    const string& type,
    double price,
    int days,
    double total)
{
    const char* sql =
        "INSERT INTO bookings "
        "(booking_id,status,customer_name,phone,pickup_location,"
        "pickup_date,return_date,vehicle,vehicle_type,price_per_day,"
        "rental_days,total_rent) "
        "VALUES ($1,'Confirmed',$2,$3,$4,$5,$6,$7,$8,$9,$10,$11)";

    string priceText = to_string(price);
    string daysText = to_string(days);
    string totalText = to_string(total);

    const char* values[] = {
        id.c_str(), name.c_str(), phone.c_str(), location.c_str(),
        pickup.c_str(), returning.c_str(), vehicle.c_str(), type.c_str(),
        priceText.c_str(), daysText.c_str(), totalText.c_str()
    };

    PGresult* result = PQexecParams(
        databaseConnection, sql, 11, nullptr,
        values, nullptr, nullptr, 0
    );

    bool success = PQresultStatus(result) == PGRES_COMMAND_OK;

    if (!success)
        cerr << "Save failed: "
             << PQerrorMessage(databaseConnection) << endl;

    PQclear(result);
    return success;
}
#endif

// ==================================================
// VEHICLE CLASS
// ==================================================

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
        cout << "Vehicle object destroyed: "
             << name << endl;
    }

    string getName() const { return name; }
    string getType() const { return type; }
    double getRent() const { return rent; }

    double calculateRent(int days) const
    {
        return rent * days;
    }
};

// ==================================================
// CUSTOMER CLASS
// ==================================================

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
        cout << "Customer object destroyed: "
             << name << endl;
    }

    string getName() const { return name; }
    string getPhone() const { return phone; }
};

// ==================================================
// BASIC HELPERS
// ==================================================

void closeSocket(socket_t s)
{
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

string urlDecode(const string& input)
{
    string result;

    for (size_t i = 0; i < input.size(); i++)
    {
        if (input[i] == '+' )
            result += ' ';
        else if (input[i] == '%' && i + 2 < input.size())
        {
            string hexText = input.substr(i + 1, 2);
            char ch = static_cast<char>(
                strtol(hexText.c_str(), nullptr, 16)
            );
            result += ch;
            i += 2;
        }
        else
            result += input[i];
    }

    return result;
}

string urlEncode(const string& input)
{
    ostringstream out;

    for (unsigned char c : input)
    {
        if (isalnum(c) || c == '-' || c == '_' ||
            c == '.' || c == '~')
            out << c;
        else
            out << '%' << uppercase << hex
                << setw(2) << setfill('0')
                << static_cast<int>(c)
                << nouppercase << dec;
    }

    return out.str();
}

string getValue(const string& body, const string& key)
{
    string search = key + "=";
    size_t start = body.find(search);

    if (start == string::npos)
        return "";

    start += search.length();
    size_t end = body.find('&', start);

    if (end == string::npos)
        end = body.length();

    return urlDecode(body.substr(start, end - start));
}

string htmlEscape(const string& input)
{
    string output;

    for (char c : input)
    {
        switch (c)
        {
        case '&': output += "&amp;"; break;
        case '<': output += "&lt;"; break;
        case '>': output += "&gt;"; break;
        case '"': output += "&quot;"; break;
        case '\'': output += "&#039;"; break;
        default: output += c;
        }
    }

    return output;
}

string generateBookingID()
{
    static unsigned long counter = 0;
    ++counter;

    ostringstream id;
    id << "DG-" << time(nullptr) << "-" << counter;
    return id.str();
}

// ==================================================
// HTTP REQUEST / RESPONSE
// ==================================================

string receiveRequest(socket_t client)
{
    string request;
    char buffer[4096];

    while (true)
    {
        int received = recv(client, buffer, sizeof(buffer), 0);

        if (received <= 0)
            break;

        request.append(buffer, received);

        size_t headerEnd = request.find("\r\n\r\n");

        if (headerEnd != string::npos)
        {
            size_t contentPos = request.find("Content-Length:");
            int contentLength = 0;

            if (contentPos != string::npos)
            {
                size_t valueStart = contentPos + 15;
                size_t valueEnd = request.find("\r\n", valueStart);

                if (valueEnd != string::npos)
                    contentLength = atoi(
                        request.substr(
                            valueStart, valueEnd - valueStart
                        ).c_str()
                    );
            }

            size_t bodyStart = headerEnd + 4;

            if (request.size() - bodyStart >=
                static_cast<size_t>(contentLength))
                break;
        }

        if (request.size() > 1000000)
            break;
    }

    return request;
}

void sendResponse(
    socket_t client,
    const string& body,
    const string& status = "200 OK",
    const string& contentType = "text/plain; charset=UTF-8")
{
    string response =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Type: " + contentType + "\r\n"
        "Content-Length: " + to_string(body.size()) + "\r\n"
        "Connection: close\r\n\r\n" + body;

    send(client, response.c_str(),
         static_cast<int>(response.size()), 0);
}

void sendFile(socket_t client, const string& fileName)
{
    ifstream file(fileName, ios::binary);

    if (!file)
    {
        sendResponse(client, "File not found", "404 Not Found");
        return;
    }

    string content(
        (istreambuf_iterator<char>(file)),
        istreambuf_iterator<char>()
    );

    string contentType = "application/octet-stream";

    if (fileName == "index.html" || fileName == "login.html")
        contentType = "text/html; charset=UTF-8";
    else if (fileName == "style.css")
        contentType = "text/css; charset=UTF-8";
    else if (fileName == "script.js")
        contentType = "application/javascript; charset=UTF-8";
    else if (fileName == "logo.png")
        contentType = "image/png";

    sendResponse(client, content, "200 OK", contentType);
}

// ==================================================
// ADMIN AUTHENTICATION
// ==================================================

string base64Decode(const string& input)
{
    const string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    string output;
    int value = 0, bits = -8;

    for (unsigned char c : input)
    {
        if (c == '=')
            break;

        size_t pos = chars.find(c);
        if (pos == string::npos)
            continue;

        value = (value << 6) + static_cast<int>(pos);
        bits += 6;

        if (bits >= 0)
        {
            output += static_cast<char>((value >> bits) & 0xFF);
            bits -= 8;
        }
    }

    return output;
}

bool isAdminAuthenticated(const string& request)
{
    const char* user = getenv("ADMIN_USER");
    const char* password = getenv("ADMIN_PASSWORD");

    if (!user || !password)
        return false;

    size_t pos = request.find("Authorization: Basic ");

    if (pos == string::npos)
        return false;

    pos += string("Authorization: Basic ").length();

    size_t end = request.find("\r\n", pos);
    if (end == string::npos)
        return false;

    string decoded = base64Decode(request.substr(pos, end - pos));

    return decoded == string(user) + ":" + string(password);
}

// ==================================================
// DATABASE BOOKING SEARCH / CANCELLATION
// ==================================================

#ifndef _WIN32
PGresult* findBooking(const string& id)
{
    const char* sql =
        "SELECT booking_id,status,customer_name,phone,pickup_location,"
        "pickup_date,return_date,vehicle,vehicle_type,price_per_day,"
        "rental_days,total_rent FROM bookings WHERE booking_id=$1";

    const char* values[] = { id.c_str() };

    return PQexecParams(
        databaseConnection, sql, 1, nullptr,
        values, nullptr, nullptr, 0
    );
}

bool cancelDatabaseBooking(const string& id)
{
    const char* sql =
        "UPDATE bookings SET status='Cancelled' "
        "WHERE booking_id=$1 AND status='Confirmed'";

    const char* values[] = { id.c_str() };

    PGresult* result = PQexecParams(
        databaseConnection, sql, 1, nullptr,
        values, nullptr, nullptr, 0
    );

    bool success =
        PQresultStatus(result) == PGRES_COMMAND_OK &&
        string(PQcmdTuples(result)) == "1";

    PQclear(result);
    return success;
}
#endif

// ==================================================
// FIND BOOKING ROUTE
// ==================================================

void handleFindBooking(socket_t client, const string& request)
{
    size_t pos = request.find("\r\n\r\n");
    string body = pos == string::npos ? "" : request.substr(pos + 4);

    string id = getValue(body, "bookingId");

#ifdef _WIN32
    sendResponse(
        client,
        "Booking search requires the deployed PostgreSQL database.",
        "501 Not Implemented"
    );
#else
    PGresult* result = findBooking(id);

    if (PQresultStatus(result) != PGRES_TUPLES_OK ||
        PQntuples(result) == 0)
    {
        PQclear(result);
        sendResponse(client, "Booking not found.", "404 Not Found");
        return;
    }

    const char* labels[] = {
        "id", "status", "name", "phone", "location",
        "pickupDate", "returnDate", "vehicle", "type",
        "price", "days", "total"
    };

    string output;

    for (int i = 0; i < 12; i++)
    {
        if (i > 0)
            output += "\n";

        output += string(labels[i]) + "=" +
                  urlEncode(PQgetvalue(result, 0, i));
    }

    PQclear(result);
    sendResponse(client, output);
#endif
}

// ==================================================
// CANCEL BOOKING ROUTE
// ==================================================

void handleCancelBooking(socket_t client, const string& request)
{
    size_t pos = request.find("\r\n\r\n");
    string body = pos == string::npos ? "" : request.substr(pos + 4);

    string id = getValue(body, "bookingId");

#ifdef _WIN32
    sendResponse(
        client,
        "Booking cancellation requires the deployed PostgreSQL database.",
        "501 Not Implemented"
    );
#else
    if (cancelDatabaseBooking(id))
        sendResponse(client, "Booking cancelled successfully.\nBooking ID: " + id);
    else
        sendResponse(
            client,
            "Unable to cancel booking. Booking may not exist or may already be cancelled.",
            "400 Bad Request"
        );
#endif
}

// ==================================================
// ADMIN BOOKINGS PAGE
// ==================================================

void sendAdminBookings(socket_t client, const string& request)
{
    if (!isAdminAuthenticated(request))
    {
        sendResponse(
            client,
            "<h2>Authentication required</h2>",
            "401 Unauthorized",
            "text/html; charset=UTF-8"
        );
        return;
    }

    string status = "all";
    size_t q = request.find("status=");

    if (q != string::npos)
    {
        size_t end = request.find(' ', q);
        string query = request.substr(
            q + 7,
            end == string::npos ? string::npos : end - q - 7
        );

        if (query.find('&') != string::npos)
            query = query.substr(0, query.find('&'));

        status = urlDecode(query);
    }

    if (status != "all" && status != "confirmed" &&
        status != "cancelled")
        status = "all";

    string html =
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>DriveGo Admin</title>"
        "<style>"
        "body{font-family:Arial;background:#f4f5f7;margin:0;color:#222}"
        "header{background:#111;color:white;padding:22px 6%;font-size:24px}"
        "header span{color:#e63946}"
        "main{padding:30px 5%}"
        ".filters a{display:inline-block;padding:10px 16px;margin:4px;"
        "background:white;border:1px solid #ddd;border-radius:5px;"
        "text-decoration:none;color:#222}"
        ".filters .active{background:#e63946;color:white}"
        ".table{overflow-x:auto;background:white;border-radius:8px}"
        "table{border-collapse:collapse;width:100%;min-width:1000px}"
        "th{background:#222;color:white;text-align:left}"
        "th,td{padding:12px;border-bottom:1px solid #eee}"
        ".cancelled{color:#e63946}.confirmed{color:green}"
        "</style></head><body>"
        "<header>Drive<span>Go</span> &nbsp; Admin Panel</header>"
        "<main><h1>Booking Management</h1>"
        "<p>View customer bookings stored in PostgreSQL.</p>"
        "<div class='filters'>"
        "<a href='/admin?status=all' class='" +
        string(status == "all" ? "active" : "") + "'>All bookings</a>"
        "<a href='/admin?status=confirmed' class='" +
        string(status == "confirmed" ? "active" : "") + "'>Confirmed</a>"
        "<a href='/admin?status=cancelled' class='" +
        string(status == "cancelled" ? "active" : "") + "'>Cancelled</a>"
        "</div><br><div class='table'><table><thead><tr>"
        "<th>Booking ID</th><th>Status</th><th>Customer</th><th>Phone</th>"
        "<th>Location</th><th>Pickup</th><th>Return</th><th>Vehicle</th>"
        "<th>Days</th><th>Total Rent</th>"
        "</tr></thead><tbody>";

#ifndef _WIN32
    const char* sqlAll =
        "SELECT booking_id,status,customer_name,phone,pickup_location,"
        "pickup_date,return_date,vehicle,rental_days,total_rent "
        "FROM bookings ORDER BY created_at DESC";

    const char* sqlFiltered =
        "SELECT booking_id,status,customer_name,phone,pickup_location,"
        "pickup_date,return_date,vehicle,rental_days,total_rent "
        "FROM bookings WHERE status=$1 ORDER BY created_at DESC";

    PGresult* result;

    if (status == "all")
        result = PQexec(databaseConnection, sqlAll);
    else
    {
        string dbStatus = status == "cancelled" ? "Cancelled" : "Confirmed";
        const char* values[] = { dbStatus.c_str() };

        result = PQexecParams(
            databaseConnection, sqlFiltered, 1, nullptr,
            values, nullptr, nullptr, 0
        );
    }

    int count = 0;

    if (PQresultStatus(result) == PGRES_TUPLES_OK)
    {
        count = PQntuples(result);

        for (int r = 0; r < count; r++)
        {
            html += "<tr>";

            for (int c = 0; c < 10; c++)
            {
                string value = PQgetvalue(result, r, c);
                string cls;

                if (c == 1)
                    cls = value == "Cancelled" ? "cancelled" : "confirmed";

                if (c == 9)
                    value = "Rs. " + value;

                html += "<td class='" + cls + "'>" +
                        htmlEscape(value) + "</td>";
            }

            html += "</tr>";
        }
    }

    PQclear(result);

    if (count == 0)
        html += "<tr><td colspan='10'>No bookings found.</td></tr>";
#else
    html += "<tr><td colspan='10'>The admin database page is available on Render.</td></tr>";
#endif

    html += "</tbody></table></div></main></body></html>";

    sendResponse(
        client,
        html,
        "200 OK",
        "text/html; charset=UTF-8"
    );
}

// ==================================================
// MAIN SERVER
// ==================================================

int main()
{
    cout << "=====================================\n";
    cout << "       VEHICLE RENTAL SYSTEM\n";
    cout << "=====================================\n";

    Vehicle car("Maruti Swift", "Hatchback", 1500);
    Vehicle suv("Hyundai Creta", "SUV", 2500);
    Vehicle bike("Royal Enfield", "Motorcycle", 800);

    const char* portEnv = getenv("PORT");
    int port = portEnv ? atoi(portEnv) : 8080;

#ifndef _WIN32
    if (!initializeDatabase())
    {
        cerr << "Database initialization failed. Server stopped."
             << endl;
        return 1;
    }

    cout << "PostgreSQL connected successfully!" << endl;
#endif

#ifdef _WIN32
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        cerr << "WSAStartup failed." << endl;
        return 1;
    }
#endif

    socket_t serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == INVALID_SOCKET)
    {
        cerr << "Socket creation failed." << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    int option = 1;

    setsockopt(
        serverSocket, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<char*>(&option), sizeof(option)
    );

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)) == SOCKET_ERROR)
    {
        cerr << "Bind failed." << endl;
        closeSocket(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (listen(serverSocket, 10) == SOCKET_ERROR)
    {
        cerr << "Listen failed." << endl;
        closeSocket(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    cout << "\nServer Started!\n";
    cout << "Port: " << port << "\n";
    cout << "Open: http://localhost:" << port << "\n";

    while (true)
    {
        socket_t client = accept(serverSocket, nullptr, nullptr);

        if (client == INVALID_SOCKET)
            continue;

        string request = receiveRequest(client);

        if (request.empty())
        {
            closeSocket(client);
            continue;
        }

        cout << "\n===== REQUEST RECEIVED =====\n";
        cout << request << endl;

        // ADMIN PANEL
        if (request.find("GET /admin") != string::npos)
        {
            sendAdminBookings(client, request);
        }

        // NEW BOOKING
        else if (request.find("POST /booking") != string::npos)
        {
            size_t pos = request.find("\r\n\r\n");
            string body = pos == string::npos ? "" : request.substr(pos + 4);

            string name = getValue(body, "name");
            string phone = getValue(body, "phone");
            string location = getValue(body, "location");
            string pickup = getValue(body, "pickupDate");
            string returning = getValue(body, "returnDate");
            string vehicleName = getValue(body, "vehicle");
            string vehicleType = getValue(body, "type");
            string daysText = getValue(body, "days");

            if (name.empty() || phone.empty() ||
                location.empty() || pickup.empty() ||
                returning.empty() || vehicleName.empty())
            {
                sendResponse(
                    client,
                    "Please fill in all required booking details.",
                    "400 Bad Request"
                );
                closeSocket(client);
                continue;
            }

            Customer customer(name, phone);

            Vehicle* selected = nullptr;

            if (vehicleName == car.getName())
                selected = &car;
            else if (vehicleName == suv.getName())
                selected = &suv;
            else if (vehicleName == bike.getName())
                selected = &bike;

            if (selected == nullptr)
            {
                sendResponse(
                    client,
                    "Invalid vehicle selected.",
                    "400 Bad Request"
                );
                closeSocket(client);
                continue;
            }

            int days = atoi(daysText.c_str());

            if (days < 1 || days > 365)
            {
                sendResponse(
                    client,
                    "Invalid rental days.",
                    "400 Bad Request"
                );
                closeSocket(client);
                continue;
            }

            double price = selected->getRent();
            double total = selected->calculateRent(days);
            string bookingID = generateBookingID();

#ifndef _WIN32
            bool saved = saveBooking(
                bookingID, name, phone, location, pickup,
                returning, selected->getName(), selected->getType(),
                price, days, total
            );

            if (!saved)
            {
                sendResponse(
                    client,
                    "Booking could not be saved. Please try again.",
                    "500 Internal Server Error"
                );
                closeSocket(client);
                continue;
            }
#else
            // Local Windows fallback: append booking details to a file.
            ofstream file("bookings.txt", ios::app);

            if (!file)
            {
                sendResponse(
                    client,
                    "Could not open bookings.txt.",
                    "500 Internal Server Error"
                );
                closeSocket(client);
                continue;
            }

            file << "========================================\n"
                 << "Booking ID: " << bookingID << "\n"
                 << "Status: Confirmed\n"
                 << "Customer Name: " << name << "\n"
                 << "Phone: " << phone << "\n"
                 << "Pickup Location: " << location << "\n"
                 << "Pickup Date: " << pickup << "\n"
                 << "Return Date: " << returning << "\n"
                 << "Vehicle: " << selected->getName() << "\n"
                 << "Vehicle Type: " << selected->getType() << "\n"
                 << "Price Per Day: " << price << "\n"
                 << "Rental Days: " << days << "\n"
                 << "Total Rent: " << total << "\n"
                 << "========================================\n\n";
#endif

            cout << "\n===== NEW BOOKING =====\n";
            cout << "Customer: " << name << "\n";
            cout << "Booking ID: " << bookingID << "\n";
            cout << "Vehicle: " << selected->getName() << "\n";
            cout << "Rental Days: " << days << "\n";
            cout << "Total Rent: Rs. " << total << "\n";

            sendResponse(
                client,
                "Booking received successfully by C++.\n"
                "Booking ID: " + bookingID
            );
        }

        // FIND BOOKING
        else if (request.find("POST /find-booking") != string::npos)
        {
            handleFindBooking(client, request);
        }

        // CANCEL BOOKING
        else if (request.find("POST /cancel-booking") != string::npos)
        {
            handleCancelBooking(client, request);
        }

        // WEBSITE PAGES
        else if (request.find("GET / ") != string::npos ||
                 request.find("GET /index.html") != string::npos)
        {
            sendFile(client, "index.html");
        }
        else if (request.find("GET /login.html") != string::npos)
        {
            sendFile(client, "login.html");
        }
        else if (request.find("GET /style.css") != string::npos)
        {
            sendFile(client, "style.css");
        }
        else if (request.find("GET /script.js") != string::npos)
        {
            sendFile(client, "script.js");
        }
        else if (request.find("GET /logo.png") != string::npos)
        {
            sendFile(client, "logo.png");
        }
        else if (request.find("GET /favicon.ico") != string::npos)
        {
            sendResponse(client, "", "204 No Content");
        }
        else
        {
            sendResponse(client, "404 - Page Not Found", "404 Not Found");
        }

        closeSocket(client);
    }

    closeSocket(serverSocket);

#ifdef _WIN32
    WSACleanup();
#else
    if (databaseConnection != nullptr)
    {
        PQfinish(databaseConnection);
        databaseConnection = nullptr;
    }
#endif

    return 0;
}