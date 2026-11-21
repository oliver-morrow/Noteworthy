#include "Config.hpp"
#include "ServerState.hpp"
#include "User.hpp"
#include "WebSocketHandler.hpp"
#include "base64.hpp"
#include "fpdfview.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <pistache/endpoint.h>
#include <pistache/http.h>
#include <pistache/http_headers.h>
#include <pistache/router.h>
#include <algorithm>
#include <cmath>
#include <string>
#define STB_IMAGE_WRITE_IMPLEMENTATION // Define the macro to enable implementation
#include "stb_image_write.h"
#include <fstream>
#include <mutex>

// PDFium is not thread-safe; every PDFium call goes through this lock.
static std::mutex pdfium_mutex;

using namespace Pistache;

class RoomHandler {
public:
    explicit RoomHandler() { }

    void setupRoutes(Rest::Router& router)
    {
        Rest::Routes::Post(router, "/v1/rooms", guarded(&RoomHandler::createRoom));
        Rest::Routes::Get(router, "/v1/rooms/:room_id", guarded(&RoomHandler::getRoom));
        Rest::Routes::Post(router, "/v1/rooms/:room_id/users", guarded(&RoomHandler::createUser));
        Rest::Routes::Post(router, "/v1/rooms/:room_id/pdf_insert", guarded(&RoomHandler::insertPDFPages));

        // Default handler for invalid routes
        router.addCustomHandler(
            Rest::Routes::bind(&RoomHandler::handleNotFound, this));
    }

private:
    using Handler = void (RoomHandler::*)(const Rest::Request&, Http::ResponseWriter&);

    // Wraps a handler so exceptions become error responses instead of
    // escaping into Pistache.
    Rest::Route::Handler guarded(Handler handler)
    {
        return [this, handler](const Rest::Request& request, Http::ResponseWriter response) {
            try {
                (this->*handler)(request, response);
            } catch (const std::range_error& e) { // room evicted mid-request
                response.send(Http::Code::Not_Found, "Room not found");
            } catch (const std::exception& e) {
                std::cerr << "HTTP handler error: " << e.what() << std::endl;
                response.send(Http::Code::Internal_Server_Error, "Internal error");
            }
            return Rest::Route::Result::Ok;
        };
    }

    void createRoom(const Rest::Request& request,
        Http::ResponseWriter& response)
    {
        auto username_query = request.query().get("username").value_or("");

        if (!isValidUsername(username_query)) {
            response.send(Http::Code::Bad_Request,
                "A username of 1-32 characters is required");
            return;
        }

        // create a room with that user as owner, with no password
        try {
            std::string room_id = state.createRoom(username_query, Config::get().max_rooms);
            response.send(Http::Code::Created, room_id);
        } catch (const ServerFullError& e) {
            response.send(Http::Code::Service_Unavailable,
                "The demo is at capacity. Try again later.");
        }
    }

    void handleNotFound(const Rest::Request& request,
        Http::ResponseWriter response)
    {
        response.send(Http::Code::Not_Found, "Invalid route");
    }

    // void handleOptionsRequest(const Rest::Request& request,
    //     Http::ResponseWriter response)
    // {
    //     response.headers()
    //         .add<Http::Header::AccessControlAllowOrigin>(
    //             "*") // or specify the origin: "http://localhost:30000"
    //         .add<Http::Header::AccessControlAllowMethods>("GET, OPTIONS")
    //         .add<Http::Header::AccessControlAllowHeaders>(
    //             "Authorization, Content-Type, Accept-Language");
    //     response.send(Http::Code::Ok, "epic");
    // }
    bool isAuthenticated(
        std::string& room_id,
        const Rest::Request& request,
        Http::ResponseWriter& response)
    {
        auto authHeader = request.headers().tryGet<Http::Header::Authorization>();

        const std::string bearerPrefix = "Bearer ";
        if (authHeader) {
            const std::string authHeaderPrefix = authHeader->value().substr(0, bearerPrefix.size());

            if (authHeaderPrefix != bearerPrefix) {
                response.send(Http::Code::Unauthorized,
                    "Unauthorized: Using incorrect auth type");
                return false;
            }
        }

        std::string receivedToken = "";
        if (authHeader) {
            receivedToken = authHeader->value().substr(7);
        }

        if (!state.hasRoom(room_id)) {
            response.send(Http::Code::Not_Found, "Room not found");
            return false;
        }

        std::string room_password;
        state.manipulateRoom(room_id, [&](RoomState& room) {
            room_password = room.password;
        });

        bool room_has_password = room_password != "";

        if (!room_has_password) {
            // the room exists, and the room has no password
            return true;
        }

        if (!authHeader && room_has_password) {
            response.send(Http::Code::Locked, "Room needs password");
            return false;
        }

        bool password_is_correct = room_password == receivedToken;

        if (!password_is_correct) {
            response.send(Http::Code::Forbidden, "Incorrect Password");
            return false;
        }

        // the room exists, has a password and the password was correct
        return true;
    }

