// ================= CUSTOMER INFORMATION =================

let customerName =
    document.getElementById("customerName");

let customerPhone =
    document.getElementById("customerPhone");


// ================= BOOKING ELEMENTS =================

let bookingForm =
    document.querySelector(".booking-form");

let pickupLocation =
    document.getElementById("pickupLocation");

let dateInputs =
    bookingForm.querySelectorAll(
        'input[type="date"]'
    );

let pickupDate =
    dateInputs[0];

let returnDate =
    dateInputs[1];

let searchBtn =
    document.querySelector(".search-btn");

let vehicleResults =
    document.getElementById("vehicleResults");


// ================= VEHICLE DATA =================

let vehicles = [

    {
        name: "Maruti Swift",
        type: "Hatchback",
        price: 1500
    },

    {
        name: "Hyundai Creta",
        type: "SUV",
        price: 2500
    },

    {
        name: "Royal Enfield",
        type: "Motorcycle",
        price: 800
    }

];


// ================= SEARCH VEHICLE =================

searchBtn.addEventListener(
    "click",
    function () {

        let name =
            customerName.value;

        let phone =
            customerPhone.value;

        let location =
            pickupLocation.value;

        let startDate =
            pickupDate.value;

        let endDate =
            returnDate.value;


        // Check empty fields

        if (
            name === "" ||
            phone === "" ||
            location === "" ||
            startDate === "" ||
            endDate === ""
        ) {

            alert(
                "Please fill all the booking details."
            );

            return;
        }


        // Convert dates

        let pickup =
            new Date(startDate);

        let returnDay =
            new Date(endDate);


        // Check date

        if (returnDay < pickup) {

            alert(
                "Return date cannot be before pickup date."
            );

            return;
        }


        // Calculate rental days

        let difference =
            returnDay - pickup;

        let rentalDays =
            difference /
            (1000 * 60 * 60 * 24);


        if (rentalDays === 0) {

            rentalDays = 1;

        }


        // Show available vehicles

        vehicleResults.innerHTML = `

            <div class="vehicle-selection">

                <h2>Available Vehicles</h2>

                <p>
                    Customer:
                    ${name}
                </p>

                <p>
                    Phone:
                    ${phone}
                </p>

                <p>
                    Location:
                    ${location}
                </p>

                <p>
                    Rental Days:
                    ${rentalDays}
                </p>


                <div class="vehicle-options">

                    ${vehicles.map(
                        function (vehicle, index) {

                            return `

                                <div class="vehicle-option">

                                    <input
                                        type="radio"
                                        name="vehicle"
                                        id="vehicle${index}"
                                        value="${index}"
                                    >

                                    <label
                                        for="vehicle${index}"
                                    >

                                        <strong>
                                            ${vehicle.name}
                                        </strong>

                                        <span>
                                            ${vehicle.type}
                                        </span>

                                        <span>
                                            ₹${vehicle.price} / day
                                        </span>

                                    </label>

                                </div>

                            `;

                        }
                    ).join("")}

                </div>


                <button
                    type="button"
                    id="calculateBtn"
                >
                    Calculate Total
                </button>


                <div id="totalResult"></div>

            </div>

        `;

    }
);


// ================= CALCULATE TOTAL =================

document.addEventListener(
    "click",
    function (event) {

        if (
            event.target.id !==
            "calculateBtn"
        ) {

            return;

        }


        // Check vehicle selection

        let selectedVehicle =
            document.querySelector(
                'input[name="vehicle"]:checked'
            );


        if (!selectedVehicle) {

            alert(
                "Please select a vehicle."
            );

            return;

        }


        // Get selected vehicle

        let vehicle =
            vehicles[
                selectedVehicle.value
            ];


        // Calculate rental days

        let pickup =
            new Date(
                pickupDate.value
            );

        let returnDay =
            new Date(
                returnDate.value
            );


        let difference =
            returnDay - pickup;

        let rentalDays =
            difference /
            (1000 * 60 * 60 * 24);


        if (rentalDays === 0) {

            rentalDays = 1;

        }


        // Calculate total

        let total =
            vehicle.price *
            rentalDays;


        // ================= BOOKING DATA =================

        let bookingData =
            "name=" +
            encodeURIComponent(
                customerName.value
            ) +

            "&phone=" +
            encodeURIComponent(
                customerPhone.value
            ) +

            "&location=" +
            encodeURIComponent(
                pickupLocation.value
            ) +

            "&pickupDate=" +
            encodeURIComponent(
                pickupDate.value
            ) +

            "&returnDate=" +
            encodeURIComponent(
                returnDate.value
            ) +

            "&vehicle=" +
            encodeURIComponent(
                vehicle.name
            ) +

            "&type=" +
            encodeURIComponent(
                vehicle.type
            ) +

            "&price=" +
            encodeURIComponent(
                vehicle.price
            ) +

            "&days=" +
            encodeURIComponent(
                rentalDays
            ) +

            "&total=" +
            encodeURIComponent(
                total
            );


        // ================= SEND TO C++ =================

        fetch(
            "/booking",
            {
                method: "POST",

                headers: {
                    "Content-Type":
                        "application/x-www-form-urlencoded"
                },

                body: bookingData
            }
        )

        .then(
            function (response) {

                if (!response.ok) {

                    throw new Error(
                        "Booking request failed."
                    );

                }

                return response.text();

            }
        )

        .then(
            function (data) {

                console.log(
                    "C++ Response:",
                    data
                );


                // ================= SUCCESS MESSAGE =================

                let totalResult =
                    document.getElementById(
                        "totalResult"
                    );


                totalResult.innerHTML = `

                    <div class="booking-success">

                        <h3>
                            Booking Confirmed!
                        </h3>

                        <p>
                            Your booking has been
                            successfully received.
                        </p>

                        <p>
                            Customer:
                            <strong>
                                ${customerName.value}
                            </strong>
                        </p>

                        <p>
                            Phone:
                            ${customerPhone.value}
                        </p>

                        <p>
                            Location:
                            ${pickupLocation.value}
                        </p>

                        <p>
                            Vehicle:
                            <strong>
                                ${vehicle.name}
                            </strong>
                        </p>

                        <p>
                            Type:
                            ${vehicle.type}
                        </p>

                        <p>
                            Price:
                            ₹${vehicle.price} / day
                        </p>

                        <p>
                            Rental Days:
                            ${rentalDays}
                        </p>

                        <h2>
                            Total Rent:
                            ₹${total}
                        </h2>

                        <p>
                            <strong>
                                Booking saved successfully!
                            </strong>
                        </p>

                    </div>

                `;

            }
        )

        .catch(
            function (error) {

                console.log(
                    "Connection Error:",
                    error
                );


                let totalResult =
                    document.getElementById(
                        "totalResult"
                    );


                totalResult.innerHTML = `

                    <div class="booking-error">

                        <h3>
                            Booking Error
                        </h3>

                        <p>
                            Unable to confirm the booking.
                        </p>

                        <p>
                            Please try again.
                        </p>

                    </div>

                `;

            }
        );

    }
);