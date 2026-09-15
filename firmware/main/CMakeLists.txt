idf_component_register(
    SRCS "main.c"
         "chords_ui.c"
         "chord_model.c"
         "capo_model.c"
         "tuner.c"
         "tuner_audio.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer
)
