#include "cleanface.h"

static Window *window;
static Layer *digitsLayer;
static DigitSlot digitSlots[4];
static int align = -1;
static int hourLeadingZero = true;

static void update_digits_layer(Layer *layer, GContext *ctx) {
	GRect r;
	graphics_context_set_fill_color(ctx, DIGIT_BACKGROUND_COLOR);

	r = layer_get_bounds(digitsLayer);
	graphics_fill_rect(ctx, GRect(0, 0, r.size.w, r.size.h), 0, GCornerNone);

	graphics_context_set_fill_color(ctx, DIGIT_BORDER_COLOR);
	graphics_fill_rect(ctx, GRect(0, 0, r.size.w, WIDGET_BORDER), 0, GCornerNone);
	graphics_fill_rect(ctx, GRect(0, r.size.h - WIDGET_BORDER, r.size.w, WIDGET_BORDER), 0, GCornerNone);
}

static void update_digit_slot(Layer *layer, GContext *ctx) {
	DigitSlot *slot = *(DigitSlot**)layer_get_data(layer);
	int col, row;
	const int texel_w = (TIME_DIGIT_W - 2 * WIDGET_BORDER) / TIME_DIGIT_COLS;
	const int texel_h = (TIME_DIGIT_H - 2 * WIDGET_BORDER) / TIME_DIGIT_ROWS;

	if (slot->curDigit < 0)
		return;

	graphics_context_set_fill_color(ctx, DIGIT_COLOR);

	for (row = 0; row < TIME_DIGIT_ROWS; row++) {
		char v = digits[slot->curDigit][row];
		for (col = 0; col < TIME_DIGIT_COLS; col++) {
			if (v & (1 << (TIME_DIGIT_COLS - col - 1))) {
				graphics_fill_rect(ctx,
					GRect(
						WIDGET_BORDER + col * texel_w,
						WIDGET_BORDER + row * texel_h,
						texel_w, texel_h
					),
					0, GCornerNone
				);
			}
		}
	}
}

static void display_value(unsigned short value, unsigned short layer_offset, bool leadingZero) {
	for (int col = 1; col >= 0; col--) {
		DigitSlot *slot = &digitSlots[layer_offset + col];
		slot->curDigit = value % 10;
		if ((slot->curDigit == 0) && (col == 0) && !leadingZero)
			slot->curDigit = -1;
		value /= 10;
	}
}

static unsigned short handle_12_24(unsigned short hour) {
	if (clock_is_24h_style()) {
		return hour;
	}

	hour %= 12;
	return (hour != 0) ? hour : 12;
}

static void tick_handler(struct tm *tickTime, TimeUnits unitsChanged)
{
	display_value(handle_12_24(tickTime->tm_hour), 0, hourLeadingZero);
	display_value(tickTime->tm_min, 2, true);
	layer_mark_dirty(digitsLayer);
}

static void window_load(Window *window) {
	Layer *windowLayer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(windowLayer);
	unsigned i;
	struct tm *tickTime;
	DigitSlot *slot;
	int digitsLayerPos = 0;

	window_set_background_color(window, BACKGROUND_COLOR);

	if (align == -1) {
		/* Clock on top */
		digitsLayerPos = BORDER_OFFSET;
	} else if (align == 0) {
		/* Clock in the middle */
		digitsLayerPos = bounds.size.h / 2 - TIME_WIDGET_H / 2;
	} else {
		/* Clock on the bottom */
		digitsLayerPos = bounds.size.h - BORDER_OFFSET - TIME_WIDGET_H;
	}

	digitsLayer = layer_create(
		GRect(
			0, digitsLayerPos,
			bounds.size.w, TIME_DIGIT_H + WIDGET_BORDER * 2
		)
	);
	layer_set_update_proc(digitsLayer, update_digits_layer);
	layer_add_child(window_get_root_layer(window), digitsLayer);

	for (i = 0; i < sizeof(digitSlots) / sizeof(digitSlots[0]); i++) {
		slot = &digitSlots[i];

		slot->curDigit = 0;
		slot->layer = layer_create_with_data(
				GRect(i * TIME_DIGIT_W, WIDGET_BORDER,
					  TIME_DIGIT_W, TIME_DIGIT_H),
				sizeof(slot)
			);

		*(DigitSlot **)layer_get_data(slot->layer) = slot;
		layer_set_update_proc(slot->layer, update_digit_slot);

		layer_add_child(digitsLayer, slot->layer);
	}

	// initial values
	time_t temp;
	temp = time(NULL);
	tickTime = localtime(&temp);
	tick_handler(tickTime, MINUTE_UNIT);
}

static void window_unload(Window *window) {
	unsigned i;

	tick_timer_service_unsubscribe();

	for (i = 0; i < sizeof(digitSlots) / sizeof(digitSlots[0]); i++) {
		layer_remove_from_parent(digitSlots[i].layer);
		layer_destroy(digitSlots[i].layer);
	}
	layer_destroy(digitsLayer);
}

static void init(void) {
	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});
	const bool animated = true;
	window_stack_push(window, animated);

	tick_timer_service_subscribe(MINUTE_UNIT, (TickHandler) tick_handler);	
}

static void deinit(void) {
	window_destroy(window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
