/* Cattle -- Flexible Brainfuck interpreter library
 * Copyright (C) 2008  Andrea Bolognani <eof@kiyuko.org>
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

#include "cattle-enums.h"
#include "cattle-marshal.h"
#include "cattle-interpreter.h"
#include <stdio.h>

/**
 * SECTION:cattle-interpreter
 * @short_description: Brainfuck interpreter
 * @see_also: #CattleConfiguration, #CattleProgram, #CattleTape
 *
 * An instance of #CattleInterpreter represents a Brainfuck interpreter, that
 * is, an object which is capable of executing a #CattleProgram.
 *
 * #CattleInterpreter handles all the aspects of execution, including input and
 * output. It also hides all the details to the user, who only needs to
 * initialize the interpreter and call cattle_interpreter_run() to execute
 * a Brainfuck program.
 *
 * The behaviour of an interpreter can be modified by providing a suitable
 * #CattleConfiguration object.
 *
 * Once initialized, a #CattleInterpreter can run the assigned program as many
 * times as you want; anyway, the memory tape is not cleaned automatically,
 * so you will probably want to replace the used tape with a newly-created
 * one between two different executions.
 */

G_DEFINE_TYPE (CattleInterpreter, cattle_interpreter, G_TYPE_OBJECT)

/**
 * CattleInterpreter:
 *
 * Opaque data structure representing an interpreter. It should never be
 * accessed directly; use the methods below instead.
 */

#define CATTLE_INTERPRETER_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), CATTLE_TYPE_INTERPRETER, CattleInterpreterPrivate))

struct _CattleInterpreterPrivate {
    gboolean    disposed;

    CattleConfiguration   *configuration;
    CattleProgram         *program;
    CattleTape            *tape;

    gboolean               had_input;
    gchar                 *input;
    gchar                 *input_cursor;
    gboolean               end_of_input_reached;
};

/* Properties */
enum {
    PROP_0,
    PROP_CONFIGURATION,
    PROP_PROGRAM,
    PROP_TAPE
};

static void   cattle_interpreter_set_property   (GObject        *object,
                                                 guint           property_id,
                                                 const GValue   *value,
                                                 GParamSpec     *pspec);
static void   cattle_interpreter_get_property   (GObject        *object,
                                                 guint           property_id,
                                                 GValue         *value,
                                                 GParamSpec     *pspec);

