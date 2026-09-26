#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

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


// ==================================================
//                  SOCKET CLOSE
// ==================================================

void closeSocket(socket_t socketValue)
{
#ifdef _WIN32
    closesocket(socketValue);
#else
    close(socketValue);
#endif
}


// ==================================================
//                  VEHICLE CLASS
// ==================================================

class Vehicle
{
private:
    string name;
    string type;
    double rent;

public:

    Vehicle(string n, string t, double r)
    {
        name = n;
        type = t;
        rent = r;
    }

    ~Vehicle()
    {
        cout << "Vehicle object destroyed: "
             << name << endl;
    }

    void showVehicle()
    {
        cout << "Vehicle: " << name << endl;
        cout << "Type: " << type << endl;
        cout << "Rent: Rs. " << rent
             << " per day" << endl;
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


// ==================================================
//                  CUSTOMER CLASS
// ==================================================

class Customer
{
private:
    string name;
    string phone;

public:

    Customer(string n, string p)
    {
        name = n;
        phone = p;
    }

    ~Customer()
    {
        cout << "Customer object destroyed: "
             << name << endl;
    }

    void showCustomer()
    {
        cout << "Customer Name: "
             << name << endl;

        cout << "Phone: "
             << phone << endl;
    }
};


// ==================================================
//                  URL DECODE
// ==================================================

string urlDecode(string value)
{
    string result;

    for (size_t i = 0; i < value.length(); i++)
    {
        if (value[i] == '%')
        {
            if (i + 2 < value.length())
            {
                string hex =
                    value.substr(i + 1, 2);

                char ch =
                    static_cast<char>(
                        strtol(
                            hex.c_str(),
                            nullptr,
                            16
                        )
                    );

                result += ch;

                i += 2;
            }
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


// ==================================================
//                  GET FORM VALUE
// ==================================================

string getValue(string body, string key)
{
    string searchKey = key + "=";

    size_t start = body.find(searchKey);

    if (start == string::npos)
    {
        return "";
    }

    start += searchKey.length();

    size_t end = body.find("&", start);

    string value;

    if (end == string::npos)
    {
        value = body.substr(start);
    }
    else
    {
        value = body.substr(
            start,
            end - start
        );
    }

    return urlDecode(value);
}


// ==================================================
//              RECEIVE COMPLETE REQUEST
// ==================================================

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
        {
            break;
        }

        request.append(buffer, received);

        size_t headerEnd =
            request.find("\r\n\r\n");

        if (headerEnd != string::npos)
        {
            size_t contentLengthPosition =
                request.find("Content-Length:");

            int contentLength = 0;

            if (contentLengthPosition != string::npos)
            {
                size_t valueStart =
                    contentLengthPosition + 15;

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
                    atoi(
                        lengthText.c_str()
                    );
            }

            size_t bodyStart =
                headerEnd + 4;

            size_t bodySize =
                request.size() - bodyStart;

            if (
                bodySize >=
                static_cast<size_t>(contentLength)
            )
            {
                break;
            }
        }

        if (request.length() > 1000000)
        {
            break;
        }
    }

    return request;
}


// ==================================================
//              SEND TEXT RESPONSE
// ==================================================

void sendResponse(
    socket_t clientSocket,
    string body,
    string status = "200 OK"
)
{
    string response =
        "HTTP/1.1 " +
        status +
        "\r\n"
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
        static_cast<int>(
            response.length()
        ),
        0
    );
}


// ==================================================
//                  SEND FILE
// ==================================================

void sendFile(
    socket_t clientSocket,
    string fileName
)
{
    ifstream file(
        fileName,
        ios::binary
    );

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
    {
        file.read(
            &content[0],
            fileSize
        );
    }

    file.close();


    // ==================================================
    //              CONTENT TYPE
    // ==================================================

    string contentType;

    if (fileName == "index.html")
    {
        contentType = "text/html";
    }
    else if (fileName == "style.css")
    {
        contentType = "text/css";
    }
    else if (fileName == "script.js")
    {
        contentType = "application/javascript";
    }
    else if (fileName == "logo.png")
    {
        contentType = "image/png";
    }
    else
    {
        contentType =
            "application/octet-stream";
    }


    // ==================================================
    //                  HTTP HEADER
    // ==================================================

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
        static_cast<int>(
            response.length()
        ),
        0
    );


    // ==================================================
    //              SEND FILE DATA
    // ==================================================

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
        {
            break;
        }

        totalSent += sent;
    }
}


// ==================================================
//              GENERATE BOOKING ID
// ==================================================

