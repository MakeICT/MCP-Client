idf_component_register(SRCS 
					"rb_tree.c"
					"reader.cpp"
					"switch.cpp"
					"light.cpp"
					"audio.c"
					"ws2812_control.c"
					INCLUDE_DIRS "."
					REQUIRES esp_http_client 
					json)
