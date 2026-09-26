#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <ctime>

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

class Vehicle
{
private:
    string name;
    string type;
    double rent;

public:
    Vehicle(string n, string t, double r) : name(n), type(t), rent(r) {}

    ~Vehicle()
    {
        cout << "Vehicle object destroyed: " << name << endl;
    }

    double calculateRent(int days) { return rent * days; }
    string getName() { return name; }
    string getType() { return type; }
    double getRent() { return rent; }
};

class Customer
{
private:
    string name;
    string phone;

public:
    Customer(string n, string p) : name(n), phone(p) {}

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

string urlDecode(const string& value)
{
    string result;

    for (size_t i = 0; i < value.length(); i++)
    {
        if (value[i] == '%' && i + 2 < value.length())
        {
            string hex = value.substr(i + 1, 2);
            char ch = static_cast<char>(strtol(hex.c_str(), nullptr, 16));
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
       << setw(3) << setfill('0') << bookingNumber;

    return id.str();
}

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

string extractBookingField(const string& block, const string& label)
{
    size_t pos = block.find(label);

    if (pos == string::npos)
        return "";

    pos += label.length();

    size_t end = block.find('\n', pos);

    if (end == string::npos)
        end = block.length();

    string value = block.substr(pos, end - pos);

    while (!value.empty() && (value.back() == '\r' || value.back() == ' '))
        value.pop_back();

    return value;
}

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

    size_t blockStart = all.rfind("========================================", idPos);

    if (blockStart == string::npos)
        return false;

    size_t blockEnd = all.find(
        "========================================",
        idPos + marker.length()
    );

    if (blockEnd == string::npos)
        return false;

    size_t statusPos = all.find("Status: Confirmed", idPos);

    if (statusPos == string::npos || statusPos > blockEnd)
    {
        if (all.find("Status: Cancelled", idPos) != string::npos)
            return false;

        return false;
    }

    all.replace(statusPos, string("Status: Confirmed").length(),
                "Status: Cancelled");

    ofstream output("bookings.txt", ios::trunc);

    if (!output)
        return false;

    output << all;
    output.close();

    return true;
}

string receiveRequest(socket_t clientSocket)
{
    string request;
    char buffer[4096];

    while (true)
    {
        int received = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (received <= 0)
            break;

        request.append(buffer, received);

        size_t headerEnd = request.find("\r\n\r\n");

        if (headerEnd != string::npos)
        {
            size_t position = request.find("Content-Length:");
            int contentLength = 0;

            if (position != string::npos)
            {
                size_t valueStart = position + 15;
                size_t valueEnd = request.find("\r\n", valueStart);

                string lengthText =
                    request.substr(valueStart, valueEnd - valueStart);

                contentLength = atoi(lengthText.c_str());
            }

            size_t bodyStart = headerEnd + 4;

            if (request.size() - bodyStart >=
                static_cast<size_t>(contentLength))
            {
                break;
            }
        }

        if (request.length() > 1000000)
            break;
    }

    return request;
}

void sendResponse(
    socket_t clientSocket,
    const string& body,
    const string& status = "200 OK"
)
{
    string response =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Length: " + to_string(body.length()) + "\r\n"
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

void sendFile(socket_t clientSocket, const string& fileName)
{
    ifstream file(fileName, ios::binary);

    if (!file)
    {
        sendResponse(clientSocket, "File not found", "404 Not Found");
        return;
    }

    file.seekg(0, ios::end);
    streamsize fileSize = file.tellg();
    file.seekg(0, ios::beg);

    string content(static_cast<size_t>(fileSize), '\0');

    if (fileSize > 0)
        file.read(&content[0], fileSize);

    file.close();

    string contentType = "application/octet-stream";

    if (fileName == "index.html")
        contentType = "text/html";
    else if (fileName == "style.css")
        contentType = "text/css";
    else if (fileName == "script.js")
        contentType = "application/javascript";
    else if (fileName == "logo.png")
        contentType = "image/png";

    string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: " + contentType + "\r\n"
        "Content-Length: " + to_string(content.size()) + "\r\n"
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
        int sent = send(
            clientSocket,
            content.data() + totalSent,
            static_cast<int>(content.size() - totalSent),
            0
        );

        if (sent <= 0)
            break;

        totalSent += sent;
    }
}

int main()
{
    cout << "=====================================\n";
    cout << "       VEHICLE RENTAL SYSTEM\n";
    cout << "=====================================\n";

    Vehicle car("Maruti Swift", "Hatchback", 1500);
    Vehicle suv("Hyundai Creta", "SUV", 2500);
    Vehicle bike("Royal Enfield", "Motorcycle", 800);

    const char* environmentPort = getenv("PORT");
    int port = 8080;

    if (environmentPort != nullptr)
        port = atoi(environmentPort);

#ifdef _WIN32
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        cerr << "WSAStartup failed.\n";
        return 1;
    }
#endif

    socket_t serverSocket = socket(AF_INET, SOCK_STREAM, 0);

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

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(
        serverSocket,
        reinterpret_cast<sockaddr*>(&serverAddress),
        sizeof(serverAddress)
    ) == SOCKET_ERROR)
    {
        cerr << "Bind failed.\n";
        closeSocket(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (listen(serverSocket, 10) == SOCKET_ERROR)
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
    cout << "Open: http://localhost:" << port << "\n";

    while (true)
    {
        socket_t clientSocket = accept(serverSocket, nullptr, nullptr);

        if (clientSocket == INVALID_SOCKET)
            continue;

        string request = receiveRequest(clientSocket);

        if (request.empty())
        {
            closeSocket(clientSocket);
            continue;
        }

        cout << "\n===== REQUEST RECEIVED =====\n";
        cout << request << "\n";

        if (request.find("POST /booking") != string::npos)
        {
            size_t bodyPosition = request.find("\r\n\r\n");
            string body;

            if (bodyPosition != string::npos)
                body = request.substr(bodyPosition + 4);

            string customerName = getValue(body, "name");
            string customerPhone = getValue(body, "phone");
            string location = getValue(body, "location");
            string pickupDate = getValue(body, "pickupDate");
            string returnDate = getValue(body, "returnDate");
            string vehicleName = getValue(body, "vehicle");
            string vehicleType = getValue(body, "type");
            string price = getValue(body, "price");
            string days = getValue(body, "days");

            Customer customer(customerName, customerPhone);

            Vehicle* selectedVehicle = nullptr;

            if (vehicleName == car.getName())
                selectedVehicle = &car;
            else if (vehicleName == suv.getName())
                selectedVehicle = &suv;
            else if (vehicleName == bike.getName())
                selectedVehicle = &bike;

            int rentalDays = atoi(days.c_str());

            if (rentalDays < 1)
                rentalDays = 1;

            double serverTotal = 0;

            if (selectedVehicle != nullptr)
                serverTotal = selectedVehicle->calculateRent(rentalDays);

            string bookingID = generateBookingID();

            cout << "\n===== NEW BOOKING =====\n";
            customer.showCustomer();
            cout << "Booking ID: " << bookingID << "\n";
            cout << "Vehicle: " << vehicleName << "\n";
            cout << "Rental Days: " << rentalDays << "\n";
            cout << "C++ Total: Rs. " << serverTotal << "\n";

            ofstream bookingFile("bookings.txt", ios::app);

            if (bookingFile)
            {
                bookingFile << "========================================\n";
                bookingFile << "Booking ID: " << bookingID << "\n";
                bookingFile << "Status: Confirmed\n";
                bookingFile << "Customer Name: " << customerName << "\n";
                bookingFile << "Phone: " << customerPhone << "\n";
                bookingFile << "Pickup Location: " << location << "\n";
                bookingFile << "Pickup Date: " << pickupDate << "\n";
                bookingFile << "Return Date: " << returnDate << "\n";
                bookingFile << "Vehicle: " << vehicleName << "\n";
                bookingFile << "Vehicle Type: " << vehicleType << "\n";
                bookingFile << "Price Per Day: " << price << "\n";
                bookingFile << "Rental Days: " << rentalDays << "\n";
                bookingFile << "Total Rent: " << serverTotal << "\n";
                bookingFile << "========================================\n\n";
                bookingFile.close();

                cout << "Booking saved successfully!\n";
            }

            sendResponse(
                clientSocket,
                "Booking received successfully by C++.\n"
                "Booking ID: " + bookingID
            );
        }

        else if (request.find("POST /find-booking") != string::npos)
        {
            size_t bodyPosition = request.find("\r\n\r\n");
            string body;

            if (bodyPosition != string::npos)
                body = request.substr(bodyPosition + 4);

            string bookingID = getValue(body, "bookingId");
            string block = getBookingBlock(bookingID);

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
                    "id=" + urlEncode(extractBookingField(block, "Booking ID: ")) + "\n" +
                    "status=" + urlEncode(extractBookingField(block, "Status: ")) + "\n" +
                    "name=" + urlEncode(extractBookingField(block, "Customer Name: ")) + "\n" +
                    "phone=" + urlEncode(extractBookingField(block, "Phone: ")) + "\n" +
                    "location=" + urlEncode(extractBookingField(block, "Pickup Location: ")) + "\n" +
                    "pickupDate=" + urlEncode(extractBookingField(block, "Pickup Date: ")) + "\n" +
                    "returnDate=" + urlEncode(extractBookingField(block, "Return Date: ")) + "\n" +
                    "vehicle=" + urlEncode(extractBookingField(block, "Vehicle: ")) + "\n" +
                    "type=" + urlEncode(extractBookingField(block, "Vehicle Type: ")) + "\n" +
                    "price=" + urlEncode(extractBookingField(block, "Price Per Day: ")) + "\n" +
                    "days=" + urlEncode(extractBookingField(block, "Rental Days: ")) + "\n" +
                    "total=" + urlEncode(extractBookingField(block, "Total Rent: "));

                sendResponse(clientSocket, response);
            }
        }

        else if (request.find("POST /cancel-booking") != string::npos)
        {
            size_t bodyPosition = request.find("\r\n\r\n");
            string body;

            if (bodyPosition != string::npos)
                body = request.substr(bodyPosition + 4);

            string bookingID = getValue(body, "bookingId");

            if (cancelBooking(bookingID))
            {
                sendResponse(
                    clientSocket,
                    "Booking cancelled successfully.\n"
                    "Booking ID: " + bookingID
                );
            }
            else
            {
                sendResponse(
                    clientSocket,
                    "Unable to cancel booking. Booking may not exist or may already be cancelled.",
                    "400 Bad Request"
                );
            }
        }

        else if (request.find("GET / ") != string::npos)
        {
            sendFile(clientSocket, "index.html");
        }

        else if (request.find("GET /style.css") != string::npos)
        {
            sendFile(clientSocket, "style.css");
        }

        else if (request.find("GET /script.js") != string::npos)
        {
            sendFile(clientSocket, "script.js");
        }

        else if (request.find("GET /logo.png") != string::npos)
        {
            sendFile(clientSocket, "logo.png");
        }

        else if (request.find("GET /favicon.ico") != string::npos)
        {
            string response =
                "HTTP/1.1 204 No Content\r\n"
                "Connection: close\r\n"
                "\r\n";

            send(
                clientSocket,
                response.c_str(),
                static_cast<int>(response.length()),
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