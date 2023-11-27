#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>

#include <wayland-client.h>
#include "cairo.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#include "pool-buffer.h"


struct wl_compositor *compositor;
struct wl_shm *shm;
struct zwlr_layer_surface_v1 *layer_surface;
struct zwlr_layer_shell_v1 *layer_shell;
struct wl_output *output;
struct wl_surface *surface;
int32_t scale;

struct wl_callback *frame_callback;
bool configured;
bool dirty;
struct pool_buffer buffers[2];
struct pool_buffer *current_buffer;


cairo_t *cairo;
int32_t width = 720;
int32_t height = 300;

static void noop() {
	// This space intentionally left blank
}


static void rounded_rectangle(cairo_t *cairo, double x, double y, double width, double height,
		int scale, int radius) {
	if (width == 0 || height == 0) {
		return;
	}
	x *= scale;
	y *= scale;
	width *= scale;
	height *= scale;
	radius *= scale;
	double degrees = M_PI / 180.0;

	cairo_new_sub_path(cairo);
	cairo_arc(cairo, x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
	cairo_arc(cairo, x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
	cairo_arc(cairo, x + radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
	cairo_arc(cairo, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
	cairo_close_path(cairo);
}

void render() {
	cairo_t *cairo = current_buffer->cairo;
 	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cairo,0, .5,.5,1);
	rounded_rectangle(cairo, 0, 0, width, height, 1, 50);
	cairo_fill(cairo);

	cairo_set_source_rgba(cairo,0,0,0,1);
	cairo_arc(cairo, 330, 160, 40, 0, 2 * M_PI);
	cairo_fill(cairo);
		
	cairo_move_to(cairo, 100, 100);
	cairo_set_source_rgba(cairo,1, 1, 1,1);
	  cairo_select_font_face(cairo, "Purisa",
      CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cairo, 16);
	cairo_show_text(cairo, "Test text antialiasing");
	
}

static void send_frame();

static void output_frame_handle_done(void *data, struct wl_callback *callback,
		uint32_t time) {
	wl_callback_destroy(callback);
	frame_callback = NULL;

	if (dirty) {
		send_frame();
	}
}

static const struct wl_callback_listener output_frame_listener = {
	.done = output_frame_handle_done,
};

static void send_frame() {
	if (!configured) return;
	
	int32_t buffer_width = width * scale;
	int32_t buffer_height = height * scale;

	current_buffer = get_next_buffer(shm, buffers, buffer_width, buffer_height);
	if (current_buffer == NULL) return;
	current_buffer->busy = true;

	cairo_scale(current_buffer->cairo, scale, scale);
	render();

	// Schedule a frame in case the output becomes dirty again
	frame_callback = wl_surface_frame(surface);
	wl_callback_add_listener(frame_callback,
		&output_frame_listener, output);

	wl_surface_attach(surface, current_buffer->buffer, 0, 0);
	wl_surface_damage(surface, 0, 0, width, height);
	wl_surface_set_buffer_scale(surface, scale);
	wl_surface_commit(surface);
	dirty = false;
}

static void layer_surface_handle_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t w, uint32_t h) {
configured = true;
zwlr_layer_surface_v1_ack_configure(surface, serial);
send_frame();
}

static void layer_surface_handle_closed(void *data,
		struct zwlr_layer_surface_v1 *surface) {

}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_handle_configure,
	.closed = layer_surface_handle_closed,
};

static void wayland_output_scale(void *data, struct wl_output *output,
                                 int32_t s) {
  scale = s;
}

static const struct wl_output_listener output_listener = {
    .geometry = noop,
    .mode = noop,
    .scale = wayland_output_scale,
    .done = noop,
};


void registry_global_handler(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version
) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
	output = wl_registry_bind(registry, name, &wl_output_interface, 3);
	wl_output_add_listener(output, &output_listener, NULL);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    } 
}


struct wl_registry_listener registry_listener = {
    .global = registry_global_handler,
    .global_remove = noop
};

int main(void)
{
    struct wl_display *display = wl_display_connect(NULL);
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
	
    // wait for the "initial" set of globals to appear
    wl_display_roundtrip(display);

    // all our objects should be ready!
    if (compositor == NULL) {
		fprintf(stderr, "compositor doesn't support wl_compositor\n");
		return EXIT_FAILURE;
	}
	else if (shm == NULL) {
		fprintf(stderr, "compositor doesn't support wl_shm\n");
		return EXIT_FAILURE;
	}
	else if (layer_shell == NULL) {
		fprintf(stderr, "compositor doesn't support zwlr_layer_shell_v1\n");
		return EXIT_FAILURE;
	}

    surface = wl_compositor_create_surface(compositor);

    layer_surface = zwlr_layer_shell_v1_get_layer_surface(layer_shell, surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "panel");
    zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener, output);

    zwlr_layer_surface_v1_set_size(layer_surface, width, height);
	zwlr_layer_surface_v1_set_anchor(layer_surface,
			ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
	zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface, 0);
	zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, 0);
	wl_surface_commit(surface);

    while (wl_display_dispatch(display)){}

    return 0;
}