string generateBookingID()
{
    time_t now = time(nullptr);

    tm localTime{};

#ifdef _WIN32
    localtime_s(
        &localTime,
        &now
    );
#else
    localtime_r(
        &now,
        &localTime
    );
#endif

    ostringstream datePart;

    datePart
        << put_time(
            &localTime,
            "%Y%m%d"
        );


    int bookingNumber = 1;

    ifstream bookingFile(
        "bookings.txt"
    );

    if (bookingFile)
    {
        string line;

        while (
            getline(
                bookingFile,
                line
            )
        )
        {
            if (
                line.find(
                    "Booking ID:"
                )
                != string::npos
            )
            {
                bookingNumber++;
            }
        }

        bookingFile.close();
    }


    ostringstream bookingID;

    bookingID
        << "DG-"
        << datePart.str()
        << "-"
        << setw(3)
        << setfill('0')
        << bookingNumber;


    return bookingID.str();
}


// ==================================================
//                      MAIN
// ==================================================

int main()
{
    cout << "====================================="
         << endl;

    cout << "       VEHICLE RENTAL SYSTEM"
         << endl;

    cout << "====================================="
         << endl;


    // ==================================================
    //              VEHICLE OBJECTS
    // ==================================================

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


    // ==================================================
    //                  PORT
    // ==================================================

    const char* environmentPort =
        getenv("PORT");

    int port = 8080;

    if (environmentPort != nullptr)
    {
        port = atoi(
            environmentPort
        );
    }


    // ==================================================
    //              WINDOWS SOCKET STARTUP
    // ==================================================

#ifdef _WIN32

    WSADATA wsa;

    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &wsa
        ) != 0
    )
    {
        cerr << "WSAStartup failed."
             << endl;

        return 1;
    }

