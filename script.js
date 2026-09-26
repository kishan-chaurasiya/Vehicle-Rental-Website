// ================= VEHICLE DATA ================= 
 
const vehicles = [ 
    { name: "Maruti Swift", type: "Hatchback", price: 1500 }, 
    { name: "Hyundai Creta", type: "SUV", price: 2500 }, 
    { name: "Royal Enfield", type: "Motorcycle", price: 800 } 
]; 
 
const customerName = document.getElementById("customerName"); 
const customerPhone = document.getElementById("customerPhone"); 
const pickupLocation = document.getElementById("pickupLocation"); 
const pickupDate = document.getElementById("pickupDate"); 
const returnDate = document.getElementById("returnDate"); 
const searchBtn = document.querySelector(".search-btn"); 
const vehicleResults = document.getElementById("vehicleResults"); 
 
function calculateRentalDays() { 
    const pickup = new Date(pickupDate.value); 
    const returnDay = new Date(returnDate.value); 
 
    if (returnDay < pickup) return -1; 
 
    const difference = returnDay - pickup; 
    const days = difference / (1000 * 60 * 60 * 24); 
    return days === 0 ? 1 : days; 
} 
 
searchBtn.addEventListener("click", function () { 
    const name = customerName.value.trim(); 
    const phone = customerPhone.value.trim(); 
    const location = pickupLocation.value.trim(); 
 
    if (!name || !phone || !location || !pickupDate.value || !returnDate.value) { 
        alert("Please fill all the booking details."); 
        return; 
    } 
 
    const rentalDays = calculateRentalDays(); 
 
    if (rentalDays < 0) { 
        alert("Return date cannot be before pickup date."); 
        return; 
    } 
 
    vehicleResults.innerHTML = ` 
        <div class="vehicle-selection"> 
            <h2>Available Vehicles</h2> 
            <p>Customer: ${escapeHtml(name)}</p> 
            <p>Phone: ${escapeHtml(phone)}</p> 
            <p>Location: ${escapeHtml(location)}</p> 
            <p>Rental Days: ${rentalDays}</p> 
 
            <div class="vehicle-options"> 
                ${vehicles.map((vehicle, index) => ` 
                    <div class="vehicle-option"> 
                        <input type="radio" name="vehicle" id="vehicle${index}" value="${index}"> 
                        <label for="vehicle${index}"> 
                            <strong>${vehicle.name}</strong> 
                            <span>${vehicle.type}</span> 
                            <span>₹${vehicle.price} / day</span> 
                        </label> 
                    </div> 
                `).join("")} 
            </div> 
 
            <button type="button" id="calculateBtn">Calculate Total</button> 
            <div id="totalResult"></div> 
        </div> 
    `; 
}); 
 
document.addEventListener("click", function (event) { 
    if (event.target.id !== "calculateBtn") return; 
 
    const selectedVehicle = document.querySelector('input[name="vehicle"]:checked'); 
 
    if (!selectedVehicle) { 
        alert("Please select a vehicle."); 
        return; 
    } 
 
    const vehicle = vehicles[Number(selectedVehicle.value)]; 
    const rentalDays = calculateRentalDays(); 
 
    if (rentalDays < 1) { 
        alert("Please select valid dates."); 
        return; 
    } 
 
    const total = vehicle.price * rentalDays; 
 
    const bookingData = 
        "name=" + encodeURIComponent(customerName.value.trim()) + 
        "&phone=" + encodeURIComponent(customerPhone.value.trim()) + 
        "&location=" + encodeURIComponent(pickupLocation.value.trim()) + 
        "&pickupDate=" + encodeURIComponent(pickupDate.value) + 
        "&returnDate=" + encodeURIComponent(returnDate.value) + 
        "&vehicle=" + encodeURIComponent(vehicle.name) + 
        "&type=" + encodeURIComponent(vehicle.type) + 
        "&price=" + encodeURIComponent(vehicle.price) + 
        "&days=" + encodeURIComponent(rentalDays) + 
        "&total=" + encodeURIComponent(total); 
 
    fetch("/booking", { 
        method: "POST", 
        headers: { "Content-Type": "application/x-www-form-urlencoded" }, 
        body: bookingData 
    }) 
    .then(response => { 
        if (!response.ok) throw new Error("Booking request failed."); 
        return response.text(); 
    }) 
    .then(data => { 
        console.log("C++ Response:", data); 
 
        const bookingIdMatch = data.match(/Booking ID:\s*([A-Z0-9-]+)/); 
        const bookingId = bookingIdMatch ? bookingIdMatch[1] : "Not Available"; 
 
        document.getElementById("totalResult").innerHTML = ` 
            <div class="booking-success"> 
                <h3>Booking Confirmed!</h3> 
                <p>Your booking has been successfully received.</p> 
                <p>Booking ID: <strong>${escapeHtml(bookingId)}</strong></p> 
                <p>Customer: <strong>${escapeHtml(customerName.value)}</strong></p> 
                <p>Phone: ${escapeHtml(customerPhone.value)}</p> 
                <p>Location: ${escapeHtml(pickupLocation.value)}</p> 
                <p>Vehicle: <strong>${vehicle.name}</strong></p> 
                <p>Type: ${vehicle.type}</p> 
                <p>Price: ₹${vehicle.price} / day</p> 
                <p>Rental Days: ${rentalDays}</p> 
                <h2>Total Rent: ₹${total}</h2> 
                <p><strong>Booking saved successfully!</strong></p> 
            </div> 
        `; 
    }) 
    .catch(error => { 
        console.error("Connection Error:", error); 
        document.getElementById("totalResult").innerHTML = ` 
            <div class="booking-error"> 
                <h3>Booking Error</h3> 
                <p>Unable to confirm the booking.</p> 
                <p>Please try again.</p> 
            </div> 
        `; 
    }); 
}); 
 
