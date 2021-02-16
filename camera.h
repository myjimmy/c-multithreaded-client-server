#ifndef CAMERA_HEADER
#define CAMERA_HEADER

/**
 * The identifiers of the camera events.
 */
typedef enum enum_camera_event {

    USER_NAME = 1,
    OPEN_DEVICE,
    CLOSE_DEVICE,
    VIDEO_DATA

} enum_camera_event;

/**
 * A packet structure of the camera event.
 */
typedef struct camera_event_packet_header {

    uint32_t event;
    uint32_t length;

} camera_event_packet_header;

typedef struct camera_event_packet_data {

    camera_event_packet_header header;
    uint8_t data[0];

} camera_event_packet_data;

#endif // CAMERA_HEADER