#endif


    // ==================================================
    //              CREATE SOCKET
    // ==================================================

    socket_t serverSocket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (
        serverSocket ==
        INVALID_SOCKET
    )
    {
        cerr << "Socket creation failed."
             << endl;

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }


    // ==================================================
    //              REUSE ADDRESS
    // ==================================================

    int option = 1;

    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<char*>(
            &option
        ),
        sizeof(option)
    );


    // ==================================================
    //              SERVER ADDRESS
    // ==================================================

    sockaddr_in serverAddress{};

    serverAddress.sin_family =
        AF_INET;

    serverAddress.sin_port =
        htons(port);

    serverAddress.sin_addr.s_addr =
        INADDR_ANY;


    // ==================================================
    //                  BIND
    // ==================================================

    if (
        bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(
                &serverAddress
            ),
            sizeof(serverAddress)
        ) == SOCKET_ERROR
    )
    {
        cerr << "Bind failed."
             << endl;

        closeSocket(
            serverSocket
        );

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }


    // ==================================================
    //                  LISTEN
    // ==================================================

    if (
        listen(
            serverSocket,
            10
        ) == SOCKET_ERROR
    )
    {
        cerr << "Listen failed."
             << endl;

        closeSocket(
            serverSocket
        );

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }


    cout << "\nServer Started!"
         << endl;

    cout << "Port: "
         << port
         << endl;

    cout << "Server is ready."
         << endl;

    cout << "Open: http://localhost:"
         << port
         << endl;


    // ==================================================
    //                  SERVER LOOP
    // ==================================================

    while (true)
    {
        socket_t clientSocket =
            accept(
                serverSocket,
                nullptr,
                nullptr
            );

        if (
            clientSocket ==
            INVALID_SOCKET
        )
        {
            continue;
        }


        // ==================================================
        //              RECEIVE REQUEST
        // ==================================================

        string request =
            receiveRequest(
                clientSocket
            );

        if (request.empty())
        {
            closeSocket(
                clientSocket
            );

            continue;
        }


        cout << "\n===== REQUEST RECEIVED ====="
             << endl;

        cout << request
             << endl;


        // ==================================================
        //                  BOOKING
        // ==================================================

        if (
            request.find(
                "POST /booking"
            )
            != string::npos
        )
        {
            size_t bodyPosition =
                request.find(
                    "\r\n\r\n"
                );

            string body;

            if (
                bodyPosition !=
                string::npos
            )
            {
                body =
                    request.substr(
                        bodyPosition + 4
                    );
            }


            // ==================================================
            //              GET BOOKING DATA
            // ==================================================

            string customerName =
                getValue(
                    body,
                    "name"
                );

            string customerPhone =
                getValue(
                    body,
                    "phone"
                );

            string location =
                getValue(
                    body,
                    "location"
                );

            string pickupDate =
                getValue(
                    body,
                    "pickupDate"
                );

            string returnDate =
                getValue(
                    body,
                    "returnDate"
                );

            string vehicleName =
                getValue(
                    body,
                    "vehicle"
                );

            string vehicleType =
                getValue(
                    body,
                    "type"
                );

            string price =
                getValue(
                    body,
                    "price"
                );

            string days =
                getValue(
                    body,
                    "days"
                );

            string clientTotal =
                getValue(
                    body,
                    "total"
                );


            // ==================================================
            //              GENERATE BOOKING ID
            // ==================================================

            string bookingID =
                generateBookingID();


            // ==================================================
            //              CUSTOMER OBJECT
            // ==================================================

            Customer customer(
                customerName,
                customerPhone
            );


            // ==================================================
            //              SELECT VEHICLE
            // ==================================================

            Vehicle* selectedVehicle =
                nullptr;

            if (
                vehicleName ==
                car.getName()
            )
            {
                selectedVehicle =
                    &car;
            }
            else if (
                vehicleName ==
                suv.getName()
            )
            {
                selectedVehicle =
                    &suv;
            }
            else if (
                vehicleName ==
                bike.getName()
            )
            {
                selectedVehicle =
                    &bike;
            }


            // ==================================================
            //              CALCULATE RENT IN C++
            // ==================================================

            int rentalDays =
                atoi(
                    days.c_str()
                );

            if (rentalDays < 1)
            {
                rentalDays = 1;
            }

            double serverTotal = 0;

            if (
                selectedVehicle !=
                nullptr
            )
            {
                serverTotal =
                    selectedVehicle
                        ->calculateRent(
                            rentalDays
                        );
            }


            // ==================================================
            //              DISPLAY BOOKING
            // ==================================================

            cout << "\n================================"
                 << endl;

            cout << "          NEW BOOKING"
                 << endl;

            cout << "================================"
                 << endl;


            cout << "\nBooking ID: "
                 << bookingID
                 << endl;


            cout << "\n===== CUSTOMER DETAILS ====="
                 << endl;

            customer.showCustomer();


            cout << "\n===== BOOKING DETAILS ====="
                 << endl;

            cout << "Pickup Location: "
                 << location
                 << endl;

            cout << "Pickup Date: "
                 << pickupDate
                 << endl;

            cout << "Return Date: "
                 << returnDate
                 << endl;


            cout << "\n===== VEHICLE DETAILS ====="
                 << endl;

            cout << "Vehicle: "
                 << vehicleName
                 << endl;

            cout << "Type: "
                 << vehicleType
                 << endl;

            cout << "Price per Day: Rs. "
                 << price
                 << endl;


            cout << "\n===== RENT DETAILS ====="
                 << endl;

            cout << "Rental Days: "
                 << rentalDays
                 << endl;

            cout << "Client Total: Rs. "
                 << clientTotal
                 << endl;

            cout << "C++ Calculated Total: Rs. "
                 << serverTotal
                 << endl;


            // ==================================================
            //              SAVE BOOKING TO FILE
            // ==================================================

            ofstream bookingFile(
                "bookings.txt",
                ios::app
            );

            if (bookingFile)
            {
                bookingFile
                    << "========================================\n";

                bookingFile
                    << "             NEW BOOKING\n";

                bookingFile
                    << "========================================\n";

                bookingFile
                    << "Booking ID: "
                    << bookingID
                    << "\n";

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
                    << "Price Per Day: Rs. "
                    << price
                    << "\n";

                bookingFile
                    << "Rental Days: "
                    << rentalDays
                    << "\n";

                bookingFile
                    << "Total Rent: Rs. "
                    << serverTotal
                    << "\n";

                bookingFile
                    << "========================================\n\n";

                bookingFile.close();

                cout << "\nBooking saved successfully!"
                     << endl;
            }
            else
            {
                cout << "\nUnable to save booking file."
                     << endl;
            }


            cout << "\n================================"
                 << endl;

            cout << "       BOOKING RECEIVED"
                 << endl;

            cout << "================================"
                 << endl;

            cout << "Booking ID: "
                 << bookingID
                 << endl;


            // ==================================================
            //              SEND RESPONSE TO WEBSITE
            // ==================================================

            string responseMessage =
                "Booking received successfully by C++.\n"
                "Booking ID: " +
                bookingID;


            sendResponse(
                clientSocket,
                responseMessage
            );
        }


        // ==================================================
        //                  HTML
        // ==================================================

        else if (
            request.find(
                "GET / "
            )
            != string::npos
        )
        {
            sendFile(
                clientSocket,
                "index.html"
            );
        }


        // ==================================================
        //                  CSS
        // ==================================================

        else if (
            request.find(
                "GET /style.css"
            )
            != string::npos
        )
        {
            sendFile(
                clientSocket,
                "style.css"
            );
        }


        // ==================================================
        //              JAVASCRIPT
        // ==================================================

        else if (
            request.find(
                "GET /script.js"
            )
            != string::npos
        )
        {
            sendFile(
                clientSocket,
                "script.js"
            );
        }


        // ==================================================
        //                  LOGO PNG
        // ==================================================

        else if (
            request.find(
                "GET /logo.png"
            )
            != string::npos
        )
        {
            sendFile(
                clientSocket,
                "logo.png"
            );
        }


        // ==================================================
        //                  FAVICON
        // ==================================================

        else if (
            request.find(
                "GET /favicon.ico"
            )
            != string::npos
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


        // ==================================================
        //                  NOT FOUND
        // ==================================================

        else
        {
            sendResponse(
                clientSocket,
                "404 - Page Not Found",
                "404 Not Found"
            );
        }


        closeSocket(
            clientSocket
        );
    }


    // ==================================================
    //              CLOSE SERVER
    // ==================================================

    closeSocket(
        serverSocket
    );

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}