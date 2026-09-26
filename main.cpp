#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;


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

    void showVehicle()
    {
        cout << "Vehicle: " << name << endl;
        cout << "Type: " << type << endl;
        cout << "Rent: Rs. " << rent << " per day" << endl;
    }

    double calculateRent(int days)
    {
        return rent * days;
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
//              GET FORM VALUE
// ==================================================

string getValue(
    string body,
    string key
)
{
    string searchKey =
        key + "=";

    size_t start =
        body.find(searchKey);

    if (start == string::npos)
    {
        return "";
    }

    start += searchKey.length();

    size_t end =
        body.find("&", start);

    string value;

    if (end == string::npos)
    {
        value =
            body.substr(start);
    }
    else
    {
        value =
            body.substr(
                start,
                end - start
            );
    }

    return urlDecode(value);
}


// ==================================================
//              SEND TEXT RESPONSE
// ==================================================

void sendResponse(
    int clientSocket,
    string body,
    string status = "200 OK"
)
{
    string response =
        "HTTP/1.1 " +
        status +
        "\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " +
        to_string(body.length()) +
        "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        body;

    send(
        clientSocket,
        response.c_str(),
        response.length(),
        0
    );
}


// ==================================================
//                  SEND FILE
// ==================================================

void sendFile(
    int clientSocket,
    string fileName
)
{
    ifstream file(fileName);

    if (!file)
    {
        sendResponse(
            clientSocket,
            "File not found",
            "404 Not Found"
        );

        return;
    }

    string content;
    string line;

    while (getline(file, line))
    {
        content += line;
        content += "\n";
    }

    file.close();

    string contentType;

    if (fileName == "index.html")
    {
        contentType =
            "text/html";
    }
    else if (fileName == "style.css")
    {
        contentType =
            "text/css";
    }
    else if (fileName == "script.js")
    {
        contentType =
            "application/javascript";
    }
    else
    {
        contentType =
            "text/plain";
    }

    string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: " +
        contentType +
        "\r\n"
        "Content-Length: " +
        to_string(content.length()) +
        "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        content;

    send(
        clientSocket,
        response.c_str(),
        response.length(),
        0
    );
}


// ==================================================
//                  MAIN
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

    int port = 10000;

    char* environmentPort =
        getenv("PORT");

    if (environmentPort != nullptr)
    {
        port =
            atoi(environmentPort);
    }


    // ==================================================
    //                  CREATE SOCKET
    // ==================================================

    int serverSocket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (serverSocket < 0)
    {
        cerr << "Socket creation failed."
             << endl;

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
        &option,
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
            (sockaddr*)&serverAddress,
            sizeof(serverAddress)
        ) < 0
    )
    {
        cerr << "Bind failed."
             << endl;

        close(serverSocket);

        return 1;
    }


    // ==================================================
    //                  LISTEN
    // ==================================================

    if (
        listen(
            serverSocket,
            10
        ) < 0
    )
    {
        cerr << "Listen failed."
             << endl;

        close(serverSocket);

        return 1;
    }


    cout << "\nServer Started!"
         << endl;

    cout << "Port: "
         << port
         << endl;

    cout << "Server is ready."
         << endl;


    // ==================================================
    //                  SERVER LOOP
    // ==================================================

    while (true)
    {
        int clientSocket =
            accept(
                serverSocket,
                nullptr,
                nullptr
            );

        if (clientSocket < 0)
        {
            continue;
        }


        // ==================================================
        //              RECEIVE REQUEST
        // ==================================================

        char buffer[16384] = {0};

        int received =
            recv(
                clientSocket,
                buffer,
                sizeof(buffer) - 1,
                0
            );

        if (received <= 0)
        {
            close(clientSocket);
            continue;
        }

        buffer[received] = '\0';

        string request(buffer);


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

            string total =
                getValue(
                    body,
                    "total"
                );


            // ==================================================
            //              CUSTOMER OBJECT
            // ==================================================

            Customer customer(
                customerName,
                customerPhone
            );


            // ==================================================
            //              DISPLAY BOOKING
            // ==================================================

            cout << "\n================================"
                 << endl;

            cout << "          NEW BOOKING"
                 << endl;

            cout << "================================"
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
                 << days
                 << endl;

            cout << "Total Rent: Rs. "
                 << total
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
                    << days
                    << "\n";

                bookingFile
                    << "Total Rent: Rs. "
                    << total
                    << "\n";

                bookingFile
                    << "========================================\n\n";

                bookingFile.close();

                cout << "\nBooking saved successfully!"
                     << endl;
            }


            cout << "\n================================"
                 << endl;

            cout << "       BOOKING RECEIVED"
                 << endl;

            cout << "================================"
                 << endl;


            sendResponse(
                clientSocket,
                "Booking received successfully by C++."
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
                response.length(),
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


        close(clientSocket);
    }


    close(serverSocket);

    return 0;
}