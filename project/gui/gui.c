#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../include/debugger.h"

static Debugger dbg;
static GtkWidget *program_entry;
static GtkWidget *breakpoint_entry;
static GtkWidget *memory_entry;
static GtkWidget *output_view;
static GtkWidget *status_label;

static void append_text(const char *text)
{
    GtkTextBuffer *buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(output_view));

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);

    gtk_text_buffer_insert(buffer, &end, text, -1);
    gtk_text_buffer_insert(buffer, &end, "\n\n", -1);
}

static void set_status(const char *text)
{
    gtk_label_set_text(GTK_LABEL(status_label), text);
}

static int parse_address(const char *text, unsigned long long *value)
{
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(text, &end, 16);

    if (errno != 0 || end == text || *end != '\0')
        return -1;

    *value = v;
    return 0;
}

static void on_launch(GtkWidget *widget, gpointer data)
{
    (void)widget; (void)data;

    const char *program =
        gtk_entry_get_text(GTK_ENTRY(program_entry));

    if (program[0] == '\0') {
        append_text("Enter the target program path first.");
        return;
    }

    char message[2048];

    debugger_init(&dbg);

    if (debugger_launch(&dbg, program, message, sizeof(message)) == 0) {
        append_text(message);
        set_status("Status: STOPPED");
    } else {
        append_text(message);
        set_status("Status: ERROR");
    }
}

static void on_set_breakpoint(GtkWidget *widget, gpointer data)
{
    (void)widget; (void)data;

    const char *text =
        gtk_entry_get_text(GTK_ENTRY(breakpoint_entry));

    unsigned long long addr;

    if (parse_address(text, &addr) == -1) {
        append_text("Enter a valid hexadecimal address, for example: 401126");
        return;
    }

    char message[2048];

    if (debugger_set_breakpoint(&dbg, addr,
                                message, sizeof(message)) == 0) {
        append_text(message);
        set_status("Status: BREAKPOINT SET");
    } else {
        append_text(message);
    }
}

static void on_continue(GtkWidget *widget, gpointer data)
{
    (void)widget; (void)data;

    char message[2048];

    debugger_continue(&dbg, message, sizeof(message));
    append_text(message);

    if (dbg.running)
        set_status("Status: STOPPED");
    else
        set_status("Status: FINISHED");
}

static void on_step(GtkWidget *widget, gpointer data)
{
    (void)widget; (void)data;

    char message[2048];

    debugger_step(&dbg, message, sizeof(message));
    append_text(message);

    if (dbg.running)
        set_status("Status: STOPPED");
    else
        set_status("Status: FINISHED");
}

static void on_stop(GtkWidget *widget, gpointer data)
{
    (void)widget; (void)data;

    char message[2048];

    debugger_stop(&dbg, message, sizeof(message));
    append_text(message);

    set_status("Status: READY");
}

static void on_registers(GtkWidget *widget, gpointer data)
{
    (void)widget; (void)data;

    char message[2048];

    debugger_get_registers(&dbg, message, sizeof(message));
    append_text(message);
}

static void on_memory(GtkWidget *widget, gpointer data)
{
    (void)widget; (void)data;

    const char *text =
        gtk_entry_get_text(GTK_ENTRY(memory_entry));

    unsigned long long addr;

    if (parse_address(text, &addr) == -1) {
        append_text("Enter a valid hexadecimal memory address.");
        return;
    }

    char message[2048];

    debugger_read_memory(&dbg, addr, message, sizeof(message));
    append_text(message);
}

static GtkWidget *make_button(const char *label,
                              GCallback callback)
{
    GtkWidget *button = gtk_button_new_with_label(label);

    g_signal_connect(button, "clicked", callback, NULL);

    return button;
}

