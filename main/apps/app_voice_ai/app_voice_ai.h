#pragma once
#include <mooncake.h>
#include "../../hal/hal.h"
#include "../utils/theme/theme_define.h"
#include "../utils/anim/anim_define.h"
#include "../utils/icon/icon_define.h"

#include "assets/voice_ai_big.h"
#include "assets/voice_ai_small.h"
#include <vector>
#include <string>
#include "esp_websocket_client.h"

namespace MOONCAKE
{
    namespace APPS
    {
        class AppVoiceAI : public APP_BASE
        {
        private:
            struct Data_t
            {
                HAL::Hal* hal = nullptr;
                bool connected = false;
                esp_websocket_client_handle_t ws = nullptr;
                std::vector<int16_t> recv_buffer;
                std::string headers;
                std::string api_key;
            };
            Data_t _data;

            bool _openai_connect(const char* api_key);
            void _openai_disconnect();
            bool _openai_send_audio(const int16_t* data, size_t length);
            size_t _openai_receive_audio(int16_t* buffer, size_t max_length);
            bool _load_api_key(std::string& key);

        public:
            void onCreate() override;
            void onResume() override;
            void onRunning() override;
            void onDestroy() override;
        };

        class AppVoiceAI_Packer : public APP_PACKER_BASE
        {
            std::string getAppName() override { return "VOICE AI"; }
            void* getAppIcon() override { return (void*)(new AppIcon_t(image_data_voice_ai_big, image_data_voice_ai_small)); }
            void* newApp() override { return new AppVoiceAI; }
            void deleteApp(void* app) override { delete (AppVoiceAI*)app; }
        };
    }
}