// ================= MY BOOKING ================= 
 
const bookingIdInput = document.getElementById("bookingIdInput"); 
const findBookingBtn = document.getElementById("findBookingBtn"); 
const bookingLookupResult = document.getElementById("bookingLookupResult"); 
 
findBookingBtn.addEventListener("click", findBooking); 
 
bookingIdInput.addEventListener("keydown", function (event) { 
    if (event.key === "Enter") findBooking(); 
}); 
 
function findBooking() { 
    const bookingId = bookingIdInput.value.trim().toUpperCase(); 
 
    if (!bookingId) { 
        alert("Please enter your Booking ID."); 
        return; 
    } 
 
    bookingLookupResult.innerHTML = "<p>Searching booking...</p>"; 
 
    fetch("/find-booking", { 
        method: "POST", 
        headers: { "Content-Type": "application/x-www-form-urlencoded" }, 
        body: "bookingId=" + encodeURIComponent(bookingId) 
    }) 
    .then(response => response.text().then(text => ({ ok: response.ok, text }))) 
    .then(result => { 
        if (!result.ok) throw new Error(result.text); 
        const booking = parseBookingResponse(result.text); 
 
        if (!booking.id) throw new Error("Booking not found."); 
 
        const cancelled = booking.status.toLowerCase() === "cancelled"; 
 
        bookingLookupResult.innerHTML = ` 
            <div class="booking-details"> 
                <h3>Booking Details</h3> 
                <p>Booking ID: <strong>${escapeHtml(booking.id)}</strong></p> 
                <p>Status: 
                    <strong class="${cancelled ? "status-cancelled" : "status-confirmed"}"> 
                        ${escapeHtml(booking.status)} 
                    </strong> 
                </p> 
                <p>Customer: <strong>${escapeHtml(booking.name)}</strong></p> 
                <p>Phone: ${escapeHtml(booking.phone)}</p> 
                <p>Pickup Location: ${escapeHtml(booking.location)}</p> 
                <p>Pickup Date: ${escapeHtml(booking.pickupDate)}</p> 
                <p>Return Date: ${escapeHtml(booking.returnDate)}</p> 
                <p>Vehicle: <strong>${escapeHtml(booking.vehicle)}</strong></p> 
                <p>Vehicle Type: ${escapeHtml(booking.type)}</p> 
                <p>Price Per Day: ₹${escapeHtml(booking.price)}</p> 
                <p>Rental Days: ${escapeHtml(booking.days)}</p> 
                <h2>Total Rent: ₹${escapeHtml(booking.total)}</h2> 
                ${cancelled ? "" : `<button type="button" class="cancel-booking-btn" data-booking-id="${escapeHtml(booking.id)}">Cancel Booking</button>`} 
            </div> 
        `; 
    }) 
    .catch(error => { 
        bookingLookupResult.innerHTML = ` 
            <div class="lookup-error"> 
                <strong>Booking not found.</strong> 
                <p>${escapeHtml(error.message || "Please check your Booking ID and try again.")}</p> 
            </div> 
        `; 
    }); 
} 
 
document.addEventListener("click", function (event) { 
    if (!event.target.classList.contains("cancel-booking-btn")) return; 
 
    const bookingId = event.target.dataset.bookingId; 
 
    if (!confirm("Are you sure you want to cancel this booking?")) return; 
 
    fetch("/cancel-booking", { 
        method: "POST", 
        headers: { "Content-Type": "application/x-www-form-urlencoded" }, 
        body: "bookingId=" + encodeURIComponent(bookingId) 
    }) 
    .then(response => response.text().then(text => ({ ok: response.ok, text }))) 
    .then(result => { 
        if (!result.ok) throw new Error(result.text); 
        alert("Booking cancelled successfully."); 
        findBooking(); 
    }) 
    .catch(error => { 
        alert("Unable to cancel booking: " + error.message); 
    }); 
}); 
 
function parseBookingResponse(text) { 
    const booking = {}; 
 
    text.split("\n").forEach(line => { 
        const index = line.indexOf("="); 
        if (index === -1) return; 
 
        const key = line.substring(0, index).trim(); 
        const value = decodeURIComponent(line.substring(index + 1).trim()); 
 
        booking[key] = value; 
    }); 
 
    return { 
        id: booking.id || "", 
        status: booking.status || "", 
        name: booking.name || "", 
        phone: booking.phone || "", 
        location: booking.location || "", 
        pickupDate: booking.pickupDate || "", 
        returnDate: booking.returnDate || "", 
        vehicle: booking.vehicle || "", 
        type: booking.type || "", 
        price: booking.price || "", 
        days: booking.days || "", 
        total: booking.total || "" 
    }; 
} 
 
function escapeHtml(value) { 
    return String(value) 
        .replaceAll("&", "&amp;") 
        .replaceAll("<", "&lt;") 
        .replaceAll(">", "&gt;") 
        .replaceAll('"', "&quot;") 
        .replaceAll("'", "&#039;"); 
}