    void getRoom(const Rest::Request& request, Http::ResponseWriter& response)
    {
        auto room_id = request.param(":room_id").as<std::string>();

        if (!isAuthenticated(room_id, request, response)) {
            return;
        }

        nlohmann::json event_list_json;
        state.manipulateRoom(room_id, [&](RoomState& room) {
            room.toJsonEventList(event_list_json);
        });
        response.setMime(MIME(Application, Json));
        response.send(Http::Code::Ok, event_list_json.dump());
    }

    void createUser(const Rest::Request& request,
        Http::ResponseWriter& response)
    {
        auto room_id = request.param(":room_id").as<std::string>();
        auto username_query = request.query().get("username").value_or("");

        if (!isAuthenticated(room_id, request, response)) {
            return;
        }

        if (!isValidUsername(username_query)) {
            response.send(Http::Code::Bad_Request,
                "A username of 1-32 characters is required");
            return;
        }

        std::string username = username_query;
        const size_t max_users = Config::get().max_users_per_room;

        // Manipulate the room state to add a new user if the username is unique
        state.manipulateRoom(room_id, [&room_id, &username, &response, max_users](RoomState& room) {
            if (room.isUserConnectedToRoom(username)) {
                response.send(Http::Code::Conflict,
                    "Username already connected in the room");
                return;
            }

            if (room.isUserKicked(username)) {
                response.send(Http::Code::Forbidden,
                    "you were kicked");
                return;
            }

            if (room.isUserInRoom(username)) {
                response.send(Http::Code::Ok, "Resumed an existing user");
                return;
            }

            if (room.userCount() >= max_users) {
                response.send(Http::Code::Forbidden, "Room is full");
                return;
            }

            auto user = std::make_unique<User>(room_id, username);
            nlohmann::json event;
            user->createCreateEvent(event);
            room.addUser(std::move(user));

            WebSocketHandler::queueBroadcast(room_id, event.dump());

            // Return success response
            response.send(Http::Code::Created, "User created successfully");
        });
    }

