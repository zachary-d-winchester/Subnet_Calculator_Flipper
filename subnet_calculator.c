#include <furi.h> // Flipper Universal Registry Implementation. Helps control the applications flow.
#include <stdlib.h> // Standard C library
#include <gui/gui.h> // Library for GUI interaction
#include <input/input.h> // Library to provide basic inputs.

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

typedef struct {
    int x;
    int y;
    FuriMutex* mutex;
} PluginState;

// Drawing the canvas
static void render_callback(Canvas* const canvas, void* ctx) {
    PluginState* plugin_state = (PluginState*)ctx;
    furi_mutex_acquire(plugin_state->mutex, FuriWaitForever);
    if(plugin_state == NULL) {
        return;
    }
    // Border around the edge of the screen
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, plugin_state->x, plugin_state->y, AlignRight, AlignBottom, "Hello World!");

    furi_mutex_release(plugin_state->mutex);
}

// Signals the plugin once a button is pressed. Event is queued into the event_queue.
static void input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

// Initializes the values of x and y
static void subnet_calculator_state_init(PluginState* const plugin_state) {
    plugin_state->x = 50;
    plugin_state->y = 30;
}

// Entry point of the plugin
int32_t subnet_calculator_app() {
    // Message queue to keep track of events
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(PluginEvent));

    PluginState* plugin_state = malloc(sizeof(PluginState)); // Mutex to prevent race conditions

    subnet_calculator_state_init(plugin_state);

    plugin_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!plugin_state->mutex) {// Not plugin_state
        FURI_LOG_E("subnet_calculator", "cannot create mutex\r\n");
        free(plugin_state);
        return 255;
    }

    // System callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, plugin_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    PluginEvent event;
    for(bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);

        furi_mutex_acquire(plugin_state->mutex, FuriWaitForever);

        if(event_status == FuriStatusOk) {
            // Press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                        case InputKeyUp:
                            plugin_state->y--;
                            break;
                        case InputKeyDown:
                            plugin_state->y++;
                            break;
                        case InputKeyRight:
                            plugin_state->x++;
                            break;
                        case InputKeyLeft:
                            plugin_state->x--;
                            break;
                        case InputKeyOk:
                        case InputKeyBack:
                            // Exit the plugin
                            processing = false;
                            break;
                        default:
                            break;
                    }
                }
            }
        } else {
            FURI_LOG_D("subnet_calculator", "FuriMessageQueue: event timeout");
            // Event timeout
        }

        view_port_update(view_port);
        furi_mutex_release(plugin_state->mutex);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(plugin_state->mutex);

    return 0;
}