/* Signals */
enum {
    INPUT_REQUEST,
    OUTPUT_REQUEST,
	DEBUG_REQUEST,
    LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = {0};

/* Internal functions */
static gboolean   run_real   (CattleInterpreter   *interpreter,
                              CattleInstruction   *instruction,
                              GError              **error);

static void
cattle_interpreter_init (CattleInterpreter *self)
{
    self->priv = CATTLE_INTERPRETER_GET_PRIVATE (self);

    self->priv->configuration = cattle_configuration_new ();
    self->priv->program = cattle_program_new ();
    self->priv->tape = cattle_tape_new ();

    self->priv->had_input = FALSE;
    self->priv->input = NULL;
    self->priv->input_cursor = NULL;
    self->priv->end_of_input_reached = FALSE;

    self->priv->disposed = FALSE;
}

static void
cattle_interpreter_dispose (GObject *object)
{
    CattleInterpreter *self = CATTLE_INTERPRETER (object);

    if (G_LIKELY (!self->priv->disposed)) {

        self->priv->disposed = TRUE;

        if (G_IS_OBJECT (self->priv->configuration)) {
            g_object_unref (G_OBJECT (self->priv->configuration));
            self->priv->configuration = NULL;
        }
        if (G_IS_OBJECT (self->priv->program)) {
            g_object_unref (G_OBJECT (self->priv->program));
            self->priv->program = NULL;
        }
        if (G_IS_OBJECT (self->priv->tape)) {
            g_object_unref (G_OBJECT (self->priv->tape));
            self->priv->tape = NULL;
        }

        /* FIXME We would also need to release the input, but we don't
         *       currently do it because it would be really tricky: we have
         *       no way to know how to release it */

        G_OBJECT_CLASS (cattle_interpreter_parent_class)->dispose (G_OBJECT (self));
    }
}

static void
cattle_interpreter_finalize (GObject *object)
{
    CattleInterpreter *self = CATTLE_INTERPRETER (object);

    G_OBJECT_CLASS (cattle_interpreter_parent_class)->finalize (G_OBJECT (self));
}

static void
cattle_interpreter_class_init (CattleInterpreterClass *self)
{
    GObjectClass *object_class = G_OBJECT_CLASS (self);
    GParamSpec *pspec;

    object_class->set_property = cattle_interpreter_set_property;
    object_class->get_property = cattle_interpreter_get_property;
    object_class->dispose = cattle_interpreter_dispose;
    object_class->finalize = cattle_interpreter_finalize;

    /**
     * CattleInterpreter:configuration:
     *
     * Configuration used by the interpreter.
     *
     * Changes to this property are not notified.
     */
    pspec = g_param_spec_object ("configuration",
                                 "Configuration for the interpreter",
                                 "Get/set interpreter's configuration",
                                 CATTLE_TYPE_CONFIGURATION,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_CONFIGURATION,
                                     pspec);

    /**
     * CattleInterpreter:program:
     *
     * Program executed by the interpreter.
     *
     * Changes to this property are not notified.
     */
    pspec = g_param_spec_object ("program",
                                 "Program to be executed by the interpreter",
                                 "Get/set interpreter's program",
                                 CATTLE_TYPE_PROGRAM,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_PROGRAM,
                                     pspec);

    /**
     * CattleInterpreter:tape:
     *
     * Tape used to store the data needed by the program.
     *
     * Changes to this property are not notified.
     */
    pspec = g_param_spec_object ("tape",
                                 "Tape used by the interpreter",
                                 "Get/set interpreter's tape",
                                 CATTLE_TYPE_TAPE,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_TAPE,
                                     pspec);

    /**
     * CattleInterpreter::input-request:
     * @interpreter: a #CattleInterpreter
     * @input: return location for the input
     * @error: a #GError to be used for reporting, or %NULL
     *
     * Emitted  whenever the interpreter needs some input.
     *
     * If the operation fails, @error have to be filled with detailed
     * information about the error.
     *
     * Return: #TRUE if the operation is successful, #FALSE otherwise.
	 *
	 * Since: 0.9.1
     */
    signals[INPUT_REQUEST] = g_signal_new ("input-request",
                                           CATTLE_TYPE_INTERPRETER,
                                           G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                                           G_STRUCT_OFFSET (CattleInterpreterClass, input_request),
                                           g_signal_accumulator_true_handled,
                                           NULL,
                                           cattle_marshal_BOOLEAN__POINTER_POINTER,
                                           G_TYPE_BOOLEAN,
                                           2,
                                           G_TYPE_POINTER,
                                           G_TYPE_POINTER);

    /**
     * CattleInterpreter::output-request:
     * @interpreter: a #CattleInterpreter
     * @output: the character that needs to be printed
     * @error: a #GError to be used for reporting, or %NULL
     *
     * Emitted whenever the interpreter needs to perform some output.
     *
     * If the operation fails, @error have to be filled with detailed
     * information about the error.
     *
     * Return: #TRUE if the operation is successful, #FALSE otherwise.
	 *
	 * Since: 0.9.1
     */
    signals[OUTPUT_REQUEST] = g_signal_new ("output-request",
                                            CATTLE_TYPE_INTERPRETER,
                                            G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                                            G_STRUCT_OFFSET (CattleInterpreterClass, output_request),
                                            g_signal_accumulator_true_handled,
                                            NULL,
                                            cattle_marshal_BOOLEAN__CHAR_POINTER,
                                            G_TYPE_BOOLEAN,
                                            2,
                                            G_TYPE_CHAR,
                                            G_TYPE_POINTER);

	/**
	 * CattleInterpreter::debug-request:
	 * @interpreter: a #CattleInterpreter
	 * @error: a #GError used for error reporting, or %NULL
	 *
	 * Emitted whenever a tape dump is requested.
	 *
	 * If the operation fails, @error have to be filled with detailed
	 * information about the error.
	 *
	 * Return: #TRUE if the operation is successful, #FALSE otherwise.
	 *
	 * Since: 0.9.2
	 */
	signals[DEBUG_REQUEST] = g_signal_new ("debug_request",
										   CATTLE_TYPE_INTERPRETER,
										   G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
										   G_STRUCT_OFFSET (CattleInterpreterClass, debug_request),
										   g_signal_accumulator_true_handled,
										   NULL,
										   cattle_marshal_BOOLEAN__POINTER,
										   G_TYPE_BOOLEAN,
										   1,
										   G_TYPE_POINTER);

    g_type_class_add_private (object_class, sizeof (CattleInterpreterPrivate));
}

static void
cattle_interpreter_set_property (GObject        *object,
                                 guint           property_id,
                                 const GValue   *value,
                                 GParamSpec     *pspec)
{
    CattleInterpreter *self = CATTLE_INTERPRETER (object);

    if (G_LIKELY (!self->priv->disposed)) {

        switch (property_id) {

            case PROP_CONFIGURATION:
                cattle_interpreter_set_configuration (self, g_value_get_object (value));
                break;

            case PROP_PROGRAM:
                cattle_interpreter_set_program (self, g_value_get_object (value));
                break;

            case PROP_TAPE:
                cattle_interpreter_set_tape (self, g_value_get_object (value));
                break;

            default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
    }
}

static void
cattle_interpreter_get_property (GObject      *object,
                                 guint         property_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
    CattleInterpreter *self = CATTLE_INTERPRETER (object);

    if (G_LIKELY (!self->priv->disposed)) {

        switch (property_id) {

            case PROP_CONFIGURATION:
                g_value_set_object (value, cattle_interpreter_get_configuration (self));
                break;

            case PROP_PROGRAM:
                g_value_set_object (value, cattle_interpreter_get_program (self));
                break;

            case PROP_TAPE:
                g_value_set_object (value, cattle_interpreter_get_tape (self));
                break;

            default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
    }
}

static gboolean
run_real (CattleInterpreter    *self,
          CattleInstruction    *instruction,
          GError              **error)
{
    CattleConfiguration *configuration;
    CattleProgram *program;
    CattleTape *tape;
    CattleInstruction *loop;
    gboolean success;
    gchar temp;
    glong i;

    configuration = cattle_interpreter_get_configuration (self);
    program = cattle_interpreter_get_program (self);
    tape = cattle_interpreter_get_tape (self);
    success = TRUE;

    do {

        switch (cattle_instruction_get_value (instruction)) {

            case CATTLE_INSTRUCTION_LOOP_BEGIN:

                loop = cattle_instruction_get_loop (instruction);

                if (CATTLE_IS_INSTRUCTION (loop)) {

                    while (cattle_tape_get_current_value (tape) != 0) {
                        success = run_real (self,
                                            loop,
                                            error);
                    }
                }
                break;

            case CATTLE_INSTRUCTION_LOOP_END:

                return success;
                break;

            case CATTLE_INSTRUCTION_MOVE_LEFT:

                for (i = 0; i < cattle_instruction_get_quantity (instruction); i++) {
                    cattle_tape_move_left (tape);
                }
                break;

            case CATTLE_INSTRUCTION_MOVE_RIGHT:
                for (i = 0; i < cattle_instruction_get_quantity (instruction); i++) {
                    cattle_tape_move_right (tape);
                }
                break;

            case CATTLE_INSTRUCTION_INCREASE:
                cattle_tape_set_current_value (tape,
                                               cattle_tape_get_current_value (tape) + cattle_instruction_get_quantity (instruction));
                break;

            case CATTLE_INSTRUCTION_DECREASE:
                cattle_tape_set_current_value (tape,
                                               cattle_tape_get_current_value (tape) - cattle_instruction_get_quantity (instruction));
                break;

            case CATTLE_INSTRUCTION_READ:

                for (i = 0; i < cattle_instruction_get_quantity (instruction); i++) {

                    /* The following code is quite complicated because it
                     * needs to handle different situations.
                     * The execution of a read instruction is divided into
                     * three steps:
                     *
                     *     1) Fetch more input if needed: the "input-request"
                     *        signal is emitted, and the signal handler is
                     *        responsible for fetching more input from the
                     *        source. This step is obviously skipped if some
                     *        input was included in the program.
                     *
                     *     2) Get the char and normalize it: this step allows
                     *        us to handle all the input consistently in the
                     *        last step, regardless of its source.
                     *
                     *     3) Perform the actual operation: the easy part ;)
                     *
                     * TODO This step can probably be semplified.
                     */

                    /* STEP 1: Fetch more input if needed */

                    /* We don't even try to fetch more input if the input was
                     * included in the program or if the end of input has
                     * alredy been reached */
                    if (self->priv->had_input == FALSE && !self->priv->end_of_input_reached) {

                        /* We are at the end of the current line of input,
                         * or we haven't fetched any input yet. We need to
                         * emit the "input-request" signal to get more input */
                        if (self->priv->input_cursor == NULL || g_utf8_get_char (self->priv->input_cursor) == 0) {

                            g_signal_emit (self,
                                           signals[INPUT_REQUEST],
                                           0,
                                           &(self->priv->input),
                                           error,
                                           &success);

                            /* The operation failed: we abort immediately;
                             * the signal handler must set the error */
                            if (success == FALSE) {
                                break;
                            }

                            /* A return value of NULL from the signal handler
                             * means the end of input was reached. We set the
                             * appropriate flag */
                            if (self->priv->input == NULL) {
                                self->priv->end_of_input_reached = TRUE;
                            }

                            /* Move the cursor to the beginning of the new
                             * input line */
                            self->priv->input_cursor = self->priv->input;
                        }
                    }

                    /* STEP 2: Get the char and normalize it */

                    /* If we have already reached the end of input, the
                     * current char is obviously an EOF */
                    if (self->priv->end_of_input_reached) {
                        temp = (gchar) EOF;
                    }

                    else {
                        temp = g_utf8_get_char (self->priv->input_cursor);

                        /* The end of the saved input is converted into an EOF
                         * character for consistency. We don't need to move the
                         * cursor forward if we are already at the end of the
                         * input */
                        if (temp == 0) {

                            if (cattle_program_get_input (self->priv->program) == NULL) { 
                                temp = (gchar) EOF;
                                self->priv->end_of_input_reached = TRUE;
                            }
                        }

                        /* There is some more input: we have to move the cursor
                         * one position forward */
                        else {
                            self->priv->input_cursor = g_utf8_next_char (self->priv->input_cursor);
                        }
                    }
                }

                /* STEP 3: Perform the actual operation */

                /* We are at the end of the input: we have to check the
                 * configuration to know which action we should perform */
                if (temp == (gchar) EOF) {

                    switch (cattle_configuration_get_on_eof_action (configuration)) {

                        case CATTLE_ON_EOF_STORE_ZERO:
                            cattle_tape_set_current_value (tape, (gchar) 0);
                            break;

                        case CATTLE_ON_EOF_STORE_EOF:
                            cattle_tape_set_current_value (tape, temp);
                            break;

                        case CATTLE_ON_EOF_DO_NOTHING:
                        default:
                            /* Do nothing */
                            break;
                    }
                }

                /* We are not at the end of the input: just save the
                 * input into the current cell of the tape */
                else {
                    cattle_tape_set_current_value (tape, temp);
                }
                break;

            case CATTLE_INSTRUCTION_PRINT:

                /* Write the value in the current cell to standard output */
                for (i = 0; i < cattle_instruction_get_quantity (instruction); i++) {

                    g_signal_emit (self,
                                   signals[OUTPUT_REQUEST],
                                   0,
                                   cattle_tape_get_current_value (tape),
                                   error,
                                   &success);

                    /* Stop at the first error, even if we should output
                     * the content of the current cell more than once */
                    if (success == FALSE) {
                        break;
                    }
                }
                break;

            case CATTLE_INSTRUCTION_DUMP_TAPE:

                /* Dump the tape only if debugging is enabled in the
                 * configuration */
                if (cattle_configuration_get_debug_is_enabled (configuration)) {

					for (i = 0; i < cattle_instruction_get_quantity (instruction); i++) {

						g_signal_emit (self,
									   signals[DEBUG_REQUEST],
									   0,
									   error,
									   &success);

						if (success == FALSE) {
							break;
						}
					}
                }
                break;

            case CATTLE_INSTRUCTION_NONE:
                /* Do nothing */
                break;
        }

        /* Go to the next instruction */
        instruction = cattle_instruction_get_next (instruction);

    } while (CATTLE_IS_INSTRUCTION (instruction) && success);

    return success;
}

/**
 * cattle_interpreter_new:
 *
 * Create and initialize a new interpreter.
 *
 * Returns: a new #CattleInterpreter.
 **/
CattleInterpreter*
cattle_interpreter_new (void)
{
    return g_object_new (CATTLE_TYPE_INTERPRETER, NULL);
}

/**
 * cattle_interpreter_run:
 * @interpreter: a #CattleInterpreter
 * @error: #GError used for error reporting
 *
 * Run the interpreter.
 *
 * Please note that this method is always successful at the moment. There is
 * no condition under which the execution of a program can fail; anyway,
 * this will change in future versions, and you are encouraged to check the
 * return value of this method.
 *
 * Return: #TRUE if @interpreter completed the execution of its program
 * successfully, #FALSE otherwise.
 */
gboolean
cattle_interpreter_run (CattleInterpreter    *self,
                        GError              **error)
{
    CattleProgram *program;
    CattleInstruction *instruction;

    g_return_val_if_fail (CATTLE_IS_INTERPRETER (self), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    if (G_LIKELY (!self->priv->disposed)) {

        program = cattle_interpreter_get_program (self);
        instruction = cattle_program_get_instructions (program);

        self->priv->input_cursor = (gchar *) cattle_program_get_input (program);
        if (self->priv->input_cursor != NULL) {
            self->priv->had_input = TRUE;
        }

        return run_real (self,
                         instruction,
                         error);
    }

    return FALSE;
}

/**
 * cattle_interpreter_set_configuration:
 * @interpreter: a #CattleInterpreter
 * @configuration: configuration for the interpreter
 *
 * Set the configuration for @interpreter.
 *
 * The same configuration can be used for as many interpreters as you want,
 * but modifying it after it has been assigned to an interpreter may
 * result in undefined behaviour, and as such is discouraged.
 */
void
cattle_interpreter_set_configuration (CattleInterpreter     *self,
                                      CattleConfiguration   *configuration)
{
    g_return_if_fail (CATTLE_IS_INTERPRETER (self));
    g_return_if_fail (CATTLE_IS_CONFIGURATION (configuration));

    if (G_LIKELY (!self->priv->disposed)) {

        /* Release the reference held on the previous configuration */
        if (G_IS_OBJECT (self->priv->configuration)) {
            g_object_unref (G_OBJECT (self->priv->configuration));
        }

        self->priv->configuration = configuration;
        g_object_ref (G_OBJECT (self->priv->configuration));
    }
}

/**
 * cattle_interpreter_get_configuration:
 * @interpreter: a #CattleInterpreter
 *
 * Get the configuration for @interpreter.
 * See cattle_interpreter_set_configuration().
 *
 * The returned object is owned by @interpreter and must not be modified or
 * freed.
 *
 * Return: configuration for @interpreter.
 */
CattleConfiguration*
cattle_interpreter_get_configuration (CattleInterpreter *self)
{
    CattleConfiguration *configuration = NULL;

    g_return_val_if_fail (CATTLE_IS_INTERPRETER (self), NULL);

    if (G_LIKELY (!self->priv->disposed)) {
        configuration = self->priv->configuration;
    }

    return configuration;
}

/**
 * cattle_interpreter_set_program:
 * @interpreter: a #CattleInterpreter
 * @program: a #CattleProgram
 *
 * Set the program to be executed by @interpreter.
 *
 * You can share a single program between multiple interpreters, as long as
 * you take care not to modify it after it has been assigned to any of them.
 */
void
cattle_interpreter_set_program (CattleInterpreter   *self,
                                CattleProgram       *program)
{
    g_return_if_fail (CATTLE_IS_INTERPRETER (self));
    g_return_if_fail (CATTLE_IS_PROGRAM (program));

    if (G_LIKELY (!self->priv->disposed)) {

        /* Release the reference held to the previous program, if any */
        if (G_IS_OBJECT (self->priv->program)) {
            g_object_unref (G_OBJECT (self->priv->program));
        }

        self->priv->program = program;
        g_object_ref (G_OBJECT (self->priv->program));
    }
}

/**
 * cattle_interpreter_get_program:
 * @interpreter: a #CattleInterpreter
 *
 * Get the current program for @interpreter.
 * See cattle_interpreter_set_program().
 *
 * The returned object is owned by @interpreter and must not be modified or
 * freed.
 *
 * Return: the program @interpreter will run.
 */
CattleProgram*
cattle_interpreter_get_program (CattleInterpreter *self)
{
    CattleProgram *program = NULL;

    g_return_val_if_fail (CATTLE_IS_INTERPRETER (self), NULL);

    if (G_LIKELY (!self->priv->disposed)) {
        program = self->priv->program;
    }

    return program;
}

/**
 * cattle_interpreter_set_tape:
 * @interpreter: a #CattleInterpreter
 * @tape: memory tape for the interpreter
 *
 * Set the memory tape used by @interpreter.
 */
void
cattle_interpreter_set_tape (CattleInterpreter   *self,
                             CattleTape          *tape)
{
    g_return_if_fail (CATTLE_IS_INTERPRETER (self));
    g_return_if_fail (CATTLE_IS_TAPE (tape));

    if (G_LIKELY (!self->priv->disposed)) {

        /* Release the reference held to the previous tape */
        if (G_IS_OBJECT (self->priv->tape)) {
            g_object_unref (G_OBJECT (self->priv->tape));
        }

        self->priv->tape = tape;
        g_object_ref (G_OBJECT (self->priv->tape));
    }
}

/**
 * cattle_interpreter_get_tape:
 * @interpreter: a #CattleInterpreter
 *
 * Get the memory tape used by @interpreter.
 * See cattle_interpreter_set_tape().
 *
 * The returned object is owned by @interpreter and must not be modified
 * of freed.
 *
 * Return: the memory tape for @interpreter.
 */
CattleTape*
cattle_interpreter_get_tape (CattleInterpreter *self)
{
    CattleTape *tape = NULL;

    g_return_val_if_fail (CATTLE_IS_INTERPRETER (self), NULL);

    if (G_LIKELY (!self->priv->disposed)) {
        tape = self->priv->tape;
    }

    return tape;
}