    std::string saveBitmapAsBase64(const uint8_t* buffer, int width, int height, int stride)
    {
        // Convert the bitmap to PNG format
        std::vector<uint8_t> pngData;
        auto writeCallback = [](void* context, void* data, int size) {
            auto* out = static_cast<std::vector<uint8_t>*>(context);
            out->insert(out->end(), static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
        };

        if (!stbi_write_png_to_func(writeCallback, &pngData, width, height, 4, buffer, stride)) {
            std::cerr << "Failed to write PNG.\n";
            return "";
        }

        // Now encode the PNG data to Base64 using std::string_view
        std::string_view dataView(reinterpret_cast<const char*>(pngData.data()), pngData.size());
        return base64::to_base64(dataView);
    }

    void insertPDFPages(const Rest::Request& request,
        Http::ResponseWriter& response)
    {
        const auto& config = Config::get();
        if (!config.enable_pdf) {
            response.send(Http::Code::Not_Found, "PDF import is disabled on this server");
            return;
        }

        auto room_id = request.param(":room_id").as<std::string>();
        auto previous_page_id_query = request.query().get("previous_page_id").value_or("");

        if (!isAuthenticated(room_id, request, response)) {
            return;
        }

        uint64_t previous_page_id;
        try {
            previous_page_id = std::stoull(previous_page_id_query);
        } catch (const std::exception& e) {
            response.send(Http::Code::Bad_Request, "Invalid previous page id");
            return;
        }

        // Check the insert position before spending CPU on rendering.
        bool position_ok = false;
        size_t page_count = 0;
        state.manipulateRoom(room_id, [&](RoomState& room) {
            position_ok = previous_page_id == 0 || room.hasPage(previous_page_id);
            page_count = room.pageCount();
        });
        if (!position_ok) {
            response.send(Http::Code::Bad_Request, "previous page id not found");
            return;
        }
        if (page_count >= config.max_pages_per_room) {
            response.send(Http::Code::Forbidden, "Page limit reached");
            return;
        }

        const std::string& body = request.body();
        size_t pages_allowed = std::min(config.max_pdf_pages, config.max_pages_per_room - page_count);
        std::vector<std::string> base64_list = renderPDFtoBase64List(body, pages_allowed);

        if (base64_list.empty()) {
            response.send(Http::Code::Bad_Request, "Empty or unreadable pdf file");
            return;
        }

        bool inserted = false;
        state.manipulateRoom(room_id, [&](RoomState& room) {
            if (previous_page_id != 0 && !room.hasPage(previous_page_id)) {
                return; // deleted while rendering
            }
            for (std::string& base64 : base64_list) {
                auto page = std::make_unique<Page>();
                page->room_id = room_id;
                uint64_t new_id = IDGenerator::newID();
                page->page_id = new_id;
                page->base64_image = std::move(base64);
                room.addPageAfter(previous_page_id, std::move(page));
                previous_page_id = new_id;
            }
            inserted = true;

            // make everyone reset their room
            nlohmann::json event;
            room.createResetEvent(event);
            WebSocketHandler::queueBroadcast(room_id, event.dump());
        });

        if (!inserted) {
            response.send(Http::Code::Bad_Request, "previous page id not found");
            return;
        }
        response.send(Http::Code::Created, "successfully created");
    }

    std::vector<std::string> renderPDFtoBase64List(const std::string& pdf_bytes, size_t max_pages)
    {
        const size_t max_pixels = Config::get().max_pdf_page_pixels;
        std::vector<std::string> base64_list;

        std::lock_guard<std::mutex> lock(pdfium_mutex);

        // Parse straight from memory: no temp files on disk.
        FPDF_DOCUMENT document = FPDF_LoadMemDocument64(pdf_bytes.data(), pdf_bytes.size(), nullptr);
        if (!document) {
            return {};
        }

        int page_count = FPDF_GetPageCount(document);
        for (int page_index = 0; page_index < page_count && base64_list.size() < max_pages; ++page_index) {
            FPDF_PAGE page = FPDF_LoadPage(document, page_index);
            if (!page) {
                continue;
            }

            double page_width = FPDF_GetPageWidth(page);
            double page_height = FPDF_GetPageHeight(page);
            if (!(page_width >= 1 && page_height >= 1)) {
                FPDF_ClosePage(page);
                continue;
            }

            // Scale oversized pages down to the pixel budget.
            double scale = 1.0;
            double area = page_width * page_height;
            if (area > static_cast<double>(max_pixels)) {
                scale = std::sqrt(static_cast<double>(max_pixels) / area);
            }
            int width = std::max(1, static_cast<int>(page_width * scale));
            int height = std::max(1, static_cast<int>(page_height * scale));

            FPDF_BITMAP bitmap = FPDFBitmap_Create(width, height, 0);
            if (!bitmap) {
                FPDF_ClosePage(page);
                continue;
            }
            FPDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFF);
            FPDF_RenderPageBitmap(bitmap, page, 0, 0, width, height, 0, 0);

            const uint8_t* buffer = static_cast<const uint8_t*>(FPDFBitmap_GetBuffer(bitmap));
            int stride = FPDFBitmap_GetStride(bitmap);

            // PDFium renders BGRA; PNG wants RGBA.
            std::vector<uint8_t> image_data(buffer, buffer + static_cast<size_t>(stride) * height);
            for (size_t i = 0; i + 3 < image_data.size(); i += 4) {
                std::swap(image_data[i], image_data[i + 2]);
            }

            std::string base64_image = saveBitmapAsBase64(image_data.data(), width, height, stride);
            if (!base64_image.empty()) {
                base64_list.push_back(std::move(base64_image));
            }

            FPDFBitmap_Destroy(bitmap);
            FPDF_ClosePage(page);
        }

        FPDF_CloseDocument(document);
        return base64_list;
    }
};

// Main function to start the server
void startServer(int port)
{

    Http::Endpoint server(Address(Ipv4::any(), Port(port)));
    Rest::Router router;

    RoomHandler handler;
    handler.setupRoutes(router);

    auto options = Http::Endpoint::options().threads(5).maxRequestSize(10000000);
    server.init(options);
    server.setHandler(router.handler());
    std::cout << "HTTP server running on port " << port << std::endl;
    server.serve();

    server.shutdown();
}