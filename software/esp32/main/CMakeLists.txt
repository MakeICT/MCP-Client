idf_component_register(SRCS "wifi_client_main.cpp"
					INCLUDE_DIRS "."
                    REQUIRES mcp_client esp_wifi nvs_flash )