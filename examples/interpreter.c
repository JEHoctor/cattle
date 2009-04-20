/* interpreter -- Simple Brainfuck interpreter
 * Copyright (C) 2008-2009  Andrea Bolognani <eof@kiyuko.org>
 * This file is part of Cattle
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Homepage: http://www.kiyuko.org/software/cattle
 */

#include <glib.h>
#include <glib-object.h>
#include <cattle/cattle.h>

gint
main (gint argc, gchar **argv)
{
    CattleInterpreter *interpreter;
    CattleProgram *program;
    CattleConfiguration *configuration;
    GError *error = NULL;

    g_type_init ();
    g_set_prgname ("interpreter");

    if (argc != 2) {
        g_warning ("Usage: %s FILENAME", argv[0]);
        return 1;
    }

    /* Create a new interpreter */
    interpreter = cattle_interpreter_new ();

    configuration = cattle_interpreter_get_configuration (interpreter);
    cattle_configuration_set_debug_is_enabled (configuration, TRUE);
    g_object_unref (configuration);

    program = cattle_interpreter_get_program (interpreter);

    /* Load the program, aborting on failure */
    if (!cattle_program_load_from_file (program, argv[1], &error)) {

        g_warning ("Cannot load program: %s", error->message);

        g_error_free (error);
        g_object_unref (program);
        g_object_unref (interpreter);

        return 1;
    }
    g_object_unref (program);

    /* Start the execution */
    if (!cattle_interpreter_run (interpreter, &error)) {

        g_warning ("Cannot run program: %s", error->message);

        g_error_free (error);
        g_object_unref (interpreter);

        return 1;
    }
    g_object_unref (interpreter);

    return 0;
}
