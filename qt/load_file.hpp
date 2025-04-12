#ifndef LOAD_FILE_HPP
#define LOAD_FILE_HPP


#include <cstdint>
#include <qlogging.h>
#include <QDebug>
#include <QByteArray>
#ifdef __EMSCRIPTEN__ // if its webassembly
#include <emscripten.h>
#include "hosts.hpp"

// Embedded JavaScript function using EM_JS
EM_JS(void, open_file_and_process, (const char* room_id_c, const char* previous_page_id_c, const char* nw_host_c, const char* password_c), {
    // Create a hidden file input
    const nw_host = UTF8ToString(nw_host_c);
    const room_id = UTF8ToString(room_id_c);
    const previous_page_id = UTF8ToString(previous_page_id_c);
    const password = UTF8ToString(password_c);

    const input = document.createElement("input");
    input.type = "file";
    input.style.display = "none";

           // Function to handle file loading
    function handleFile(event) {
        const file = event.target.files[0];
        if (!file) {
            console.error("No file selected");
            return;
        }

        const reader = new FileReader();
        reader.onload = function() {
            const arrayBuffer = reader.result; // Read file as ArrayBuffer
            const byteArray = new Uint8Array(arrayBuffer); // Convert to Uint8Array

            // Log file data in JavaScript
            console.log("File read as byte array:", byteArray);

            var headers;
            if (password == "") {
                headers = {
                    "Content-Type": "application/octet-stream",
                };
            } else {
                headers = {
                    "Content-Type": "application/octet-stream",
                    "Authorization": `Bearer ${password}`
                };
            }
            fetch(`${nw_host}/v1/rooms/${room_id}/pdf_insert?previous_page_id=${previous_page_id}`, {
                method: "POST",
                headers: headers,
                body: byteArray // Send the byte array as the request body
            })
            .then(response => {
                if (response.ok) {
                    console.log("File uploaded successfully:", response);
                } else {
                    console.error("File upload failed:", response);
                }
            })
            .catch(error => {
                console.error("Error during POST request:", error);
            });

        };

        reader.onerror = function() {
            console.error("Error reading file");
        };

        reader.readAsArrayBuffer(file); // Read file as ArrayBuffer
    }

           // Set up the input element and trigger the dialog
    input.addEventListener("change", handleFile);
    document.body.appendChild(input);
    input.click();

           // Clean up
    input.remove();
});
// Define the C++ function to process the file data
extern "C" void process_file_data(const uint8_t* data, size_t length) {
    QByteArray fileData(reinterpret_cast<const char*>(data), static_cast<int>(length));
    qDebug() << "File loaded in C++ with size:" << length;
    // Process the file data in C++ as needed
}

#endif

#endif // LOAD_FILE_HPP
