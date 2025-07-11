#include "app_voice_ai.h"
#include "spdlog/spdlog.h"
#include <cstring>
#include <algorithm>
#include <cstdio>
#include "esp_websocket_client.h"
#include "esp_event.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define _canvas _data.hal->canvas()
#define _canvas_update _data.hal->canvas_update
#define _speaker _data.hal->Speaker()
#define _mic _data.hal->mic()

static constexpr const char* OPENAI_KEY_FILE = "key.txt";

using namespace MOONCAKE::APPS;

static void ws_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    AppVoiceAI* self = static_cast<AppVoiceAI*>(handler_args);
    if (!self)
    {
        return;
    }

    if (event_id == WEBSOCKET_EVENT_DATA)
    {
        auto* data = (esp_websocket_event_data_t*)event_data;
        if (data->op_code == 0x2 && data->data_len > 0)
        {
            size_t samples = data->data_len / sizeof(int16_t);
            const int16_t* ptr = reinterpret_cast<const int16_t*>(data->data_ptr);
            self->_data.recv_buffer.insert(self->_data.recv_buffer.end(), ptr, ptr + samples);
        }
    }
}

bool AppVoiceAI::_load_api_key(std::string& key)
{
    auto sd = _data.hal->sdcard();
    if (!sd->mount(false))
    {
        spdlog::error("Failed to mount SD card");
        return false;
    }

    char* path = sd->get_filepath(OPENAI_KEY_FILE);
    FILE* f = fopen(path, "r");
    free(path);
    if (!f)
    {
        spdlog::error("Failed to open %s", OPENAI_KEY_FILE);
        return false;
    }

    char buf[128] = {0};
    size_t len = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (len == 0)
    {
        spdlog::error("%s is empty", OPENAI_KEY_FILE);
        return false;
    }
    buf[len] = '\0';
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
    {
        buf[--len] = '\0';
    }
    key = buf;
    return true;
}

bool AppVoiceAI::_openai_connect(const char* api_key)
{
    if (_data.connected)
    {
        return true;
    }

    if (api_key == nullptr || strlen(api_key) == 0)
    {
        spdlog::error("OpenAI API key missing");
        return false;
    }

    esp_websocket_client_config_t config = {};
    config.uri = "wss://api.openai.com/v1/realtime";
    config.buffer_size = 4096;

    _data.headers = std::string("Authorization: Bearer ") + api_key + "\r\n";
    config.headers = _data.headers.c_str();

    _data.ws = esp_websocket_client_init(&config);
    if (!_data.ws)
    {
        spdlog::error("Failed to init websocket client");
        return false;
    }

    esp_websocket_register_events(_data.ws, WEBSOCKET_EVENT_DATA, ws_event_handler, this);

    esp_err_t err = esp_websocket_client_start(_data.ws);
    if (err != ESP_OK)
    {
        spdlog::error("Websocket start failed: {}", err);
        esp_websocket_client_destroy(_data.ws);
        _data.ws = nullptr;
        return false;
    }

    _data.connected = true;
    return true;
}

void AppVoiceAI::_openai_disconnect()
{
    if (_data.ws)
    {
        esp_websocket_client_close(_data.ws, portMAX_DELAY);
        esp_websocket_client_destroy(_data.ws);
        _data.ws = nullptr;
    }
    _data.connected = false;
    _data.recv_buffer.clear();
    _data.headers.clear();
}

bool AppVoiceAI::_openai_send_audio(const int16_t* data, size_t length)
{
    if (!_data.connected || !_data.ws)
    {
        return false;
    }
    int sent = esp_websocket_client_send_bin(_data.ws, (const char*)data, length * sizeof(int16_t), portMAX_DELAY);
    return sent > 0;
}

size_t AppVoiceAI::_openai_receive_audio(int16_t* buffer, size_t max_length)
{
    if (!_data.connected || !_data.ws || _data.recv_buffer.empty())
    {
        return 0;
    }

    size_t to_copy = std::min(max_length, _data.recv_buffer.size());
    memcpy(buffer, _data.recv_buffer.data(), to_copy * sizeof(int16_t));
    _data.recv_buffer.erase(_data.recv_buffer.begin(), _data.recv_buffer.begin() + to_copy);
    return to_copy;
}

void AppVoiceAI::onCreate()
{
    spdlog::info("{} onCreate", getAppName());
    _data.hal = mcAppGetDatabase()->Get("HAL")->value<HAL::Hal *>();
}

void AppVoiceAI::onResume()
{
    spdlog::info("{} onResume", getAppName());
    ANIM_APP_OPEN();

    _canvas->fillScreen(THEME_COLOR_BG);
    _canvas->setFont(FONT_REPL);
    _canvas->setTextColor(THEME_COLOR_REPL_TEXT, THEME_COLOR_BG);
    _canvas->setTextSize(FONT_SIZE_REPL);
    _canvas->setCursor(0,0);
    _canvas->println("Voice AI Chat");
    _canvas->println("Model: gpt-4o");
    _canvas_update();

    if (_load_api_key(_data.api_key))
    {
        _openai_connect(_data.api_key.c_str());
    }

    //_speaker->begin();
    //_mic->begin();
}

void AppVoiceAI::onRunning()
{
    // TODO: capture from microphone, send to OpenAI, and play response
    if (_data.hal->homeButton()->pressed())
    {
        _data.hal->playNextSound();
        destroyApp();
    }
}

void AppVoiceAI::onDestroy()
{
    _openai_disconnect();
    //_mic->end();
    //_speaker->end();
}

