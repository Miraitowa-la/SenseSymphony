set(src_dirs
	lcd
	audio
)
set(include_dirs
	lcd
	audio
)
set(requires
	driver
	esp_lcd
	espressif__i2c_bus
	waveshare__esp_lcd_jd9365_10_1
	espressif__esp_codec_dev
)

idf_component_register(
	SRC_DIRS
	${src_dirs}
	INCLUDE_DIRS
	${include_dirs}
	REQUIRES
	${requires}
)
if(src_dirs)
	component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)
endif()