static void activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;

    GtkWidget *window =
        gtk_application_window_new(app);

    gtk_window_set_title(
        GTK_WINDOW(window),
        "Mini Linux Debugger"
    );

    gtk_window_set_default_size(
        GTK_WINDOW(window),
        1000,
        700
    );

    gtk_container_set_border_width(
        GTK_CONTAINER(window),
        15
    );

    GtkWidget *main_box =
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_container_add(
        GTK_CONTAINER(window),
        main_box
    );

    GtkWidget *title =
        gtk_label_new(NULL);

    gtk_label_set_markup(
        GTK_LABEL(title),
        "<b><big>MINI LINUX DEBUGGER</big></b>"
    );

    gtk_box_pack_start(
        GTK_BOX(main_box),
        title,
        FALSE, FALSE, 5
    );

    status_label =
        gtk_label_new("Status: READY");

    gtk_box_pack_start(
        GTK_BOX(main_box),
        status_label,
        FALSE, FALSE, 5
    );

    /* Target program */
    GtkWidget *program_box =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    gtk_box_pack_start(
        GTK_BOX(main_box),
        program_box,
        FALSE, FALSE, 5
    );

    gtk_box_pack_start(
        GTK_BOX(program_box),
        gtk_label_new("Target Program:"),
        FALSE, FALSE, 0
    );

    program_entry =
        gtk_entry_new();

    gtk_entry_set_text(
        GTK_ENTRY(program_entry),
        "./bin/test_program"
    );

    gtk_box_pack_start(
        GTK_BOX(program_box),
        program_entry,
        TRUE, TRUE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(program_box),
        make_button("RUN", G_CALLBACK(on_launch)),
        FALSE, FALSE, 0
    );

    /* Control buttons */
    GtkWidget *control_box =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    gtk_box_pack_start(
        GTK_BOX(main_box),
        control_box,
        FALSE, FALSE, 5
    );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        make_button("CONTINUE", G_CALLBACK(on_continue)),
        FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        make_button("STEP", G_CALLBACK(on_step)),
        FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        make_button("REGISTERS", G_CALLBACK(on_registers)),
        FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        make_button("STOP", G_CALLBACK(on_stop)),
        FALSE, FALSE, 0
    );

    /* Breakpoint */
    GtkWidget *bp_box =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    gtk_box_pack_start(
        GTK_BOX(main_box),
        bp_box,
        FALSE, FALSE, 5
    );

    gtk_box_pack_start(
        GTK_BOX(bp_box),
        gtk_label_new("Breakpoint address (hex):"),
        FALSE, FALSE, 0
    );

    breakpoint_entry =
        gtk_entry_new();

    gtk_entry_set_placeholder_text(
        GTK_ENTRY(breakpoint_entry),
        "e.g. 401126"
    );

    gtk_box_pack_start(
        GTK_BOX(bp_box),
        breakpoint_entry,
        TRUE, TRUE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(bp_box),
        make_button("SET BREAKPOINT",
                     G_CALLBACK(on_set_breakpoint)),
        FALSE, FALSE, 0
    );

    /* Memory */
    GtkWidget *mem_box =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    gtk_box_pack_start(
        GTK_BOX(main_box),
        mem_box,
        FALSE, FALSE, 5
    );

    gtk_box_pack_start(
        GTK_BOX(mem_box),
        gtk_label_new("Memory address (hex):"),
        FALSE, FALSE, 0
    );

    memory_entry =
        gtk_entry_new();

    gtk_entry_set_placeholder_text(
        GTK_ENTRY(memory_entry),
        "e.g. 404000"
    );

    gtk_box_pack_start(
        GTK_BOX(mem_box),
        memory_entry,
        TRUE, TRUE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(mem_box),
        make_button("READ MEMORY",
                     G_CALLBACK(on_memory)),
        FALSE, FALSE, 0
    );

    /* Output */
    GtkWidget *frame =
        gtk_frame_new("Debug Output");

    gtk_box_pack_start(
        GTK_BOX(main_box),
        frame,
        TRUE, TRUE, 5
    );

    GtkWidget *scroll =
        gtk_scrolled_window_new(NULL, NULL);

    gtk_container_add(
        GTK_CONTAINER(frame),
        scroll
    );

    output_view =
        gtk_text_view_new();

    gtk_text_view_set_editable(
        GTK_TEXT_VIEW(output_view),
        FALSE
    );

    gtk_text_view_set_monospace(
        GTK_TEXT_VIEW(output_view),
        TRUE
    );

    gtk_container_add(
        GTK_CONTAINER(scroll),
        output_view
    );

    append_text(
        "Welcome to Mini Linux Debugger.\n"
        "1. Click RUN.\n"
        "2. Use STEP or CONTINUE.\n"
        "3. Use REGISTERS to inspect CPU registers.\n"
        "4. Set a breakpoint using a hexadecimal address."
    );

    gtk_widget_show_all(window);
}

int main(int argc, char **argv)
{
    GtkApplication *app =
        gtk_application_new(
            "com.example.minidebugger",
            G_APPLICATION_DEFAULT_FLAGS
        );

    g_signal_connect(
        app,
        "activate",
        G_CALLBACK(activate),
        NULL
    );

    int status =
        g_application_run(
            G_APPLICATION(app),
            argc,
            argv
        );

    g_object_unref(app);

    return status;
}
