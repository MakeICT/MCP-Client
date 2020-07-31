idf_component_register(SRCS "GeneralUtils.cpp"
							"GPIO.cpp"
							"SPI.cpp"
							"FreeRTOS.cpp"
							"MFRC522.cpp"
					INCLUDE_DIRS .
					REQUIRES nvs_flash
					)

